#ifndef LOCALDB_H
#define LOCALDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include <QVariantMap>

class LocalDB : public QObject
{
    Q_OBJECT
public:
    explicit LocalDB(QObject *parent = nullptr);
    ~LocalDB();

    bool open();
    bool createTable();

    // users
    QList<QVariantMap> loadUsers();
    void insertUser(int id, const QString &name, int age); // insert or replace
    void saveUser(const QString &name, int age, int id);   // alias
    void deleteUser(int id);

    // pending ops
    void addPendingOperation(const QString &opType, int serverId, int localTempId, const QString &name, int age);
    QList<QVariantMap> loadPendingOperations();
    void removePendingOperation(int pendingId);
    bool removePendingInsertForLocalTempId(int tempId);

    // temp id generator
    int generateTempId();

    void replaceTempId(int tempId, int realId);

private:
    QSqlDatabase m_db;
};

#endif // LOCALDB_H
