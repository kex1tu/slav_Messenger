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

/**
* @file Includes.h
* @brief Основные заголовочные файлы для сервера на Qt.
* Включает необходимые системные и проектные заголовки для работы с сетью,
* базой данных, JSON-сообщениями, криптографией и многопоточностью.
*/

#include <QCoreApplication> ///< Основной класс для консольных Qt-приложений, где запускается цикл событий.
#include <QTcpServer> ///< Класс сервера для принятия TCP-соединений.
#include <QTcpSocket> ///< Класс клиента для работы с TCP-соединениями.
#include <QDebug> ///< Утилита для отладочного вывода в консоль.
#include <QSqlDatabase> ///< Класс для работы с базами данных (например, SQLite).
#include <QSqlQuery> ///< Обеспечивает выполнение SQL-запросов.
#include <QSqlError> ///< Хранит информацию об ошибках при работе с БД.
#include <QCryptographicHash> ///< Класс для хеширования данных (SHA256, MD5 и др.).
#include <QJsonObject> ///< Представление JSON-объекта.
#include <QJsonDocument> ///< Сериализация и десериализация JSON-объектов.
#include <QJsonArray> ///< Представление JSON-массива.
#include <QDataStream> ///< Для последовательной записи и чтения бинарных данных.
#include <QSqlRecord> ///< Представляет запись результата SQL-запроса.
#include <algorithm> ///< Стандартные алгоритмы C++, например, min, max.
#include <QWebSocket> ///< Работа с WebSocket соединениями.
#include <thread> ///< Работа с потоками из стандартной библиотеки C++.
#include <QUuid> ///< Генерация уникальных UUID.
#include "cryptoutils.h" ///< Пользовательская библиотека для криптографических функций.
#include "structures.h" ///< Пользовательские структуры и типы данных.
#include "server.h" ///< Основной класс сервера и его методы.

/// Этот набор include директив покрывает весь необходимый функционал для создания комплексного и безопасного месенджера с поддержкой TCP и WebSocket, базой данных, шифрованием и асинхронной обработкой.



/**
 * @brief Конструктор класса Server.
 *
 * @details Этот конструктор выполняет всю необходимую первоначальную настройку
 *          для запуска сервера:
 *          1.  Создает экземпляры TCP и WebSocket серверов.
 *          2.  Соединяет их сигналы `newConnection` со слотами для обработки
 *              новых клиентских подключений.
 *          3.  Инициализирует базу данных, создавая необходимые таблицы, если их нет.
 *              В случае неудачи, приложение аварийно завершается.
 *          4.  Инициализирует карту обработчиков команд (`m_handlers`).
 *
 * @param parent Родительский объект в иерархии Qt.
 */
Server::Server(QObject *parent) : QObject(parent)
{
    // -----------------------------------------------------------------------
    // 1. Инициализация TCP-сервера
    // -----------------------------------------------------------------------
    // Создаем экземпляр QTcpServer. Передача `this` в конструктор гарантирует,
    // что объект `m_secureTcpServer` будет автоматически удален при уничтожении `Server`.
    m_secureTcpServer = new QTcpServer(this);

    // (Опционально) Сервер для незащищенных соединений (сейчас отключен)
    // m_nonsecureTcpServer = new QTcpServer(this);

    // -----------------------------------------------------------------------
    // 2. Инициализация WebSocket-сервера
    // -----------------------------------------------------------------------
    // Параметры:
    // - "MessengerServer": Имя сервера (может передаваться в HTTP-заголовках).
    // - QWebSocketServer::NonSecureMode: Используем режим 'ws://' (без SSL/TLS).
    //   В продакшене SSL часто терминируется на уровне Nginx/Reverse Proxy.
    m_webSocketServer = new QWebSocketServer("MessengerServer", QWebSocketServer::NonSecureMode, this);

    // -----------------------------------------------------------------------
    // 3. Подключение сигналов (Signal-Slot connections)
    // -----------------------------------------------------------------------

    // Обработка новых TCP-подключений.
    // Когда клиент стучится на порт, вызывается слот onNewTcpConnection.
    connect(m_secureTcpServer, &QTcpServer::newConnection, this, &Server::onNewTcpConnection);

    // Обработка новых WebSocket-подключений.
    connect(m_webSocketServer, &QWebSocketServer::newConnection, this, &Server::onNewWebSocketConnection);

    // -----------------------------------------------------------------------
    // 4. Подключение к Базе Данных
    // -----------------------------------------------------------------------
    if (!initDatabase()) {
        // Критическая ошибка: если нет доступа к БД, сервер бесполезен.
        // qFatal пишет в лог и немедленно завершает процесс (abort).
        qFatal("Fatal: Database initialization failed! Check permissions or disk space.");
    }

    // -----------------------------------------------------------------------
    // 5. Инициализация логики (Dispatch Map)
    // -----------------------------------------------------------------------
    // Заполняем карту `m_handlers`, связывая текстовые команды JSON (например, "LOGIN")
    // с методами C++ (например, &Server::handleLogin).
    initHandlers();
}


/**
 * @brief Запускает сетевые службы сервера.
 *
 * @details Пытается привязать (bind) и начать прослушивание портов для обоих протоколов:
 * - **TCP** (через `m_secureTcpServer`) для основных клиентов (Desktop/Mobile).
 * - **WebSocket** (через `m_webSocketServer`) для веб-клиентов.
 *
 * @param address IP-адрес интерфейса для прослушивания (например, QHostAddress::Any для всех).
 * @param tcpPort Порт для TCP-соединений (стандарт: 1234).
 * @param wsPort Порт для WebSocket-соединений (стандарт: 8080).
 *
 * @return `true`, только если **оба** сервера успешно запустились. Если хотя бы один не смог занять порт, возвращает `false`.
 */
bool Server::listen(const QHostAddress &address, quint16 tcpPort, quint16 wsPort)
{
    // 1. Попытка запуска защищенного TCP-сервера
    //QHostAddress address("26.57.175.166");
    bool tcpSuccess = m_secureTcpServer->listen(address, tcpPort);

    // 2. Попытка запуска WebSocket-сервера
    bool wsSuccess = m_webSocketServer->listen(address, wsPort);

    // 3. Проверка результатов
    if (tcpSuccess && wsSuccess) {
        qInfo() << "Server started successfully!";
        qInfo() << " - TCP Server listening on:" << address.toString() << ":" << tcpPort;
        qInfo() << " - WebSocket Server listening on:" << address.toString() << ":" << wsPort;
        return true;
    }

    // 4. Обработка ошибок запуска (Logging)
    if (!tcpSuccess) {
        qCritical() << "TCP Server failed to start on port" << tcpPort
                    << ". Error:" << m_secureTcpServer->errorString();
    }

    if (!wsSuccess) {
        qCritical() << "WebSocket Server failed to start on port" << wsPort
                    << ". Error:" << m_webSocketServer->errorString();
    }

    // Если хотя бы один сервер не стартовал — считаем запуск неудачным.
    // (В зависимости от требований можно закрыть тот, что открылся, но здесь оставляем как есть).
    return false;
}


/**
 * @brief Слот обработки нового TCP-подключения.
 *
 * @details Вызывается автоматически сигналом `QTcpServer::newConnection`.
 * Выполняет первичную настройку клиента:
 * 1. Принимает входящий сокет.
 * 2. Подписывается на события ввода/вывода (чтение данных, разрыв связи).
 * 3. Инициализирует буфер для парсинга потока данных.
 * 4. Создает уникальный криптографический контекст для будущей защищенной сессии.
 */
void Server::onNewTcpConnection()
{
    // 1. Извлекаем сокет клиента из очереди ожидающих подключений.
    // Этот объект сокета теперь представляет соединение с конкретным клиентом.
    QTcpSocket *socket = m_secureTcpServer->nextPendingConnection();

    if (!socket) return; // На всякий случай проверяем валидность

    qInfo() << "New TCP client connected from:" << socket->peerAddress().toString();

    // 2. Настраиваем асинхронную обработку событий через сигналы и слоты.

    // readyRead: срабатывает, когда в сокет приходят новые данные.
    connect(socket, &QTcpSocket::readyRead, this, &Server::onTcpReadyRead);

    // disconnected: срабатывает, когда клиент закрывает соединение или происходит обрыв.
    connect(socket, &QTcpSocket::disconnected, this, &Server::onClientDisconnected);

    // 3. Инициализируем состояние парсера входящего потока.
    // В TCP данные приходят потоком, а не пакетами. Мы используем QDataStream с префиксом длины.
    // Значение '0' означает, что мы ожидаем заголовок с размером следующего блока данных.
    m_nextBlockSizes.insert(socket, 0);

    // 4. Инициализируем криптографию.
    // Создаем экземпляр CryptoManager. В его конструкторе сразу генерируется
    // эфемерная пара ключей (Private/Public) для этого конкретного соединения.
    // Это подготовка к этапу Handshake.
    m_clientCrypto[socket] = new CryptoManager();
}


/**
 * @brief Слот обработки нового WebSocket-подключения.
 *
 * @details Вызывается сигналом `QWebSocketServer::newConnection`.
 * Настраивает взаимодействие с веб-клиентами (браузеры, веб-приложения):
 * 1. Принимает входящее соединение.
 * 2. Подписывается на текстовые сообщения (JSON).
 * 3. Подписывается на отключение клиента.
 * 
 * @note В отличие от "сырого" TCP, здесь не нужна ручная сборка пакетов (m_nextBlockSizes),
 * так как WebSocket сам обеспечивает границы сообщений (frames).
 */
void Server::onNewWebSocketConnection()
{
    // 1. Извлекаем ожидающее WebSocket-подключение.
    QWebSocket *socket = m_webSocketServer->nextPendingConnection();
    
    if (!socket) return;

    qInfo() << "New WebSocket client connected from:" << socket->peerAddress().toString();

    // 2. Настраиваем обработчики событий.
    
    // textMessageReceived: Срабатывает, когда приходит полный текстовый фрейм.
    // WebSocket автоматически склеивает фрагменты, поэтому мы получаем готовую строку (JSON).
    connect(socket, &QWebSocket::textMessageReceived, this, &Server::onWebSocketTextMessageReceived);
    
    // disconnected: Общий слот для очистки ресурсов при разрыве.
    connect(socket, &QWebSocket::disconnected, this, &Server::onClientDisconnected);
}


/**
 * @brief Инициализирует карту обработчиков команд (Command Pattern / Dispatch Table).
 * 
 * @details Заполняет таблицу `m_handlers`, связывая строковые ключи (приходящие в JSON-поле "type")
 *          с указателями на методы-члены класса `Server`.
 *          Это позволяет динамически выбирать метод обработки без использования громоздких конструкций switch/if-else.
 */
void Server::initHandlers() {
    // --- Аутентификация и регистрация ---
    m_handlers["login"] = &Server::handleLogin;          // Вход по паролю
    m_handlers["token_login"] = &Server::handleTokenLogin; // Авто-вход по токену
    m_handlers["register"] = &Server::handleRegister;    // Регистрация нового пользователя
    m_handlers["logout_request"] = &Server::handleLogoutRequest; // Явный выход

    // --- Управление контактами и пользователями ---
    m_handlers["search_users"] = &Server::handleSearchUsers; // Поиск глобально
    m_handlers["add_contact_request"] = &Server::handleAddContactRequest; // Запрос дружбы
    m_handlers["contact_request_response"] = &Server::handleContactRequestResponse; // Ответ на запрос
    m_handlers["update_profile"] = &Server::handleUpdateProfile; // Смена аватара/ника

    // --- Обмен сообщениями (Messaging) ---
    m_handlers["private_message"] = &Server::handlePrivateMessage; // Отправка нового сообщения
    m_handlers["get_history"] = &Server::handleGetHistory;         // Загрузка истории чата
    m_handlers["delete_message"] = &Server::handleDeleteMessage;   // Удаление сообщения
    m_handlers["edit_message"] = &Server::handleEditMessage;       // Редактирование сообщения
    m_handlers["typing"] = &Server::handleTyping;                  // Статус "печатает..."

    // --- Статусы доставки (Delivery Reports) ---
    m_handlers["message_delivered"] = &Server::handleMessageDelivered; // Сообщение доставлено (2 серые галочки)
    m_handlers["message_read"] = &Server::handleMessageRead;           // Сообщение прочитано (2 синие галочки)

    // --- VoIP (Звонки) ---
    m_handlers["call_request"] = &Server::handleCallRequest;   // Инициация звонка (Offer)
    m_handlers["call_accepted"] = &Server::handleCallAccepted; // Принятие звонка (Answer)
    m_handlers["call_rejected"] = &Server::handleCallRejected; // Отклонение
    m_handlers["call_end"] = &Server::handleCallEnd;           // Завершение разговора
    m_handlers["get_call_history"] = &Server::handleGetCallHistory; // Лог звонков

    // --- Криптография ---
    m_handlers["handshake"] = &Server::handleHandshake; // Обмен ключами (Diffie-Hellman)
}


/**
 * @brief Обрабатывает рукопожатие (ECDH handshake) для установления зашифрованного соединения.
 *
 * @details Метод вызывается первым при подключении TCP-клиента. Последовательность действий:
 * 1. Получает публичный ключ клиента, закодированный в base64.
 * 2. Конвертирует ключ в бинарный формат и проверяет корректность размера (32 байта).
 * 3. Вычисляет общий секрет (shared secret) с использованием приватного ключа сервера.
 * 4. Отправляет клиенту публичный ключ сервера для завершения обмена ключами.
 * 5. Переключает соединение в состояние зашифрованного.
 * 
 * В случае вызова с WebSocket игнорирует, так как обычно WSS-либы сами защищают канал.
 *
 * @param socket Сокет клиента, инициировавшего рукопожатие (ожидается QTcpSocket).
 * @param request JSON-объект с ключом клиента в поле "key" (base64).
 */
void Server::handleHandshake(QObject* socket, const QJsonObject& request)
{
    // Приведение общего сокета к QTcpSocket
    if (auto tcpSocket = qobject_cast<QTcpSocket*>(socket)) {

        if (!m_clientCrypto.contains(tcpSocket)) {
            qWarning() << "[SERVER] Crypto context missing for socket:" << tcpSocket->peerAddress().toString();
            return;
        }

        CryptoManager* crypto = m_clientCrypto.value(tcpSocket);

        // Защита от повторного рукопожатия
        if (crypto->isEncrypted()) {
            qWarning() << "[SERVER] Handshake attempted on already encrypted connection from:" << tcpSocket->peerAddress().toString();
            return;
        }

        // 1. Извлекаем публичный ключ клиента, base64->QByteArray
        QString clientKeyBase64 = request["key"].toString();
        QByteArray clientKey = QByteArray::fromBase64(clientKeyBase64.toLatin1());

        if (clientKey.size() != 32) {
            qWarning() << "[SERVER] Invalid client public key size:" << clientKey.size() << "expected 32 bytes";
            return;
        }

        // 2. Вычисляем общий секрет (ECDH)
        crypto->computeSharedSecret(clientKey);

        // 3. Отправляем клиенту наш публичный ключ
        sendServerPublicKey(tcpSocket);

        qInfo() << "[SERVER] Handshake complete with" << tcpSocket->peerAddress().toString() << ". Connection secured.";

    } else {
        // WebSocket с WSS обычно обрабатывается отдельно, шифрование не нужно вручную
        qDebug() << "[SERVER] Handshake received from non-TCP socket (e.g. WebSocket), ignoring custom crypto.";
    }
}


/**
 * @brief Обрабатывает запрос на обновление профиля пользователя.
 * 
 * @details Метод позволяет пользователю изменить свои публичные данные:
 * отображаемое имя, текстовый статус и аватар.
 * 
 * Алгоритм работы:
 * 1. Идентифицирует пользователя по сокету (через `m_clientsReverse`).
 * 2. Извлекает новые данные из JSON-запроса.
 * 3. Выполняет SQL-запрос `UPDATE` к таблице `users`.
 * 4. Отправляет клиенту результат операции (успех/ошибка) и обновленные данные.
 * 
 * @param socket Указатель на сокет пользователя, инициировавшего запрос.
 * @param request JSON-объект с полями:
 *        - `display_name`: Новое отображаемое имя.
 *        - `status_message`: Новый текстовый статус.
 *        - `avatar_url`: (Опционально) Ссылка или Base64 строка аватара.
 */
void Server::handleUpdateProfile(QObject* socket, const QJsonObject& request)
{
    // 1. Определяем имя пользователя по сокету (проверка авторизации)
    QString username = m_clientsReverse.value(socket);
    if (username.isEmpty()) {
        qWarning() << "[SERVER] Profile update attempt from unauthorized socket";
        return;
    }

    // 2. Извлекаем новые значения из запроса
    QString display_name  = request.value("display_name").toString();
    QString statusMessage = request.value("status_message").toString();
    QString avatarUrl     = request.value("avatar_url").toString(); 

    // 3. Подготовка и выполнение SQL-запроса
    QSqlQuery query;
    query.prepare("UPDATE users SET display_name = :display_name, "
                  "status_message = :status_message, "
                  "avatar_url = :avatar_url "
                  "WHERE username = :username");
    
    query.bindValue(":display_name", display_name);
    query.bindValue(":status_message", statusMessage);
    query.bindValue(":avatar_url", avatarUrl);
    query.bindValue(":username", username);

    // 4. Формирование ответа
    QJsonObject response;
    response["type"] = "update_profile_result";

    if (query.exec()) {
        // Успешное обновление
        response["success"] = true;
        response["username"] = username; // Возвращаем, чтобы клиент мог подтвердить свои данные
        response["displayname"] = display_name;
        response["status_message"] = statusMessage;
        response["avatar_url"] = avatarUrl;
        
        qInfo() << "[SERVER] User" << username << "successfully updated their profile.";
    } else {
        // Ошибка при работе с БД
        response["success"] = false;
        response["reason"] = "Database error: " + query.lastError().text();
        
        qWarning() << "[SERVER] Profile update FAILED for" << username 
                   << ". SQL Error:" << query.lastError().text();
    }

    // 5. Отправка результата
    sendJson(socket, response);
}


/**
 * @brief Обрабатывает подтверждение доставки сообщения (Delivery Receipt).
 *
 * @details Этот метод реализует механизм "двух серых галочек":
 * 1. Клиент-получатель сообщает серверу, что сообщение получено.
 * 2. Сервер обновляет статус сообщения в базе данных (`is_delivered = 1`).
 * 3. Сервер находит автора исходного сообщения и пересылает ему уведомление,
 *    чтобы тот увидел обновление статуса в своем интерфейсе.
 *
 * @param socket Сокет клиента-получателя (того, кто подтверждает доставку).
 * @param request JSON-объект, содержащий `id` доставленного сообщения.
 */
