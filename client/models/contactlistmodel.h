#ifndef CONTACTLISTMODEL_H
#define CONTACTLISTMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QMap>
#include "structures.h"

class DataService;

/**
 * @brief Модель списка контактов/чатов для главного экрана.
 *
 * Предоставляет данные для отображения списка диалогов. Данные подтягиваются
 * напрямую из DataService, модель хранит только упорядоченный список имен пользователей (usernames).
 * Реализует множество кастомных ролей для отображения аватаров, статусов и последних сообщений.
 */
class ContactListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /**
     * @brief Роли модели для доступа к полям контакта и чата.
     */
    enum ContactRoles {
        UsernameRole = Qt::UserRole + 1,  ///< Уникальное имя пользователя (ID чата)
        DisplayNameRole,                  ///< Отображаемое имя
        IsOnlineRole,                     ///< Статус "В сети"
        LastSeenRole,                     ///< Время последней активности
        IsTypingRole,                     ///< Флаг "печатает..."
        UnreadCountRole,                  ///< Число непрочитанных сообщений
        LastMessageRole,                  ///< Текст последнего сообщения
        LastMessageTimestampRole,         ///< Время последнего сообщения (для сортировки и UI)
        IsLastMessageOutgoingRole,        ///< Флаг исходящего сообщения (для иконки "Вы:")
        IsPinnedRole,                     ///< Флаг закрепленного чата
        DraftTextRole,                    ///< Текст черновика, если есть
        AvatarRole,                       ///< Путь к файлу аватара
        ChatRole                          ///< Объект Chat целиком
    };

    /**
     * @brief Конструктор модели.
     * @param dataService Указатель на DataService, источник данных.
     * @param parent Родительский объект.
     */
    explicit ContactListModel(DataService* dataService, QObject *parent = nullptr);

    /**
     * @brief Возвращает количество чатов в списке.
     * @param parent Не используется.
     * @return Количество элементов.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Ищет индекс чата по имени пользователя.
     * @param username Имя пользователя.
     * @return QModelIndex найденного элемента или invalid, если не найден.
     */
    QModelIndex indexForUsername(const QString& username) const;

    /**
     * @brief Получает данные для отображения.
     *
     * Поддерживает все роли из ContactRoles. Данные извлекаются из кешей DataService
     * (UserCache и ChatMetadataCache) в реальном времени.
     *
     * @param index Индекс элемента.
     * @param role Запрашиваемая роль.
     * @return Данные в формате QVariant.
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    /** @brief Очищает список контактов в модели. */
    void clear();

signals:
    // Сигналы наследуются от QAbstractItemModel (dataChanged, rowsInserted и т.д.)

public slots:
    /**
     * @brief Полностью обновляет список контактов новыми данными.
     *
     * Вызывает reset модели.
     * @param usernames Новый список имен пользователей (уже отсортированный).
     */
    void updateContacts(const QStringList& usernames);

    /**
     * @brief Обновляет данные для конкретного контакта по имени.
     *
     * Вызывает dataChanged для соответствующей строки.
     * @param username Имя пользователя, данные которого изменились.
     */
    void refreshContact(const QString& username);

    /**
     * @brief Обновляет данные контакта по индексу.
     * @param index Индекс строки для обновления.
     */
    void refreshContact(const QModelIndex& index);

    /**
     * @brief Слот реакции на изменение метаданных чата.
     * @param username Имя пользователя чата.
     */
    void onChatMetadataChanged(const QString& username);

    /**
     * @brief Перезапрашивает актуальный список чатов у DataService и обновляет модель.
     * Используется при изменении сортировки или добавлении новых чатов.
     */
    void refreshList();

private:
    DataService* m_dataService;        ///< Источник данных (кеши пользователей и чатов)
    QStringList m_contactUsernames;    ///< Текущий список контактов (ключи для доступа к данным)
};

#endif // CONTACTLISTMODEL_H
