#include "databaseservice.h"
#include <QSqlTableModel>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>

DatabaseService::DatabaseService(QObject *parent)
    : QObject(parent), m_initialized(false) {}

DatabaseService::~DatabaseService() {
    close();
}

bool DatabaseService::initialize(const QString &dbPath)
{
    Q_UNUSED(dbPath);
    
    // Логируем доступные драйверы SQL (для диагностики проблем с подключением)
    qDebug() << "[DatabaseService] Available SQL drivers:" << QSqlDatabase::drivers();

    // Создаем соединение с SQLite базой данных
    m_db = QSqlDatabase::addDatabase("QSQLITE");

    // Формируем полный путь к файлу БД (рядом с исполняемым файлом)
    QString fullPath = QCoreApplication::applicationDirPath() + "/database.db";
    m_db.setDatabaseName(fullPath);

    qDebug() << "[DatabaseService] Trying to open:" << fullPath;

    // Пытаемся открыть базу данных
    if (!m_db.open()) {
        QString error = "[DatabaseService] ERROR: Cannot open database: " + m_db.lastError().text();
        qDebug() << error;
        emit databaseError(error);
        return false;
    }

    qDebug() << "[DatabaseService] Database opened OK";

    // Создаем таблицы, если они еще не существуют
    if (!createTables()) {
        qDebug() << "[DatabaseService] ERROR: Failed to create tables";
        return false;
    }

    // Выполняем миграции схемы БД (добавление новых полей, индексов и т.д.)
    if (!migrateDatabase()) {
        qDebug() << "[DatabaseService] WARNING: Migration issues detected, continuing ...";
    }

    // Помечаем сервис как готовый к работе
    m_initialized = true;
    printDatabaseStats();

    return true;
}

void DatabaseService::close() {
    // Проверяем, открыта ли база данных
    if (m_db.isOpen()) {
        // Закрываем соединение с БД
        m_db.close();
        
        // Помечаем сервис как неинициализированный
        m_initialized = false;
        
        qDebug() << "[DatabaseService] Database closed";
    }
}

bool DatabaseService::isConnected() const {
    // Проверяем, что БД открыта и сервис инициализирован
    return m_db.isOpen() && m_initialized;
}

