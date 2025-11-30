#include "DbUserModel.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

DbUserModel::DbUserModel(QObject *parent)
    : QAbstractListModel(parent)
{
    mpManager = make_unique<QNetworkAccessManager>();

    // init local db
    initLocalDb();

    // init websocket
    initSocketClient();

    // load data from server (if online)
    getUsers();
}

DbUserModel::~DbUserModel()
{
    qDeleteAll(mUserList);
    mUserList.clear();
}

void DbUserModel::testPendingOps()
{
    // Simulate offline insert
    handleInsertOffline("OfflineUser1", 25);
    handleInsertOffline("OfflineUser2", 30);

    // simulate offline delete
    handleDeleteOffline(-1);  // -1 for testing temp ID

    // check pending ops
    auto ops = mpLocalDB->loadPendingOperations();
    qDebug() << "Pending ops after inserts/deletes:";
    for (auto &op : ops)
        qDebug() << op;
}

QHash<int, QByteArray> DbUserModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[nameRole] = "Name";
    roles[ageRole] = "Age";
    roles[tableIdRole] = "TableId";
    return roles;
}

int DbUserModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return mUserList.count();
}

QVariant DbUserModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= mUserList.size())
        return QVariant();

    const DbUser *u = mUserList.at(index.row());
    if (!u) return QVariant();

    if (role == nameRole) return u->name();
    if (role == ageRole) return u->age();
    if (role == tableIdRole) return u->tableId();

    return QVariant();
}

void DbUserModel::initLocalDb()
{
    if (!mpLocalDB)
        mpLocalDB = make_unique<LocalDB>(this);

    if (!mpLocalDB->open())
    {
        qWarning() << "LocalDB open failed"; 
    }

    mpLocalDB->createTable();
    loadLocalUsers();
}

void DbUserModel::loadLocalUsers()
{
    auto list = mpLocalDB->loadUsers();
    beginResetModel();
    qDeleteAll(mUserList);
    mUserList.clear();
    for (const QVariantMap &m : list) {
        DbUser *u = new DbUser();
        u->setTableId(m["id"].toInt());
        u->setName(m["name"].toString());
        u->setAge(m["age"].toInt());
        mUserList.append(u);
    }
    endResetModel();
}

void DbUserModel::initSocketClient()
{
    if (!mpSocketClient)
        mpSocketClient = make_unique<WebSocketClient>(QUrl(WEBSOCKET_URL));

    // SERVER ONLINE: sync pending ops + update gui
    connect(mpSocketClient.get(), &WebSocketClient::serverOnline,
            this, &DbUserModel::onServerOnline);

    // SERVER OFFLINE: notify offline state
    connect(mpSocketClient.get(), &WebSocketClient::serverOffline, [this](){
        mServerOnline = false;
        qDebug() << "Server offline!";
    });

    mpSocketClient->start();
}

void DbUserModel::addUser(const QString &name, int age, int tableId, bool insertRows)
{
    auto it = std::find_if(mUserList.begin(), mUserList.end(), [&](DbUser* el){
        return el->name() == name && el->age() == age;
    });

    if (it == mUserList.end()) {
        if (insertRows)
            beginInsertRows(QModelIndex(), rowCount(), rowCount());

        DbUser *u = new DbUser();
        u->setName(name);
        u->setAge(age);
        u->setTableId(tableId);
        mUserList.append(u);

        if (insertRows)
            endInsertRows();
    }
}

void DbUserModel::createListFromLocalDb()
{
    loadLocalUsers();
}

void DbUserModel::createList(const QByteArray &jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isArray()) {
        qWarning() << "Expected array from server";
        return;
    }

    QJsonArray arr = doc.array();
    beginResetModel();
    qDeleteAll(mUserList);
    mUserList.clear();

    for (const QJsonValue &v : arr) {
        if (!v.isObject()) continue;
        QJsonObject o = v.toObject();
        QString name = o["name"].toString();
        int age = o["age"].toInt();
        int id = o["id"].toInt();

        // save local copy
        mpLocalDB->saveUser(name, age, id);

        DbUser *u = new DbUser();
        u->setName(name);
        u->setAge(age);
        u->setTableId(id);
        mUserList.append(u);
    }
    endResetModel();
}

