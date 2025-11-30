#include "dbuser.h"
#include <qobject.h>

DbUser::DbUser(QObject *parent)
    : QObject(parent),
    m_age(0),
    m_tableId(0)
{
}

QString DbUser::name() const
{
    return m_name;
}

void DbUser::setName(const QString &name)
{
    m_name = name;
}

int DbUser::age() const
{
    return m_age;
}

void DbUser::setAge(int age)
{
    m_age = age;
}

int DbUser::tableId() const
{
    return m_tableId;
}

void DbUser::setTableId(int id)
{
    m_tableId = id;
}