bool DatabaseService::createTables() {
    QSqlQuery query(m_db);

    // Создание таблицы сообщений
    // Хранит все сообщения: текст, метаданные, статус доставки, вложения
    QString createMessagesTable = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            server_id INTEGER UNIQUE,              -- ID сообщения на сервере (для синхронизации)
            temp_id TEXT,                          -- Временный ID до получения server_id
            from_user TEXT NOT NULL,               -- Отправитель
            to_user TEXT NOT NULL,                 -- Получатель
            payload TEXT NOT NULL,                 -- Зашифрованный текст сообщения
            timestamp TEXT,                        -- Время отправки (локальное)
            server_timestamp TEXT,                 -- Время получения сервером
            status INTEGER DEFAULT 0,              -- Статус: 0=отправка, 1=доставлено, 2=прочитано
            is_edited INTEGER DEFAULT 0,           -- Флаг редактирования
            reply_to_id INTEGER DEFAULT 0,         -- ID сообщения, на которое отвечаем
            is_outgoing INTEGER DEFAULT 0,         -- 1=исходящее, 0=входящее
            file_id TEXT,                          -- ID файла (если есть вложение)
            file_name TEXT,                        -- Имя файла
            file_url TEXT,                         -- URL для скачивания файла
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";
    
    if (!query.exec(createMessagesTable)) {
        qDebug() << "[DatabaseService] ERROR: Failed to create 'messages' table:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "[DatabaseService] Table 'messages' is OK";

    // Создание индексов для ускорения запросов
    QString createIndexes = R"(
        CREATE INDEX IF NOT EXISTS idx_from_to ON messages(from_user, to_user);
        CREATE INDEX IF NOT EXISTS idx_server_id ON messages(server_id);
        CREATE INDEX IF NOT EXISTS idx_temp_id ON messages(temp_id);
        CREATE INDEX IF NOT EXISTS idx_status ON messages(status);
    )";
    
    // Выполняем каждый запрос создания индекса отдельно
    for (const QString &indexQuery : createIndexes.split(";")) {
        if (!indexQuery.trimmed().isEmpty()) {
            if (!query.exec(indexQuery)) {
                qDebug() << "[DatabaseService] WARNING: Index creation issue:" << query.lastError().text();
            }
        }
    }
    qDebug() << "[DatabaseService] Indexes are OK";

    // Создание таблицы чатов (список диалогов в UI)
    // Хранит метаданные каждого диалога: последнее сообщение, закрепление, черновики
    QString createChatsTable = R"(
CREATE TABLE IF NOT EXISTS chats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,             -- Собеседник
    display_name TEXT,                         -- Отображаемое имя
    last_message_payload TEXT,                 -- Текст последнего сообщения
    last_message_timestamp TEXT,               -- Время последнего сообщения
    last_message_id INTEGER,                   -- ID последнего сообщения
    is_pinned INTEGER DEFAULT 0,               -- Закреплен ли чат
    is_archived INTEGER DEFAULT 0,             -- Архивирован ли чат
    is_last_message_outgoing INTEGER DEFAULT 0, -- Последнее сообщение исходящее?
    draft_text TEXT,                           -- Черновик сообщения
    is_muted INTEGER DEFAULT 0                 -- Отключены ли уведомления
);
    )";
    
    if (!query.exec(createChatsTable)) {
        qDebug() << "[DatabaseService] ERROR: Failed to create 'chats' table:" << query.lastError().text();
        return false;
    }
    qDebug() << "[DatabaseService] Table 'chats' is OK";

    // Создание таблицы контактов
    // Хранит информацию о пользователях: статус онлайн, аватар, статусное сообщение
    QString createContactsTable = R"(
        CREATE TABLE IF NOT EXISTS contacts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT UNIQUE NOT NULL,
            display_name TEXT,
            is_online INTEGER DEFAULT 0,           -- Онлайн статус
            last_seen TEXT,                        -- Время последней активности
            avatar_url TEXT,                       -- URL аватара
            status_message TEXT,                   -- Статусное сообщение пользователя
            synced_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";
    
    if (!query.exec(createContactsTable)) {
        qDebug() << "[DatabaseService] ERROR: Failed to create 'contacts' table:" << query.lastError().text();
        return false;
    }
    qDebug() << "[DatabaseService] Table 'contacts' is OK";
    
    // Создание таблицы черновиков (отдельное хранение для каждого пользователя)
    // Позволяет сохранять черновики для нескольких аккаунтов на одном устройстве
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS drafts (
            owner_username TEXT NOT NULL,          -- Владелец черновика (текущий пользователь)
            chat_username TEXT NOT NULL,           -- Чат, к которому относится черновик
            draft_text TEXT,                       -- Текст черновика
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (owner_username, chat_username)
        )
    )")) {
        qCritical() << "[DB] Error: failed to create drafts table:" << query.lastError().text();
        return false;
    }

    qDebug() << "[DB] Drafts table initialized successfully";
    
    return true;
}


bool DatabaseService::migrateDatabase() {
    // Заглушка для будущих миграций схемы БД (добавление новых полей, изменение индексов и т.д.)
    // В будущем здесь можно добавить проверку версии схемы и выполнение ALTER TABLE
    return true;
}

