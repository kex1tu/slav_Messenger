#ifndef DATASERVICE_H
#define DATASERVICE_H

#include <QObject>
#include <QMap>
#include <QJsonObject>
#include "structures.h"
#include "cryptoutils.h"
#include <QTimer>
#include <databaseservice.h>
#include "avatarcache.h"

/**
 * @brief Основной сервис управления состоянием приложения и данными.
 *
 * DataService является центральным узлом, координирующим взаимодействие
 * между сетевым слоем, базой данных и UI. Он управляет кешем пользователей,
 * чатов, сообщений, а также обрабатывает все входящие JSON-ответы от сервера.
 */
class DataService : public QObject
{
    Q_OBJECT
public:

    /**
     * @brief Конструктор сервиса данных.
     * @param parent Родительский объект.
     */
    explicit DataService(QObject *parent = nullptr);

    // --- Управление файлами ---

    /**
     * @brief Сохраняет информацию о загруженном файле в кеш.
     * @param fileId Уникальный ID файла на сервере
     * @param fileName Имя файла
     * @param fileUrl URL для скачивания
     */
    void cacheUploadedFile(QString fileId, const QString& fileName, const QString& fileUrl);

    /**
     * @brief Получает текущий кеш загруженных файлов.
     * @return Указатель на Map с файлами
     */
    QMap<QString, CachedFile>* getCachedFiles();

    /** @brief Удаляет файл из кеша загрузок по ID. */
    void removeCachedFile(QString fileId);

    /** @brief Полностью очищает кеш загруженных файлов. */
    void clearUploadedFilesCache();

    /** @brief Возвращает экземпляр сервиса кеширования аватаров. */
    AvatarCache* getAvatarCache();

    // --- Аутентификация ---

    /**
     * @brief Запускает процесс входа пользователя.
     * @param username Имя пользователя
     * @param password Пароль (будет захеширован перед отправкой)
     */
    void loginProcess(const QString& username, const QString& password);

    /**
     * @brief Запускает процесс регистрации нового пользователя.
     * @param username Логин
     * @param displayName Отображаемое имя
     * @param password Пароль
     */
    void registerProcess(const QString& username, const QString& displayName, const QString& password);

    // --- Управление пользователями ---

    /** @brief Получает данные текущего авторизованного пользователя. */
    const User* getCurrentUser();

    /** @brief Обновляет или устанавливает данные текущего пользователя. */
    void updateOrAddCurrentUser(const User& user);

    /** @brief Получает криптографический менеджер текущего пользователя. */
    const CryptoManager* getCurrentUserCrypto();

    /** @brief Устанавливает крипто-менеджер для текущей сессии. */
    void updateOrAddCurrentUserCrypto(CryptoManager* cryptoManager);

    /** @brief Получает данные пользователя, с которым открыт чат. */
    const User* getCurrentChatPartner();

    /** @brief Устанавливает текущего собеседника (при открытии чата). */
    void updateOrAddCurrentChatPartner(const User& user);

    // --- Состояние UI чата ---

    /** @brief Возвращает true, если в данный момент загружается история сообщений. */
    bool getIsLoadingHistory() const;

    /** @brief Устанавливает флаг загрузки истории. */
    void updateOrAddIsLoadingHistory(bool isLoadingHistory);

    /** @brief Получает ID самого старого загруженного сообщения (для пагинации). */
    qint64 getOldestMessageId() const;

    /** @brief Обновляет ID самого старого сообщения. */
    void updateOrAddOldestMessageId(qint64 messageId);

    /** @brief Получает ID сообщения, которое сейчас редактируется. */
    qint64 getEditinigMessageId() const;

    /** @brief Устанавливает режим редактирования для сообщения ID. */
    void updateOrAddEditingMessageId(qint64 messageId);

    /** @brief Получает ID сообщения, на которое пишется ответ. */
    qint64 getReplyToMessageId() const;

    /** @brief Устанавливает ID сообщения для ответа. */
    void updateOrAddReplyToMessageId(qint64 replyMessageId);

    // --- Таймеры ---

    /** @brief Таймер для задержки глобального поиска (debounce). */
    QTimer* getGlobalSearchTimer();

    /** @brief Таймер отправки статуса "печатает...". */
    QTimer* getTypingSendTimer();

    /** @brief Таймеры сброса статуса "печатает..." для входящих событий. */
    const QMap<QString, QTimer*>* getTypingRecieveTimers();

    // --- Кеши ---

    /** @brief Ищет пользователя в локальном кеше по имени. */
    const User* getUserFromCache(const QString& username);

    /** @brief Добавляет или обновляет пользователя в кеше. */
    void updateOrAddUser(const User& user);

    /** @brief Возвращает полный кеш чатов. */
    const QMap<QString, ChatCache>& getChatCache();

    /** @brief Обновляет кеш чата для конкретного пользователя. */
    void updateOrAddChatCache(const QString& username, const ChatCache& chatCache);