void DbUserModel::sendUserToServer(const QString &name, int age)
{
    qDebug() << "sendUserToServer:" << name << age;

    // === CASE A : SERVER ONLINE ===
    if (mServerOnline)
    {
        QUrl url(SERVER_URL);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/json");

        QJsonObject userJson;
        userJson["name"] = name;
        userJson["age"] = age;

        QNetworkReply *reply = mpManager->sendCustomRequest(
            request,
            "POST",
            QJsonDocument(userJson).toJson()
            );

        QObject::connect(reply, &QNetworkReply::finished,
            [this, reply]() {

                if (reply->error() == QNetworkReply::NoError)
                {
                    QByteArray data = reply->readAll();
                    QJsonDocument doc = QJsonDocument::fromJson(data);
                    QJsonObject obj = doc.object();

                    QString name = obj["name"].toString();
                    int age = obj["age"].toInt();
                    int serverId = obj["id"].toInt();

                    // salva su db locale
                    mpLocalDB->saveUser(name, age, serverId);

                    // aggiorna UI
                    getUsers();
                }
                else
                {
                    qWarning() << "POST error:" << reply->errorString();
                    // fallback offline
                    handleInsertOffline("fallbackName", 0);
                }

                reply->deleteLater();
            });

        return;
    }

    // === CASE B : SERVER OFFLINE ===
    handleInsertOffline(name, age);
}


void DbUserModel::handleInsertOffline(const QString &name, int age)
{
    qDebug() << "Handling insert offline, name =" << name << " age =" << age;

    int tempId = mpLocalDB->generateTempId();

    // save locally
    mpLocalDB->saveUser(name, age, tempId);

    // save pending op
    mpLocalDB->addPendingOperation("insert", 0, tempId, name, age);

    // reload UI
    createListFromLocalDb();
}

void DbUserModel::handleDeleteOffline(int id)
{
    qDebug() << "Handling delete offline, id =" << id;
    if (id < 0) {
        // remove local unsynced insert
        mpLocalDB->deleteUser(id);
        mpLocalDB->removePendingInsertForLocalTempId(id);
        createListFromLocalDb();
        return;
    }
    // add pending delete
    mpLocalDB->addPendingOperation("delete", id, -1, "", -1);
    mpLocalDB->deleteUser(id);
    createListFromLocalDb();
}

void DbUserModel::deleteUserFromServer(int id)
{
    if (mServerOnline)
    {
        // ------- SERVER ONLINE: normal DELETE -------

        QUrl url(QString("%1/%2").arg(SERVER_URL).arg(id));
        QNetworkRequest request(url);

        QNetworkReply *reply = mpManager->sendCustomRequest(
            request,
            "DELETE"
            );

        connect(reply, &QNetworkReply::finished, [this, id, reply]() {
            if (reply->error() == QNetworkReply::NoError)
            {
                qDebug() << "User deleted on server OK.";

                mpLocalDB->deleteUser(id);
                getUsers();
            }
            else
            {
                qWarning() << "Error DELETE:" << reply->errorString();

                // fallback: salva offline
                mpLocalDB->addPendingOperation("delete", id, -1, "", -1);

                mpLocalDB->deleteUser(id);
                createListFromLocalDb();
            }

            reply->deleteLater();
        });
    }
    else
    {
        handleDeleteOffline(id);
    }
}

void DbUserModel::syncPendingOperations()
{
    if (!mServerOnline)
        return;

    auto ops = mpLocalDB->loadPendingOperations();

    for (auto &op : ops)
    {
        QString type = op["op_type"].toString();
        int serverId = op["server_id"].toInt();
        int localTempId = op["local_temp_id"].toInt();
        QString name = op["name"].toString();
        int age = op["age"].toInt();

        if (type == "insert")
        {
            // send insert to server
            sendUserToServer(name, age);
            mpLocalDB->removePendingOperation(op["pending_id"].toInt());
        }
        else if (type == "delete")
        {
            deleteUserFromServer(serverId);
            mpLocalDB->removePendingOperation(op["pending_id"].toInt());
        }
    }
}