bool DatabaseService::saveMessage(const ChatMessage &msg, const QString &currentUsername) {

    Q_UNUSED(currentUsername);
    
    // Проверяем, что БД доступна
    if (!isConnected()) {
        qDebug() << "[DatabaseService] ERROR: Not connected";
        return false;
    }

    // Подготавливаем запрос на вставку сообщения
    // INSERT OR IGNORE предотвращает дубликаты по UNIQUE полям (server_id)
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR IGNORE INTO messages (
            server_id, temp_id, from_user, to_user, payload,
            timestamp, server_timestamp, status, is_edited, reply_to_id, is_outgoing, file_id, file_name, file_url
        ) VALUES (
            :server_id, :temp_id, :from_user, :to_user, :payload,
            :timestamp, :server_timestamp, :status, :is_edited, :reply_to_id, :is_outgoing, :file_id, :file_name, :file_url
        )
    )");

    // Биндим значения полей сообщения
    query.addBindValue(msg.id > 0 ? msg.id : QVariant());  // server_id (NULL если еще не получен)
    query.addBindValue(msg.tempId.isEmpty() ? QVariant() : msg.tempId);  // temp_id (локальный ID до получения server_id)
    query.addBindValue(msg.fromUser);       // Отправитель
    query.addBindValue(msg.toUser);         // Получатель
    query.addBindValue(msg.payload);        // Зашифрованный текст
    query.addBindValue(msg.timestamp);      // Временная метка клиента
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));  // Временная метка сервера (локальная)
    query.addBindValue((int)msg.status);    // Статус доставки
    query.addBindValue(msg.isEdited ? 1 : 0);  // Флаг редактирования
    query.addBindValue(msg.replyToId);         // ID сообщения, на которое отвечаем
    query.addBindValue(msg.isOutgoing ? 1 : 0);  // Направление сообщения
    query.addBindValue(msg.fileId);         // ID вложенного файла
    query.addBindValue(msg.fileName);       // Имя файла
    query.addBindValue(msg.fileUrl);        // URL файла
    
    qDebug() << "MESSAGE LOCALLYSAVED";
    qDebug() << msg.id << msg.tempId << msg.fromUser << msg.toUser << msg.payload << msg.timestamp << msg.status << msg.isEdited << msg.replyToId << msg.isOutgoing << msg.fileId << msg.fileName << msg.fileUrl;

    // Выполняем запрос
    if (!query.exec()) {
        QString error = "[DatabaseService] ERROR: Failed to insert message:" + query.lastError().text();
        qDebug() << error;
        emit databaseError(error);
        return false;
    }

    qDebug() << "[DatabaseService] Message saved, id:" << msg.id << "payload:" << msg.payload.left(50);
    return true;
}
bool DatabaseService::updateMessageStatus(qint64 messageId, ChatMessage::MessageStatus status) {
    // Проверяем соединение
    if (!isConnected()) return false;

    // Обновляем статус сообщения по его server_id
    QSqlQuery query(m_db);
    query.prepare("UPDATE messages SET status = :status, updated_at = CURRENT_TIMESTAMP WHERE server_id = :id");
    query.addBindValue((int)status);  // Новый статус (0=отправка, 1=доставлено, 2=прочитано)
    query.addBindValue(messageId);    // ID сообщения на сервере

    // Выполняем обновление
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to update status for" << messageId << ":" << query.lastError().text();
        return false;
    }

    qDebug() << "[DatabaseService] Status updated for id:" << messageId << "to status:" << (int)status;

    return true;
}

bool DatabaseService::updateAllMessagesStatusForChat(const QString &withUser, const QString &currentUsername,
                                                     ChatMessage::MessageStatus status) {
    // Проверяем соединение
    if (!isConnected()) return false;

    // Массовое обновление статуса для всех сообщений в диалоге
    // Обновляем только те сообщения, у которых статус меньше нового (не откатываем назад)
    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE messages
        SET status = :status, updated_at = CURRENT_TIMESTAMP
        WHERE from_user = :from_user AND to_user = :to_user AND status < :status
    )");
    query.addBindValue((int)status);     // Новый статус
    query.addBindValue(withUser);        // Отправитель (собеседник)
    query.addBindValue(currentUsername); // Получатель (текущий пользователь)
    query.addBindValue((int)status);     // Условие: только если текущий статус меньше

    // Выполняем массовое обновление
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to bulk-update status:" << query.lastError().text();
        return false;
    }

    qDebug() << "[DatabaseService] Bulk status updated for chat:" << withUser;
    return true;
}

QList<ChatMessage> DatabaseService::loadRecentMessages(const QString &fromUser, const QString &toUser, int limit) {
    QList<ChatMessage> messages;
    
    // Проверяем соединение
    if (!isConnected()) {
        qDebug() << "[DatabaseService] ERROR: Not connected";
        return messages;
    }

    // Загружаем последние N сообщений из диалога между двумя пользователями
    // Условие OR охватывает оба направления переписки (A->B и B->A)
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT server_id, temp_id, from_user, to_user, payload, timestamp,
               status, is_edited, reply_to_id, is_outgoing, file_id, file_name, file_url
        FROM messages
        WHERE (from_user = :user1 AND to_user = :user2) OR
              (from_user = :user2 AND to_user = :user1)
        ORDER BY timestamp DESC
        LIMIT :limit
    )");
    query.bindValue(":user1", fromUser);
    query.bindValue(":user2", toUser);
    query.bindValue(":limit", limit);

    // Выполняем запрос
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to load recent messages:" << query.lastError().text();
        return messages;
    }

    // Читаем результаты и формируем список сообщений
    QList<ChatMessage> tempMessages;
    while (query.next()) {
        ChatMessage msg;
        msg.id = query.value(0).toLongLong();       // server_id
        msg.tempId = query.value(1).toString();     // temp_id
        msg.fromUser = query.value(2).toString();   // from_user
        msg.toUser = query.value(3).toString();     // to_user
        msg.payload = query.value(4).toString();    // payload (зашифрованный текст)
        msg.timestamp = query.value(5).toString();  // timestamp
        msg.status = (ChatMessage::MessageStatus)query.value(6).toInt();  // status
        msg.isEdited = query.value(7).toInt() == 1;        // is_edited
        msg.replyToId = query.value(8).toLongLong();       // reply_to_id
        msg.isOutgoing = query.value(9).toInt() == 1;      // is_outgoing
        msg.fileId = query.value(10).toString();           // file_id
        msg.fileName = query.value(11).toString();         // file_name
        msg.fileUrl = query.value(12).toString();          // file_url
        
        // Добавляем в начало, чтобы восстановить хронологический порядок (т.к. выбирали DESC)
        tempMessages.prepend(msg);
    }
    
    qDebug() << "[DatabaseService] Loaded" << tempMessages.size() << "recent messages for chat";
    return tempMessages;
}

