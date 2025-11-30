#ifndef DBUSERMODEL_H
#define DBUSERMODEL_H

#include <QAbstractListModel>
#include <memory>
#include "DbUser.h"
#include "LocalDB.h"
#include "websocketclient.h"

class QNetworkAccessManager;

using namespace std;

class DbUserModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        nameRole = Qt::UserRole + 1,
        ageRole,
        tableIdRole
    };

    explicit DbUserModel(QObject *parent = nullptr);
    ~DbUserModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void sendUserToServer(const QString &name, int age);
    Q_INVOKABLE void deleteUserFromServer(int id);

private:
    void initLocalDb();
    void initSocketClient();
    void loadLocalUsers();

    void createListFromLocalDb();
    void createList(const QByteArray &jsonData);

    void addUser(const QString &name, int age, int tableId, bool insertRows = false);

    // offline/online helpers
    void handleInsertOffline(const QString &name, int age);
    void handleDeleteOffline(int id);

    void syncPendingOperations();
    void flushPendingOperations(); // alias for clarity
    void onServerOnline();

    void testPendingOps();

    // network
    void getUsers();

private:
    QList<DbUser*> mUserList;
    unique_ptr<QNetworkAccessManager> mpManager;
    unique_ptr<LocalDB> mpLocalDB;
    unique_ptr<WebSocketClient> mpSocketClient;

    bool mServerOnline = false;
    const QString SERVER_URL = QStringLiteral("http://localhost:3000/api/users");
    const QString WEBSOCKET_URL = QStringLiteral("ws://localhost:3001");

    void processPendingDelete(QVariantMap op,
                              QList<QVariantMap> ops,
                              int index);

    void processPendingInsert(QVariantMap op,
                         QList<QVariantMap> ops,
                              int index);

    void processNextPendingOperation(const QList<QVariantMap> &ops, int index);

};

#endif // DBUSERMODEL_H
