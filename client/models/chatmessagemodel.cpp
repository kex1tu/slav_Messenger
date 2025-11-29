#include "chatmessagemodel.h"
#include <QDebug>  
#include <QDate>
ChatMessageModel::ChatMessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Регистрируем тип ChatMessage для использования в QVariant и сигналах
    qRegisterMetaType<ChatMessage>();
    qDebug() << "[ChatMessageModel] Конструктор вызван";
}

QModelIndex ChatMessageModel::findFirstUnreadMessage() const
{
    for (int i = 0; i < m_messages.count(); ++i) {
        const ChatMessage& msg = m_messages.at(i);

        // Первое входящее сообщение со статусом Sent или Delivered считаем непрочитанным
        if (!msg.isOutgoing &&
            (msg.status == ChatMessage::Delivered ||
             msg.status == ChatMessage::Sent)) {
            qDebug() << "[ChatMessageModel] First unread message found at row:" << i;
            return index(i, 0);
        }
    }

    qDebug() << "[ChatMessageModel] No unread messages found.";
    return QModelIndex();
}

int ChatMessageModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_messages.count();
}

QVariant ChatMessageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.count()) {
        qDebug() << "[ChatMessageModel] data: invalid index" << index.row();
        return QVariant();
    }

    const ChatMessage &message = m_messages.at(index.row());
    if (role == Qt::UserRole) {
        return QVariant::fromValue(message);
    }

    return QVariant();
}

bool ChatMessageModel::setData(const QModelIndex &index,
                               const QVariant &value,
                               int role)
{
    if (!index.isValid() || role != Qt::UserRole) {
        qDebug() << "[ChatMessageModel] setData: invalid index or role";
        return false;
    }

    const ChatMessage& msg = value.value<ChatMessage>();

    // Обновляем только если id совпадает (защита от рассинхронизации)
    if (m_messages[index.row()].id == msg.id) {
        m_messages[index.row()] = msg;
        emit dataChanged(index, index, { Qt::UserRole });
        qDebug() << "[ChatMessageModel] setData: message updated for index" << index.row();
        return true;
    }

    return false;
}

void ChatMessageModel::addMessage(const ChatMessage &message)
{
    const int newRow = m_messages.count();

    beginInsertRows(QModelIndex(), newRow, newRow);
    m_messages.append(message);
    if (message.id > 0)
        m_messageMap[message.id] = message;
    endInsertRows();

    qDebug() << "[ChatMessageModel] addMessage: добавлено сообщение с id"
             << message.id << "rows:" << newRow + 1;
}

void ChatMessageModel::addMessages(const QList<ChatMessage> &messages)
{
    if (messages.isEmpty())
        return;

    int first = m_messages.count();
    int last  = first + messages.count() - 1;

    beginInsertRows(QModelIndex(), first, last);
    m_messages.append(messages);
    for (const auto& msg : messages) {
        m_messageMap[msg.id] = msg;
    }
    endInsertRows();

    qDebug() << "[ChatMessageModel] addMessages: добавлено"
             << messages.size() << "сообщений. Новый размер:"
             << m_messages.size();
}

void ChatMessageModel::prependMessages(const QList<ChatMessage> &messages)
{
    if (messages.isEmpty())
        return;

    beginInsertRows(QModelIndex(), 0, messages.count() - 1);

    // Добавляем сообщения в начало в обратном порядке,
    // чтобы сохранить хронологию
    for (int i = messages.count() - 1; i >= 0; --i) {
        const auto& msg = messages.at(i);
        m_messages.prepend(msg);
        m_messageMap[msg.id] = msg;
    }

    endInsertRows();
    qDebug() << "[ChatMessageModel] prependMessages: prepend"
             << messages.size();
}

void ChatMessageModel::clearMessages()
{
    if (m_messages.isEmpty())
        return;

    beginResetModel();
    m_messages.clear();
    m_messageMap.clear();
    endResetModel();

    qDebug() << "[ChatMessageModel] clearMessages: очищены все сообщения";
}

void ChatMessageModel::removeMessage(qint64 messageId)
{
    for (int i = 0; i < m_messages.count(); ++i) {
        if (m_messages[i].id == messageId) {
            beginRemoveRows(QModelIndex(), i, i);
            m_messages.removeAt(i);
            m_messageMap.remove(messageId);
            endRemoveRows();

            qDebug() << "[ChatMessageModel] removeMessage: сообщение id"
                     << messageId << "удалено из строки" << i;
            return;
        }
    }
    qDebug() << "[ChatMessageModel] removeMessage: сообщение id"
             << messageId << "не найдено для удаления";
}

void ChatMessageModel::confirmMessage(const QString& tempId,
                                      const ChatMessage& confirmedMessage)
{
    for (int i = 0; i < m_messages.count(); ++i) {
        if (m_messages[i].tempId == tempId &&
            !m_messages[i].tempId.isEmpty())
        {
            m_messages[i] = confirmedMessage;
            m_messageMap[confirmedMessage.id] = confirmedMessage;

            QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx, { Qt::UserRole });

            qDebug() << "[ChatMessageModel] confirmMessage: подтверждено tempId"
                     << tempId << "-> id" << confirmedMessage.id;
            return;
        }
    }
    qDebug() << "[ChatMessageModel] confirmMessage: tempId"
             << tempId << "не найден";
}

void ChatMessageModel::updateMessageStatus(qint64 messageId,
                                           ChatMessage::MessageStatus newStatus)
{
    for (int i = 0; i < m_messages.count(); ++i) {
        if (m_messages[i].id == messageId) {
            m_messages[i].status = newStatus;
            if (m_messageMap.contains(messageId))
                m_messageMap[messageId].status = newStatus;

            QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx, { Qt::UserRole });

            qDebug() << "[ChatMessageModel] updateMessageStatus: обновлён статус для id"
                     << messageId << "->" << newStatus;
            return;
        }
    }
    qDebug() << "[ChatMessageModel] updateMessageStatus: сообщение id"
             << messageId << "не найдено";
}

void ChatMessageModel::editMessage(qint64 messageId,
                                   const QString& newPayload)
{
    for (int i = 0; i < m_messages.count(); ++i) {
        if (m_messages[i].id == messageId) {
            m_messages[i].payload  = newPayload;
            m_messages[i].isEdited = true;

            if (m_messageMap.contains(messageId)) {
                m_messageMap[messageId].payload  = newPayload;
                m_messageMap[messageId].isEdited = true;
            }

            QModelIndex idx = index(i, 0);
            emit dataChanged(idx, idx, { Qt::UserRole });

            qDebug() << "[ChatMessageModel] editMessage: сообщение id"
                     << messageId << "отредактировано";
            return;
        }
    }
    qDebug() << "[ChatMessageModel] editMessage: сообщение id"
             << messageId << "не найдено для редактирования";
}

bool ChatMessageModel::getMessageById(qint64 id, ChatMessage &msg) const
{
    if (m_messageMap.contains(id)) {
        msg = m_messageMap.value(id);
        return true;
    }

    qDebug() << "[ChatMessageModel] getMessageById:" << id << "не найден";
    return false;
}