QList<User> DatabaseService::loadAllUsers() {
    QList<User> users;
    
    // Загружаем всех контактов из таблицы contacts
    QSqlQuery query;
    query.prepare("SELECT username, display_name, avatar_url, status_message FROM contacts");
    
    if (query.exec()) {
        while (query.next()) {
            User user;
            user.username      = query.value(0).toString();  // username
            user.displayName   = query.value(1).toString();  // display_name
            user.avatarUrl     = query.value(2).toString();  // avatar_url
            user.statusMessage = query.value(3).toString();  // status_message
            users.append(user);
        }
    } else {
        qDebug() << "[DatabaseService] ERROR: loadAllUsers:" << query.lastError().text();
    }
    
    return users;
}

QList<Chat> DatabaseService::loadAllChats() {
    QList<Chat> chats;
    
    // Загружаем все чаты из таблицы chats (список диалогов для UI)
    QSqlQuery query;
    query.prepare(
        "SELECT id, username, display_name, last_message_payload, last_message_timestamp, "
        "last_message_id, is_pinned, is_archived, is_last_message_outgoing, "
        "draft_text, is_muted FROM chats"
    );
    
    if (query.exec()) {
        while (query.next()) {
            Chat chat;
            chat.id = query.value(0).toInt();                          // id
            chat.username = query.value(1).toString();                 // username (собеседник)
            chat.displayName = query.value(2).toString();              // display_name
            chat.lastMessagePayload = query.value(3).toString();       // last_message_payload
            chat.lastMessageTimestamp = query.value(4).toString();     // last_message_timestamp
            chat.lastMessageId = query.value(5).toLongLong();          // last_message_id
            chat.isPinned = query.value(6).toInt();                    // is_pinned (закреплен ли чат)
            chat.isArchived = query.value(7).toInt();                  // is_archived (архивирован ли)
            chat.isLastMessageOutgoing = query.value(8).toInt();       // is_last_message_outgoing
            chat.draftText = query.value(9).toString();                // draft_text (черновик)
            chat.isMuted = query.value(10).toInt();                    // is_muted (отключены ли уведомления)
            chats.append(chat);
        }
    } else {
        qDebug() << "[DatabaseService] ERROR: loadAllChats:" << query.lastError().text();
    }
    
    return chats;
}

void DatabaseService::addOrUpdateChat(const Chat& chat) {
    // Добавляем новый чат или обновляем существующий (по username)
    // ON CONFLICT используется для UPSERT логики (INSERT + UPDATE)
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO chats (
            username,
            display_name,
            last_message_payload,
            last_message_timestamp,
            last_message_id,
            is_pinned,
            is_archived,
            is_last_message_outgoing,
            draft_text,
            is_muted
        ) VALUES (
            :username,
            :displayName,
            :payload,
            :timestamp,
            :msgId,
            :pinned,
            :archived,
            :isOutgoing,
            :draft,
            :muted
        )
        ON CONFLICT(username) DO UPDATE SET
            display_name = excluded.display_name,
            last_message_payload = excluded.last_message_payload,
            last_message_timestamp = excluded.last_message_timestamp,
            last_message_id = excluded.last_message_id,
            is_pinned = excluded.is_pinned,
            is_archived = excluded.is_archived,
            is_last_message_outgoing = excluded.is_last_message_outgoing,
            draft_text = excluded.draft_text,
            is_muted = excluded.is_muted
    )");

    // Биндим значения для вставки/обновления
    query.bindValue(":username", chat.username);
    query.bindValue(":displayName", chat.displayName);
    query.bindValue(":payload", chat.lastMessagePayload);
    query.bindValue(":timestamp", chat.lastMessageTimestamp);
    query.bindValue(":msgId", chat.lastMessageId);
    query.bindValue(":pinned", chat.isPinned ? 1 : 0);
    query.bindValue(":archived", chat.isArchived ? 1 : 0);
    query.bindValue(":isOutgoing", chat.isLastMessageOutgoing ? 1 : 0);
    query.bindValue(":draft", chat.draftText);
    query.bindValue(":muted", chat.isMuted ? 1 : 0);
    
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to add/update chat:"
                 << query.lastError().text();
    }
}