void DbUserModel::flushPendingOperations()
{
    if (!mServerOnline) return;

    auto ops = mpLocalDB->loadPendingOperations();

    for (auto &op : ops)
    {
        QString type = op["op_type"].toString();
        int pendingId = op["pending_id"].toInt();
        int localTempId = op["local_temp_id"].toInt();
        int serverId = op["server_id"].toInt();
        QString name = op["name"].toString();
        int age = op["age"].toInt();

        if (type == "insert")
        {
            // POST insert
            QUrl url(SERVER_URL);
            QNetworkRequest req(url);
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

            QJsonObject obj;
            obj["name"] = name;
            obj["age"] = age;

            QNetworkReply *reply = mpManager->post(req, QJsonDocument(obj).toJson());

            connect(reply, &QNetworkReply::finished, this, [this, reply, pendingId, localTempId]() {
                if (reply->error() == QNetworkReply::NoError)
                {
                    QByteArray data = reply->readAll();
                    QJsonDocument doc = QJsonDocument::fromJson(data);
                    if (doc.isObject())
                    {
                        QJsonObject o = doc.object();
                        int serverId = o["id"].toInt();
                        QString name = o["name"].toString();
                        int age = o["age"].toInt();

                        // update local temp ID with final ID
                        mpLocalDB->deleteUser(localTempId);
                        mpLocalDB->insertUser(serverId, name, age);

                        // Remove pending op
                        mpLocalDB->removePendingOperation(pendingId);
                    }
                }
                else
                {
                    qWarning() << "Sync insert failed:" << reply->errorString();
                }
                reply->deleteLater();
            });
        }
        else if (type == "delete")
        {
            // DELETE
            QUrl url(QString("%1/%2").arg(SERVER_URL).arg(serverId));
            QNetworkRequest req(url);

            QNetworkReply *reply = mpManager->sendCustomRequest(req, "DELETE");

            connect(reply, &QNetworkReply::finished, this, [this, reply, pendingId, serverId]() {
                if (reply->error() == QNetworkReply::NoError)
                {
                    mpLocalDB->removePendingOperation(pendingId);
                    mpLocalDB->deleteUser(serverId);
                }
                else
                {
                    qWarning() << "Sync delete failed:" << reply->errorString();
                }
                reply->deleteLater();
            });
        }
    }
}

void DbUserModel::onServerOnline()
{
    mServerOnline = true;

    syncPendingOperations();
    createListFromLocalDb();
    getUsers();
}

void DbUserModel::getUsers()
{
    QUrl url(SERVER_URL);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // use GET (no body)
    QNetworkReply *reply = mpManager->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            createList(data);
        } else {
            qWarning() << "GET error:" << reply->errorString();
            // fallback to local DB
            createListFromLocalDb();
        }
        reply->deleteLater();
    });
}

void DbUserModel::processPendingDelete(QVariantMap op,
                                       QList<QVariantMap> ops,
                                       int index)
{
    int serverId = op["server_id"].toInt();
    int pendingId = op["pending_id"].toInt();

    qDebug() << "Processing pending DELETE for id =" << serverId;

    QUrl url(QString("%1/%2").arg(SERVER_URL).arg(serverId));
    QNetworkRequest req(url);

    QNetworkReply *reply = mpManager->sendCustomRequest(req, "DELETE");

    connect(reply, &QNetworkReply::finished, [=]() mutable {

        if (reply->error() == QNetworkReply::NoError)
        {
            mpLocalDB->removePendingOperation(pendingId);
            processNextPendingOperation(ops, index + 1);
        }
        else
        {
            qWarning() << "Pending delete failed:" << reply->errorString();
            // STOP sync
        }

        reply->deleteLater();
    });
}

void DbUserModel::processPendingInsert(QVariantMap op,
                                       QList<QVariantMap> ops,
                                       int index)
{
    QString name = op["name"].toString();
    int age = op["age"].toInt();
    int localTempId = op["local_temp_id"].toInt();
    int pendingId = op["pending_id"].toInt();

    qDebug() << "Processing pending INSERT:" << name << age << "temp id =" << localTempId;

    // ----- Send to server -----
    QUrl url(SERVER_URL);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["name"] = name;
    json["age"] = age;

    QNetworkReply *reply = mpManager->sendCustomRequest(
        req, "POST", QJsonDocument(json).toJson()
        );

    connect(reply, &QNetworkReply::finished, [=]() mutable {

        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray resp = reply->readAll();
            QJsonObject obj = QJsonDocument::fromJson(resp).object();

            int newId = obj["id"].toInt();

            // update local cache: replace temp id → new id
            mpLocalDB->replaceTempId(localTempId, newId);

            // remove pending op
            mpLocalDB->removePendingOperation(pendingId);

            // continue chain
            processNextPendingOperation(ops, index + 1);
        }
        else
        {
            qWarning() << "Insert sync failed:" << reply->errorString();
            // STOP sync → server offline again?
        }

        reply->deleteLater();
    });
}

void DbUserModel::processNextPendingOperation(const QList<QVariantMap> &ops, int index)
{
    if (index >= ops.size()) {
        qDebug() << "All pending operations processed.";
        getUsers();
        return;
    }

    QVariantMap op = ops[index];
    QString type = op["op_type"].toString();

    if (type == "insert")
        processPendingInsert(op, ops, index);
    else if (type == "delete")
        processPendingDelete(op, ops, index);
}