void Server::handleMessageDelivered(QObject* socket, const QJsonObject& request) {
    Q_UNUSED(socket); // Сокет здесь не обязателен, так как мы работаем по ID сообщения
    
    quint64 messageId = request["id"].toInt();

    // -----------------------------------------------------------------------
    // 1. Обновляем статус в базе данных
    // -----------------------------------------------------------------------
    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE messages SET is_delivered = 1 WHERE id = :id");
    updateQuery.bindValue(":id", messageId);
    
    if (!updateQuery.exec()) {
        qWarning() << "[SERVER] Failed to mark message" << messageId 
                   << "as delivered. DB Error:" << updateQuery.lastError().text();
        // Если не удалось обновить БД, лучше не уведомлять отправителя ложным статусом
        return; 
    } else {
        qDebug() << "[SERVER] Marked message" << messageId << "as delivered in DB.";
    }

    // -----------------------------------------------------------------------
    // 2. Находим автора исходного сообщения
    // -----------------------------------------------------------------------
    QSqlQuery query;
    query.prepare("SELECT fromUser FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);
    
    if (!query.exec() || !query.next()) {
        qWarning() << "[SERVER] Could not find original sender for message ID" << messageId 
                   << ":" << query.lastError().text();
        return;
    }
    
    QString originalSender = query.value("fromUser").toString();

    // -----------------------------------------------------------------------
    // 3. Формируем и отправляем уведомление автору
    // -----------------------------------------------------------------------
    QJsonObject deliveredCmd;
    deliveredCmd["type"] = "message_delivered";
    deliveredCmd["id"] = (double)messageId; // JSON number

    // Проверяем, онлайн ли автор сообщения
    QObject* senderSocket = m_clients.value(originalSender);
    
    if (senderSocket) {
        sendJson(senderSocket, deliveredCmd);
        qDebug() << "[SERVER] Notification sent to original sender:" << originalSender;
    } else {
        // Если автор офлайн, он увидит статус доставки при следующей загрузке истории (get_history),
        // так как мы уже обновили базу данных.
        qDebug() << "[SERVER] Original sender" << originalSender << "is offline. Notification delayed.";
    }
}


/**
 * @brief Обрабатывает подтверждение прочтения сообщения (Read Receipt).
 *
 * @details Этот метод реализует механизм "двух синих галочек":
 * 1. Клиент-получатель сообщает серверу, что сообщение было показано на экране.
 * 2. Сервер обновляет статус сообщения в базе данных (`is_read = 1`).
 * 3. Сервер находит автора сообщения и пересылает ему уведомление о прочтении.
 *
 * @note Как правило, клиент вызывает этот метод, когда сообщение появляется в видимой
 *       области экрана (viewport) или когда пользователь открывает чат.
 *
 * @param socket Сокет клиента-получателя (того, кто читает сообщение).
 * @param request JSON-объект, содержащий `id` прочитанного сообщения.
 */
void Server::handleMessageRead(QObject* socket, const QJsonObject& request) {
    Q_UNUSED(socket); // Идентификация происходит по ID сообщения
    
    quint64 messageId = request["id"].toInt();

    // -----------------------------------------------------------------------
    // 1. Обновляем статус в базе данных
    // -----------------------------------------------------------------------
    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE messages SET is_read = 1 WHERE id = :id");
    updateQuery.bindValue(":id", messageId);
    
    if (!updateQuery.exec()) {
        qWarning() << "[SERVER] Failed to mark message" << messageId 
                   << "as read. DB Error:" << updateQuery.lastError().text();
        return; // Прерываем, чтобы не отправлять ложный статус
    } else {
        qDebug() << "[SERVER] Marked message" << messageId << "as read in DB.";
    }

    // -----------------------------------------------------------------------
    // 2. Находим автора исходного сообщения
    // -----------------------------------------------------------------------
    QSqlQuery query;
    query.prepare("SELECT fromUser FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);
    
    if (!query.exec() || !query.next()) {
        qWarning() << "[SERVER] Could not find original sender for message ID" << messageId 
                   << ":" << query.lastError().text();
        return;
    }
    
    QString originalSender = query.value("fromUser").toString();

    // -----------------------------------------------------------------------
    // 3. Формируем и отправляем уведомление автору
    // -----------------------------------------------------------------------
    QJsonObject readCmd;
    readCmd["type"] = "message_read";
    readCmd["id"] = (double)messageId;

    // Проверяем, онлайн ли автор сообщения
    QObject* senderSocket = m_clients.value(originalSender);
    
    if (senderSocket) {
        sendJson(senderSocket, readCmd);
        qDebug() << "[SERVER] Read notification sent to original sender:" << originalSender;
    } else {
        // Если автор офлайн, статус всё равно сохранён в БД и будет виден при следующей синхронизации
        qDebug() << "[SERVER] Original sender" << originalSender << "is offline. Read status cached.";
    }
}



/**
 * @brief Обрабатывает запрос клиента на явный выход из системы (Logout).
 *
 * @details Этот метод выполняет полную процедуру выхода пользователя:
 * 1. Проверяет валидность сессии (аутентифицирован ли клиент).
 * 2. Удаляет токен авторизации из памяти и базы данных.
 * 3. Удаляет пользователя из списка онлайн-пользователей.
 * 4. Рассылает всем активным клиентам обновленный список присутствия.
 * 5. Отправляет подтверждение клиенту.
 *
 * @note После успешного logout клиент должен сам закрыть соединение.
 *       Метод не закрывает сокет принудительно, чтобы клиент успел получить ответ.
 *
 * @param socket Сокет клиента, инициировавшего выход.
 * @param request JSON-объект (может содержать `username` для дополнительной проверки, но не обязательно).
 */
void Server::handleLogoutRequest(QObject* socket, const QJsonObject& request)
{
    Q_UNUSED(request); // username определяется по сокету, а не из запроса (защита от подделки)
    
    // -----------------------------------------------------------------------
    // 1. Идентификация пользователя
    // -----------------------------------------------------------------------
    QString username = m_clientsReverse.value(socket, "");
    
    if (username.isEmpty()) {
        qWarning() << "[SERVER] Logout request from unauthenticated or unknown socket";
        
        QJsonObject response;
        response["type"] = "logout_failure";
        response["reason"] = "Not authenticated";
        sendJson(socket, response);
        return;
    }
    
    qInfo() << "[SERVER] User" << username << "initiated logout.";

    // -----------------------------------------------------------------------
    // 2. Удаление токена из памяти (Server RAM)
    // -----------------------------------------------------------------------
    m_userTokens.remove(username);

    // -----------------------------------------------------------------------
    // 3. Удаление токена из базы данных (Persistent Storage)
    // -----------------------------------------------------------------------
    QSqlQuery query;
    query.prepare("DELETE FROM tokens WHERE username = :username");
    query.bindValue(":username", username);
    
    if (!query.exec()) {
        qWarning() << "[SERVER] Failed to delete token from DB for user" << username 
                   << ":" << query.lastError().text();
        
        // Несмотря на ошибку БД, логика продолжается, так как токен уже удален из памяти.
        // Но клиенту отправим сигнал о проблеме.
        QJsonObject response;
        response["type"] = "logout_failure";
        response["reason"] = "Database error";
        sendJson(socket, response);
        return;
    } else {
        qDebug() << "[SERVER] Token deleted from DB for user:" << username;
    }

    // -----------------------------------------------------------------------
    // 4. Удаление из списка онлайн-пользователей
    // -----------------------------------------------------------------------
    m_clients.remove(username);         // Удаляем из карты username -> socket
    m_clientsReverse.remove(socket);    // Удаляем из обратной карты socket -> username

    // -----------------------------------------------------------------------
    // 5. Оповещение всех остальных клиентов
    // -----------------------------------------------------------------------
    // Рассылаем обновленный список онлайн-пользователей всем, кто сейчас подключен.
    broadcastUserList();

    // -----------------------------------------------------------------------
    // 6. Отправка подтверждения клиенту
    // -----------------------------------------------------------------------
    QJsonObject response;
    response["type"] = "logout_success";
    sendJson(socket, response);
    
    qInfo() << "[SERVER] User" << username << "successfully logged out.";
}




/**
 * @brief Слот обработки входящих данных из TCP-сокета.
 *
 * @details Этот метод вызывается автоматически при наличии данных для чтения в TCP-сокете.
 * Реализует протокол передачи данных переменной длины с префиксом размера:
 * 
 * **Формат пакета (незашифрованный):**
 * ```
 * [4 байта: размер] [N байт: JSON]
 * ```
 * 
 * **Формат пакета (зашифрованный):**
 * ```
 * [4 байта: размер] [24 байта: nonce] [16 байт: MAC] [N байт: шифртекст]
 * ```
 * 
 * Алгоритм работы:
 * 1. Читает префикс размера (quint32).
 * 2. Ждет, пока накопится полный пакет в буфере.
 * 3. Проверяет статус шифрования сессии.
 * 4. Если зашифровано — расшифровывает с помощью XChaCha20-Poly1305.
 * 5. Парсит JSON и передает в диспетчер команд.
 * 6. Повторяет цикл, если в буфере есть еще данные.
 */
void Server::onTcpReadyRead()
{
    // -----------------------------------------------------------------------
    // 1. Идентификация отправителя
    // -----------------------------------------------------------------------
    // Определяем, какой именно сокет отправил сигнал readyRead.
    auto socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    // -----------------------------------------------------------------------
    // 2. Инициализация потока для чтения
    // -----------------------------------------------------------------------
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_6_2); // Важно: версия должна совпадать с клиентом
    
    quint32 &nextBlockSize = m_nextBlockSizes[socket]; // Ссылка для изменения значения в карте

    // -----------------------------------------------------------------------
    // 3. Цикл обработки входящих пакетов
    // -----------------------------------------------------------------------
    // TCP — это поток. За один сигнал readyRead может прийти несколько пакетов.
    while (true) {
        
        // --- Этап 1: Чтение размера следующего блока данных ---
        if (nextBlockSize == 0) {
            // Проверяем, достаточно ли байт для чтения заголовка (quint32 = 4 байта)
            if (socket->bytesAvailable() < (qint64)sizeof(quint32)) {
                break;  // Ждем следующего сигнала readyRead
            }
            // Читаем размер блока
            in >> nextBlockSize;
        }

        // --- Этап 2: Ожидание полного тела сообщения ---
        if (socket->bytesAvailable() < nextBlockSize) {
            break;  // Данных пока недостаточно — выходим и ждем
        }

        // -----------------------------------------------------------------------
        // 4. Проверка статуса шифрования
        // -----------------------------------------------------------------------
        CryptoManager* crypto = m_clientCrypto[socket];
        
        if (!crypto->isEncrypted()) {
            // ====================================================================
            // РЕЖИМ 1: Незашифрованное соединение (handshake, login, register)
            // ====================================================================
            QByteArray jsonData;
            in >> jsonData;
            nextBlockSize = 0; // Сбрасываем для следующей итерации

            // Парсинг JSON
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);

            if (!doc.isNull() && doc.isObject()) {
                processJsonRequest(doc.object(), socket);
                continue; // Проверяем, есть ли еще данные в буфере
            } else {
                qWarning() << "[SERVER] Invalid JSON received from" << socket->peerAddress().toString();
                sendJson(socket, {{"type", "error"}, {"reason", "Malformed JSON"}});
                return;
            }
        }

        // ====================================================================
        // РЕЖИМ 2: Зашифрованное соединение (XChaCha20-Poly1305)
        // ====================================================================

        // --- Этап 3: Чтение зашифрованных данных ---
        QByteArray nonceArray;
        QByteArray encryptedData;
        
        in >> nonceArray;       // Nonce (24 байта)
        in >> encryptedData;    // MAC (16 байт) + Ciphertext (N байт)

        // Проверка корректности nonce
        if (nonceArray.size() != 24) {
            qCritical() << "[SERVER] Invalid nonce size:" << nonceArray.size() 
                        << "from" << socket->peerAddress().toString();
            socket->abort();
            return;
        }

        // Проверка минимального размера зашифрованных данных (MAC + хотя бы 1 байт)
        if (encryptedData.size() < 16) {
            qCritical() << "[SERVER] Encrypted data too short! Possible corruption.";
            socket->abort();
            return;
        }

        // --- Этап 4: Извлечение компонентов ---
        const uint8_t* nonce = reinterpret_cast<const uint8_t*>(nonceArray.constData());
        const uint8_t* mac   = reinterpret_cast<const uint8_t*>(encryptedData.constData());
        const uint8_t* cipherText = mac + 16; // Шифртекст начинается после MAC

        qDebug() << "[SERVER] Decrypting packet. Nonce:" << QByteArray((char*)nonce, 24).toHex();

        int textLen = encryptedData.size() - 16; // Длина шифртекста (без MAC)
        QByteArray decrypted(textLen, Qt::Uninitialized);

        // --- Этап 5: Расшифровка и верификация ---
        // crypto_aead_unlock — это XChaCha20-Poly1305 AEAD (Authenticated Encryption with Associated Data).
        // Возвращает 0 при успехе, ненулевое значение при подделке MAC (атака).
        int status = crypto_aead_unlock(
            reinterpret_cast<uint8_t*>(decrypted.data()), // Выход: расшифрованный текст
            mac,                                          // Вход: MAC для проверки подлинности (16 байт)
            crypto->getSessionKey(),                      // Общий ключ сессии (32 байта)
            nonce,                                        // Nonce (24 байта, должен быть уникальным)
            nullptr, 0,                                   // Associated Data (не используем)
            cipherText,                                   // Вход: шифртекст
            textLen                                      // Длина шифртекста
        );

        if (status != 0) {
            qCritical() << "[SERVER] DECRYPTION FAILED! MAC mismatch. Possible attack or corruption from" 
                        << socket->peerAddress().toString();
            socket->abort(); // Разрываем соединение немедленно
            return;
        }

        // Сброс значения размера для следующего пакета
        nextBlockSize = 0;

        // --- Этап 6: Парсинг расшифрованного JSON ---
        QJsonDocument doc = QJsonDocument::fromJson(decrypted);

        if (!doc.isNull() && doc.isObject()) {
            processJsonRequest(doc.object(), socket);
            continue; // Проверяем, есть ли еще пакеты в буфере
        } else {
            qWarning() << "[SERVER] Invalid JSON after decryption";
            sendJson(socket, {{"type", "error"}, {"reason", "Invalid request format"}});
            return;
        }
    }
}


/**
 * @brief Отправляет публичный ключ сервера клиенту для завершения рукопожатия.
 *
 * @details Этот метод вызывается после получения публичного ключа от клиента.
 * Он выполняет вторую половину обмена ключами Диффи-Хеллмана (ECDH):
 * 
 * 1. Извлекает публичный ключ сервера из контекста `CryptoManager`.
 * 2. Формирует JSON-ответ с типом "handshake" и ключом в формате Base64.
 * 3. Отправляет данные, используя правильный протокол (TCP с префиксом длины или WebSocket).
 * 
 * После получения этого сообщения клиент сможет вычислить общий секрет (Shared Secret)
 * и начать шифрованный обмен данными.
 *
 * @param socket Сокет клиента (QTcpSocket или QWebSocket).
 * 
 * @note Метод автоматически определяет тип сокета и использует соответствующий формат передачи.
 */
void Server::sendServerPublicKey(QTcpSocket* socket) {
    // -----------------------------------------------------------------------
    // 1. Валидация входных данных
    // -----------------------------------------------------------------------
    if (!socket) {
        qWarning() << "[SERVER] sendServerPublicKey called with null socket";
        return;
    }
    
    if (!m_clientCrypto.contains(socket)) {
        qWarning() << "[SERVER] No crypto context found for socket:" << socket->peerAddress().toString();
        return;
    }

    CryptoManager* crypto = m_clientCrypto[socket];

    // -----------------------------------------------------------------------
    // 2. Формирование JSON с публичным ключом
    // -----------------------------------------------------------------------
    QJsonObject json;
    json["type"] = "handshake";
    
    // Преобразуем публичный ключ (32 байта бинарных данных) в Base64-строку для JSON.
    json["key"] = QString::fromLatin1(
        QByteArray((const char*)crypto->getMyPublicKey(), 32).toBase64()
    );

    QByteArray jsonData = QJsonDocument(json).toJson(QJsonDocument::Compact);

    // -----------------------------------------------------------------------
    // 3. Отправка в зависимости от типа сокета
    // -----------------------------------------------------------------------
    
    // Попытка интерпретировать как TCP-сокет
    if (auto tcpSocket = qobject_cast<QTcpSocket*>(socket)) {
        // --- TCP: Используем протокол с префиксом длины ---
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_2);
        
        // Шаг 1: Резервируем место для размера (4 байта)
        out << (quint32)0;
        
        // Шаг 2: Записываем JSON
        out << jsonData;
        
        // Шаг 3: Возвращаемся в начало и записываем реальный размер данных
        out.device()->seek(0);
        out << (quint32)(block.size() - sizeof(quint32));
        
        // Шаг 4: Отправляем пакет
        tcpSocket->write(block);
        
        qDebug() << "[SERVER] Sent handshake via TCP. Payload size:" << jsonData.size() << "bytes";
    } 
    // Попытка интерпретировать как WebSocket-сокет
    else if (auto wsSocket = qobject_cast<QWebSocket*>(socket)) {
        // --- WebSocket: Отправляем JSON как текстовое сообщение ---
        // WebSocket автоматически обрамляет данные в фреймы, поэтому префикс длины не нужен.
        wsSocket->sendTextMessage(QString::fromUtf8(jsonData));
        
        qDebug() << "[SERVER] Sent handshake via WebSocket. Payload size:" << jsonData.size() << "bytes";
    } 
    else {
        qWarning() << "[SERVER] Unknown socket type in sendServerPublicKey";
        return;
    }

    qInfo() << "[SERVER] Handshake response (Public Key) sent to:" << socket->peerAddress().toString();
}


/**
 * @brief Слот для обработки текстового сообщения, полученного по WebSocket.
 *
 * @details WebSocket работает на уровне сообщений (message-oriented), а не потока байт.
 * Это означает, что каждый вызов этого слота содержит **полное** сообщение от клиента,
 * уже собранное из фрагментов (если они были).
 * 
 * Алгоритм работы:
 * 1. Определяет отправителя через `sender()`.
 * 2. Парсит входящую строку как JSON.
 * 3. Передает распарсенный объект в общий диспетчер команд `processJsonRequest`.
 * 
 * @note В отличие от TCP, здесь **не требуется** ручная сборка пакетов или обработка префиксов длины.
 * @note Шифрование для WebSocket обычно реализуется на уровне WSS (WebSocket Secure / TLS),
 *       поэтому здесь не используется `CryptoManager`.
 *
 * @param message Полученное текстовое сообщение в формате JSON.
 */
void Server::onWebSocketTextMessageReceived(const QString &message)
{
    // -----------------------------------------------------------------------
    // 1. Идентификация отправителя
    // -----------------------------------------------------------------------
    auto socket = qobject_cast<QWebSocket*>(sender());
    if (!socket) {
        qWarning() << "[SERVER] onWebSocketTextMessageReceived called with invalid sender";
        return;
    }

    // -----------------------------------------------------------------------
    // 2. Парсинг JSON
    // -----------------------------------------------------------------------
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[SERVER] Received invalid JSON from WebSocket client:" 
                   << socket->peerAddress().toString();
        
        // Отправляем клиенту уведомление об ошибке
        QJsonObject errorResponse;
        errorResponse["type"] = "error";
        errorResponse["reason"] = "Malformed JSON";
        sendJson(socket, errorResponse);
        return;
    }

    // -----------------------------------------------------------------------
    // 3. Передача в диспетчер команд
    // -----------------------------------------------------------------------
    // processJsonRequest автоматически найдет нужный обработчик по полю "type" в JSON.
    processJsonRequest(doc.object(), socket);
}


/**
 * @brief Центральный диспетчер JSON-запросов (Command Dispatcher).
 *
 * @details Этот метод является единой точкой входа для всех запросов после их парсинга.
 * Он реализует паттерн **Command Pattern** через таблицу диспетчеризации (`m_handlers`).
 * 
 * **Алгоритм работы:**
 * 1. Извлекает поле `"type"` из JSON (например, `"login"`, `"private_message"`).
 * 2. Ищет соответствующий метод-обработчик в `m_handlers`.
 * 3. Вызывает найденный метод с помощью указателя на член класса (member function pointer).
 * 4. Если команда неизвестна, отправляет клиенту сообщение об ошибке.
 * 
 * **Преимущества такого подхода:**
 * - Избегает длинных цепочек `if-else` или `switch`.
 * - Позволяет добавлять новые команды без изменения этого метода (Open/Closed Principle).
 * - Обеспечивает константное время поиска обработчика O(1).
 * 
 * **Примеры команд:**
 * - `"login"` → вызывает `handleLogin`
 * - `"private_message"` → вызывает `handlePrivateMessage`
 * - `"call_request"` → вызывает `handleCallRequest`
 *
 * @param request Распарсенный JSON-объект с полями команды.
 * @param socket Указатель на сокет клиента (может быть `QTcpSocket*` или `QWebSocket*`).
 */