void DatabaseService::addOrUpdateUser(const User& user) {
    // Добавляем нового пользователя или обновляем существующего (по username)
    // ON CONFLICT используется для UPSERT логики
    QSqlQuery query;
    query.prepare(R"(
        INSERT INTO contacts (
            username, display_name, status_message, avatar_url, last_seen, is_online
        ) VALUES (
            :username, :displayName, :statusMessage, :avatarUrl, :lastSeen, :isOnline
        )
        ON CONFLICT(username) DO UPDATE SET
            display_name = excluded.display_name,
            status_message = excluded.status_message,
            avatar_url = excluded.avatar_url,
            last_seen = excluded.last_seen,
            is_online = excluded.is_online
    )");

    // Биндим данные пользователя
    query.bindValue(":username", user.username);
    query.bindValue(":displayName", user.displayName);
    query.bindValue(":statusMessage", user.statusMessage);
    query.bindValue(":avatarUrl", user.avatarUrl);
    query.bindValue(":lastSeen", user.lastSeen);
    query.bindValue(":isOnline", user.isOnline);

    if (!query.exec()) {
        qWarning() << "[DatabaseService] addOrUpdateUser error:" << query.lastError().text();
    }
}

Chat DatabaseService::getChatByUsername(const QString& username) {
    Chat chat;  // Создаем пустой объект чата

    // Ищем чат по username в таблице chats
    QSqlQuery query;
    query.prepare(R"(
        SELECT id, username, display_name, last_message_payload, last_message_timestamp, 
               last_message_id, is_pinned, is_archived, is_last_message_outgoing,
               draft_text, is_muted
        FROM chats
        WHERE username = :username
        LIMIT 1
    )");
    query.bindValue(":username", username);

    // Если чат найден, заполняем объект
    if (query.exec() && query.next()) {
        chat.id                     = query.value(0).toInt();
        chat.username               = query.value(1).toString();
        chat.displayName            = query.value(2).toString();
        chat.lastMessagePayload     = query.value(3).toString();
        chat.lastMessageTimestamp   = query.value(4).toString();
        chat.lastMessageId          = query.value(5).toLongLong();
        chat.unreadCount            = 0;  // Счетчик непрочитанных (вычисляется отдельно)
        chat.isPinned               = query.value(6).toInt();
        chat.isArchived             = query.value(7).toInt();
        chat.isLastMessageOutgoing  = query.value(8).toInt();
        chat.draftText              = query.value(9).toString();
        chat.isMuted                = query.value(10).toInt();
    } else {
        qDebug() << "[DatabaseService] getChatByUsername: not found or DB error for" << username << ":" << query.lastError().text();
    }
    
    return chat;
}

