#ifndef SERVER_H
#define SERVER_H

// Включения базовых классов и утилит Qt
#include <QObject>
#include <QHostAddress>

// Включения для сетевого взаимодействия
#include <QTcpServer>
#include <QWebSocketServer>

// Включения для структур данных
#include <QMap>
#include <QJsonObject>
#include "cryptoutils.h"
#include "structures.h" // Кастомные структуры данных (User, ChatMessage).

// Прямые объявления (Forward Declarations) для использования указателей
// без включения полных заголовочных файлов.
class QTcpSocket;
class QWebSocket;

/**
 * @class Server
 * @brief Главный класс, реализующий логику чат-сервера с поддержкой VoIP и шифрования.
 *
 * @details Этот класс выступает центральным узлом системы. Он выполняет следующие задачи:
 * - Управляет входящими TCP (для десктопных клиентов) и WebSocket (для веб-клиентов) соединениями.
 * - Маршрутизирует JSON-запросы от клиентов к соответствующим обработчикам.
 * - Поддерживает базу данных SQLite для хранения истории и пользователей.
 * - Управляет сигнализацией для VoIP звонков (рукопожатия, статусы звонков).
 * - Реализует механизмы аутентификации и проверки токенов.
 */
class Server : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор сервера.
     * @param parent Родительский объект (обычно nullptr или QCoreApplication).
     */
    explicit Server(QObject *parent = nullptr);

    /**
     * @brief Запускает сервер для прослушивания входящих подключений.
     *
     * @details Инициализирует базу данных, настраивает маршруты команд и начинает слушать
     * заданные порты для TCP и WebSocket соединений.
     *
     * @param address IP-адрес для прослушивания (по умолчанию QHostAddress::Any - все интерфейсы).
     * @param tcpPort Порт для защищенных TCP-подключений (по умолчанию 1234).
     * @param wsPort Порт для WebSocket-подключений (по умолчанию 8080).
     * @return `true`, если оба сервера (TCP и WS) успешно запущены, иначе `false`.
     */
    bool listen(const QHostAddress &address = QHostAddress::Any, quint16 tcpPort = 1234, quint16 wsPort = 8080);

protected:
    // --- Методы-обработчики (handlers), вынесенные в protected для наглядности ---

    /**
     * @brief Инициализирует таблицу обработчиков команд (m_handlers).
     * @details Связывает строковые названия команд (например, "LOGIN", "SEND_MESSAGE")
     * с методами класса Server.
     */
    void initHandlers();

    /**
     * @brief Обрабатывает подтверждение доставки сообщения клиенту.
     * @param socket Сокет клиента, приславшего подтверждение.
     * @param request JSON-объект с данными (messageId, status).
     */
    void handleMessageDelivered(QObject* socket, const QJsonObject& request);

    /**
     * @brief Обрабатывает уведомление о прочтении сообщения.
     * @param socket Сокет клиента.
     * @param request JSON-объект с ID сообщения и ID чата.
     */
    void handleMessageRead(QObject* socket, const QJsonObject& request);

    /**
     * @brief Обрабатывает запрос на выход из системы (Logout).
     * @details Удаляет сессию клиента и закрывает соединение.
     * @param socket Сокет клиента.
     * @param request JSON-объект запроса.
     */
    void handleLogoutRequest(QObject* socket, const QJsonObject& request);

public slots:
    // --- Слоты для обработки событий TCP-сервера ---

    /**
     * @brief Слот, вызываемый при новом входящем TCP-подключении.
     * @details Настраивает сокет, подключает сигналы (readyRead, disconnected) и добавляет его в список ожидания рукопожатия.
     */
    void onNewTcpConnection();

    //void onNewNonSecureTcpConnection();

    /**
     * @brief Слот для чтения данных из TCP-сокета.
     * @details Собирает полные JSON-пакеты из потока данных, используя m_nextBlockSizes для определения границ сообщений.
     */
    void onTcpReadyRead();

    /**
     * @brief Общий слот обработки отключения клиента (TCP и WebSocket).
     * @details Очищает ресурсы, удаляет клиента из списков онлайн и уведомляет других пользователей, если необходимо.
     */
    void onClientDisconnected();

    // --- Слоты для обработки событий WebSocket-сервера ---

    /**
     * @brief Слот, вызываемый при новом WebSocket-подключении.
     */
    void onNewWebSocketConnection();

    /**
     * @brief Обрабатывает текстовые сообщения от WebSocket-клиентов.
     * @param message Входящее сообщение в формате JSON-строки.
     */
    void onWebSocketTextMessageReceived(const QString &message);


