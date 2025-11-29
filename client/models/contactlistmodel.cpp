#include "contactlistmodel.h"
#include "mainwindow.h"
#include "dataservice.h"
#include <QSet>
#include <QDebug>

ContactListModel::ContactListModel(DataService* dataService, QObject *parent)
    : QAbstractListModel(parent)
    , m_dataService(dataService)
{
    // Обновление списка контактов при изменении набора юзеров в DataService
    connect(m_dataService, &DataService::contactsUpdated,
            this, &ContactListModel::updateContacts);
}

int ContactListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_contactUsernames.count();
}

QModelIndex ContactListModel::indexForUsername(const QString& username) const
{
    for (int i = 0; i < m_contactUsernames.size(); ++i) {
        if (m_contactUsernames[i] == username)
            return index(i, 0);
    }
    return QModelIndex();
}

QVariant ContactListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_contactUsernames.size())
        return QVariant();

    QString username = m_contactUsernames.at(index.row());
    const User* user = m_dataService->getUserFromCache(username);
    const Chat& chat = m_dataService->getChatMetadata(username);

    if (!user)
        return QVariant();

    switch (role) {
    case UsernameRole:
        return username;
    case DisplayNameRole:
        return user->displayName;
    case IsOnlineRole:
        return user->isOnline;
    case LastSeenRole:
        return user->lastSeen;
    case IsTypingRole:
        return user->isTyping;

    case UnreadCountRole:
        return !chat.username.isEmpty() ? chat.unreadCount : 0;
    case LastMessageRole:
        return !chat.username.isEmpty() ? chat.lastMessagePayload : QString();
    case LastMessageTimestampRole:
        return !chat.username.isEmpty() ? chat.lastMessageTimestamp : QString();
    case IsLastMessageOutgoingRole:
        return !chat.username.isEmpty() ? chat.isLastMessageOutgoing : false;
    case IsPinnedRole:
        return !chat.username.isEmpty() ? chat.isPinned : false;
    case DraftTextRole:
        return !chat.username.isEmpty() ? chat.draftText : QString();

    case Qt::UserRole:
        return QVariant::fromValue(user);
    case ChatRole:
        return QVariant::fromValue(chat);
    case AvatarRole:
        return user->avatarUrl;

    default:
        return QVariant();
    }
}

void ContactListModel::clear()
{
    beginResetModel();
    m_contactUsernames.clear();
    endResetModel();
    qDebug() << "[ContactListModel] Contact list cleared";
}

void ContactListModel::refreshContact(const QString& username)
{
    int row = m_contactUsernames.indexOf(username);
    if (row == -1) {
        qWarning() << "[ContactListModel] Cannot refresh: user not in list:" << username;
        return;
    }

    QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
    qDebug() << "[ContactListModel] Refreshed contact:" << username << "at row:" << row;
}

void ContactListModel::refreshContact(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    emit dataChanged(index, index);
    qDebug() << "[ContactListModel] Refreshed contact at row:" << index.row();
}

void ContactListModel::updateContacts(const QStringList& usernames)
{
    Q_UNUSED(usernames);
    // Перечитываем отсортированный список чатов из DataService
    refreshList();
}

void ContactListModel::onChatMetadataChanged(const QString& username)
{
    // Текущая позиция чата
    int oldIndex = m_contactUsernames.indexOf(username);

    if (oldIndex == -1) {
        qDebug() << "[ContactListModel] Chat not in list, refreshing";
        refreshList();
        return;
    }

    // Новый порядок с учётом пинов, времени последнего сообщения и т.п.
    QStringList newSortedList = m_dataService->getSortedChatList();
    int newIndex = newSortedList.indexOf(username);

    if (newIndex == -1) {
        qDebug() << "[ContactListModel] Chat removed from list";
        refreshList();
        return;
    }

    if (oldIndex == newIndex) {
        // Порядок не изменился — просто обновляем данные по контакту
        refreshContact(username);
    } else {
        // Позиция изменилась — двигаем строку с beginMoveRows/endMoveRows
        int targetRow = (newIndex > oldIndex) ? newIndex + 1 : newIndex;

        beginMoveRows(QModelIndex(), oldIndex, oldIndex,
                      QModelIndex(), targetRow);

        m_contactUsernames = newSortedList;

        endMoveRows();
    }
}

void ContactListModel::refreshList()
{
    beginResetModel();
    // Берем актуальный отсортированный список чатов из DataService
    m_contactUsernames = m_dataService->getSortedChatList();
    endResetModel();
}