QList<ChatMessage> DatabaseService::loadOlderMessages(const QString &fromUser, const QString &toUser,
                                                      qint64 beforeId, int limit) {
    QList<ChatMessage> messages;
    
    // Проверяем соединение
    if (!isConnected()) return messages;

    // Загружаем старые сообщения (для прокрутки истории назад)
    // Условие server_id < beforeId обеспечивает загрузку только более старых сообщений
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT server_id, temp_id, from_user, to_user, payload, timestamp,
               status, is_edited, reply_to_id, is_outgoing, file_id, file_name, file_url
        FROM messages
        WHERE ((from_user = :user1 AND to_user = :user2) OR
               (from_user = :user2 AND to_user = :user1))
              AND server_id < :before_id
        ORDER BY timestamp DESC
        LIMIT :limit
    )");
    query.addBindValue(fromUser);
    query.addBindValue(toUser);
    query.addBindValue(beforeId);  // Загружаем только сообщения старше этого ID
    query.addBindValue(limit);

    // Выполняем запрос
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to load older messages:" << query.lastError().text();
        return messages;
    }

    // Читаем результаты и формируем список
    QList<ChatMessage> tempMessages;
    while (query.next()) {
        ChatMessage msg;
        msg.id = query.value(0).toLongLong();
        msg.tempId = query.value(1).toString();
        msg.fromUser = query.value(2).toString();
        msg.toUser = query.value(3).toString();
        msg.payload = query.value(4).toString();
        msg.timestamp = query.value(5).toString();
        msg.status = (ChatMessage::MessageStatus)query.value(6).toInt();
        msg.isEdited = query.value(7).toInt() == 1;
        msg.replyToId = query.value(8).toLongLong();
        msg.isOutgoing = query.value(9).toInt() == 1;
        msg.fileId = query.value(10).toString();
        msg.fileName = query.value(11).toString();
        msg.fileUrl = query.value(12).toString();
        
        // Добавляем в начало для восстановления хронологии (т.к. ORDER BY DESC)
        tempMessages.prepend(msg);
    }
    
    qDebug() << "[DatabaseService] Loaded" << tempMessages.size() << "older messages for chat";
    return tempMessages;
}
QList<ChatMessage> DatabaseService::loadMessagesForUser(const QString &currentUsername) {
    QList<ChatMessage> messages;
    
    // Проверяем соединение
    if (!isConnected()) return messages;

    // Загружаем все сообщения, связанные с текущим пользователем
    // (как входящие, так и исходящие, из всех диалогов)
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT server_id, temp_id, from_user, to_user, payload, timestamp,
               status, is_edited, reply_to_id, is_outgoing, file_id, file_name, file_url
        FROM messages
        WHERE from_user = :username OR to_user = :username
        ORDER BY timestamp DESC
    )");
    query.addBindValue(currentUsername);

    // Выполняем запрос
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to load messages for user:" << query.lastError().text();
        return messages;
    }

    // Читаем все сообщения из результата
    while (query.next()) {
        ChatMessage msg;
        msg.id = query.value(0).toLongLong();
        msg.tempId = query.value(1).toString();
        msg.fromUser = query.value(2).toString();
        msg.toUser = query.value(3).toString();
        msg.payload = query.value(4).toString();
        msg.timestamp = query.value(5).toString();
        msg.status = (ChatMessage::MessageStatus)query.value(6).toInt();
        msg.isEdited = query.value(7).toInt() == 1;
        msg.replyToId = query.value(8).toLongLong();
        msg.isOutgoing = query.value(9).toInt() == 1;
        msg.fileId = query.value(10).toString();
        msg.fileName = query.value(11).toString();
        msg.fileUrl = query.value(12).toString();
        messages.append(msg);
    }
    
    qDebug() << "[DatabaseService] Loaded" << messages.size() << "messages for user" << currentUsername;
    return messages;
}

qint64 DatabaseService::getOldestMessageId(const QString &fromUser, const QString &toUser) {
    // Проверяем соединение
    if (!isConnected()) return -1;

    // Находим минимальный server_id в диалоге (самое старое сообщение)
    // Используется для определения границы при подгрузке истории
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT MIN(server_id) FROM messages
        WHERE (from_user = :user1 AND to_user = :user2) OR (from_user = :user2 AND to_user = :user1)
    )");
    query.addBindValue(fromUser);
    query.addBindValue(toUser);

    // Выполняем запрос и возвращаем результат
    if (query.exec() && query.next()) {
        qint64 result = query.value(0).toLongLong();
        qDebug() << "[DatabaseService] Oldest message ID for chat:" << result;
        return result;
    }
    
    return -1;  // Нет сообщений или ошибка
}

bool DatabaseService::deleteMessage(qint64 messageId) {
    // Проверяем соединение
    if (!isConnected()) return false;

    // Удаляем сообщение по его server_id
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM messages WHERE server_id = :id");
    query.addBindValue(messageId);

    // Выполняем удаление
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to delete message:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "[DatabaseService] Message deleted, id:" << messageId;
    return true;
}

bool DatabaseService::editMessage(qint64 messageId, const QString &newPayload) {
    // Проверяем подключение к базе
    if (!isConnected()) return false;

    // Обновляем текст сообщения и помечаем его как отредактированное
    QSqlQuery query(m_db);
    query.prepare(R"(
        UPDATE messages SET payload = :payload, is_edited = 1, updated_at = CURRENT_TIMESTAMP WHERE server_id = :id
    )");
    query.addBindValue(newPayload);  // Новый текст сообщения
    query.addBindValue(messageId);   // ID сообщения на сервере

    // Выполняем запрос
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to edit message:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "[DatabaseService] Message edited, id:" << messageId;
    return true;
}

int DatabaseService::getUnreadCountForChat(const QString &fromUser, const QString &currentUsername) {
    // Проверяем соединение
    if (!isConnected()) return 0;

    // Считаем количество непрочитанных сообщений в конкретном чате
    // status < 3 — используется как условие "не прочитано" (например, 0,1,2)
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT COUNT(*) FROM messages
        WHERE from_user = :from_user AND to_user = :to_user AND status < 3
    )");
    query.addBindValue(fromUser);        // Собеседник
    query.addBindValue(currentUsername); // Текущий пользователь

    // Выполняем запрос и возвращаем число
    if (query.exec() && query.next())
        return query.value(0).toInt();

    return 0;
}