private:
    /**
     * @brief Псевдоним для указателя на метод-обработчик команды.
     * @details Используется в `m_handlers` для реализации паттерна "Команда".
     * Сигнатура: void (Server::*)(QObject* socket, const QJsonObject& request)
     */
    using Handler = void (Server::*)(QObject*, const QJsonObject&);

    // --- Методы инициализации ---

    /**
     * @brief Инициализирует соединение с базой данных SQLite.
     * @details Создает необходимые таблицы (users, messages, calls), если они не существуют.
     * @return `true` при успешном подключении, иначе `false`.
     */
    bool initDatabase();

    // --- Набор приватных методов-обработчиков для каждой команды от клиента ---

    /** @brief Отправляет клиенту историю сообщений с определенным пользователем. */
    void handleGetHistory(QObject* socket, const QJsonObject& request);

    /** @brief Регистрирует нового пользователя в БД (сохраняет хеш пароля и соль). */
    void handleRegister(QObject* socket, const QJsonObject& request);

    /** @brief Выполняет поиск пользователей по username или части имени. */
    void handleSearchUsers(QObject* socket, const QJsonObject& request);

    /** @brief Обрабатывает запрос на добавление в контакты. */
    void handleAddContactRequest(QObject* socket, const QJsonObject& request);

    /** @brief Производит аутентификацию пользователя по логину и паролю. */
    void handleLogin(QObject* socket, const QJsonObject& request);

    /**
     * @brief Маршрутизирует личное сообщение от одного пользователя к другому.
     * @details Если получатель офлайн, сохраняет сообщение в БД как "недоставленное".
     */
    void handlePrivateMessage(QObject* fromUserSocket, const QJsonObject& request);

    /** @brief Обрабатывает редактирование существующего сообщения. */
    void handleEditMessage(QObject* socket, const QJsonObject& request);

    /** @brief Обрабатывает удаление сообщения (у себя или у всех). */
    void handleDeleteMessage(QObject* socket, const QJsonObject& request);

    /** @brief Обрабатывает ответ на запрос добавления в контакты (Принять/Отклонить). */
    void handleContactRequestResponse(QObject* socket, const QJsonObject& request);

    /** @brief Пересылает статус "печатает..." собеседнику. */
    void handleTyping(QObject *socket, const QJsonObject &request);

    // --- Вспомогательные методы для отправки данных клиентам ---

    /**
     * @brief Универсальный метод отправки JSON-ответа клиенту.
     * @details Определяет тип сокета (TCP или WebSocket) и отправляет данные в нужном формате.
     * @param socket Указатель на сокет получателя.
     * @param response JSON-объект для отправки.
     */
    void sendJson(QObject* socket, const QJsonObject& response);

    /** @brief Отправляет список контактов авторизованному пользователю. */
    void sendContactList(QObject* socket, const QString& username);

    /** @brief (Устаревший) Отправляет полный список пользователей. */
    void sendFullUserList(QObject* socket);

    /** @brief Рассылает всем активным клиентам обновленный список онлайн-пользователей. */
    void broadcastUserList();

    /** @brief Отправляет пользователю сообщения, полученные пока он был офлайн. */
    void sendOfflineMessages(QObject* socket, const QString& username);

    /** @brief Отправляет клиенту список статусов (онлайн/офлайн) его контактов. */
    void sendOnlineStatusList(QObject* clientSocket);

    /** @brief Отправляет количество непрочитанных сообщений для каждого чата. */
    void sendUnreadCounts(QObject* socket, const QString& username);

    /** @brief Отправляет список входящих запросов на добавление в контакты. */
    void sendPendingContactRequests(QObject* socket, const QString& username);

    // --- Методы VoIP (Signaling) ---

    /** @brief Инициирует звонок: пересылает offer SDP получателю. */
    void handleCallRequest(QObject* socket, const QJsonObject& request);

    /** @brief Обрабатывает принятие звонка: пересылает answer SDP звонящему. */
    void handleCallAccepted(QObject* socket, const QJsonObject& request);

    /** @brief Обрабатывает отклонение звонка. */
    void handleCallRejected(QObject* socket, const QJsonObject& request);

    /** @brief Завершает активный звонок и уведомляет собеседника. */
    void handleCallEnd(QObject* socket, const QJsonObject& request);

    /** @brief Возвращает историю звонков пользователя. */
    void handleGetCallHistory(QObject* socket, const QJsonObject& request);

    /** @brief Возвращает статистику по звонкам (длительность, качество и т.д.). */
    void handleGetCallStats(QObject* socket, const QJsonObject& request);

    /** @brief Обновляет профиль пользователя (аватар, статус и т.д.). */
    void handleUpdateProfile(QObject* socket, const QJsonObject& request);

    /**
     * @brief Обрабатывает начальное криптографическое рукопожатие (Handshake).
     * @details Используется для обмена ключами шифрования перед началом основной сессии.
     */
    void handleHandshake(QObject* socket, const QJsonObject& request);

    /**
     * @brief Главный диспетчер входящих JSON-запросов.
     * @details Проверяет поле "type" в JSON и вызывает соответствующий метод из `m_handlers`.
     * @param request Входящий JSON.
     * @param clientSocket Сокет, от которого пришел запрос.
     */
    void processJsonRequest(const QJsonObject& request, QObject* clientSocket);

    void removeClient(QObject* clientSocket); // (Заготовка/Не используется)

    /** @brief Создает запись о начале звонка в базе данных. */
    void createCallRecord(const QString& callId, const QString& from,
                          const QString& to, const QString& fromIp, quint16 fromPort);

    /** @brief Обновляет запись звонка в БД при установлении соединения. */
    void updateCallConnected(const QString& callId, const QString& toIp, quint16 toPort);

    /** @brief Обновляет запись звонка при его завершении (статус, время окончания). */
    void updateCallEnded(const QString& callId, const QString& status);

    /** @brief Генерирует уникальный токен сессии для пользователя. */
    QString generateToken(const QString& username);

    /** @brief Проверяет валидность токена сессии. */
    bool validateToken(const QString& username, const QString& token);

    /** @brief Обрабатывает автоматический вход по токену (без пароля). */
    void handleTokenLogin(QObject* socket, const QJsonObject& request);

    /** @brief Отправляет публичный ключ сервера клиенту для начала защищенного соединения. */
    void sendServerPublicKey(QTcpSocket* socket);

