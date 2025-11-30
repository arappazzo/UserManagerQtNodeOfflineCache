#include "localdb.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

LocalDB::LocalDB(QObject *parent) : QObject(parent)
{
}

LocalDB::~LocalDB()
{
    if (m_db.isOpen()) m_db.close();
}

bool LocalDB::open()
{
    if (QSqlDatabase::contains("local"))
        m_db = QSqlDatabase::database("local");
    else
        m_db = QSqlDatabase::addDatabase("QSQLITE", "local");

    m_db.setDatabaseName("local_users.db");
    if (!m_db.open()) {
        qWarning() << "Cannot open local DB:" << m_db.lastError().text();
        return false;
    }
    return true;
}

bool LocalDB::createTable()
{
    QSqlQuery q(m_db);
    const char *users_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT, "
        "age INTEGER)";
    if (!q.exec(users_sql)) {
        qWarning() << "Create users table FAILED:" << q.lastError().text();
        return false;
    }

    const char *pending_sql =
        "CREATE TABLE IF NOT EXISTS pending_ops ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "op_type TEXT NOT NULL,"
        "server_id INTEGER,"
        "local_temp_id INTEGER,"
        "name TEXT,"
        "age INTEGER,"
        "created_at INTEGER)";
    if (!q.exec(pending_sql)) {
        qWarning() << "Create pending_ops FAILED:" << q.lastError().text();
        return false;
    }

    return true;
}

QList<QVariantMap> LocalDB::loadUsers()
{
    QList<QVariantMap> out;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, name, age FROM users")) {
        qWarning() << "loadUsers failed:" << q.lastError().text();
        return out;
    }

    while (q.next()) {
        QVariantMap m;
        m["id"] = q.value(0).toInt();
        m["name"] = q.value(1).toString();
        m["age"] = q.value(2).toInt();
        out.append(m);
    }
    return out;
}

void LocalDB::insertUser(int id, const QString &name, int age)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO users (id, name, age) VALUES (?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(name);
    q.addBindValue(age);
    if (!q.exec()) {
        qWarning() << "insertUser failed:" << q.lastError().text();
    }
}

void LocalDB::saveUser(const QString &name, int age, int id)
{
    insertUser(id, name, age);
}

void LocalDB::deleteUser(int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM users WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec()) {
        qWarning() << "deleteUser failed:" << q.lastError().text();
    }
}

void LocalDB::addPendingOperation(const QString &opType, int serverId, int localTempId, const QString &name, int age)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO pending_ops (op_type, server_id, local_temp_id, name, age, created_at) VALUES (?, ?, ?, ?, ?, ?)");
    q.addBindValue(opType);
    if (serverId <= 0) q.addBindValue(QVariant()); else q.addBindValue(serverId);
    if (localTempId == 0) q.addBindValue(QVariant()); else q.addBindValue(localTempId);
    q.addBindValue(name);
    q.addBindValue(age);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        qWarning() << "addPendingOperation failed:" << q.lastError().text();
    }
}

QList<QVariantMap> LocalDB::loadPendingOperations()
{
    QList<QVariantMap> result;
    QSqlQuery q(m_db);
    if (!q.exec("SELECT id, op_type, server_id, local_temp_id, name, age, created_at FROM pending_ops ORDER BY created_at ASC")) {
        qWarning() << "loadPendingOperations FAILED:" << q.lastError().text();
        return result;
    }

    while (q.next()) {
        QVariantMap m;
        m["pending_id"] = q.value(0).toInt();
        m["op_type"] = q.value(1).toString();
        m["server_id"] = q.value(2).isNull() ? QVariant() : q.value(2).toInt();
        m["local_temp_id"] = q.value(3).isNull() ? QVariant() : q.value(3).toInt();
        m["name"] = q.value(4).toString();
        m["age"] = q.value(5).toInt();
        m["created_at"] = q.value(6).toLongLong();
        result.append(m);
    }
    return result;
}

void LocalDB::removePendingOperation(int pendingId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM pending_ops WHERE id = ?");
    q.addBindValue(pendingId);
    if (!q.exec()) {
        qWarning() << "removePendingOperation FAILED:" << q.lastError().text();
    }
}

bool LocalDB::removePendingInsertForLocalTempId(int tempId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM pending_ops WHERE op_type='insert' AND local_temp_id=?");
    q.addBindValue(tempId);
    if (!q.exec()) {
        qWarning() << "removePendingInsertForLocalTempId FAILED:" << q.lastError().text();
        return false;
    }
    return true;
}

int LocalDB::generateTempId()
{
    QSqlQuery q(m_db);
    q.exec("SELECT MIN(id) FROM users");
    if (q.next())
    {
        int minId = q.value(0).toInt();
        return (minId <= 0 ? minId - 1 : -1);
    }
    return -1;
}

void LocalDB::replaceTempId(int tempId, int realId)
{
    QSqlQuery q(m_db);

    q.prepare("UPDATE users SET id = ? WHERE id = ?");
    q.addBindValue(realId);
    q.addBindValue(tempId);

    if (!q.exec())
        qWarning() << "replaceTempId FAILED:" << q.lastError();
}