QMap<QString, int> DatabaseService::getAllUnreadCounts(const QString &currentUsername) {
    QMap<QString, int> unreadCounts;
    
    // Проверяем соединение
    if (!isConnected()) return unreadCounts;

    // Получаем количество непрочитанных сообщений по каждому собеседнику
    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT from_user, COUNT(*) as count
        FROM messages
        WHERE to_user = :to_user AND status < 3
        GROUP BY from_user
    )");
    query.addBindValue(currentUsername);

    // Заполняем карту: ключ — username собеседника, значение — число непрочитанных
    if (query.exec()) {
        while (query.next()) {
            QString user = query.value(0).toString();
            int count = query.value(1).toInt();
            unreadCounts[user] = count;
        }
    }
    
    qDebug() << "[DatabaseService] Loaded unread counts for" << unreadCounts.size() << "chats";
    return unreadCounts;
}

bool DatabaseService::clearAllData() {
    // Проверяем соединение с базой
    if (!isConnected()) return false;

    QSqlQuery query(m_db);
    QStringList tables = {"messages", "chats", "contacts"};
    
    // Очищаем данные из всех основных таблиц
    for (const QString &table : tables) {
        if (!query.exec("DELETE FROM " + table)) {
            qDebug() << "[DatabaseService] ERROR: Failed to clear table" << table << ":" << query.lastError().text();
            return false;
        }
    }
    
    qDebug() << "[DatabaseService] All data cleared";
    return true;
}

void DatabaseService::printDatabaseStats() {
    // Если БД недоступна — ничего не выводим
    if (!isConnected()) return;

    QSqlQuery query(m_db);

    // Считаем количество сообщений
    query.exec("SELECT COUNT(*) FROM messages");
    query.next();
    int messageCount = query.value(0).toInt();

    // Считаем количество контактов
    query.exec("SELECT COUNT(*) FROM contacts");
    query.next();
    int contactCount = query.value(0).toInt();

    // Считаем количество чатов
    query.exec("SELECT COUNT(*) FROM chats");
    query.next();
    int chatCount = query.value(0).toInt();

    // Выводим простую статистику по базе данных
    qDebug() << "[DatabaseService] === DATABASE STATS ===";
    qDebug() << "[DatabaseService] Messages:" << messageCount;
    qDebug() << "[DatabaseService] Contacts:" << contactCount;
    qDebug() << "[DatabaseService] Chats:" << chatCount;
    qDebug() << "[DatabaseService] === END DATABASE STATS ===";
}