private:
    // --- Указатели на серверные объекты ---
    //QTcpServer *m_nonsecureTcpServer;
    QTcpServer *m_secureTcpServer;   ///< TCP сервер для зашифрованных соединений (основной).
    QWebSocketServer *m_webSocketServer; ///< WebSocket сервер для веб-клиентов.

    // --- Структуры для управления состоянием онлайн-клиентов ---

    /**
     * @brief Карта активных клиентов: `username` -> `указатель на сокет`.
     * @details Используется для быстрой маршрутизации сообщений конкретному пользователю.
     */
    QMap<QString, QObject*> m_clients;

    /**
     * @brief Обратная карта клиентов: `указатель на сокет` -> `username`.
     * @details Используется для быстрой идентификации, кто именно отключился или прислал запрос.
     */
    QMap<QObject*, QString> m_clientsReverse;

    /**
     * @brief Карта активных звонков: `callId` -> `CallInfo`.
     * @details Хранит состояние текущих сессий VoIP (участники, сокеты, IP).
     */
    QMap<QString, CallInfo> m_activeCalls;

    // --- Специфично для TCP ---
    /**
     * @brief Хранит ожидаемый размер следующего TCP-пакета для каждого сокета.
     * @details Ключ - сокет, Значение - размер блока данных (quint32).
     * Необходимо для сборки фрагментированных пакетов в `onTcpReadyRead`.
     */
    QMap<QTcpSocket*, quint32> m_nextBlockSizes;

    // --- Карта обработчиков ---
    /**
     * @brief Карта команд, реализующая паттерн Dispatcher.
     * @details Ключ - тип команды (строка из JSON), Значение - метод-обработчик.
     */
    QMap<QString, Handler> m_handlers;

    /**
     * @brief Хранилище активных токенов сессий (username -> token).
     */
    QMap<QString, QString> m_userTokens;

    /**
     * @brief Менеджеры шифрования для каждого подключенного TCP-клиента.
     * @details Хранит контекст шифрования (ключи сессии, счетчики) для `QTcpSocket`.
     */
    QMap<QTcpSocket*, CryptoManager*> m_clientCrypto;
};

#endif // SERVER_H