void Server::processJsonRequest(const QJsonObject& request, QObject* socket)
{
    // -----------------------------------------------------------------------
    // 1. Извлечение типа команды
    // -----------------------------------------------------------------------
    QString type = request["type"].toString();
    
    // Логируем входящий запрос для отладки и аудита.
    // В продакшене можно ограничить логирование чувствительных команд (например, "login").
    QString username = m_clientsReverse.value(socket, "<unauthenticated>");
    qDebug() << "[SERVER] Processing command:" << type << "from user:" << username;

    // -----------------------------------------------------------------------
    // 2. Поиск обработчика в таблице диспетчеризации
    // -----------------------------------------------------------------------
    if (m_handlers.contains(type)) {
        // Получаем указатель на метод-член класса Server
        Handler handler = m_handlers.value(type);
        
        // Вызываем метод через указатель. Синтаксис:
        // (this->*handler)(args...)
        // где:
        // - `this` — указатель на текущий экземпляр Server
        // - `*handler` — разыменование указателя на метод
        // - `(socket, request)` — аргументы метода
        (this->*handler)(socket, request);
        
    } else {
        // -----------------------------------------------------------------------
        // 3. Обработка неизвестной команды
        // -----------------------------------------------------------------------
        qWarning() << "[SERVER] Unknown command received:" << type << "from" << username;
        
        // Отправляем клиенту ошибку с описанием проблемы
        QJsonObject errorResponse;
        errorResponse["type"] = "error";
        errorResponse["reason"] = "Unknown command: " + type;
        sendJson(socket, errorResponse);
    }
}


/**
 * @brief Слот обработки отключения клиента (TCP или WebSocket).
 *
 * @details Этот универсальный обработчик вызывается сигналом `disconnected` от любого типа сокета.
 * Он выполняет полный цикл очистки ресурсов и обновления состояния сервера:
 * 
 * **Этапы работы:**
 * 1. Определяет, был ли клиент аутентифицирован (проверка в `m_clientsReverse`).
 * 2. Обновляет время последнего визита (`last_seen`) в базе данных.
 * 3. Удаляет пользователя из списков онлайн-статуса (`m_clients`, `m_clientsReverse`).
 * 4. Рассылает всем активным клиентам обновленный список присутствия.
 * 5. Для TCP-клиентов: очищает буферы парсера (`m_nextBlockSizes`) и криптографический контекст (`m_clientCrypto`).
 * 6. Безопасно удаляет объект сокета через `deleteLater()`.
 * 
 * @note Метод безопасен для вызова как от аутентифицированных, так и от неаутентифицированных клиентов.
 * @note Использует `deleteLater()` вместо `delete` для предотвращения крашей при наличии необработанных событий.
 */
void Server::onClientDisconnected()
{
    // -----------------------------------------------------------------------
    // 1. Идентификация отключившегося клиента
    // -----------------------------------------------------------------------
    auto socket = qobject_cast<QObject*>(sender());
    if (!socket) {
        qWarning() << "[SERVER] onClientDisconnected called with invalid sender";
        return;
    }

    // -----------------------------------------------------------------------
    // 2. Проверка статуса аутентификации
    // -----------------------------------------------------------------------
    QString username = m_clientsReverse.value(socket);
    
    if (!username.isEmpty()) {
        // ====================================================================
        // СЛУЧАЙ 1: Аутентифицированный пользователь
        // ====================================================================
        qInfo() << "[SERVER] User" << username << "disconnected from" 
                << (qobject_cast<QTcpSocket*>(socket) ? "TCP" : "WebSocket");

        // --- Обновление метаданных в базе данных ---
        QSqlQuery updateQuery;
        updateQuery.prepare("UPDATE users SET last_seen = :lastSeen WHERE username = :username");
        updateQuery.bindValue(":lastSeen", QDateTime::currentDateTime().toString(Qt::ISODate));
        updateQuery.bindValue(":username", username);
        
        if (!updateQuery.exec()) {
            qWarning() << "[SERVER] Failed to update last_seen for user" << username 
                       << ":" << updateQuery.lastError().text();
        } else {
            qDebug() << "[SERVER] Updated last_seen for user" << username;
        }

        // --- Удаление из списков онлайн-статуса ---
        m_clients.remove(username);         // username -> socket
        m_clientsReverse.remove(socket);    // socket -> username

        // --- Оповещение других пользователей об изменении статуса ---
        broadcastUserList();
        
    } else {
        // ====================================================================
        // СЛУЧАЙ 2: Неаутентифицированный клиент (отключился до логина)
        // ====================================================================
        qDebug() << "[SERVER] Unauthenticated client disconnected";
    }

    // -----------------------------------------------------------------------
    // 3. Очистка TCP-специфичных ресурсов
    // -----------------------------------------------------------------------
    if (auto tcpSocket = qobject_cast<QTcpSocket*>(socket)) {
        
        // Удаляем состояние парсера потока данных
        m_nextBlockSizes.remove(tcpSocket);
        
        // Освобождаем криптографический контекст (если был создан)
        if (m_clientCrypto.contains(tcpSocket)) {
            delete m_clientCrypto[tcpSocket]; // Освобождаем память CryptoManager
            m_clientCrypto.remove(tcpSocket);
            qDebug() << "[SERVER] Crypto context cleaned for TCP socket";
        }
    }
    // Итерируемся по всем активным звонкам
    auto it = m_activeCalls.begin();
    while (it != m_activeCalls.end()) {
        CallInfo& info = it.value();

        // Проверяем, участвует ли этот сокет в звонке
        if (info.fromSocket == socket || info.toSocket == socket) {
            qInfo() << "[SERVER] Active call found for disconnected user. Terminating call:" << info.callId;

            // 1. Определяем сокет собеседника (того, кто остался жив)
            QObject* peerSocket = (info.fromSocket == socket) ? info.toSocket : info.fromSocket;

            // 2. Если собеседник есть и соединение живо, уведомляем его
            // (Проверяем peerSocket на null, хотя он может быть уже невалиден,
            // но sendJson внутри себя обычно безопасен или peerSocket уже удален из m_clients)
            if (peerSocket) {
                QJsonObject response;
                response["type"] = "call_end";
                response["call_id"] = info.callId;
                sendJson(peerSocket, response);
            }

            // 3. Обновляем статус в БД
            updateCallEnded(info.callId, "completed");

            // 4. Удаляем звонок из памяти.
            // Важно: it = erase(it) возвращает итератор на следующий элемент,
            // поэтому цикл не ломается.
            it = m_activeCalls.erase(it);
        } else {
            // Если звонок не касается этого юзера, идем дальше
            ++it;
        }
    }

    // -----------------------------------------------------------------------
    // 4. Безопасное удаление объекта сокета
    // -----------------------------------------------------------------------
    // deleteLater() планирует удаление объекта при следующей итерации event loop.
    // Это предотвращает крашы, если в очереди событий остались сигналы от этого объекта.
    socket->deleteLater();
    
    qDebug() << "[SERVER] Socket marked for deletion";
}


/**
 * @brief Инициализирует соединение с базой данных SQLite и создает схему таблиц.
 *
 * @details Этот метод вызывается один раз в конструкторе сервера и выполняет критически важную
 * задачу по настройке слоя персистентности (хранения данных).
 * 
 * **Выполняемые операции:**
 * 1. Устанавливает соединение с файлом `messenger.db` (создает, если не существует).
 * 2. Создает таблицы для хранения пользователей, сообщений, контактов, звонков, файлов и токенов.
 * 3. Создает индексы для оптимизации частых запросов.
 * 4. Очищает истекшие токены сессий.
 * 
 * **Структура базы данных:**
 * - `users`: Профили пользователей (логин, хеш пароля, метаданные).
 * - `messages`: История сообщений с флагами доставки/прочтения.
 * - `contacts`: Связи между пользователями (друзья, запросы).
 * - `call_history`: Журнал VoIP звонков (статистика, длительность).
 * - `files`: Метаданные загруженных файлов.
 * - `tokens`: Токены автологина с временем истечения.
 * 
 * @return `true`, если соединение установлено и все таблицы успешно созданы/проверены, иначе `false`.
 * 
 * @note Использует `CREATE TABLE IF NOT EXISTS`, поэтому безопасен для повторного вызова.
 * @note В случае ошибки выводит критическое сообщение (`qCritical`) и возвращает `false`.
 */
