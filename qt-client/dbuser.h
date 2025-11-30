#ifndef DBUSER_H
#define DBUSER_H

#include <QObject>
#include <QString>

class DbUser : public QObject
{
    Q_OBJECT

public:
    explicit DbUser(QObject *parent = nullptr);

    QString name() const;
    void setName(const QString &name);

    int age() const;
    void setAge(int age);

    int tableId() const;
    void setTableId(int id);

private:
    QString m_name;
    int m_age;
    int m_tableId;
};

#endif // DBUSER_H