    /** @brief Возвращает карту непрочитанных сообщений. */
    const QMap<QString, int>& getUnreadCounts();

    /** @brief Устанавливает количество непрочитанных для чата. */
    void updateOrAddUnreadCount(const QString& username, int count);

    /** @brief Получает указатель на кеш чата для модификации. */
    ChatCache* getChatCacheForUser(const QString& username);

    /** @brief Возвращает указатель на кеш пользователей. */
    QMap<QString, User> * getUserCache();

    /** @brief Возвращает доступ к сервису базы данных. */
    DatabaseService* getDatabaseService();

    /** @brief Получает метаданные чата (настройки, статус). */
    const Chat& getChatMetadata(const QString& username) const;

    /** @brief Обновляет метаданные чата. */
    void updateOrAddChatMetadata(const Chat& chat);

    /** @brief Обновляет последнее сообщение в списке чатов (для превью). */
    void updateLastMessage(const QString& username, const ChatMessage& message);

    /** @brief Увеличивает счетчик непрочитанных на 1. */
    void incrementUnreadCount(const QString& username);

    /** @brief Сбрасывает счетчик непрочитанных в 0. */
    void resetUnreadCount(const QString& username);

    /** @brief Возвращает отсортированный список чатов (по времени/закрепу). */
    QStringList getSortedChatList() const;

    /** @brief Закрепляет или открепляет чат. */
    void setPinned(const QString& username, bool pinned);

    /** @brief Архивирует или разархивирует чат. */
    void setArchived(const QString& username, bool archived);

    /** @brief Сохраняет черновик сообщения. */
    void saveDraft(const QString& username, const QString& draftText);

    /** @brief Получает текст черновика. */
    QString getDraft(const QString& username) const;

    /** @brief Инициализирует черновики из БД при запуске. */
    void loadDraftsFromDatabase();

    /** @brief Обновляет статус сообщения (прочитано/доставлено). */
    void updateMessageStatus(qint64 messageId, ChatMessage::MessageStatus newStatus);

public slots:
    /** @brief Обработчик завершения загрузки аватара. */
    void onAvatarDownloaded(const QString& username, const QString& localPath);

    /** @brief Центральный диспетчер входящих JSON ответов. */
    void processResponse(const QJsonObject& response);

    // Обработчики конкретных типов ответов
    void handleLoginSuccess(const QJsonObject& response);
    void handleLoginFailure(const QJsonObject& response);
    void handleRegisterSuccess(const QJsonObject& response);
    void handleRegisterFailure(const QJsonObject& response);
    void handleContactList(const QJsonObject& response);
    void handleHistoryData(const QJsonObject& response);
    void handleOldHistoryData(const QJsonObject& response);
    void handlePrivateMessage(const QJsonObject& response);
    void handleUserList(const QJsonObject& response);
    void handleMessageDelivered(const QJsonObject& response);
    void handleMessageRead(const QJsonObject& response);
    void handleEditMessage(const QJsonObject& response);
    void handleDeleteMessage(const QJsonObject& response);
    void handleSearchResults(const QJsonObject& response);
    void handleAddContactSuccess(const QJsonObject& response);
    void handleAddContactFailure(const QJsonObject& response);
    void handleIncomingContactRequest(const QJsonObject& response);
    void handlePendingRequestsList(const QJsonObject& response);
    void handleLogoutSuccess(const QJsonObject& response);
    void handleLogoutFailure(const QJsonObject& response);
    void handleTypingResponse(const QJsonObject& response);
    void handleUnreadCounts(const QJsonObject& response);

    // Обработчики звонков
    void handleCallRequestSent(const QJsonObject& response);
    void handleIncomingCall(const QJsonObject& response);
    void handleCallAccepted(const QJsonObject& response);
    void handleCallRejected(const QJsonObject& response);
    void handleCallEnd(const QJsonObject& response);
    void handleCallHistory(const QJsonObject& response);
    void handleCallStats(const QJsonObject& response);

    void handleUpdateProfileResult(const QJsonObject& response);
    void handleTokenLoginFailure(const QJsonObject& response);
    void handleHandshake(const QJsonObject& request);

    /** @brief Запрашивает историю звонков с сервера. */
    void requestCallHistory();

    /** @brief Запускает синхронизацию истории сообщений для чата. */
    void syncChatHistory(const QString& chatPartner);

    /** @brief Очищает все данные при выходе. */
    void clearAllData();

    /** @brief Первоначальная загрузка данных при старте. */
    void initLoad();

signals:
    // Сигналы изменения данных
    void avatarUpdated(const QString& username, const QString& localPath);
    void encryptionEnabled();
    void currentChatPartnerChanged(const QString& username);
    void currentUserChanged(const QString& username);
    void userCacheChanged(const QString& username);
    void historyLoadingChanged(bool isLoadingHistory);
    void oldestMessageIdChanged(qint64 oldestMessageId);
    void editingMessageIdChanged(qint64 editingMessageId);
    void replyToMessageIdChanged(qint64 replyMessageId);
    void chatCacheUpdated(const QString& username);
    void unreadCountUpdated(const QString& username);
    void loginFailed(const QString& error);
    void registerFailed(const QString& error);
    void sendJson(const QJsonObject& request);
    void cryptoManagerChanged(const CryptoManager* cryptoManager);
    void chatMetadataChanged(const QString& username);
    void contactsUpdated(const QStringList& sortedUsernames);
    void onlineStatusUpdated();

