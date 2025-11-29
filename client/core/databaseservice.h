#ifndef DATABASE_SERVICE_H
#define DATABASE_SERVICE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "structures.h"

/**
 * @brief Сервис для работы с локальной базой данных SQLite.
 *
 * Отвечает за хранение сообщений, чатов, пользователей, черновиков и статусов прочтения.
 * Обеспечивает персистентность данных между запусками приложения.
 */
class DatabaseService : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Конструктор сервиса базы данных.
     * @param parent Родительский объект.
     */
    explicit DatabaseService(QObject *parent = nullptr);

    /**
     * @brief Деструктор. Закрывает соединение с базой.
     */
    ~DatabaseService();

    /**
     * @brief Инициализирует подключение к базе данных.
     *
     * Открывает файл БД, создает необходимые таблицы, если они отсутствуют,
     * и выполняет миграции при необходимости.
     *
     * @param dbPath Путь к файлу базы данных (по умолчанию "messenger.sqlite")
     * @return true при успешной инициализации, false при ошибке
     */
    bool initialize(const QString &dbPath = "messenger.sqlite");

    /**
     * @brief Проверяет статус подключения к БД.
     * @return true, если база открыта и доступна
     */
    bool isConnected() const;

    /**
     * @brief Закрывает соединение с базой данных.
     */
    void close();

    /**
     * @brief Сохраняет сообщение в историю.
     * @param msg Объект сообщения для сохранения
     * @param currentUsername Имя текущего пользователя (владельца истории)
     * @return true при успешной записи
     */
    bool saveMessage(const ChatMessage &msg, const QString &currentUsername);

    /**
     * @brief Обновляет статус конкретного сообщения.
     * @param messageId ID сообщения
     * @param status Новый статус (например, Read или Delivered)
     * @return true при успешном обновлении
     */
    bool updateMessageStatus(qint64 messageId, ChatMessage::MessageStatus status);

    /**
     * @brief Помечает все сообщения в чате как прочитанные/доставленные.
     * @param withUser Имя собеседника (ID чата)
     * @param currentUsername Имя текущего пользователя
     * @param status Статус, который нужно установить
     * @return true при успехе
     */
    bool updateAllMessagesStatusForChat(const QString &withUser, const QString &currentUsername,
                                        ChatMessage::MessageStatus status);

    /**
     * @brief Загружает последние сообщения чата (для инициализации UI).
     * @param fromUser Имя отправителя
     * @param toUser Имя получателя
     * @param limit Количество сообщений (по умолчанию 20)
     * @return Список сообщений, отсортированный по времени
     */
    QList<ChatMessage> loadRecentMessages(const QString &fromUser, const QString &toUser, int limit = 20);

    /**
     * @brief Обновляет метаданные чата (имя, аватар и т.д.).
     * @param chat Объект с обновленными данными чата
     */
    void updateChatMetadata(const Chat& chat);

    /**
     * @brief Добавляет новый чат или обновляет существующий.
     * @param chat Объект чата
     */
    void addOrUpdateChat(const Chat& chat);

    /**
     * @brief Обновляет счетчик непрочитанных сообщений для пользователя.
     * @param username Имя пользователя, от которого сообщения
     * @param count Новое количество непрочитанных
     */
    void updateOrAddUnreadCount(const QString& username, int count);

    /**
     * @brief Загружает более старые сообщения (пагинация вверх).
     * @param fromUser Имя собеседника 1
     * @param toUser Имя собеседника 2
     * @param beforeId ID сообщения, до которого нужно загружать историю
     * @param limit Лимит сообщений
     * @return Список старых сообщений
     */
    QList<ChatMessage> loadOlderMessages(const QString &fromUser, const QString &toUser,
                                         qint64 beforeId, int limit = 20);

    /**
     * @brief Загружает все сообщения, связанные с текущим пользователем (для экспорта/поиска).
     * @param currentUsername Имя текущего пользователя
     * @return Полный список сообщений
     */
    QList<ChatMessage> loadMessagesForUser(const QString &currentUsername);

    /**
     * @brief Получает ID самого старого сообщения в переписке.
     * @param fromUser Собеседник 1
     * @param toUser Собеседник 2
     * @return ID сообщения или -1, если история пуста
     */
    qint64 getOldestMessageId(const QString &fromUser, const QString &toUser);

    /**
     * @brief Удаляет сообщение по ID.
     * @param messageId Идентификатор сообщения
     * @return true при успешном удалении
     */
    bool deleteMessage(qint64 messageId);

    /**
     * @brief Редактирует текст сообщения.
     * @param messageId ID редактируемого сообщения
     * @param newPayload Новый текст
     * @return true при успехе
     */
    bool editMessage(qint64 messageId, const QString &newPayload);

    /**
     * @brief Получает количество непрочитанных сообщений в конкретном чате.
     * @param fromUser Имя собеседника
     * @param currentUsername Имя текущего пользователя
     * @return Число непрочитанных
     */
    int getUnreadCountForChat(const QString &fromUser, const QString &currentUsername);

    /**
     * @brief Получает карту непрочитанных сообщений по всем чатам.
     * @param currentUsername Имя текущего пользователя
     * @return Map, где ключ - имя собеседника, значение - число сообщений
     */
    QMap<QString, int> getAllUnreadCounts(const QString &currentUsername);

    /**
     * @brief Полностью очищает все данные в БД.
     * @return true при успешной очистке
     */
    bool clearAllData();

    /**
     * @brief Выводит статистику базы данных в лог (размер, число записей).
     */
    void printDatabaseStats();

    /**
     * @brief Подтверждает отправку сообщения, заменяя временный ID на постоянный.
     * @param tempId Временный ID, присвоенный при отправке
     * @param confirmedMsg Сообщение с сервера с постоянным ID и Timestamp
     * @return true если обновление прошло успешно
     */
    bool confirmSentMessageByTempId(const QString& tempId, const ChatMessage& confirmedMsg);

    /**
     * @brief Сохраняет черновик сообщения для чата.
     * @param username Имя собеседника (чат)
     * @param ownerUsername Владелец черновика
     * @param draftText Текст черновика
     * @return true при успехе
     */
    bool saveDraft(const QString& username, const QString& ownerUsername, const QString& draftText);

    /**
     * @brief Загружает сохраненный черновик.
     * @param username Имя собеседника
     * @param ownerUsername Владелец черновика
     * @return Текст черновика или пустая строка
     */
    QString loadDraft(const QString& username, const QString& ownerUsername);

    /**
     * @brief Удаляет черновик из базы.
     * @param username Имя собеседника
     * @param ownerUsername Владелец черновика
     */
    void deleteDraft(const QString& username, const QString& ownerUsername);

    /**
     * @brief Загружает последнее сообщение чата для отображения в списке контактов.
     * @param username Имя собеседника
     * @param ownerUsername Владелец чата
     * @return Объект последнего сообщения
     */
    ChatMessage loadLastMessage(const QString& username, const QString& ownerUsername);

    /**
     * @brief Загружает список всех известных чатов.
     * @return Список объектов Chat
     */
    QList<Chat> loadAllChats();

    /**
     * @brief Загружает всех известных пользователей.
     * @return Список объектов User
     */
    QList<User> loadAllUsers();

    /**
     * @brief Получает информацию о чате по имени пользователя.
     * @param username Имя искомого пользователя
     * @return Объект Chat или пустой объект, если не найден
     */
    Chat getChatByUsername(const QString& username);

    /**
     * @brief Добавляет или обновляет информацию о пользователе.
     * @param user Объект пользователя с данными
     */
    void addOrUpdateUser(const User& user);

signals:
    /**
     * @brief Сигнал ошибки базы данных.
     * @param error Текст ошибки
     */
    void databaseError(const QString &error);

private:
    QSqlDatabase m_db;          ///< Объект подключения Qt SQL
    bool m_initialized = false; ///< Флаг успешной инициализации

    /**
     * @brief Создает структуру таблиц (messages, chats, users, drafts).
     * @return true при успешном создании
     */
    bool createTables();

    /**
     * @brief Выполняет миграцию схемы БД при обновлении версии приложения.
     * @return true при успехе
     */
    bool migrateDatabase();

    /**
     * @brief Обработчик ошибок SQL запросов.
     * @param query Объект запроса, в котором произошла ошибка
     * @return Строка с описанием ошибки для логирования
     */
    QString executeSqlError(const QSqlQuery &query);
};

#endif // DATABASE_SERVICE_H