bool Server::initDatabase()
{
    // ═══════════════════════════════════════════════════════════════════════
    // 1. Установка соединения с базой данных
    // ═══════════════════════════════════════════════════════════════════════
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("messenger.db"); // Файл БД в текущей рабочей директории

    if (!db.open()) {
        qCritical() << "[DB] FATAL: Failed to open database:" << db.lastError().text();
        return false;
    }
    qInfo() << "[DB] Database connection established successfully.";

    QSqlQuery query;

    // ═══════════════════════════════════════════════════════════════════════
    // 2. Создание таблицы пользователей (users)
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Таблица: users
     * Описание: Хранит учетные записи пользователей с хешированными паролями.
     * Поля:
     *  - id: Первичный ключ (автоинкремент).
     *  - username: Уникальный логин (используется для входа).
     *  - display_name: Отображаемое имя (никнейм).
     *  - password_hash: Хеш пароля (Argon2i).
     *  - salt: Соль для хеширования (случайная, уникальная для каждого пользователя).
     *  - creation_date: Дата регистрации (ISO 8601).
     *  - last_seen: Время последнего визита (обновляется при отключении).
     *  - avatar_url: Путь к аватару или Base64 строка.
     *  - status_message: Текстовый статус ("О себе").
     */
    if (!query.exec("CREATE TABLE IF NOT EXISTS users ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "username TEXT UNIQUE NOT NULL, "
                    "display_name TEXT NOT NULL, "
                    "password_hash TEXT NOT NULL, "
                    "salt TEXT NOT NULL, "
                    "creation_date TEXT NOT NULL, "
                    "last_seen TEXT, "
                    "avatar_url TEXT, "
                    "status_message TEXT "
                    ");")) {
        qCritical() << "[DB] Error: Failed to create 'users' table:" << query.lastError().text();
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 3. Создание таблицы сообщений (messages)
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Таблица: messages
     * Описание: Хранит все сообщения между пользователями.
     * Поля:
     *  - id: Уникальный ID сообщения (глобальный, автоинкремент).
     *  - fromUser: Отправитель (username).
     *  - toUser: Получатель (username).
     *  - payload: Текст сообщения или описание медиа.
     *  - timestamp: Время отправки (ISO 8601).
     *  - is_delivered: Флаг доставки (0/1) — "две серые галочки".
     *  - is_read: Флаг прочтения (0/1) — "две синие галочки".
     *  - is_edited: Флаг редактирования (0/1).
     *  - reply_to_id: ID цитируемого сообщения (NULL, если не ответ).
     *  - forwarded_from: Username оригинального автора (для пересланных).
     *  - message_type: Тип контента (0=текст, 1=изображение, 2=файл, и т.д.).
     *  - media_url: Локальный путь или URL к файлу (для медиа).
     */
    if (!query.exec("CREATE TABLE IF NOT EXISTS messages ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "fromUser TEXT NOT NULL, "
                    "toUser TEXT NOT NULL, "
                    "payload TEXT NOT NULL, "
                    "timestamp TEXT NOT NULL, "
                    "is_delivered INTEGER NOT NULL DEFAULT 0, "
                    "is_read INTEGER NOT NULL DEFAULT 0, "
                    "is_edited INTEGER NOT NULL DEFAULT 0, "
                    "reply_to_id INTEGER, "
                    "forwarded_from TEXT, "
                    "message_type INTEGER NOT NULL DEFAULT 0, "
                    "media_url TEXT, "
                    "file_id TEXT, "
                    "file_name TEXT, "
                    "file_url TEXT"
                    ");")) {
        qCritical() << "[DB] Error: Failed to create 'messages' table:" << query.lastError().text();
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 4. Создание таблицы контактов (contacts)
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Таблица: contacts
     * Описание: Связующая таблица для отношений "многие ко многим" (друзья, подписки).
     * Поля:
     *  - id: Первичный ключ.
     *  - user_id_1, user_id_2: ID двух пользователей (внешние ключи на users.id).
     *  - status: Статус связи (0=ожидание подтверждения, 1=подтверждено, 2=заблокировано).
     *  - creation_date: Время создания связи.
     * Ограничения:
     *  - UNIQUE(user_id_1, user_id_2): Предотвращает дублирование.
     *  - CHECK(user_id_1 < user_id_2): Обеспечивает каноническое представление (меньший ID всегда первый).
     */
    if (!query.exec("CREATE TABLE IF NOT EXISTS contacts ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "user_id_1 INTEGER NOT NULL, "
                    "user_id_2 INTEGER NOT NULL, "
                    "status INTEGER NOT NULL DEFAULT 0, "
                    "creation_date TEXT NOT NULL, "
                    "FOREIGN KEY(user_id_1) REFERENCES users(id), "
                    "FOREIGN KEY(user_id_2) REFERENCES users(id), "
                    "UNIQUE(user_id_1, user_id_2), "
                    "CHECK(user_id_1 < user_id_2)"
                    ");")) {
        qCritical() << "[DB] Error: Failed to create 'contacts' table:" << query.lastError().text();
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 5. Создание таблицы истории звонков (call_history)
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Таблица: call_history
     * Описание: Журнал VoIP звонков (для аналитики и UI "Недавние звонки").
     * Поля:
     *  - call_id: Уникальный UUID звонка.
     *  - caller_username, callee_username: Инициатор и получатель.
     *  - status: Статус завершения (ringing, connected, completed, rejected, missed).
     *  - start_time: Время начала попытки соединения.
     *  - connect_time: Время фактического соединения (NULL, если не состоялся).
     *  - end_time: Время завершения.
     *  - duration_seconds: Длительность разговора.
     *  - caller_ip, caller_port, callee_ip, callee_port: Сетевая информация (для NAT traversal логов).
     */
    if (!query.exec("CREATE TABLE IF NOT EXISTS call_history ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "call_id TEXT UNIQUE NOT NULL, "
                    "caller_username TEXT NOT NULL, "
                    "callee_username TEXT NOT NULL, "
                    "status VARCHAR(20) NOT NULL DEFAULT 'ringing', "
                    "start_time TEXT NOT NULL, "
                    "connect_time TEXT, "
                    "end_time TEXT, "
                    "duration_seconds INTEGER DEFAULT 0, "
                    "caller_ip VARCHAR(45), "
                    "caller_port INTEGER, "
                    "callee_ip VARCHAR(45), "
                    "callee_port INTEGER, "
                    "FOREIGN KEY(caller_username) REFERENCES users(username), "
                    "FOREIGN KEY(callee_username) REFERENCES users(username)"
                    ");")) {
        qCritical() << "[DB] Error: Failed to create 'call_history' table:" << query.lastError().text();
        return false;
    }

    qDebug() << "[DB] Call history table initialized successfully.";

    // ═══════════════════════════════════════════════════════════════════════
    // 6. Создание индексов для оптимизации запросов
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Индексы значительно ускоряют выборки по часто используемым полям.
     * Например: "Показать все звонки пользователя X за последний месяц".
     */
    query.exec("CREATE INDEX IF NOT EXISTS idx_call_caller ON call_history(caller_username);");
    query.exec("CREATE INDEX IF NOT EXISTS idx_call_callee ON call_history(callee_username);");
    query.exec("CREATE INDEX IF NOT EXISTS idx_call_start_time ON call_history(start_time DESC);");
    query.exec("CREATE INDEX IF NOT EXISTS idx_call_id ON call_history(call_id);");

    // ═══════════════════════════════════════════════════════════════════════
    // 7. Создание таблицы метаданных файлов (files)
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Таблица: files
     * Описание: Хранит информацию о загруженных файлах (изображения, документы).
     * Поля:
     *  - file_uuid: Уникальный идентификатор файла на сервере.
     *  - owner_username: Кто загрузил файл.
     *  - original_filename: Исходное имя файла (для скачивания).
     *  - filesize: Размер в байтах.
     *  - status: Статус загрузки (0=в процессе, 1=завершено, 2=ошибка).
     *  - upload_date: Время загрузки.
     */
    if (!query.exec("CREATE TABLE IF NOT EXISTS files ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "file_uuid TEXT UNIQUE NOT NULL, "
                    "owner_username TEXT NOT NULL, "
                    "original_filename TEXT NOT NULL, "
                    "filesize INTEGER NOT NULL, "
                    "status INTEGER NOT NULL DEFAULT 0, "
                    "upload_date TEXT NOT NULL"
                    ");")) {
        qCritical() << "[DB] Error: Failed to create 'files' table:" << query.lastError().text();
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 8. Создание таблицы токенов автологина (tokens)
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Таблица: tokens
     * Описание: Хранит токены сессий для автоматического входа (без повторного ввода пароля).
     * Поля:
     *  - username: Владелец токена (PRIMARY KEY).
     *  - token: Случайная криптостойкая строка (UUID или Base64).
     *  - created_at: Время создания токена.
     *  - expires_at: Время истечения (например, created_at + 30 дней).
     * Ограничения:
     *  - ON DELETE CASCADE: При удалении пользователя его токены удаляются автоматически.
     */
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS tokens (
            username TEXT PRIMARY KEY,
            token TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            expires_at DATETIME NOT NULL,
            FOREIGN KEY (username) REFERENCES users(username) ON DELETE CASCADE
        )
    )")) {
        qCritical() << "[DB] Error: Failed to create 'tokens' table:" << query.lastError().text();
        return false;
    }

    qDebug() << "[DB] Tokens table initialized successfully.";

    // ═══════════════════════════════════════════════════════════════════════
    // 9. Очистка истекших токенов при запуске сервера
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Удаляем все токены, время действия которых истекло.
     * Это предотвращает накопление "мертвых" записей в БД.
     */
    if (!query.exec("DELETE FROM tokens WHERE expires_at < CURRENT_TIMESTAMP")) {
        qWarning() << "[DB] Failed to clean expired tokens:" << query.lastError().text();
    } else {
        qDebug() << "[DB] Expired tokens cleaned successfully.";
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 10. Финализация
    // ═══════════════════════════════════════════════════════════════════════
    qInfo() << "[DB] All database tables and indices initialized successfully.";
    return true;
}



/**
 * @brief Обрабатывает и пересылает уведомление о статусе "печатает..." (Typing Indicator).
 *
 * @details Этот метод реализует функционал "X печатает..." в мессенджере.
 * Сервер выступает в роли простого транслятора:
 * 1. Определяет отправителя уведомления (кто начал печатать).
 * 2. Извлекает получателя из запроса (кому показать индикатор).
 * 3. Проверяет, онлайн ли получатель.
 * 4. Если онлайн — пересылает уведомление в реальном времени.
 * 
 * **Особенности:**
 * - Уведомления о печати **не сохраняются** в базе данных (они эфемерные).
 * - Если получатель офлайн, уведомление просто игнорируется.
 * - Клиент обычно отправляет это уведомление каждые 3-5 секунд, пока печатает.
 * 
 * @param socket Сокет клиента, который начал печатать.
 * @param request JSON-объект с полями:
 *        - `toUser`: Username получателя (кому показать индикатор).
 */
void Server::handleTyping(QObject *socket, const QJsonObject &request)
{
    // -----------------------------------------------------------------------
    // 1. Идентификация отправителя
    // -----------------------------------------------------------------------
    QString fromUsername = m_clientsReverse.value(socket);
    
    if (fromUsername.isEmpty()) {
        qWarning() << "[SERVER] Typing indicator from unauthenticated socket";
        return;
    }

    // -----------------------------------------------------------------------
    // 2. Извлечение получателя из запроса
    // -----------------------------------------------------------------------
    QString toUsername = request["toUser"].toString();
    
    if (toUsername.isEmpty()) {
        qWarning() << "[SERVER] Typing indicator missing 'toUser' field from" << fromUsername;
        return;
    }

    // -----------------------------------------------------------------------
    // 3. Поиск получателя в списке онлайн-пользователей
    // -----------------------------------------------------------------------
    QObject* toSocket = m_clients.value(toUsername, nullptr);

    if (toSocket) {
        // ====================================================================
        // СЛУЧАЙ 1: Получатель онлайн — пересылаем уведомление
        // ====================================================================
        QJsonObject forwardMessage;
        forwardMessage["type"] = "typing";
        forwardMessage["fromUser"] = fromUsername; // Кто печатает
        
        sendJson(toSocket, forwardMessage);
        
        // Для отладки можно убрать это логирование, так как typing-событий очень много
        // qDebug() << "[SERVER] Typing indicator forwarded:" << fromUsername << "->" << toUsername;
        
    } else {
        // ====================================================================
        // СЛУЧАЙ 2: Получатель офлайн — игнорируем
        // ====================================================================
        // Это нормальная ситуация, не требующая логирования.
        // Клиент получателя увидит индикатор, когда вернется онлайн и отправитель все еще будет печатать.
    }
}


/**
 * @brief Обрабатывает запрос клиента на получение истории сообщений с пагинацией.
 *
 * @details Этот метод извлекает из базы данных порцию сообщений для указанного чата.
 * Он поддерживает **пагинацию** (постраничную загрузку) для оптимизации трафика и производительности.
 * 
 * **Алгоритм работы:**
 * 1. Идентифицирует запрашивающего пользователя.
 * 2. Извлекает параметры запроса (`with_user`, `before_id`).
 * 3. Формирует SQL-запрос для выборки сообщений:
 *    - Выбирает сообщения между двумя пользователями (в обе стороны).
 *    - Если указан `before_id`, загружает только сообщения старше этого ID.
 *    - Ограничивает выборку 20 сообщениями.
 * 4. Инвертирует порядок сообщений (БД возвращает от новых к старым, клиенту нужно наоборот).
 * 5. Отправляет результат клиенту.
 * 
 * **Пример использования:**
 * - Первая загрузка чата: `before_id = 0` (или отсутствует) → возвращает последние 20 сообщений.
 * - Прокрутка вверх: `before_id = 12345` → возвращает до 20 сообщений старше ID 12345.
 * 
 * @param socket Сокет клиента, запросившего историю.
 * @param request JSON-объект с полями:
 *        - `with_user`: Username собеседника (обязательно).
 *        - `before_id`: ID сообщения, старше которого нужно загрузить историю (опционально, 0 = загрузить самые новые).
 */
void Server::handleGetHistory(QObject* socket, const QJsonObject& request)
{
    // -----------------------------------------------------------------------
    // 1. Извлечение параметров запроса
    // -----------------------------------------------------------------------
    QString requestingUser = m_clientsReverse.value(socket);
    
    if (requestingUser.isEmpty()) {
        qWarning() << "[SERVER] History request from unauthenticated socket";
        return;
    }
    
    QString chatPartner = request["with_user"].toString();
    
    if (chatPartner.isEmpty()) {
        qWarning() << "[SERVER]History request missing 'with_user' field from" << requestingUser;
        return;
    }
    
    qint64 beforeId = request["before_id"].toDouble(); // 0, если поле отсутствует

    qDebug() << "[SERVER] History request from" << requestingUser
             << "for chat with" << chatPartner
             << (beforeId > 0 ? QString("(before message ID: %1)").arg(beforeId) : "(initial load)");

    // -----------------------------------------------------------------------
    // 2. Формирование SQL-запроса с поддержкой пагинации
    // -----------------------------------------------------------------------
    QString queryString =
        "SELECT id, fromUser, toUser, payload, timestamp, reply_to_id, is_read, is_edited, is_delivered, file_id, file_name, file_url "
        "FROM messages "
        "WHERE ((fromUser = :user1 AND toUser = :user2) OR (fromUser = :user2 AND toUser = :user1)) ";

    // Добавляем условие пагинации: загружать только сообщения старше указанного ID
    if (beforeId > 0) {
        queryString += "AND id < :beforeId ";
    }

    // Сортировка: от новых к старым (DESC), чтобы взять последние 20 перед beforeId
    queryString += "ORDER BY id DESC LIMIT 20";

    // -----------------------------------------------------------------------
    // 3. Подготовка и выполнение запроса
    // -----------------------------------------------------------------------
    QSqlQuery query;
    query.prepare(queryString);
    query.bindValue(":user1", requestingUser);
    query.bindValue(":user2", chatPartner);
    
    if (beforeId > 0) {
        query.bindValue(":beforeId", beforeId);
    }

    if (!query.exec()) {
        qWarning() << "[SERVER] Failed to fetch history for" << requestingUser 
                   << "and" << chatPartner << ":" << query.lastError().text();
        
        // Отправляем клиенту ответ с пустой историей, чтобы он не завис в ожидании
        QJsonObject errorResponse;
        errorResponse["type"] = beforeId > 0 ? "old_history_data" : "history_data";
        errorResponse["with_user"] = chatPartner;
        errorResponse["history"] = QJsonArray(); // Пустой массив
        sendJson(socket, errorResponse);
        return;
    }

    // -----------------------------------------------------------------------
    // 4. Сборка JSON-массива из результатов запроса
    // -----------------------------------------------------------------------
    QJsonArray historyArray;
    
    while (query.next()) {
        QSqlRecord record = query.record();
        QJsonObject messageObject;
        
        // Заполняем JSON-объект данными из записи БД
        messageObject["id"] = record.value("id").toLongLong();
        messageObject["fromUser"] = record.value("fromUser").toString();
        messageObject["toUser"] = record.value("toUser").toString();
        messageObject["payload"] = record.value("payload").toString();
        messageObject["timestamp"] = record.value("timestamp").toString();
        messageObject["is_read"] = record.value("is_read").toInt();
        messageObject["is_delivered"] = record.value("is_delivered").toInt();
        messageObject["is_edited"] = record.value("is_edited").toInt();
        messageObject["reply_to_id"] = record.value("reply_to_id").toLongLong();
        messageObject["file_id"] = record.contains("file_id") ? record.value("file_id").toString() : "";
        messageObject["file_name"] = record.contains("file_name") ? record.value("file_name").toString() : "";
        messageObject["file_url"] = record.contains("file_url") ? record.value("file_url").toString() : "";

        historyArray.append(messageObject);
    }

    qDebug() << "[SERVER] Found" << historyArray.count() << "messages for chat" 
             << requestingUser << "<->" << chatPartner;

    // -----------------------------------------------------------------------
    // 5. Инверсия порядка сообщений
    // -----------------------------------------------------------------------
    // SQL вернул сообщения в порядке DESC (от новых к старым), но клиенту нужно
    // отображать их от старых к новым (снизу вверх в UI), поэтому переворачиваем массив.
    QJsonArray reversedArray;
    for (int i = historyArray.size() - 1; i >= 0; --i) {
        reversedArray.append(historyArray.at(i));
    }

    // -----------------------------------------------------------------------
    // 6. Формирование и отправка ответа
    // -----------------------------------------------------------------------
    QJsonObject response;
    
    // Тип ответа зависит от контекста запроса:
    // - "history_data": Первоначальная загрузка чата (beforeId = 0).
    // - "old_history_data": Подгрузка старых сообщений при прокрутке вверх (beforeId > 0).
    response["type"] = (beforeId > 0) ? "old_history_data" : "history_data";
    response["with_user"] = chatPartner;
    response["history"] = reversedArray;
    
    sendJson(socket, response);
}


/**
 * @brief Обрабатывает регистрацию нового пользователя с двухэтапным хешированием.
 *
 * @details Этот метод реализует безопасную процедуру регистрации с многоуровневой защитой:
 * 
 * **Этапы обработки:**
 * 1. **Валидация данных:** Проверка длины, формата и допустимых символов в username.
 * 2. **Генерация соли:** Создание уникальной криптостойкой соли (16 байт).
 * 3. **Хеширование пароля:** Применение Argon2i с солью (защита от rainbow table атак).
 * 4. **Сохранение в БД:** Запись username, хеша пароля и соли.
 * 5. **Оповещение клиента:** Отправка результата (успех/ошибка).
 * 
 * **Меры безопасности:**
 * - Пароли **никогда не сохраняются** в открытом виде.
 * - Каждый пользователь имеет **уникальную соль** (предотвращает атаки по предвычисленным хешам).
 * - Используется **Argon2i** — алгоритм, устойчивый к GPU/ASIC брутфорсу.
 * - Проверка на дубликаты username через UNIQUE constraint в БД.
 * 
 * **Формат запроса (JSON):**
 * ```
 * {
 *   "type": "register",
 *   "username": "john_doe",
 *   "displayname": "John Doe",
 *   "password": "mySecretPassword123"
 * }
 * ```
 * 
 * **Формат ответа (успех):**
 * ```
 * {
 *   "type": "register_success",
 *   "username": "john_doe",
 *   "displayname": "John Doe"
 * }
 * ```
 * 
 * **Формат ответа (ошибка):**
 * ```
 * {
 *   "type": "register_failure",
 *   "reason": "Username already exists"
 * }
 * ```
 *
 * @param socket Сокет клиента, отправившего запрос на регистрацию.
 * @param request JSON-объект с полями:
 *        - `username`: Уникальный логин (3-20 символов, только буквы, цифры, _, -).
 *        - `displayname`: Отображаемое имя (опционально, если пусто — используется username).
 *        - `password`: Пароль в открытом виде (будет захеширован на сервере).
 */
void Server::handleRegister(QObject* socket, const QJsonObject& request)
{
    // ═══════════════════════════════════════════════════════════════════════
    // 1. Извлечение данных из запроса
    // ═══════════════════════════════════════════════════════════════════════
    QString username = request.value("username").toString().trimmed();
    QString displayname = request.value("displayname").toString().trimmed();
    QString password = request.value("password").toString();
    
    qDebug() << "[SERVER] Registration attempt for username:" << username;
    
    QJsonObject response;
    
    // ═══════════════════════════════════════════════════════════════════════
    // 2. Валидация username (длина)
    // ═══════════════════════════════════════════════════════════════════════
    if (username.isEmpty() || username.length() < 3 || username.length() > 20) {
        response["type"] = "register_failure";
        response["reason"] = "Invalid username (3-20 characters required)";
        sendJson(socket, response);
        qWarning() << "[SERVER] Registration failed: invalid username length for" << username;
        return;
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // 3. Валидация username (допустимые символы)
    // ═══════════════════════════════════════════════════════════════════════
    // Разрешены только: латинские буквы, цифры, подчеркивание, дефис.
    // Это предотвращает SQL injection и проблемы с отображением в UI.
    QRegularExpression usernameRegex("^[a-zA-Z0-9_-]+$");
    if (!usernameRegex.match(username).hasMatch()) {
        response["type"] = "register_failure";
        response["reason"] = "Username contains invalid characters (allowed: a-z, A-Z, 0-9, _, -)";
        sendJson(socket, response);
        qWarning() << "[SERVER] Registration failed: invalid characters in username" << username;
        return;
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // 4. Обработка displayname
    // ═══════════════════════════════════════════════════════════════════════
    if (displayname.isEmpty()) {
        displayname = username; // По умолчанию displayname = username
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // 5. Валидация пароля (опционально)
    // ═══════════════════════════════════════════════════════════════════════
    // Можно добавить проверку минимальной длины/сложности пароля:
    // if (password.length() < 8) { ... }
    
    // ═══════════════════════════════════════════════════════════════════════
    // 6. Генерация уникальной соли
    // ═══════════════════════════════════════════════════════════════════════
    // Соль делает невозможным использование rainbow tables и предвычисленных хешей.
    // Каждый пользователь имеет свою уникальную соль, даже если пароли одинаковые.
    QByteArray serverSalt = CryptoUtils::generateSalt(16); // 16 байт = 128 бит
    
    qDebug() << "[SERVER] Generated salt for user:" << username;
    
    // ═══════════════════════════════════════════════════════════════════════
    // 7. Хеширование пароля с помощью Argon2i
    // ═══════════════════════════════════════════════════════════════════════
    // Argon2i — современный алгоритм, победитель Password Hashing Competition (2015).
    // Он медленный по дизайну, что делает брутфорс экономически нецелесообразным.
    QString finalHashString = CryptoUtils::hashPasswordArgon2(password, serverSalt);
    
    if (finalHashString.isEmpty()) {
        response["type"] = "register_failure";
        response["reason"] = "Password hashing failed (server error)";
        sendJson(socket, response);
        qCritical() << "[SERVER] Argon2 hashing failed for user" << username;
        return;
    }
    
    qDebug() << "[SERVER] Password hashed successfully. Hash length:" << finalHashString.length();
    
    // ═══════════════════════════════════════════════════════════════════════
    // 8. Подготовка SQL-запроса
    // ═══════════════════════════════════════════════════════════════════════
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO users (username, display_name, password_hash, salt, creation_date)
        VALUES (:username, :display_name, :password_hash, :salt, :creation_date)
    )");
    
    query.bindValue(":username", username);
    query.bindValue(":display_name", displayname);
    query.bindValue(":password_hash", finalHashString); // Хеш в Hex-формате
    query.bindValue(":salt", serverSalt.toHex());       // Соль в Hex-формате (для текстового хранения)
    query.bindValue(":creation_date", QDateTime::currentDateTime().toString(Qt::ISODate));
    
    // ═══════════════════════════════════════════════════════════════════════
    // 9. Выполнение запроса и обработка результата
    // ═══════════════════════════════════════════════════════════════════════
    if (query.exec()) {
        // ====================================================================
        // УСПЕХ: Пользователь зарегистрирован
        // ====================================================================
        response["type"] = "register_success";
        response["username"] = username;
        response["displayname"] = displayname;
        
        qInfo() << "[SERVER] ✅ New user registered:" << username;
        
        // Обновляем список пользователей для всех подключенных клиентов
        // (хотя новый пользователь еще не авторизован, но это для полноты)
        broadcastUserList();
        
    } else {
        // ====================================================================
        // ОШИБКА: Регистрация не удалась
        // ====================================================================
        response["type"] = "register_failure";
        
        // Проверка на дубликат username (UNIQUE constraint)
        if (query.lastError().text().contains("UNIQUE constraint failed")) {
            response["reason"] = "Username already exists";
            qWarning() << "[SERVER] ❌ Registration failed: username already taken" << username;
        } else {
            response["reason"] = "Database error";
            qCritical() << "[SERVER] ❌ Registration failed for" << username
                       << "with DB error:" << query.lastError().text();
        }
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // 10. Отправка ответа клиенту
    // ═══════════════════════════════════════════════════════════════════════
    sendJson(socket, response);
}


/**
 * @brief Обрабатывает запрос на глобальный поиск пользователей в системе.
 *
 * @details Этот метод реализует функционал поиска, похожий на Telegram или WhatsApp:
 * пользователь вводит часть имени, и сервер возвращает список совпадений.
 * 
 * **Алгоритм работы:**
 * 1. Извлекает поисковый запрос (`term`) из JSON.
 * 2. Выполняет SQL-запрос с оператором `LIKE` для частичного совпадения.
 * 3. Ищет по двум полям: `username` (логин) и `display_name` (отображаемое имя).
 * 4. Исключает из результатов самого пользователя (нельзя найти себя).
 * 5. Ограничивает выдачу 20 результатами для производительности.
 * 6. Отправляет клиенту массив найденных пользователей.
 * 
 * **Примеры поиска:**
 * - Запрос `"john"` найдет: `john_doe`, `johnny`, `John Smith`.
 * - Запрос `"sm"` найдет: `smith123`, `Mike Smith`, `smolder`.
 * 
 * **Оптимизация:**
 * - Для больших баз данных (>10000 пользователей) рекомендуется добавить индексы:
 *   ```
 *   CREATE INDEX idx_username ON users(username);
 *   CREATE INDEX idx_display_name ON users(display_name);
 *   ```
 * - Или использовать полнотекстовый поиск (FTS5 в SQLite).
 * 
 * **Формат запроса:**
 * ```
 * {
 *   "type": "search_users",
 *   "term": "john"
 * }
 * ```
 * 
 * **Формат ответа:**
 * ```
 * {
 *   "type": "search_results",
 *   "users": [
 *     {"username": "john_doe", "displayname": "John Doe"},
 *     {"username": "johnny", "displayname": "Johnny Bravo"}
 *   ]
 * }
 * ```
 *
 * @param socket Сокет клиента, выполнившего поиск.
 * @param request JSON-объект с полем:
 *        - `term`: Строка для поиска (минимум 1 символ).
 */
void Server::handleSearchUsers(QObject* socket, const QJsonObject& request)
{
    // ═══════════════════════════════════════════════════════════════════════
    // 1. Извлечение параметров запроса
    // ═══════════════════════════════════════════════════════════════════════
    QString searchTerm = request["term"].toString().trimmed();
    QString currentUser = m_clientsReverse.value(socket);

    if (currentUser.isEmpty()) {
        qWarning() << "[SERVER] Search request from unauthenticated socket";
        return;
    }

    // Проверка на пустой поисковый запрос
    if (searchTerm.isEmpty()) {
        qDebug() << "[SERVER] Empty search term from user" << currentUser;
        
        // Отправляем пустой результат
        QJsonObject response;
        response["type"] = "search_results";
        response["users"] = QJsonArray();
        sendJson(socket, response);
        return;
    }

    qDebug() << "[SERVER] User" << currentUser << "searching for:" << searchTerm;

    // ═══════════════════════════════════════════════════════════════════════
    // 2. Подготовка SQL-запроса с частичным совпадением
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * SQL-запрос выполняет поиск по двум полям:
     * - username: Уникальный логин (обычно латиница).
     * - display_name: Отображаемое имя (может содержать любые символы, включая кириллицу).
     * 
     * Оператор LIKE:
     * - '%term%' находит 'term' в любом месте строки.
     * - Чувствителен к регистру в SQLite по умолчанию, но можно изменить через COLLATE NOCASE.
     */
    QSqlQuery query;
    query.prepare("SELECT username, display_name FROM users "
                  "WHERE (username LIKE :term OR display_name LIKE :term) "
                  "AND username != :currentUser "
                  "LIMIT 20");
    
    // Добавляем символы '%' для поиска подстроки в любой позиции
    query.bindValue(":term", "%" + searchTerm + "%");
    query.bindValue(":currentUser", currentUser); // Исключаем себя из результатов

    // ═══════════════════════════════════════════════════════════════════════
    // 3. Выполнение запроса
    // ═══════════════════════════════════════════════════════════════════════
    if (!query.exec()) {
        qWarning() << "[SERVER] User search failed for term '" << searchTerm 
                   << "':" << query.lastError().text();
        
        // Отправляем пустой результат при ошибке БД
        QJsonObject errorResponse;
        errorResponse["type"] = "search_results";
        errorResponse["users"] = QJsonArray();
        sendJson(socket, errorResponse);
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 4. Формирование JSON-массива с результатами
    // ═══════════════════════════════════════════════════════════════════════
    QJsonArray usersFound;
    
    while (query.next()) {
        QJsonObject userObject;
        userObject["username"] = query.value("username").toString();
        userObject["displayname"] = query.value("display_name").toString();
        // Можно добавить дополнительные поля: avatar_url, status_message, last_seen
        
        usersFound.append(userObject);
    }

    qDebug() << "[SERVER] Found" << usersFound.count() 
             << "user(s) matching term '" << searchTerm << "'";

    // ═══════════════════════════════════════════════════════════════════════
    // 5. Отправка результатов клиенту
    // ═══════════════════════════════════════════════════════════════════════
    QJsonObject response;
    response["type"] = "search_results";
    response["users"] = usersFound;
    
    sendJson(socket, response);
}


/**
 * @brief Отправляет клиенту его персональный список подтвержденных контактов.
 *
 * @details Этот метод вызывается автоматически после успешной аутентификации пользователя.
 * Он извлекает из базы данных всех пользователей, с которыми установлена двусторонняя
 * связь со статусом "подтверждено" (`status = 1`).
 * 
 * **Особенности работы с таблицей `contacts`:**
 * - Таблица хранит связи между пользователями в **канонической форме**: `user_id_1 < user_id_2`.
 * - Это значит, что для пары пользователей с ID 5 и 10 в БД будет только одна запись: `(5, 10)`.
 * - Поэтому в SQL-запросе используется условие `OR`: текущий пользователь может быть как в `user_id_1`, так и в `user_id_2`.
 * 
 * **Алгоритм работы:**
 * 1. Получает внутренний ID пользователя по его username.
 * 2. Выполняет JOIN между таблицами `users` и `contacts`.
 * 3. Фильтрует только подтвержденные связи (`status = 1`).
 * 4. Формирует JSON-массив с данными контактов (username, displayname, last_seen, status_message).
 * 5. Отправляет список клиенту.
 * 
 * **Формат ответа:**
 * ```
 * {
 *   "type": "contact_list",
 *   "users": [
 *     {
 *       "username": "alice",
 *       "displayname": "Alice Cooper",
 *       "last_seen": "2025-11-24T14:30:00",
 *       "statusmessage": "Always online!"
 *     },
 *     ...
 *   ]
 * }
 * ```
 *
 * @param socket Сокет клиента, которому нужно отправить список контактов.
 * @param username Username пользователя, для которого формируется список.
 */
void Server::sendContactList(QObject* socket, const QString& username)
{
    // ═══════════════════════════════════════════════════════════════════════
    // 1. Получение внутреннего ID пользователя
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Таблица contacts работает с числовыми ID (FOREIGN KEY), а не с username.
     * Поэтому сначала нужно преобразовать username → user_id.
     */
    QSqlQuery userQuery;
    userQuery.prepare("SELECT id FROM users WHERE username = :username");
    userQuery.bindValue(":username", username);

    if (!userQuery.exec() || !userQuery.next()) {
        qWarning() << "[SERVER] Could not find user ID for username:" << username;
        
        // Отправляем пустой список контактов (чтобы клиент не завис в ожидании)
        QJsonObject emptyResponse;
        emptyResponse["type"] = "contact_list";
        emptyResponse["users"] = QJsonArray();
        sendJson(socket, emptyResponse);
        return;
    }

    qint64 userId = userQuery.value("id").toLongLong();
    qDebug() << "[SERVER] Building contact list for user" << username << "(ID:" << userId << ")";

    // ═══════════════════════════════════════════════════════════════════════
    // 2. Основной SQL-запрос для получения контактов
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Этот запрос выполняет JOIN между таблицами users и contacts.
     * 
     * Логика:
     * - Таблица contacts хранит пары (user_id_1, user_id_2), где всегда user_id_1 < user_id_2.
     * - Для пользователя с userId нужно найти все записи, где он упоминается:
     *   либо в user_id_1, либо в user_id_2.
     * - Затем извлечь данные "другой стороны" этой связи.
     * - Условие `u.id != :userId` гарантирует, что мы не получим самого себя.
     * - Фильтр `c.status = 1` отбирает только подтверждённые контакты.
     */
    QSqlQuery query;
    query.prepare(
        "SELECT u.username, u.display_name, u.last_seen, u.status_message, u.avatar_url "
        "FROM users u "
        "JOIN contacts c ON (u.id = c.user_id_1 OR u.id = c.user_id_2) "
        "WHERE (c.user_id_1 = :userId OR c.user_id_2 = :userId) "
        "AND c.status = 1 "          // Только подтверждённые контакты
        "AND u.id != :userId"         // Исключаем самого себя
    );
    query.bindValue(":userId", userId);

    if (!query.exec()) {
        qWarning() << "[SERVER] Failed to retrieve contact list for user" << username 
                   << ":" << query.lastError().text();
        
        // Отправляем пустой список при ошибке БД
        QJsonObject errorResponse;
        errorResponse["type"] = "contact_list";
        errorResponse["users"] = QJsonArray();
        sendJson(socket, errorResponse);
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 3. Формирование JSON-массива с данными контактов
    // ═══════════════════════════════════════════════════════════════════════
    QJsonArray contactsArray;
    
    while (query.next()) {
        QJsonObject userObject;
        userObject["username"] = query.value("username").toString();
        userObject["displayname"] = query.value("display_name").toString();
        userObject["last_seen"] = query.value("last_seen").toString();
        userObject["statusmessage"] = query.value("status_message").toString();
        userObject["avatar_url"] = query.value("avatar_url").toString();
        
        contactsArray.append(userObject);
    }

    qDebug() << "[SERVER] Found" << contactsArray.count() << "contact(s) for user" << username;

    // ═══════════════════════════════════════════════════════════════════════
    // 4. Отправка списка контактов клиенту
    // ═══════════════════════════════════════════════════════════════════════
    QJsonObject message;
    message["type"] = "contact_list";
    message["users"] = contactsArray;
    
    sendJson(socket, message);
}


/**
 * @brief Обрабатывает запрос на добавление пользователя в контакты (Friend Request).
 *
 * @details Этот метод реализует механизм "запроса на добавление в друзья", похожий на Facebook или VK:
 * 1. Один пользователь отправляет запрос другому.
 * 2. Запрос сохраняется в БД со статусом "pending" (`status = 0`).
 * 3. Целевой пользователь получает уведомление (если онлайн).
 * 4. Позже целевой пользователь может принять или отклонить запрос (метод `handleContactRequestResponse`).
 * 
 * **Алгоритм работы:**
 * 1. Идентификация инициатора и получателя.
 * 2. Валидация (нельзя добавить самого себя, пустое имя и т.д.).
 * 3. Получение внутренних ID обоих пользователей из БД.
 * 4. Проверка на существующие связи (чтобы избежать дубликатов).
 * 5. Создание новой записи в таблице `contacts` со статусом 0 (pending).
 * 6. Отправка push-уведомления получателю (если он онлайн).
 * 7. Отправка подтверждения инициатору.
 * 
 * **Особенности хранения связей:**
 * - Таблица `contacts` использует **каноническую форму**: `user_id_1 < user_id_2`.
 * - Это означает, что для пары пользователей (5, 10) в БД будет только одна запись: `(5, 10)`.
 * - Это предотвращает дублирование и упрощает запросы.
 * 
 * **Статусы контактов:**
 * - `0`: Pending (ожидает подтверждения).
 * - `1`: Accepted (подтверждено, пользователи — друзья).
 * - `2`: Blocked (один пользователь заблокировал другого).
 * 
 * **Формат запроса:**
 * ```
 * {
 *   "type": "add_contact_request",
 *   "username": "alice"
 * }
 * ```
 * 
 * **Формат ответа (успех):**
 * ```
 * {
 *   "type": "add_contact_success",
 *   "reason": "Запрос на добавление успешно отправлен пользователю alice."
 * }
 * ```
 * 
 * **Формат уведомления получателю:**
 * ```
 * {
 *   "type": "incoming_contact_request",
 *   "fromUsername": "bob",
 *   "fromDisplayname": "Bob Smith"
 * }
 * ```
 *
 * @param socket Сокет клиента-инициатора запроса.
 * @param request JSON-объект с полем:
 *        - `username`: Username пользователя, которого хотят добавить.
 */
void Server::handleAddContactRequest(QObject* socket, const QJsonObject& request)
{
    // ═══════════════════════════════════════════════════════════════════════
    // 1. Идентификация инициатора и получателя
    // ═══════════════════════════════════════════════════════════════════════
    QString fromUsername = m_clientsReverse.value(socket);
    QString toUsername = request["username"].toString().trimmed();

    if (fromUsername.isEmpty()) {
        qWarning() << "[SERVER] Add contact request from unauthenticated socket";
        return;
    }

    qDebug() << "[SERVER] Contact request:" << fromUsername << "->" << toUsername;

    // ═══════════════════════════════════════════════════════════════════════
    // 2. Валидация входных данных
    // ═══════════════════════════════════════════════════════════════════════
    
    // Проверка 1: Целевой пользователь указан
    if (toUsername.isEmpty()) {
        sendJson(socket, {
            {"type", "add_contact_failure"},
            {"reason", "Некорректное имя пользователя."}
        });
        return;
    }

    // Проверка 2: Нельзя добавить самого себя
    if (fromUsername == toUsername) {
        sendJson(socket, {
            {"type", "add_contact_failure"},
            {"reason", "Вы не можете добавить самого себя в контакты."}
        });
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 3. Получение ID и display_name обоих пользователей
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Выполняем один запрос вместо двух для оптимизации.
     * Это эффективнее, чем два отдельных SELECT.
     */
    QSqlQuery idQuery;
    idQuery.prepare("SELECT id, username, display_name FROM users WHERE username = :from OR username = :to");
    idQuery.bindValue(":from", fromUsername);
    idQuery.bindValue(":to", toUsername);
    
    if (!idQuery.exec()) {
        qWarning() << "[SERVER] Failed to find user IDs:" << idQuery.lastError().text();
        sendJson(socket, {
            {"type", "add_contact_failure"},
            {"reason", "Ошибка базы данных."}
        });
        return;
    }

    qint64 fromId = -1, toId = -1;
    QString fromDisplayName;
    
    while (idQuery.next()) {
        QString username = idQuery.value("username").toString();
        if (username == fromUsername) {
            fromId = idQuery.value("id").toLongLong();
            fromDisplayName = idQuery.value("display_name").toString();
        } else if (username == toUsername) {
            toId = idQuery.value("id").toLongLong();
        }
    }

    // Проверяем, что оба пользователя были найдены
    if (fromId == -1 || toId == -1) {
        sendJson(socket, {
            {"type", "add_contact_failure"},
            {"reason", "Запрашиваемый пользователь не существует."}
        });
        qWarning() << "[SERVER] User not found: fromId=" << fromId << ", toId=" << toId;
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 4. Приведение к канонической форме (меньший ID всегда первый)
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Это критически важно! Без этого в БД могут появиться дубликаты:
     * - Запись (5, 10)
     * - Запись (10, 5)
     * Что нарушит логику и займет лишнее место.
     */
    qint64 userId1 = std::min(fromId, toId);
    qint64 userId2 = std::max(fromId, toId);

    // ═══════════════════════════════════════════════════════════════════════
    // 5. Проверка на существующие связи
    // ═══════════════════════════════════════════════════════════════════════
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT status FROM contacts WHERE user_id_1 = :id1 AND user_id_2 = :id2");
    checkQuery.bindValue(":id1", userId1);
    checkQuery.bindValue(":id2", userId2);
    
    if (!checkQuery.exec()) {
        qWarning() << "[SERVER] Failed to check for existing contact:" << checkQuery.lastError().text();
        sendJson(socket, {
            {"type", "add_contact_failure"},
            {"reason", "Ошибка базы данных."}
        });
        return;
    }

    // Если запрос вернул хотя бы одну строку — связь уже существует
    if (checkQuery.next()) {
        int status = checkQuery.value("status").toInt();
        QString reason;
        
        switch (status) {
            case 0:
                reason = "Запрос этому пользователю уже отправлен и ожидает ответа.";
                break;
            case 1:
                reason = "Этот пользователь уже в вашем списке контактов.";
                break;
            case 2:
                reason = "Связь с этим пользователем заблокирована.";
                break;
            default:
                reason = "С этим пользователем уже существует связь.";
        }
        
        sendJson(socket, {
            {"type", "add_contact_failure"},
            {"reason", reason}
        });
        qDebug() << "[SERVER] Contact request rejected: existing status =" << status;
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 6. Создание нового запроса в контакты
    // ═══════════════════════════════════════════════════════════════════════
    QSqlQuery insertQuery;
    insertQuery.prepare("INSERT INTO contacts (user_id_1, user_id_2, status, creation_date) "
                        "VALUES (:id1, :id2, 0, :date)"); // status = 0 (pending)
    insertQuery.bindValue(":id1", userId1);
    insertQuery.bindValue(":id2", userId2);
    insertQuery.bindValue(":date", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!insertQuery.exec()) {
        qWarning() << "[SERVER] Failed to insert contact request:" << insertQuery.lastError().text();
        sendJson(socket, {
            {"type", "add_contact_failure"},
            {"reason", "Ошибка базы данных при отправке запроса."}
        });
        return;
    }

    qInfo() << "[SERVER] Contact request created:" << fromUsername << "->" << toUsername;

    // ═══════════════════════════════════════════════════════════════════════
    // 7. Отправка push-уведомления получателю (если онлайн)
    // ═══════════════════════════════════════════════════════════════════════
    QObject* toSocket = m_clients.value(toUsername, nullptr);
    
    if (toSocket) {
        QString avatarUrl = "";

        // Корректный SQL-запрос: указать поля после SELECT, убрать лишнюю скобку
        QSqlQuery query;
        query.prepare("SELECT avatar_url FROM users WHERE username = :usernameFrom");
        query.bindValue(":usernameFrom", fromUsername);

        // Выполняем запрос
        if (!query.exec()) {
            qDebug() << "cannot select avatar_url";
        } else if (query.next()) { // Переходим к первой строке результата
            avatarUrl = query.value("avatar_url").toString();
        } else {
            qDebug() << "avatar_url not found for user" << fromUsername;
        }


        QJsonObject notification;
        notification["type"] = "incoming_contact_request";
        notification["fromUsername"] = fromUsername;
        notification["fromDisplayname"] = fromDisplayName;
        notification["fromAvatarUrl"] = avatarUrl; // передаём аватарку!
        
        sendJson(toSocket, notification);
        qDebug() << "[SERVER] Push notification sent to" << toUsername;
    } else {
        qDebug() << "[SERVER] User" << toUsername << "is offline. Notification will be shown on next login.";
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 8. Отправка подтверждения инициатору
    // ═══════════════════════════════════════════════════════════════════════
    sendJson(socket, {
        {"type", "add_contact_success"},
        {"reason", "Запрос на добавление успешно отправлен пользователю " + toUsername + "."}
    });
}


/**
 * @brief Обрабатывает аутентификацию пользователя (Login).
 *
 * @details Этот метод реализует безопасную процедуру входа с проверкой пароля через Argon2i:
 * 
 * **Алгоритм аутентификации:**
 * 1. Извлекает username и пароль из запроса.
 * 2. Ищет пользователя в базе данных.
 * 3. Извлекает сохранённый хеш пароля и соль.
 * 4. Повторно хеширует введённый пароль с той же солью и параметрами.
 * 5. Сравнивает полученный хеш с сохранённым (constant-time comparison желателен).
 * 6. При успехе:
 *    - Генерирует токен автологина (JWT-подобный).
 *    - Добавляет пользователя в список онлайн-клиентов.
 *    - Отправляет профиль пользователя.
 *    - Отправляет список контактов, входящие запросы, счётчики непрочитанных.
 *    - Рассылает всем остальным обновлённый список присутствия.
 * 7. При ошибке — отправляет причину отказа.
 * 
 * **Меры безопасности:**
 * - Пароли проверяются через медленное хеширование (Argon2i), что предотвращает брутфорс.
 * - Соль уникальна для каждого пользователя (защита от rainbow table атак).
 * - Не раскрывается, существует ли пользователь или пароль неверен (generic error для защиты от enumeration).
 * - Токен автологина позволяет избежать повторного ввода пароля при перезапуске клиента.
 * 
 * **Формат запроса:**
 * ```
 * {
 *   "type": "login",
 *   "username": "john_doe",
 *   "password": "mySecretPassword123"
 * }
 * ```
 * 
 * **Формат ответа (успех):**
 * ```
 * {
 *   "type": "login_success",
 *   "username": "john_doe",
 *   "displayname": "John Doe",
 *   "statusmessage": "Always online!",
 *   "avatarurl": "/avatars/john.jpg",
 *   "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
 * }
 * ```
 * 
 * **Формат ответа (ошибка):**
 * ```
 * {
 *   "type": "login_failure",
 *   "reason": "Incorrect password"
 * }
 * ```
 *
 * @param socket Сокет клиента, отправившего запрос на вход.
 * @param request JSON-объект с полями:
 *        - `username`: Логин пользователя.
 *        - `password`: Пароль в открытом виде (будет захеширован на сервере).
 */
void Server::handleLogin(QObject* socket, const QJsonObject& request)
{
    // ═══════════════════════════════════════════════════════════════════════
    // 1. Извлечение данных из запроса
    // ═══════════════════════════════════════════════════════════════════════
    QString username = request.value("username").toString().trimmed();
    QString password = request.value("password").toString();
    
    qDebug() << "[SERVER] Login attempt for username:" << username;
    
    QJsonObject response;
    
    // ═══════════════════════════════════════════════════════════════════════
    // 2. Валидация входных данных
    // ═══════════════════════════════════════════════════════════════════════
    if (username.isEmpty()) {
        response["type"] = "login_failure";
        response["reason"] = "Username cannot be empty";
        sendJson(socket, response);
        qWarning() << "[SERVER] Login failed: empty username";
        return;
    }
    
    if (password.isEmpty()) {
        response["type"] = "login_failure";
        response["reason"] = "Password cannot be empty";
        sendJson(socket, response);
        qWarning() << "[SERVER] Login failed: empty password";
        return;
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // 3. Получение данных пользователя из БД
    // ═══════════════════════════════════════════════════════════════════════
    QSqlQuery query;
    query.prepare(R"(
        SELECT password_hash, salt, display_name, avatar_url, status_message 
        FROM users 
        WHERE username = :username
    )");
    query.bindValue(":username", username);
    
    if (!query.exec()) {
        response["type"] = "login_failure";
        response["reason"] = "Database error";
        sendJson(socket, response);
        qCritical() << "[SERVER] DB error during login:" << query.lastError().text();
        return;
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // 4. Проверка существования пользователя
    // ═══════════════════════════════════════════════════════════════════════
    if (!query.next()) {
        response["type"] = "login_failure";
        response["reason"] = "Invalid credentials"; // Generic error (не раскрываем, что пользователя нет)
        sendJson(socket, response);
        qWarning() << "[SERVER] Login failed: user" << username << "not found";
        return;
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // 5. Извлечение сохранённых данных
    // ═══════════════════════════════════════════════════════════════════════
    QString storedHash = query.value("password_hash").toString();
    QString serverSalt = query.value("salt").toString();
    QString displayname = query.value("display_name").toString();
    QString statusmessage = query.value("status_message").toString();
    QString avatarurl = query.value("avatar_url").toString();

    // ═══════════════════════════════════════════════════════════════════════
    // 6. Преобразование соли из Hex обратно в байты
    // ═══════════════════════════════════════════════════════════════════════
    QByteArray saltBytes = QByteArray::fromHex(serverSalt.toLatin1());

    // ═══════════════════════════════════════════════════════════════════════
    // 7. Хеширование введённого пароля с той же солью
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Критически важно: используются те же параметры (соль, NB_BLOCKS, NB_ITERATIONS),
     * что и при регистрации. Иначе хеши не совпадут даже при правильном пароле.
     */
    QString computedHashString = CryptoUtils::hashPasswordArgon2(password, saltBytes);
    
    if (computedHashString.isEmpty()) {
        response["type"] = "login_failure";
        response["reason"] = "Server error";
        sendJson(socket, response);
        qCritical() << "[SERVER] Argon2 hashing failed during login for user" << username;
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 8. Сравнение хешей (проверка пароля)
    // ═══════════════════════════════════════════════════════════════════════
    qDebug() << "[SERVER] Computed hash:" << computedHashString;
    qDebug() << "[SERVER] Stored hash:  " << storedHash;
    
    if (computedHashString == storedHash) {
        // ====================================================================
        // УСПЕХ: Пароль верный
        // ====================================================================
        qInfo() << "[SERVER] ✅ User" << username << "authenticated successfully";
        
        // --- 8.1. Генерация токена автологина ---
        QString token = generateToken(username);
        m_userTokens[username] = token; // Сохраняем в памяти для быстрой проверки
        
        // Сохранение токена в БД (для постоянства между перезапусками сервера)
        QSqlQuery tokenQuery;
        tokenQuery.prepare(R"(
            INSERT OR REPLACE INTO tokens (username, token, expires_at)
            VALUES (:username, :token, datetime('now', '+30 days'))
        )");
        tokenQuery.bindValue(":username", username);
        tokenQuery.bindValue(":token", token);
        
        if (!tokenQuery.exec()) {
            qWarning() << "[SERVER] Failed to save token to DB:" << tokenQuery.lastError().text();
        }
        
        // --- 8.2. Формирование ответа ---
        response["type"] = "login_success";
        response["username"] = username;
        response["displayname"] = displayname;
        response["statusmessage"] = statusmessage;
        response["avatar_url"] = avatarurl;
        response["token"] = token; // Клиент сохранит для автологина
        
        // --- 8.3. Добавление в список онлайн-пользователей ---
        m_clients[username] = socket;         // username -> socket
        m_clientsReverse[socket] = username;  // socket -> username
        
        // --- 8.4. Отправка основного ответа ---
        sendJson(socket, response);
        
        // --- 8.5. Отправка дополнительных данных ---
        sendContactList(socket, username);              // Список друзей
        sendPendingContactRequests(socket, username);   // Входящие запросы на добавление
        sendUnreadCounts(socket, username);             // Счётчики непрочитанных сообщений
        
        // --- 8.6. Оповещение всех онлайн-клиентов ---
        broadcastUserList(); // Всем отправляем обновлённый список "кто онлайн"
        
    } else {
        // ====================================================================
        // ОШИБКА: Пароль неверный
        // ====================================================================
        qWarning() << "[SERVER] ❌ Login failed for" << username << ": incorrect password";
        
        response["type"] = "login_failure";
        response["reason"] = "Invalid credentials"; // Generic error (не указываем, что именно пароль неверен)
        
        sendJson(socket, response);
    }
}


/**
 * @brief Обрабатывает отправку личного сообщения между пользователями.
 *
 * @details Это ключевой метод для обмена сообщениями в мессенджере. Он реализует полный цикл:
 * от получения сообщения от клиента до сохранения в БД и доставки получателю.
 * 
 * **Алгоритм работы:**
 * 1. **Валидация отправителя**: Проверяет, что пользователь не подделывает поле `fromUser`.
 * 2. **Сохранение в БД**: Записывает сообщение в таблицу `messages` с серверной временной меткой.
 * 3. **Получение ID**: Извлекает автоинкрементный ID, присвоенный сообщению БД.
 * 4. **Echo-ответ**: Отправляет подтверждение отправителю с глобальным ID сообщения.
 * 5. **Пересылка получателю**: Если получатель онлайн — немедленно пересылает сообщение.
 * 6. **Офлайн-хранение**: Если получатель офлайн — сообщение остаётся в БД для последующей загрузки.
 * 
 * **Особенности реализации:**
 * - **temp_id**: Временный UUID, генерируемый клиентом для отслеживания сообщения до получения серверного ID.
 *   Используется только в echo-ответе, не передаётся получателю.
 * - **Серверная временная метка**: Гарантирует, что все сообщения имеют согласованное время,
 *   независимо от рассинхронизации часов клиентов.
 * - **reply_to_id**: Поддержка цитирования (ответ на конкретное сообщение).
 * - **Безопасность**: Проверяется, что отправитель действительно аутентифицирован под этим username.
 * 
 * **Формат запроса:**
 * ```
 * {
 *   "type": "private_message",
 *   "fromUser": "alice",
 *   "toUser": "bob",
 *   "payload": "Hello, Bob!",
 *   "reply_to_id": 0,
 *   "temp_id": "550e8400-e29b-41d4-a716-446655440000"
 * }
 * ```
 * 
 * **Формат echo-ответа (отправителю):**
 * ```
 * {
 *   "type": "private_message",
 *   "id": 12345,
 *   "fromUser": "alice",
 *   "toUser": "bob",
 *   "payload": "Hello, Bob!",
 *   "timestamp": "2025-11-24T14:30:00",
 *   "is_delivered": 0,
 *   "is_read": 0,
 *   "is_edited": 0,
 *   "reply_to_id": 0,
 *   "temp_id": "550e8400-e29b-41d4-a716-446655440000"
 * }
 * ```
 * 
 * **Формат сообщения получателю:**
 * ```
 * {
 *   "type": "private_message",
 *   "id": 12345,
 *   "fromUser": "alice",
 *   "toUser": "bob",
 *   "payload": "Hello, Bob!",
 *   "timestamp": "2025-11-24T14:30:00",
 *   "is_delivered": 0,
 *   "is_read": 0,
 *   "is_edited": 0
 * }
 * ```
 *
 * @param socket Сокет клиента-отправителя.
 * @param request JSON-объект с полями:
 *        - `fromUser`: Username отправителя (должен совпадать с аутентифицированным).
 *        - `toUser`: Username получателя.
 *        - `payload`: Текст сообщения.
 *        - `reply_to_id`: ID цитируемого сообщения (0, если не ответ).
 *        - `temp_id`: Временный UUID клиента для отслеживания.
 */
void Server::handlePrivateMessage(QObject* socket, const QJsonObject& request)
{
    // ═══════════════════════════════════════════════════════════════════════
    // 1. Извлечение данных из запроса
    // ═══════════════════════════════════════════════════════════════════════
    QString fromUser = request["fromUser"].toString();
    QString toUser = request["toUser"].toString();
    QString payload = request["payload"].toString();
    qint64 replyToId = request["reply_to_id"].toVariant().toLongLong();
    QString tempId = request["temp_id"].toString();
    QString fileId = request.contains("file_id") ? request["file_id"].toVariant().toString() : "";
    QString fileName = request.contains("file_name") ? request.value("file_name").toString() : "";
    QString fileUrl = request.contains("file_url") ? request.value("file_url").toString() : "";


    
    // Генерируем серверную временную метку (единый источник правды для времени)
    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    qDebug() << "[SERVER] Private message:" << fromUser << "->" << toUser 
             << "(temp_id:" << tempId << ")";

    // ═══════════════════════════════════════════════════════════════════════
    // 2. Проверка безопасности: Валидация отправителя
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Критически важно! Проверяем, что пользователь не пытается отправить сообщение
     * от имени другого пользователя. Сравниваем fromUser с username, привязанным
     * к сокету при аутентификации.
     */
    QString authenticatedUser = m_clientsReverse.value(socket);
    
    if (fromUser != authenticatedUser) {
        qCritical() << "[SERVER] 🚨 SECURITY WARNING: User" << authenticatedUser
                    << "attempted to impersonate" << fromUser;
        
        // Можно разорвать соединение или забанить пользователя
        QJsonObject errorResponse;
        errorResponse["type"] = "error";
        errorResponse["reason"] = "Authentication mismatch";
        sendJson(socket, errorResponse);
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 3. Сохранение сообщения в базу данных
    // ═══════════════════════════════════════════════════════════════════════
    QSqlQuery query;
    query.prepare("INSERT INTO messages (fromUser, toUser, payload, timestamp, file_id, file_name, file_url,  reply_to_id) "
                  "VALUES (:fromUser, :toUser, :payload, :timestamp,:fileId, :fileName, :fileUrl, :reply_to_id)");
    query.bindValue(":fromUser", fromUser);
    query.bindValue(":toUser", toUser);
    query.bindValue(":payload", payload);
    query.bindValue(":timestamp", timestamp);
    query.bindValue(":fileId", fileId);
    query.bindValue(":fileName", fileName);
    query.bindValue(":fileUrl", fileUrl);
    
    // Обрабатываем reply_to_id: если 0, сохраняем NULL в БД
    query.bindValue(":reply_to_id", (replyToId > 0) ? QVariant(replyToId) : QVariant());

    if (!query.exec()) {
        qCritical() << "[SERVER] Failed to save message to DB:" << query.lastError().text();
        
        // Отправляем отправителю уведомление об ошибке
        QJsonObject errorResponse;
        errorResponse["type"] = "message_send_failed";
        errorResponse["temp_id"] = tempId;
        errorResponse["reason"] = "Database error";
        sendJson(socket, errorResponse);
        return;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 4. Получение глобального ID сообщения
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * База данных автоматически присвоила сообщению уникальный ID.
     * Этот ID используется для:
     * - Идентификации сообщения в истории чата.
     * - Отслеживания статусов доставки/прочтения.
     * - Цитирования (reply_to_id).
     */
    quint64 messageId = query.lastInsertId().toULongLong();
    
    qInfo() << "[SERVER] Message saved with ID:" << messageId;

    // ═══════════════════════════════════════════════════════════════════════
    // 5. Формирование и отправка Echo-ответа отправителю
    // ═══════════════════════════════════════════════════════════════════════
    /**
     * Echo-ответ позволяет отправителю:
     * 1. Узнать глобальный ID своего сообщения.
     * 2. Сопоставить его с temp_id через поле "temp_id".
     * 3. Обновить UI (заменить временную запись на постоянную).
     */
    QJsonObject echoMessage;
    echoMessage["type"] = "private_message";
    echoMessage["id"] = static_cast<double>(messageId); // JSON number
    echoMessage["fromUser"] = fromUser;
    echoMessage["toUser"] = toUser;
    echoMessage["payload"] = payload;
    echoMessage["timestamp"] = timestamp;
    echoMessage["is_delivered"] = 0; // Ещё не доставлено
    echoMessage["is_read"] = 0;
    echoMessage["is_edited"] = 0;
    echoMessage["file_id"] = fileId;
    echoMessage["file_name"] = fileName;
    echoMessage["file_url"] = fileUrl;
    
    if (replyToId > 0) {
        echoMessage["reply_to_id"] = static_cast<double>(replyToId);
    }
    
    echoMessage["temp_id"] = tempId; // Критически важно для сопоставления на клиенте

    sendJson(socket, echoMessage);

    // ═══════════════════════════════════════════════════════════════════════
    // 6. Пересылка сообщения получателю (если онлайн)
    // ═══════════════════════════════════════════════════════════════════════
    
    // Удаляем temp_id, так как он не нужен получателю (это внутренний ID отправителя)
    echoMessage.remove("temp_id");

    // Ищем сокет получателя в списке онлайн-клиентов
    QObject* toUserSocket = m_clients.value(toUser, nullptr);

    if (toUserSocket) {
        // ====================================================================
        // СЛУЧАЙ 1: Получатель онлайн — немедленная доставка
        // ====================================================================
        sendJson(toUserSocket, echoMessage);
        qDebug() << "[SERVER] Message" << messageId << "delivered to online user" << toUser;
        
    } else {
        // ====================================================================
        // СЛУЧАЙ 2: Получатель офлайн — сообщение ждёт в БД
        // ====================================================================
        qDebug() << "[SERVER] User" << toUser << "is offline. Message" << messageId 
                 << "stored for later delivery.";
        
        // Сообщение будет загружено через handleGetHistory при следующем входе получателя
    }
}


/**
 * @brief Универсальный метод отправки JSON-объекта клиенту.
 *
 * @details Этот метод автоматически определяет тип сокета (QTcpSocket или QWebSocket)
 * и использует соответствующий протокол для сериализации и отправки данных.
 * 
 * **Для TCP:**
 * - Если канал ещё не зашифрован — отправляет "plain" JSON с префиксом длины.
 * - Если канал зашифрован — шифрует сообщение с помощью XChaCha20-Poly1305,
 *   формирует пакет nonce+mac+ciphertext с префиксом длины.
 * 
 * **Для WebSocket:**
 * - Отправляет JSON как текстовое сообщение (WebSocket сам обеспечивает фрейминг и, обычно, TLS).
 * 
 * @param socket Указатель на сокет получателя (`QObject*`, может быть `QTcpSocket*` или `QWebSocket*`).
 * @param response JSON-объект, который надо отправить.
 */
void Server::sendJson(QObject* socket, const QJsonObject& response)
{
    qDebug() << "---------------------------------";
    qDebug() << "[SERVER] Preparing to send JSON of type:" << response["type"].toString();
    //qDebug() << "[SERVER] Full JSON content:" << response;
    qDebug() << "---------------------------------";

    QByteArray jsonData = QJsonDocument(response).toJson(QJsonDocument::Compact);

    if (auto tcpSocket = qobject_cast<QTcpSocket*>(socket)) {
        if (tcpSocket->state() != QAbstractSocket::ConnectedState) {
            qWarning() << "Socket not connected, state:" << tcpSocket->state();
            return;
        }

        CryptoManager* crypto = m_clientCrypto[tcpSocket];

        if (!crypto->isEncrypted()) {
            // --- Отправляем открытый JSON с префиксом длины ---
            QByteArray block;
            QDataStream out(&block, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_2);
            out << (quint32)0;              // Резервируем место под длину
            out << jsonData;                // Пишем данные
            out.device()->seek(0);          // Возвращаемся в начало
            out << (quint32)(block.size() - sizeof(quint32)); // Записываем реальный размер

            qint64 written = tcpSocket->write(block);
            if (written == -1) {
                qWarning() << "Write failed:" << tcpSocket->errorString();
                return;
            }

            qDebug() << "[SERVER] JSON UNSAFE (cleartext) send:" << jsonData.size() << "bytes";
            return;
        }

        // --- ШИФРОВАНИЕ (XChaCha20-Poly1305) ---
        uint8_t nonce[24];
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(nonce), 24 / 4);

        QByteArray encryptedData;
        encryptedData.resize(16 + jsonData.size()); // MAC (16) + ciphertext

        // Проводим аутентифицированное шифрование
        crypto_aead_lock(
            reinterpret_cast<uint8_t*>(encryptedData.data()) + 16, // ciphertext (выход)
            reinterpret_cast<uint8_t*>(encryptedData.data()),      // MAC (выход)
            crypto->getSessionKey(),                                // ключ
            nonce,                                                  // nonce
            nullptr, 0,                                            // associated data (нет)
            reinterpret_cast<const uint8_t*>(jsonData.constData()),// plaintext
            jsonData.size()                                         // длина
        );

        // --- ФОРМИРУЕМ ПАКЕТ TCP с префиксом длины ---
        QByteArray packet;
        QDataStream out(&packet, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_2);

        // [Length (4 байта)][Nonce (24 байта)][MAC+Ciphertext]
        out << (quint32)0; // резерв под длину
        out << QByteArray(reinterpret_cast<const char*>(nonce), 24);
        out << encryptedData;
        out.device()->seek(0);

        quint32 totalSize = packet.size() - sizeof(quint32);
        out << totalSize;

        //tcpSocket->write(packet);


        qint64 written = tcpSocket->write(packet);
        if (written == -1) {
            qWarning() << "Write failed:" << tcpSocket->errorString();
            return;
        }
        qDebug() << "[SERVER] JSON send (encrypted):" << totalSize << "bytes";

    } else if (auto wsSocket = qobject_cast<QWebSocket*>(socket)) {
        // --- WebSocket: просто отправляем как текстовое сообщение ---
        wsSocket->sendTextMessage(QString::fromUtf8(jsonData));
    }
}



/**
 * @brief (Устарело / Для админ-панели) Отправляет клиенту полный список всех зарегистрированных пользователей.
 *
 * @details Извлекает из базы всех пользователей без исключения и отправляет
 * JSON-массив с парами `username` и `displayname`.
 * 
 * Обычно не используется для обычных клиентов, которым достаточно списка контактов.
 * Может быть полезен для административных инструментов или функций поиска всех пользователей.
 *
 * @param socket Сокет клиента, которому нужно отправить полный список.
 */
void Server::sendFullUserList(QObject* socket)
{
    QSqlQuery query;
    query.prepare("SELECT username, display_name FROM users");
    
    if (!query.exec()) {
        qWarning() << "[SERVER] Failed to get full user list:" << query.lastError().text();
        return;
    }

    QJsonArray allUsers;
    while (query.next()) {
        QJsonObject userObject;
        userObject["username"] = query.value(0).toString();
        userObject["displayname"] = query.value(1).toString();
        allUsers.append(userObject);
    }

    QJsonObject message;
    message["type"] = "full_user_list";
    message["users"] = allUsers;

    sendJson(socket, message);
    
    qDebug() << "[SERVER] Sent full user list to client.";
}


/**
 * @brief Рассылает всем онлайн-клиентам актуальный список пользователей, находящихся в сети.
 *
 * @details Метод формирует JSON-сообщение с типом "user_list", содержащим массив строк — имён
 * всех аутентифицированных и подключенных в данный момент пользователей. Затем отправляет
 * это сообщение каждому клиенту из карты `m_clients`.
 * 
 * Обычно вызывается:
 * - После успешного входа нового пользователя.
 * - После выхода пользователя из сети.
 * - В других случаях обновления статуса онлайн-пользователей.
 * 
 * Клиенты используют полученный список для обновления UI и отображения актуального онлайн-статуса контактов.
 */
void Server::broadcastUserList()
{
    // Получаем список всех имен пользователей, которые сейчас онлайн.
    QStringList onlineUsers = m_clients.keys();
    qDebug() << "[SERVER] Broadcasting ONLINE user list:" << onlineUsers;

    // Формируем JSON-сообщение с массивом онлайн-имён.
    QJsonObject message;
    message["type"] = "user_list";
    message["users"] = QJsonArray::fromStringList(onlineUsers);

    // Отправляем сообщение каждому онлайн-клиенту.
    for (QObject* socket : m_clients.values()) {
        sendJson(socket, message);
    }
}


/**
 * @brief Обрабатывает запрос на редактирование ранее отправленного сообщения.
 * 
 * @details Метод выполняет полный цикл обработки:
 * 1. Определяет пользователя, сделавшего запрос, по связанному с сокетом имени.
 * 2. Получает ID редактируемого сообщения и новый текст.
 * 3. Проверяет, что пользователь аутентифицирован и является автором сообщения.
 * 4. Обновляет сообщение в базе, устанавливая флаг `is_edited`.
 * 5. Формирует JSON-уведомление о редактировании.
 * 6. Отправляет уведомление автору и получателю (если они онлайн).
 * 
 * @param clientSocket Сокет клиента, инициировавшего редактирование.
 * @param request JSON-запрос с полями:
 *        - `id`: ID сообщения.
 *        - `payload`: Новый текст сообщения.
 */
void Server::handleEditMessage(QObject* clientSocket, const QJsonObject& request)
{
    QString requestingUser = m_clientsReverse.value(clientSocket);
    quint64 messageId = request["id"].toDouble();

    qDebug() << "[SERVER] Edit message request from" << requestingUser << "for message ID:" << messageId;
    if (messageId == 0) {
        qWarning() << "[SERVER] Invalid message ID in edit request.";
        return;
    }

    // Проверка аутентификации
    if (requestingUser.isEmpty()) {
        qWarning() << "[SECURITY] Edit request from unauthenticated socket.";
        return;
    }

    // Получаем из базы авторство сообщения и получателя
    QSqlQuery query;
    query.prepare("SELECT fromUser, toUser FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);

    if (query.exec() && query.next()) {
        QString fromUser = query.value("fromUser").toString();
        QString toUser = query.value("toUser").toString();

        qDebug() << "[SERVER] Message author:" << fromUser << ", recipient:" << toUser;

        // Проверяем, что редактирует именно автор сообщения
        if (fromUser == requestingUser) {
            QString newPayload = request["payload"].toString();

            QSqlQuery updateQuery;
            updateQuery.prepare("UPDATE messages SET payload = :payload, is_edited = 1 WHERE id = :id");
            updateQuery.bindValue(":payload", newPayload);
            updateQuery.bindValue(":id", messageId);

            if (updateQuery.exec()) {
                qInfo() << "[SERVER] User" << requestingUser << "edited message" << messageId;

                // Формируем уведомление о редактировании
                QJsonObject editCmd;
                editCmd["type"] = "edit_message";
                editCmd["id"] = static_cast<double>(messageId);
                editCmd["payload"] = newPayload;

                // Отправляем автору сообщения
                QObject* fromSocket = m_clients.value(fromUser, nullptr);
                if (fromSocket) {
                    editCmd["with_user"] = toUser;
                    sendJson(fromSocket, editCmd);
                }

                // Отправляем получателю сообщения
                QObject* toSocket = m_clients.value(toUser, nullptr);
                if (toSocket) {
                    editCmd["with_user"] = fromUser;
                    sendJson(toSocket, editCmd);
                }
            } else {
                qWarning() << "[SERVER] Failed to update message payload in DB:" << updateQuery.lastError().text();
            }
        } else {
            qWarning() << "[SECURITY] User" << requestingUser << "attempted to edit message not authored by them (author:" << fromUser << ")";
        }
    } else {
        qWarning() << "[SERVER] Failed to find message with ID" << messageId << "for editing.";
    }
}


/**
 * @brief Обрабатывает запрос на удаление ранее отправленного сообщения.
 *
 * @details Метод выполняет несколько шагов проверки и действий:
 * 1. Определяет пользователя, который запросил удаление, по сокету.
 * 2. Проверяет, что пользователь аутентифицирован.
 * 3. Извлекает из БД автора и получателя указанного сообщения.
 * 4. Проверяет, что запрос отправлен именно автором сообщения.
 * 5. Если проверка прошла успешно — удаляет запись из таблицы `messages`.
 * 6. Формирует и рассылает всем участникам чата уведомление типа `delete_message`.
 * 
 * @param clientSocket Сокет клиента, который инициировал удаление.
 * @param request JSON-запрос, содержащий поле `id` с идентификатором сообщения для удаления.
 */
void Server::handleDeleteMessage(QObject* clientSocket, const QJsonObject& request)
{
    // 1. Идентификация пользователя по сокету
    QString requestingUser = m_clientsReverse.value(clientSocket);
    quint64 messageId = request["id"].toDouble();

    qDebug() << "[SERVER] Delete message request from" << requestingUser << "for message ID:" << messageId;
    if (messageId == 0) {
        qWarning() << "[SERVER] Invalid message ID in delete request.";
        return;
    }

    // 2. Проверка аутентификации
    if (requestingUser.isEmpty()) {
        qWarning() << "[SECURITY] Delete request from unauthenticated socket.";
        return;
    }

    // 3. Получение автора и получателя сообщения из БД
    QSqlQuery query;
    query.prepare("SELECT fromUser, toUser FROM messages WHERE id = :id");
    query.bindValue(":id", messageId);

    if (query.exec() && query.next()) {
        QString fromUser = query.value("fromUser").toString();
        QString toUser = query.value("toUser").toString();

        // 4. Проверка прав: только автор может удалить свое сообщение
        if (requestingUser == fromUser) {
            // 5. Удаление сообщения из БД
            QSqlQuery deleteQuery;
            deleteQuery.prepare("DELETE FROM messages WHERE id = :id");
            deleteQuery.bindValue(":id", messageId);

            if (deleteQuery.exec()) {
                qInfo() << "[SERVER] User" << requestingUser << "deleted message" << messageId;

                // 6. Формирование уведомления об удалении
                QJsonObject deleteCmd;
                deleteCmd["type"] = "delete_message";
                deleteCmd["id"] = static_cast<double>(messageId);

                // 7. Отправка уведомлений автору и получателю, если они онлайн
                QObject* fromSocket = m_clients.value(fromUser, nullptr);
                if (fromSocket) {
                    deleteCmd["with_user"] = toUser;
                    sendJson(fromSocket, deleteCmd);
                }

                QObject* toSocket = m_clients.value(toUser, nullptr);
                if (toSocket) {
                    deleteCmd["with_user"] = fromUser;
                    sendJson(toSocket, deleteCmd);
                }
            } else {
                qWarning() << "[SERVER] Failed to delete message from DB:" << deleteQuery.lastError().text();
            }
        } else {
            qWarning() << "[SECURITY] User" << requestingUser << "tried to delete a message they do not own (author:" << fromUser << ")";
        }
    } else {
        qWarning() << "[SERVER] Failed to find message with ID" << messageId << "for deletion.";
    }
}


/**
 * @brief Обрабатывает ответ пользователя на входящий запрос на добавление в контакты.
 *
 * @details Метод реализует логику:
 * - При принятии (`"accepted"`) обновляет статус связи на `1` (принята).
 * - При отклонении (`"declined"`) удаляет запись запроса.
 * После принятия обеим сторонам отправляется обновлённый список контактов и онлайн-статусов.
 *
 * @param clientSocket Сокет пользователя, который ответил на запрос.
 * @param request JSON-объект с полями:
 *        - `fromUsername`: Имя пользователя, отправившего запрос.
 *        - `response`: Строка `"accepted"` или `"declined"`.
 */
void Server::handleContactRequestResponse(QObject* clientSocket, const QJsonObject& request)
{
    qDebug() << "[SERVER] Received contact_request_response:" << request;

    // 1. Определяем участников: пользователь, ответивший (toUsername), и отправитель запроса (fromUsername).
    QString toUsername = m_clientsReverse.value(clientSocket);
    QString fromUsername = request["fromUsername"].toString();
    QString response = request["response"].toString();

    qDebug() << "[SERVER] Parsed response value:" << response;

    // 2. Получаем ID обоих пользователей из базы данных.
    QSqlQuery idQuery;
    idQuery.prepare("SELECT id, username FROM users WHERE username = :from OR username = :to");
    idQuery.bindValue(":from", fromUsername);
    idQuery.bindValue(":to", toUsername);

    if (!idQuery.exec()) {
        qWarning() << "[SERVER] DB Error: Failed to get user IDs for contact response";
        return;
    }

    qint64 fromId = -1, toId = -1;
    while (idQuery.next()) {
        if (idQuery.value("username").toString() == fromUsername) {
            fromId = idQuery.value("id").toLongLong();
        } else if (idQuery.value("username").toString() == toUsername) {
            toId = idQuery.value("id").toLongLong();
        }
    }

    if (fromId == -1 || toId == -1) {
        qWarning() << "[SERVER] DB Error: fromId or toId not found";
        return; // Один из пользователей не найден – прекращаем обработку
    }

    // Каноническое упорядочивание ID
    qint64 userId1 = std::min(fromId, toId);
    qint64 userId2 = std::max(fromId, toId);

    // 3. Обрабатываем ответ пользователя
    if (response == "accepted") {
        // Обновляем статус на 'accepted' (1) в таблице contacts
        QSqlQuery updateQuery;
        updateQuery.prepare("UPDATE contacts SET status = 1 WHERE user_id_1 = :id1 AND user_id_2 = :id2 AND status = 0");
        updateQuery.bindValue(":id1", userId1);
        updateQuery.bindValue(":id2", userId2);

        if (updateQuery.exec() && updateQuery.numRowsAffected() > 0) {
            qDebug() << "[SERVER]" << toUsername << "accepted contact request from" << fromUsername;

            // Получаем сокеты обоих пользователей
            QObject* fromSocket = m_clients.value(fromUsername, nullptr);
            QObject* toSocket = m_clients.value(toUsername, nullptr);

            // Отправляем обновлённые списки контактов
            if (fromSocket) sendContactList(fromSocket, fromUsername);
            if (toSocket) sendContactList(toSocket, toUsername);

            // Отправляем обновление онлайн-статусов
            if (fromSocket) sendOnlineStatusList(fromSocket);
            if (toSocket) sendOnlineStatusList(toSocket);
        }
    } else if (response == "declined") {
        // Удаляем запрос из таблицы contacts (status = 0)
        QSqlQuery deleteQuery;
        deleteQuery.prepare("DELETE FROM contacts WHERE user_id_1 = :id1 AND user_id_2 = :id2 AND status = 0");
        deleteQuery.bindValue(":id1", userId1);
        deleteQuery.bindValue(":id2", userId2);

        if (deleteQuery.exec()) {
            qDebug() << "[SERVER]" << toUsername << "declined contact request from" << fromUsername;
        }
    } else {
        qWarning() << "[SERVER] Unknown response value in contact request response:" << response;
    }
}


/**
 * @brief Отправляет клиенту счетчики непрочитанных сообщений, сгруппированные по отправителям.
 *
 * @details Метод выполняет запрос к базе данных для подсчёта всех сообщений,
 * адресованных указанному пользователю, которые ещё не были прочитаны (`is_read = 0`).
 * Результаты группируются по полю `fromUser` (отправитель), и для каждого подсчитывается количество.
 * Затем формируется JSON-объект с результатами, который отправляется клиенту.
 * 
 * Это позволяет клиентскому приложению отображать индикаторы количества новых сообщений.
 * 
 * @param socket Сокет клиента, которому отправлять данные.
 * @param username Имя пользователя, для которого нужно получить счётчики непрочитанных сообщений.
 */
void Server::sendUnreadCounts(QObject* socket, const QString& username)
{
    qDebug() << "[SERVER][UNREAD] Gathering unread message counts for user:" << username;

    // 1. Получаем ID пользователя (хотя в основном запросе он не используется, это хорошая практика)
    QSqlQuery idQuery;
    idQuery.prepare("SELECT id FROM users WHERE username = :username");
    idQuery.bindValue(":username", username);
    if (!idQuery.exec() || !idQuery.next()) {
        qDebug() << "[SERVER][UNREAD][ERROR] Failed to find ID for user:" << username;
        return;
    }
    qint64 userId = idQuery.value(0).toLongLong();
    Q_UNUSED(userId); // В данном запросе ID не используется

    // 2. Основной запрос: подсчитываем непрочитанные сообщения, группируя по отправителю
    QSqlQuery query;
    query.prepare(
        "SELECT fromUser, COUNT(*) as unread_count "
        "FROM messages "
        "WHERE toUser = :username AND is_read = 0 "
        "GROUP BY fromUser"
    );
    query.bindValue(":username", username);

    if (!query.exec()) {
        qDebug() << "[SERVER][UNREAD][ERROR] Database query failed:" << query.lastError().text();
        return;
    }

    // 3. Формируем JSON-массив с результатами
    QJsonArray countsArray;
    while (query.next()) {
        QJsonObject countObject;
        countObject["username"] = query.value("fromUser").toString();
        countObject["count"] = query.value("unread_count").toInt();
        countsArray.append(countObject);
    }

    if (countsArray.isEmpty()) {
        qDebug() << "[SERVER][UNREAD] No unread messages found for user:" << username;
        // Можно отправить пустой массив клиенту, если нужно
    }

    // 4. Упаковываем и отправляем JSON-ответ клиенту
    QJsonObject response;
    response["type"] = "unread_counts";
    response["counts"] = countsArray;

    qDebug() << "[SERVER][UNREAD] Sending unread counts to" << username << ":" << response;
    sendJson(socket, response);
}


/**
 * @brief Отправляет список онлайн-пользователей одному конкретному клиенту.
 *
 * @details Формирует JSON-сообщение с типом "user_list", содержащее список имен
 * всех пользователей, которые в данный момент подключены к серверу (находятся в `m_clients`).
 * 
 * В отличие от `broadcastUserList()`, которая делает рассылку всем, этот метод
 * отправляет данные **только одному** указанному сокету.
 * 
 * **Сценарии использования:**
 * - При первичной загрузке клиента после входа.
 * - После принятия запроса в друзья (чтобы обновить статус нового друга).
 * - При принудительном обновлении списка контактов пользователем.
 *
 * @param clientSocket Сокет целевого клиента, которому нужно отправить список.
 */
void Server::sendOnlineStatusList(QObject* clientSocket)
{
    // 1. Получаем список всех онлайн-пользователей из ключей карты m_clients.
    QStringList onlineUsers = m_clients.keys();

    // 2. Создаем JSON-сообщение.
    QJsonObject message;
    message["type"] = "user_list";
    message["users"] = QJsonArray::fromStringList(onlineUsers);

    // 3. Отправляем сообщение указанному клиенту.
    sendJson(clientSocket, message);
    
    qDebug() << "[SERVER] Sent individual online status list to client.";
}


/**
 * @brief Находит и отправляет пользователю список ожидающих подтверждения запросов в контакты.
 *
 * @details Метод вызывается после успешного входа пользователя и извлекает из базы все
 * записи из таблицы `contacts`, где пользователь участвует со статусом `0` (pending).
 * С помощью JOIN и CASE определяется "другой" пользователь в каждой паре, и его данные отправляются
 * клиенту в виде JSON-массива с типом сообщения `pending_requests_list`.
 * 
 * Это необходимо для отображения пользователю входящих запросов, которые он может принять или отклонить.
 *
 * @param socket Сокет клиента, которому отправляется список.
 * @param username Имя пользователя, для которого выполняется поиск входящих запросов.
 */
void Server::sendPendingContactRequests(QObject* socket, const QString& username)
{
    qDebug() << "[SERVER][PENDING] Checking for pending requests for user:" << username;

    // 1. Получаем ID пользователя
    QSqlQuery userQuery;
    userQuery.prepare("SELECT id FROM users WHERE username = :username");
    userQuery.bindValue(":username", username);
    if (!userQuery.exec() || !userQuery.next()) {
        qDebug() << "[SERVER][PENDING][ERROR] Could not find ID for user:" << username;
        return;
    }
    qint64 userId = userQuery.value(0).toLongLong();
    qDebug() << "[SERVER][PENDING] User ID is:" << userId;

    // 2. Выполняем основной запрос для поиска входящих запросов на добавление в друзья
    QSqlQuery query;
    query.prepare(
        "SELECT u.username, u.display_name, u.avatar_url FROM users u "
        "JOIN contacts c ON u.id = (CASE WHEN c.user_id_1 = :userId THEN c.user_id_2 ELSE c.user_id_1 END) "
        "WHERE (c.user_id_1 = :userId OR c.user_id_2 = :userId) AND c.status = 0"
    );
    query.bindValue(":userId", QVariant(userId));

    if (!query.exec()) {
        qDebug() << "[SERVER][PENDING][ERROR] DB Error: Failed to fetch pending requests:" << query.lastError().text();
        return;
    }

    qDebug() << "[SERVER][PENDING] Main SQL query executed successfully. Processing results...";

    // 3. Формируем JSON-массив с запросами
    QJsonArray pendingRequests;

    while (query.next()) {
        QString fromUser = query.value(0).toString();
        QString displayName = query.value(1).toString();
        QString avatarUrl = query.value(2).toString();
        qDebug() << "[SERVER][PENDING] Found a pending request from:" << fromUser << displayName << avatarUrl;

        QJsonObject reqObject;
        reqObject["fromUsername"] = fromUser;
        reqObject["fromDisplayname"] = displayName;
        reqObject["fromAvatarUrl"] = avatarUrl; // обязательно поле!
        pendingRequests.append(reqObject);
    }
    qDebug() << "[SERVER][PENDING] Total pending requests found:" << pendingRequests.count();

    // 4. Отправляем JSON с запросами, если есть хоть один
    if (!pendingRequests.isEmpty()) {
        qDebug() << "[SERVER] Sending" << pendingRequests.count() << "pending contact requests to" << username;
        QJsonObject response;
        response["type"] = "pending_requests_list";
        response["requests"] = pendingRequests;
        sendJson(socket, response);
    } else {
        qDebug() << "[SERVER][PENDING] No pending contact requests found for user:" << username;
    }

}


/**
 * @brief Обрабатывает запрос на VoIP звонок от одного пользователя другому.
 *
 * @details Метод выполняет следующие действия:
 * 1. Извлекает данные звонка из JSON-запроса.
 * 2. Создаёт запись о звонке в базе данных.
 * 3. Сохраняет информацию о звонке в активных звонках сервера.
 * 4. Пересылает сигнал "call_request" целевому пользователю, если он онлайн.
 * 5. Если пользователь офлайн — помечает звонок как пропущенный.
 *
 * @param socket Сокет пользователя, сделавшего запрос.
 * @param request JSON-объект с полями:
 *        - `from`: Инициатор звонка.
 *        - `to`: Получатель звонка.
 *        - `call_id`: Уникальный идентификатор звонка (UUID).
 *        - `caller_ip`: IP-адрес инициатора.
 *        - `caller_port`: UDP порт инициатора.
 */
void Server::handleCallRequest(QObject* socket, const QJsonObject& request)
{
    qDebug() << "[SERVER] Processing message of type: call_request";

    // Извлечение параметров звонка из запроса
    QString fromUser = request["from"].toString();
    QString toUser = request["to"].toString();
    QString callId = request["call_id"].toString();
    quint16 callerPort = static_cast<quint16>(request["caller_port"].toInt());

    // IP клиента инициатора звонка, может быть передан в запросе
    QString callerIp = request["caller_ip"].toString();
    
    // Поиск сокета получателя
    QObject* toUserSocket = m_clients.value(toUser, nullptr);

    // Добавляем запись о звонке в БД и внутреннюю структуру сервера
    createCallRecord(callId, fromUser, toUser, callerIp, callerPort);

    CallInfo callInfo;
    callInfo.callId = callId;
    callInfo.from = fromUser;
    callInfo.to = toUser;
    callInfo.fromSocket = socket;
    callInfo.toSocket = toUserSocket;
    callInfo.callerPort = callerPort;
    callInfo.callerIp = callerIp;

    // Сохраняем звонок в карте активных звонков
    m_activeCalls[callId] = callInfo;

    // Формируем JSON-сообщение для получателя вызова
    QJsonObject incomingCall;
    incomingCall["type"] = "call_request";
    incomingCall["from"] = fromUser;
    incomingCall["call_id"] = callId;
    incomingCall["caller_ip"] = callerIp;
    incomingCall["caller_port"] = static_cast<int>(callerPort);

    // Отправляем запрос получателю, если он онлайн
    if (toUserSocket) {
        sendJson(toUserSocket, incomingCall);
        qDebug() << "[CALL] Forwarded call request to" << toUser;
    } else {
        // Если получатель офлайн, помечаем вызов пропущенным
        updateCallEnded(callId, "missed");
        qDebug() << "[CALL]" << toUser << "is offline - call marked as missed";
    }
}


/**
 * @brief Обрабатывает ответ пользователя о принятии входящего звонка.
 *
 * @details Метод выполняет следующие действия:
 * 1. Определяет пользователя, который принял звонок, по сокету.
 * 2. Проверяет, существует ли активный звонок с таким call_id.
 * 3. Обновляет информацию о соединении: IP и порт вызываемого.
 * 4. Отправляет уведомление инициатору звонка с данными получателя.
 *
 * @param socket Сокет клиента, который принял звонок.
 * @param request JSON-объект с полями:
 *        - `call_id`: Уникальный идентификатор звонка.
 *        - `callee_ip`: IP-адрес вызываемого.
 *        - `callee_port`: UDP порт вызываемого.
 */
void Server::handleCallAccepted(QObject* socket, const QJsonObject& request)
{
    qDebug() << "[SERVER] Processing message of type: call_accepted";

    // Определяем пользователя, который ответил на звонок
    QString respondingUser = m_clientsReverse.value(socket);
    QString callId = request.value("call_id").toString();
    quint16 calleePort = static_cast<quint16>(request.value("callee_port").toInt());
    QString calleeIp = request.value("callee_ip").toString();

    // Проверка наличия звонка в активных
    if (!m_activeCalls.contains(callId)) {
        qWarning() << "[SERVER] Unknown call_id:" << callId;
        return;
    }

    // Обновляем информацию о соединении для звонка
    updateCallConnected(callId, calleeIp, calleePort);

    CallInfo& info = m_activeCalls[callId];

    // Отправляем уведомление инициатору звонка (caller)
    QObject* initiatorSocket = info.fromSocket;
    if (initiatorSocket) {
        QJsonObject response;
        response["type"] = "call_accepted";
        response["from"] = respondingUser;
        response["call_id"] = callId;
        response["callee_ip"] = calleeIp;
        response["callee_port"] = static_cast<int>(calleePort);

        sendJson(initiatorSocket, response);
        qDebug() << "[CALL] Notified caller about accepted call, call_id:" << callId;
    } else {
        qWarning() << "[CALL] Initiator socket not found for call_id:" << callId;
    }
}


/**
 * @brief Обрабатывает отклонение входящего звонка вызываемым пользователем.
 *
 * @details Метод выполняет следующие действия:
 * 1. Извлекает `call_id` и имя пользователя, который отклоняет вызов (`toUser`).
 * 2. Проверяет наличие активного звонка с указанным `call_id`.
 * 3. Обновляет запись звонка в базе, помечая его статус как "rejected".
 * 4. Проверяет, что запрос на отклонение пришёл от пользователя, который он должен отклонить.
 * 5. Удаляет звонок из внутренней структуры активных звонков.
 * 6. Уведомляет инициатора звонка о том, что звонок был отклонён.
 * 
 * @param socket Сокет пользователя, отклоняющего звонок.
 * @param request JSON-объект с полями:
 *        - `call_id`: Идентификатор звонка.
 *        - `to`: Username вызываемого, который отклоняет вызов.
 */
void Server::handleCallRejected(QObject* socket, const QJsonObject& request)
{
    QString callId = request["call_id"].toString();
    QString toUser = request["to"].toString();

    if (!m_activeCalls.contains(callId)) {
        qWarning() << "[SERVER] Call rejected: unknown call ID" << callId;
        return;
    }

    updateCallEnded(callId, "rejected");

    CallInfo callInfo = m_activeCalls[callId];
    QString fromUser = callInfo.from;
    QObject* fromUserSocket = callInfo.fromSocket;

    // Проверка безопасности: отклонять звонок может только вызываемый (toUser)
    if (toUser != m_clientsReverse.value(socket)) {
        qWarning() << "[SERVER] SECURITY: Unauthorized call rejection attempt from"
                   << m_clientsReverse.value(socket) << "for call ID" << callId;
        return;
    }

    qDebug() << "[SERVER] Call rejected by" << toUser << "from initiator" << fromUser
             << "| callId:" << callId;

    // Удаляем звонок из списка активных
    m_activeCalls.remove(callId);

    // Формируем уведомление для инициатора звонка
    QJsonObject rejectionNotification;
    rejectionNotification["type"] = "call_rejected";
    rejectionNotification["call_id"] = callId;
    rejectionNotification["from"] = toUser;

    if (fromUserSocket) {
        sendJson(fromUserSocket, rejectionNotification);
        qDebug() << "[SERVER] Call rejection notification sent to initiator:" << fromUser;
    } else {
        qWarning() << "[SERVER] Initiator socket not found for call ID" << callId;
    }
}


/**
 * @brief Обрабатывает завершение звонка от одного из участников.
 *
 * @details Метод выполняет следующие шаги:
 * 1. Извлекает `call_id` и имя вызывающего, завершившего звонок.
 * 2. Обновляет статус звонка в базе вызовов на "completed".
 * 3. Проверяет существование активного звонка с данным ID.
 * 4. Проверяет, что звонок действительно завершает один из участников.
 * 5. Удаляет звонок из списка активных.
 * 6. Отправляет уведомление второй стороне о завершении звонка.
 *
 * @param socket Сокет пользователя, завершившего звонок.
 * @param request JSON-объект с полями:
 *        - `call_id`: Идентификатор звонка.
 *        - `to`: Имя пользователя, которому следует отправить уведомление.
 */
void Server::handleCallEnd(QObject* socket, const QJsonObject& request)
{
    QString callId = request["call_id"].toString();
    QString currentUser = m_clientsReverse.value(socket);

    // Обновляем статус звонка в БД
    updateCallEnded(callId, "completed");

    if (!m_activeCalls.contains(callId)) {
        qDebug() << "[SERVER] Active calls:" << m_activeCalls.keys();
        qWarning() << "[SERVER] Call end: unknown call ID" << callId;
        return;
    }

    CallInfo callInfo = m_activeCalls[callId];
    QObject* otherSocket = nullptr;

    // Определяем другую сторону звонка
    if (currentUser == callInfo.from) {
        otherSocket = callInfo.toSocket;
    } else if (currentUser == callInfo.to) {
        otherSocket = callInfo.fromSocket;
    } else {
        qWarning() << "[SERVER] SECURITY: Unauthorized call end attempt by" << currentUser;
        return;
    }

    qDebug() << "[SERVER] Call ended by" << currentUser << "| callId:" << callId;

    // Удаляем звонок из активных
    m_activeCalls.remove(callId);

    // Формируем уведомление о завершении звонка
    QJsonObject endNotification;
    endNotification["type"] = "call_end";
    endNotification["call_id"] = callId;
    endNotification["from"] = currentUser;

    // Отправляем уведомление другой стороне, если она онлайн
    if (otherSocket != nullptr) {
        sendJson(otherSocket, endNotification);
    }

    qDebug() << "[SERVER] Call end notification sent";
}


/**
 * @brief Создаёт новую запись звонка в базе данных при поступлении call_request.
 *
 * @details Метод сохраняет начальную информацию о звонке:
 * - Уникальный идентификатор звонка `call_id`.
 * - Логины инициатора и вызываемого пользователя.
 * - Статус `ringing` (звонок в процессе звонка).
 * - Время начала звонка (текущее время сервера).
 * - IP адрес и порт инициатора звонка.
 *
 * @param callId Уникальный идентификатор звонка (UUID).
 * @param from Логин инициатора звонка.
 * @param to Логин вызываемого пользователя.
 * @param fromIp IP адрес инициатора звонка.
 * @param fromPort UDP порт инициатора звонка.
 */
void Server::createCallRecord(const QString& callId, const QString& from,
                             const QString& to, const QString& fromIp, quint16 fromPort)
{
    QSqlQuery query;
    query.prepare("INSERT INTO call_history "
                  "(call_id, caller_username, callee_username, status, start_time, "
                  " caller_ip, caller_port) "
                  "VALUES (:callId, :from, :to, 'ringing', :startTime, :fromIp, :fromPort)");

    query.bindValue(":callId", callId);
    query.bindValue(":from", from);
    query.bindValue(":to", to);
    query.bindValue(":startTime", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":fromIp", fromIp);
    query.bindValue(":fromPort", fromPort);

    if (!query.exec()) {
        qWarning() << "[CALL] DB Error creating call record:" << query.lastError().text();
    } else {
        qDebug() << "[CALL] Created call record:" << callId;
    }
}


/**
 * @brief Обновляет запись звонка при его принятии (call_accepted).
 *
 * @details Метод сохраняет статус звонка как "connected", фиксирует время соединения,
 * а также IP адрес и UDP порт вызываемого пользователя.
 *
 * @param callId Уникальный идентификатор звонка.
 * @param toIp IP адрес вызываемого пользователя.
 * @param toPort UDP порт вызываемого пользователя.
 */
void Server::updateCallConnected(const QString& callId, const QString& toIp, quint16 toPort)
{
    QSqlQuery query;
    query.prepare("UPDATE call_history "
                  "SET status = 'connected', "
                  "    connect_time = :connectTime, "
                  "    callee_ip = :toIp, "
                  "    callee_port = :toPort "
                  "WHERE call_id = :callId");

    query.bindValue(":callId", callId);
    query.bindValue(":connectTime", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":toIp", toIp);
    query.bindValue(":toPort", toPort);

    if (!query.exec()) {
        qWarning() << "[CALL] DB Error updating call connected:" << query.lastError().text();
    } else {
        qDebug() << "[CALL] Updated call as connected:" << callId;
    }
}


/**
 * @brief Завершает звонок с учётом статуса и рассчитывает длительность.
 *
 * @details Метод обновляет в таблице `call_history` статус звонка (`status`), время окончания (`end_time`)
 * и рассчитывает длительность разговора (`duration_seconds`) в секундах на основе разницы между
 * временем подключения (`connect_time`) и текущим временем.
 * 
 * @param callId Уникальный идентификатор звонка.
 * @param status Новый статус звонка: например, "completed", "rejected", "missed".
 */
void Server::updateCallEnded(const QString& callId, const QString& status)
{
    QSqlQuery query;
    query.prepare(
        "UPDATE call_history "
        "SET status = :status, "
        "    end_time = :endTime, "
        "    duration_seconds = CAST((julianday(:endTime) - julianday(connect_time)) * 86400 AS INTEGER) "
        "WHERE call_id = :callId"
    );

    query.bindValue(":callId", callId);
    query.bindValue(":status", status);
    query.bindValue(":endTime", QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!query.exec()) {
        qWarning() << "[CALL] DB Error updating call ended:" << query.lastError().text();
    } else {
        qDebug() << "[CALL] Updated call as" << status << ":" << callId;
    }
}


/**
 * @brief Обрабатывает запрос на получение истории звонков пользователя.
 *
 * @details Метод извлекает из таблицы `call_history` до 50 последних звонков,
 * где указанный пользователь был либо инициатором, либо получателем звонка.
 * Результаты сортируются по времени начала звонка в порядке убывания.
 * Для каждого звонка в ответ добавляется поле `call_type`, указывающее
 * направление звонка относительно пользователя (`incoming` или `outgoing`).
 * 
 * Созданный JSON с массивом звонков отправляется клиенту.
 *
 * @param socket Сокет клиента, запрашивающего историю.
 * @param request JSON-объект с полем:
 *        - `username`: Имя пользователя, для которого выводится история.
 */
void Server::handleGetCallHistory(QObject* socket, const QJsonObject& request)
{
    QString username = request["username"].toString();

    QSqlQuery query;
    query.prepare(
        "SELECT call_id, caller_username, callee_username, status, "
        "       start_time, end_time, duration_seconds "
        "FROM call_history "
        "WHERE caller_username = :user OR callee_username = :user "
        "ORDER BY start_time DESC LIMIT 50"
    );
    query.bindValue(":user", username);

    if (!query.exec()) {
        qWarning() << "[CALL] Error fetching call history for user" << username << ":" << query.lastError().text();
        sendJson(socket, {{"type", "error"}, {"reason", "Failed to fetch call history"}});
        return;
    }

    QJsonArray calls;
    while (query.next()) {
        QJsonObject call;
        call["call_id"] = query.value("call_id").toString();
        call["caller"] = query.value("caller_username").toString();
        call["callee"] = query.value("callee_username").toString();
        call["status"] = query.value("status").toString();
        call["start_time"] = query.value("start_time").toString();
        call["end_time"] = query.value("end_time").toString();
        call["duration_seconds"] = query.value("duration_seconds").toInt();

        // Определяем направление звонка относительно пользователя
        call["call_type"] = (username == query.value("caller_username").toString()) ? "outgoing" : "incoming";

        calls.append(call);
    }

    QJsonObject response;
    response["type"] = "call_history";
    response["calls"] = calls;

    sendJson(socket, response);

    qDebug() << "[CALL] Sent call history to" << username << ":" << calls.size() << "records";
}


/**
 * @brief Обрабатывает запрос на получение статистики звонков пользователя.
 *
 * @details Метод выполняет агрегатный SQL-запрос для подсчёта:
 * - Общее количество исходящих звонков.
 * - Общее количество входящих звонков.
 * - Количество завершённых звонков.
 * - Количество пропущенных вызовов (для вызываемого пользователя).
 * - Общую суммарную длительность завершённых звонков в секундах.
 * 
 * По результатам формируется JSON-объект и отправляется клиенту.
 *
 * @param socket Сокет клиента, запросившего статистику.
 * @param request JSON-объект с полем:
 *        - `username`: Имя пользователя, для которого собирается статистика.
 */
void Server::handleGetCallStats(QObject* socket, const QJsonObject& request)
{
    QString username = request["username"].toString();

    QSqlQuery query;
    query.prepare(
        "SELECT "
        "   COUNT(*) FILTER (WHERE caller_username = :user) AS outgoing_count, "
        "   COUNT(*) FILTER (WHERE callee_username = :user) AS incoming_count, "
        "   COUNT(*) FILTER (WHERE status = 'completed') AS completed_count, "
        "   COUNT(*) FILTER (WHERE status = 'missed' AND callee_username = :user) AS missed_count, "
        "   SUM(CASE WHEN status = 'completed' THEN duration_seconds ELSE 0 END) AS total_duration "
        "FROM call_history "
        "WHERE caller_username = :user OR callee_username = :user"
    );

    query.bindValue(":user", username);

    if (!query.exec() || !query.next()) {
        qWarning() << "[CALL] Failed to fetch call stats for user" << username << ":" << query.lastError().text();
        sendJson(socket, {{"type", "error"}});
        return;
    }

    QJsonObject stats;
    stats["type"] = "call_stats";
    stats["outgoing"] = query.value("outgoing_count").toInt();
    stats["incoming"] = query.value("incoming_count").toInt();
    stats["completed"] = query.value("completed_count").toInt();
    stats["missed"] = query.value("missed_count").toInt();
    stats["total_duration_sec"] = query.value("total_duration").toInt();

    sendJson(socket, stats);

    qDebug() << "[CALL] Sent call stats to" << username << ":" << stats;
}


/**
 * @brief Генерирует новый токен аутентификации и сохраняет его в БД.
 *
 * @details Токен используется для автоматического входа (auto-login) без повторного ввода пароля.
 * 
 * **Алгоритм:**
 * 1. Генерирует уникальную строку на основе UUID, имени пользователя и текущего времени.
 * 2. Хеширует эту строку алгоритмом SHA-256 для получения фиксированной длины и обфускации.
 * 3. Сохраняет токен в таблицу `tokens` базы данных с сроком действия **30 дней**.
 *    Используется `INSERT OR REPLACE`, чтобы у пользователя был только один активный токен (для простоты).
 * 4. Обновляет кэш токенов в оперативной памяти (`m_userTokens`) для быстрого доступа.
 * 
 * @param username Имя пользователя, для которого генерируется токен.
 * @return QString Сгенерированный токен (Hex-строка).
 */
QString Server::generateToken(const QString& username)
{
    // 1. Генерация сырых данных для токена (энтропия)
    QString rawToken = QUuid::createUuid().toString(QUuid::WithoutBraces) 
                     + username 
                     + QString::number(QDateTime::currentMSecsSinceEpoch());
    
    // 2. Хеширование для получения красивой строки фиксированной длины
    QByteArray hash = QCryptographicHash::hash(
        rawToken.toUtf8(),
        QCryptographicHash::Sha256
    );
    
    QString token = QString(hash.toHex());
    
    // 3. Сохранение в базу данных
    // datetime('now', '+30 days') — функция SQLite для вычисления даты истечения
    QSqlQuery query;
    query.prepare(R"(
        INSERT OR REPLACE INTO tokens (username, token, expires_at)
        VALUES (:username, :token, datetime('now', '+30 days'))
    )");
    
    query.bindValue(":username", username);
    query.bindValue(":token", token);
    
    if (!query.exec()) {
        qWarning() << "[SERVER] Failed to save token to DB for" << username 
                   << ":" << query.lastError().text();
    } else {
        qDebug() << "[SERVER] Token generated and saved for user:" << username;
    }
    
    // 4. Сохранение в памяти для быстрого доступа (O(1))
    m_userTokens[username] = token;
    
    return token;
}


/**
 * @brief Проверяет валидность токена пользователя.
 *
 * @details Проверка выполняется в два этапа:
 * 1. Сравнивается с токеном в оперативной памяти (кэш).
 * 2. Если токен в памяти отсутствует или не совпадает, проверяется БД.
 *    При этом учитывается срок действия токена `expires_at`.
 * 3. Если токен в БД валиден, он восстанавливается в кэш.
 * 4. Если токен истёк, удаляется запись из БД.
 *
 * @param username Имя пользователя.
 * @param token Токен, который необходимо проверить.
 * @return true Если токен валиден и действителен.
 * @return false Если токен недействителен, отсутствует или истёк.
 */
bool Server::validateToken(const QString& username, const QString& token)
{
    // 1. Проверяем токен в оперативном кэше
    if (m_userTokens.contains(username) && m_userTokens[username] == token) {
        return true;
    }
    
    // 2. Если нет или не совпадает — проверяем в базе
    QSqlQuery query;
    query.prepare(R"(
        SELECT token, expires_at 
        FROM tokens 
        WHERE username = :username
    )");
    query.bindValue(":username", username);
    
    if (!query.exec()) {
        qWarning() << "[SERVER] Failed to validate token for user" << username << ":" << query.lastError().text();
        return false;
    }
    
    if (!query.next()) {
        qDebug() << "[SERVER] No token found in DB for user:" << username;
        return false;
    }
    
    QString storedToken = query.value("token").toString();
    QDateTime expiresAt = QDateTime::fromString(query.value("expires_at").toString(), Qt::ISODate);
    
    // 3. Проверка срока действия токена
    if (expiresAt < QDateTime::currentDateTime()) {
        qDebug() << "[SERVER] Token expired for user:" << username;
        
        // Удаляем истёкший токен из БД
        QSqlQuery deleteQuery;
        deleteQuery.prepare("DELETE FROM tokens WHERE username = :username");
        deleteQuery.bindValue(":username", username);
        deleteQuery.exec();
        
        return false;
    }
    
    // 4. Сравниваем токены
    if (storedToken == token) {
        // Восстанавливаем токен в памяти для быстрого доступа
        m_userTokens[username] = token;
        qDebug() << "[SERVER] Token validated from DB for user:" << username;
        return true;
    }
    
    return false;
}


/**
 * @brief Обрабатывает автоматический вход пользователя по токену.
 *
 * @details Метод проверяет валидность токена, и при успехе:
 * - Получает профиль пользователя из базы.
 * - Добавляет пользователя в список онлайн.
 * - Отправляет клиенту профиль и успешный ответ.
 * - Загружает и отправляет список контактов, входящие запросы и счётчики непрочитанных сообщений.
 * - Обновляет список онлайн пользователей всем клиентам.
 *
 * @param socket Сокет клиента, сделавшего запрос.
 * @param request JSON-объект с полями:
 *        - `username`: Имя пользователя.
 *        - `token`: Токен для авторизации.
 */
void Server::handleTokenLogin(QObject* socket, const QJsonObject& request)
{
    QString username = request.value("username").toString();
    QString token = request.value("token").toString();

    qDebug() << "[SERVER] Token login attempt for user:" << username;

    QJsonObject response;

    // Проверяем валидность токена
    if (!validateToken(username, token)) {
        qWarning() << "[SERVER] ❌ Invalid or expired token for user:" << username;

        response["type"] = "token_login_failure";
        response["reason"] = "Invalid or expired token";
        sendJson(socket, response);
        return;
    }

    // Токен валиден: получаем профиль пользователя
    qInfo() << "[SERVER] ✅ User" << username << "logged in via token";

    QSqlQuery query;
    query.prepare("SELECT display_name, avatar_url, status_message FROM users WHERE username = :username");
    query.bindValue(":username", username);

    if (!query.exec() || !query.next()) {
        response["type"] = "token_login_failure";
        response["reason"] = "User not found";
        sendJson(socket, response);
        return;
    }

    QString displayname = query.value("display_name").toString();
    QString avatarurl = query.value("avatar_url").toString();
    QString statusmessage = query.value("status_message").toString();

    // Формируем успешный ответ с данными профиля
    response["type"] = "login_success";
    response["username"] = username;
    response["displayname"] = displayname;
    response["statusmessage"] = statusmessage;
    response["avatar_url"] = avatarurl;
    response["token"] = token;  // Возвращаем токен для подтверждения

    // Добавляем пользователя в онлайн-список
    m_clients[username] = socket;
    m_clientsReverse[socket] = username;

    sendJson(socket, response);

    // Отправляем дополнительные данные клиенту
    sendContactList(socket, username);              // Список контактов
    sendPendingContactRequests(socket, username);   // Входящие запросы в друзья
    sendUnreadCounts(socket, username);              // Счётчики непрочитанных сообщений

    // Обновляем онлайн-списки у всех клиентов
    broadcastUserList();
}