    // Сигналы для UI
    void olderHistoryChunkPrepended(const QString& chatPartner, const QList<ChatMessage>& messages);
    void historyLoaded(const QString& chatPartner, const QList<ChatMessage>& messages);
    void loginSuccess(const QJsonObject& response);
    void loginFailure(const QString& reason);
    void tokenLoginFailed(const QString& reason);
    void registerSuccess();
    void registerFailure(const QString& reason);
    void newMessageReceived(const ChatMessage& message);
    void messageStatusChanged(qint64 messageId, ChatMessage::MessageStatus newStatus);
    void unreadCountChanged();
    void messageEdited(const QString& chatPartner, qint64 messageId, const QString& newPayload);
    void messageDeleted(const QString& chatPartner, qint64 messageId);
    void searchResultsReceived(const QJsonArray& users);
    void addContactSuccess(const QString& username);
    void addContactFailure(const QString& reason);
    void contactRequestReceived(const QJsonObject& request);
    void pendingContactRequestsUpdated(const QJsonArray& requests);
    void logoutSuccess();
    void logoutFailure(const QString& reason);
    void typingStatusChanged(const QString& username, bool isTyping);
    void confirmMessageSent(QString tempId, const ChatMessage& msg);

    // Сигналы звонков
    void callRequestSent(const QString& toUser, const QString& callId);
    void incomingCall(const QString& fromUser, const QString& callId, const QString& callerIp, quint16 callerPort);
    void callAccepted(const QString& fromUser, const QString& calleeIp, quint16 calleePort);
    void callRejected(const QString& fromUser, const QString& reason);
    void callEnded();
    void callHistoryReceived(const QJsonArray& calls);
    void callStatsReceived(const QJsonObject& stats);

    void profileUpdateResult(const QJsonObject& response);
    void requestServerHistory(const QString& chatPartner, int afterId);
    void updateMessageStatus(qint64 messageId, ChatMessage::MessageStatus newStatus, bool isRead);


private:
    /** @brief Загружает сохраненные метаданные чатов из БД. */
    void loadChatMetadataFromDatabase();

    /** @brief Инициализирует таблицу обработчиков ответов. */
    void initResponseHandlers();

    /** @brief Получает последний ID сообщения, известного серверу (для синхронизации). */
    int getLastServerIdForChat(int chatId);

    /** @brief Получает набор всех существующих ID сообщений (для дедупликации). */
    QSet<qint64> fetchExistingServerIds(const QString& chatPartner);

    /** @brief Вставляет новые сообщения с проверкой на дубликаты. */
    void insertMessagesWithUpsertFiltered(const QList<ChatMessage>& messages, const QString& chatPartner);


    DatabaseService* m_dbService = nullptr;             ///< Ссылка на сервис БД
    using ResponseHandler = void (DataService::*)(const QJsonObject&);
    QMap<QString, ResponseHandler> m_responseHandlers;  ///< Таблица маршрутизации ответов

    QMap<QString, ChatCache> m_chatHistoryCache;        ///< Кеш истории чатов
    QMap<QString, Chat> m_chatMetadataCache;            ///< Кеш настроек чатов
    QMap<QString, User> m_userCache;                    ///< Кеш пользователей
    QMap<QString, int> m_unreadCounts;                  ///< Кеш счетчиков непрочитанных

    User m_currentUser;                                 ///< Текущий авторизованный пользователь
    User m_currentChatPartner;                          ///< Текущий открытый чат
    bool m_isLoadingHistory = false;                    ///< Флаг процесса загрузки

    qint64 m_oldestMessageId = 0;                       ///< ID для пагинации вверх
    qint64 m_editingMessageId = 0;                      ///< ID редактируемого сообщения
    qint64 m_replyToMessageId = 0;                      ///< ID сообщения для ответа

    QTimer* m_globalSearchTimer;                        ///< Таймер поиска
    QTimer* m_typingSendTimer;                          ///< Таймер отправки статуса печати
    QMap<QString, QTimer*> m_typingReceiveTimers;       ///< Таймеры сброса статуса печати собеседников

    QVector<QString> m_uploadingFilePath;               ///< Очередь загрузки файлов
    bool m_isChatSearchActive = false;                  ///< Флаг активности поиска внутри чата

    CryptoManager* m_crypto;                            ///< Менеджер E2E шифрования
    QMap<QString, CachedFile> m_uploadedFilesCache;     ///< Временный кеш метаданных файлов
    AvatarCache* m_avatarCache;                         ///< Сервис кеширования аватаров
};

#endif // DATASERVICE_H
