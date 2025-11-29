#ifndef CHATMESSAGEMODEL_H
#define CHATMESSAGEMODEL_H

#include <QAbstractListModel>
#include "structures.h"

Q_DECLARE_METATYPE(ChatMessage)

/**
 * @brief Модель данных для отображения списка сообщений в конкретном чате.
 *
 * Наследуется от QAbstractListModel и предоставляет данные для QListView/QML.
 * Поддерживает операции добавления (в конец и начало), удаления, редактирования,
 * а также обновление статусов доставки.
 * Хранит данные в двух структурах: линейный список для отображения и Map для быстрого доступа по ID.
 */
class ChatMessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief Находит индекс последнего сообщения от указанного пользователя.
     * @param username Имя пользователя для поиска.
     * @return QModelIndex сообщения или невалидный индекс, если не найдено.
     */
    QModelIndex indexForUsername(const QString& username) const;

    /**
     * @brief Конструктор модели.
     * @param parent Родительский объект.
     */
    explicit ChatMessageModel(QObject *parent = nullptr);

    /**
     * @brief Находит индекс первого непрочитанного сообщения.
     *
     * Используется для автоматической прокрутки к непрочитанным при открытии чата.
     * @return QModelIndex первого сообщения со статусом != Read (для входящих).
     */
    QModelIndex findFirstUnreadMessage() const;

    /**
     * @brief Возвращает количество сообщений в модели.
     * @param parent Родительский индекс (не используется).
     * @return Число элементов в списке.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Получает данные для отображения.
     *
     * Поддерживает роли:
     * - Qt::DisplayRole: Текст сообщения
     * - UserRole: Структура ChatMessage целиком (через QVariant)
     *
     * @param index Индекс элемента.
     * @param role Роль данных.
     * @return QVariant с данными.
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /**
     * @brief Изменяет данные сообщения.
     * @param index Индекс элемента.
     * @param value Новое значение.
     * @param role Роль редактирования.
     * @return true, если данные обновлены.
     */
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

public slots:
    /**
     * @brief Добавляет одно новое сообщение в конец списка.
     * @param message Объект сообщения.
     */
    void addMessage(const ChatMessage &message);

    /**
     * @brief Добавляет пачку сообщений в конец списка.
     * Используется при получении новых сообщений или синхронизации.
     * @param messages Список сообщений.
     */
    void addMessages(const QList<ChatMessage> &messages);

    /**
     * @brief Вставляет сообщения в начало списка.
     *
     * Используется при подгрузке старой истории (пагинация вверх).
     * Сохраняет текущую позицию скролла (относительно).
     * @param messages Список старых сообщений.
     */
    void prependMessages(const QList<ChatMessage> &messages);

    /** @brief Удаляет все сообщения из модели. */
    void clearMessages();

    /**
     * @brief Удаляет сообщение по его ID.
     * @param messageId ID удаляемого сообщения.
     */
    void removeMessage(qint64 messageId);

    /**
     * @brief Подтверждает отправку сообщения (Optimistic UI).
     *
     * Заменяет временный ID (tempId) и статус "Sending" на реальный ID сервера и timestamp.
     * @param tempId Временный локальный ID.
     * @param confirmedMessage Данные сообщения, вернувшиеся от сервера.
     */
    void confirmMessage(const QString& tempId, const ChatMessage& confirmedMessage);

    /**
     * @brief Обновляет статус доставки сообщения (Sent/Delivered/Read).
     * @param messageId ID сообщения.
     * @param newStatus Новый статус.
     */
    void updateMessageStatus(qint64 messageId, ChatMessage::MessageStatus newStatus);

    /**
     * @brief Редактирует текст сообщения.
     * @param messageId ID сообщения.
     * @param newPayload Новый текст.
     */
    void editMessage(qint64 messageId, const QString& newPayload);

    /**
     * @brief Быстрый поиск сообщения по ID.
     * @param id Идентификатор сообщения.
     * @param msg [out] Ссылка для записи найденного сообщения.
     * @return true, если сообщение найдено.
     */
    bool getMessageById(qint64 id, ChatMessage &msg) const;

signals:
    /**
     * @brief Сигнал о необходимости отправить отчет о прочтении.
     * Генерируется, когда сообщение отображается в представлении (View).
     * @param messageId ID прочитанного сообщения.
     */
    void messageNeedsReadReceipt(qint64 messageId);

private:
    /** @brief Основное хранилище сообщений (упорядоченный список). */
    QList<ChatMessage> m_messages;

    /**
     * @brief Индекс для быстрого поиска сообщений по ID.
     * Дублирует данные для оптимизации методов update/edit/remove.
     */
    QMap<qint64, ChatMessage> m_messageMap;
};

#endif // CHATMESSAGEMODEL_H