bool DatabaseService::confirmSentMessageByTempId(const QString& tempId, const ChatMessage& confirmedMsg) {
    // Проверяем соединение
    if (!isConnected()) return false;
    
    QSqlQuery query(m_db);
    // Обновляем временное сообщение, подставляя окончательный server_id и статус
    query.prepare("UPDATE messages SET server_id = ?, timestamp = ?, status = ?, temp_id = NULL WHERE temp_id = ?");
    query.addBindValue(confirmedMsg.id);              // Идентификатор с сервера
    query.addBindValue(confirmedMsg.timestamp);       // Актуальная временная метка
    query.addBindValue((int)confirmedMsg.status);     // Новый статус (обычно "доставлено")
    query.addBindValue(tempId);                       // Временный ID, по которому ищем локальное сообщение
    
    if (!query.exec()) {
        qDebug() << "[DatabaseService] ERROR: Failed to confirm message by tempId:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "[DatabaseService] Confirmed message by tempId:" << tempId << "-> server_id:" << confirmedMsg.id;
    return true;
}

QString DatabaseService::executeSqlError(const QSqlQuery &query) {
    // Возвращаем текст последней SQL-ошибки для удобства логирования и отображения
    return query.lastError().text();
}
bool DatabaseService::saveDraft(const QString& username, 
                                const QString& ownerUsername, 
                                const QString& draftText)
{
    // Проверяем соединение с базой
    if (!isConnected()) {
        qWarning() << "[DB] Cannot save draft: not connected";
        return false;
    }
    
    QSqlQuery query(m_db);
    
    if (draftText.isEmpty()) {
        // Пустой текст черновика — удаляем существующую запись для этого чата и пользователя
        query.prepare(R"(
            DELETE FROM drafts 
            WHERE owner_username = :owner AND chat_username = :chat
        )");
    } else {
        // Сохраняем или обновляем черновик для пары (owner, chat)
        query.prepare(R"(
            INSERT OR REPLACE INTO drafts (owner_username, chat_username, draft_text, updated_at)
            VALUES (:owner, :chat, :draft, CURRENT_TIMESTAMP)
        )");
        query.bindValue(":draft", draftText);
    }
    
    query.bindValue(":owner", ownerUsername);
    query.bindValue(":chat", username);
    
    if (!query.exec()) {
        qWarning() << "[DB] Failed to save draft:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "[DB] Draft saved for chat:" << username;
    return true;
}

QString DatabaseService::loadDraft(const QString& username, 
                                   const QString& ownerUsername)
{
    Q_UNUSED(ownerUsername);
    
    // Если нет соединения — возвращаем пустую строку
    if (!isConnected()) {
        return QString();
    }
    
    QSqlQuery query(m_db);
    // Читаем черновик из таблицы chats (там хранится поле draft_text)
    query.prepare(R"(
        SELECT draft_text
        FROM chats
        WHERE username = :userName
    )");
    
    query.bindValue(":userName", username);
    if (!query.exec()) {
        qWarning() << "[DB] Failed to load draft:" << query.lastError().text();
        return QString();
    }
    
    if (query.next()) {
        QString draftText = query.value(0).toString();
        qDebug() << "[DB] Draft loaded for chat:" << username;
        return draftText;
    }
    
    // Черновика нет — возвращаем пустую строку
    return QString();
}

void DatabaseService::deleteDraft(const QString& username, 
                                  const QString& ownerUsername)
{
    // Если нет соединения — выходим
    if (!isConnected()) {
        return;
    }
    
    QSqlQuery query(m_db);
    // Удаляем черновик для конкретного владельца и чата
    query.prepare(R"(
        DELETE FROM drafts 
        WHERE owner_username = :owner AND chat_username = :chat
    )");
    
    query.bindValue(":owner", ownerUsername);
    query.bindValue(":chat", username);
    
    if (!query.exec()) {
        qWarning() << "[DB] Failed to delete draft:" << query.lastError().text();
    } else {
        qDebug() << "[DB] Draft deleted for chat:" << username;
    }
}


ChatMessage DatabaseService::loadLastMessage(const QString& username, 
                                             const QString& ownerUsername)
{
    ChatMessage message;
    
    // Если нет соединения с БД — возвращаем пустое сообщение
    if (!isConnected()) {
        return message;
    }
    
    QSqlQuery query(m_db);
    // Загружаем последнее сообщение между владельцем и указанным пользователем
    query.prepare(R"(
        SELECT id, from_user, to_user, payload, timestamp, is_outgoing, status, file_id, file_name, file_url
        FROM messages
        WHERE (from_user = :username AND to_user = :owner)
           OR (from_user = :owner AND to_user = :username)
        ORDER BY timestamp DESC
        LIMIT 1
    )");
    
    query.bindValue(":username", username);
    query.bindValue(":owner", ownerUsername);
    
    if (!query.exec()) {
        qWarning() << "[DB] Failed to load last message:" << query.lastError().text();
        return message;
    }
    
    // Если нашлось хотя бы одно сообщение — заполняем структуру ChatMessage
    if (query.next()) {
        message.id          = query.value("id").toLongLong();
        message.fromUser    = query.value("from_user").toString();
        message.toUser      = query.value("to_user").toString();
        message.payload     = query.value("payload").toString();
        message.timestamp   = query.value("timestamp").toString();
        message.isOutgoing  = query.value("is_outgoing").toBool();
        message.fileId      = query.value("file_id").toString();
        message.fileName    = query.value("file_name").toString();
        message.fileUrl     = query.value("file_url").toString();
        message.status      = static_cast<ChatMessage::MessageStatus>(query.value("status").toInt());
    }
    
    return message;
}

void DatabaseService::updateOrAddUnreadCount(const QString& username, int count) {
    QSqlQuery query;
    // UPSERT логика для таблицы unread_counts: добавляем или обновляем значение
    query.prepare(R"(
        INSERT INTO unread_counts (username, count)
        VALUES (:username, :count)
        ON CONFLICT(username) DO UPDATE SET
            count = excluded.count
    )");
    query.bindValue(":username", username);
    query.bindValue(":count", count);

    if (!query.exec()) {
        qWarning() << "[DatabaseService] updateOrAddUnreadCount error:" << query.lastError().text();
    }
}
