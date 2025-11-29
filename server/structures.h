#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <QString>
#include <QList>
#include <QMetaType>
#include <QObject> // Необходимо для QObject* в CallInfo

/**
 * @struct Chat
 * @brief Структура метаданных чата (элемент списка контактов).
 *
 * @details Хранит сводную информацию о диалоге, необходимую для отображения в списке чатов:
 *          имя собеседника, текст и время последнего сообщения, счетчики непрочитанных,
 *          а также флаги состояния (закреплен, без звука и т.д.).
 */
struct Chat {
    int id;                          ///< Уникальный ID чата в локальной базе данных.
    QString username;                ///< Уникальное имя пользователя (собеседника).
    QString displayName;             ///< Отображаемое имя (может отличаться от username, если задано в контактах).
    QString lastMessagePayload;      ///< Текст (или описание) последнего сообщения для превью.
    QString lastMessageTimestamp;    ///< Время последнего сообщения в формате ISO 8601.
    qint64 lastMessageId = 0;        ///< ID последнего сообщения (для синхронизации).
    int unreadCount = 0;             ///< Количество непрочитанных сообщений в этом чате.
    bool isPinned = false;           ///< Флаг: чат закреплен в верху списка.
    bool isArchived = false;         ///< Флаг: чат находится в архиве.
    bool isLastMessageOutgoing = false; ///< Флаг: последнее сообщение было отправлено текущим пользователем.
    QString draftText;               ///< Сохраненный черновик неотправленного сообщения.
    bool isMuted = false;            ///< Флаг: уведомления для этого чата отключены.

    /**
     * @brief Конструктор по умолчанию.
     * @details Инициализирует поля пустыми значениями.
     */
    Chat() {
        username = "";
        displayName = "";
        lastMessagePayload = "";
        lastMessageTimestamp = "";
        lastMessageId = 0;
        unreadCount = 0;
        isPinned = false;
        isArchived = false;
        isLastMessageOutgoing = false;
        draftText = "";
        isMuted = false;
    }

    /**
     * @brief Конструктор с инициализацией по username.
     * @param user Имя пользователя (собеседника).
     */
    explicit Chat(const QString& user)
        : username(user) {
        displayName = "";
        lastMessagePayload = "";
        lastMessageTimestamp = "";
        lastMessageId = 0;
        unreadCount = 0;
        isPinned = false;
        isArchived = false;
        isLastMessageOutgoing = false;
        draftText = "";
        isMuted = false;
    }

    /**
     * @brief Оператор сравнения для сортировки списка чатов.
     * @details Логика сортировки:
     *          1. Закрепленные чаты всегда выше незакрепленных.
     *          2. Если статус закрепления совпадает, сортировка идет по времени последнего сообщения (новые выше).
     * @param other Другой объект Chat для сравнения.
     * @return `true`, если текущий чат должен находиться выше в списке, чем `other`.
     */
    bool operator<(const Chat& other) const {
        // Сначала проверяем закрепление
        if (isPinned != other.isPinned) {
            return isPinned > other.isPinned;  // Закреплённые (true) больше незакреплённых (false)
        }
        
        // Затем сравниваем по времени (лексикографическое сравнение строк ISO 8601 корректно)
        return lastMessageTimestamp > other.lastMessageTimestamp;
    }

    /**
     * @brief Конструктор копирования (по умолчанию).
     */
    Chat(const Chat& other) = default;

    /**
     * @brief Оператор присваивания (по умолчанию).
     */
    Chat& operator=(const Chat& other) = default;
};

// Регистрация типа Chat для использования в QVariant и сигналах/слотах Qt
Q_DECLARE_METATYPE(Chat)

/**
 * @struct User
 * @brief Структура, описывающая профиль пользователя мессенджера.
 */
struct User {
    qint64 id = 0;                   ///< Уникальный числовой идентификатор пользователя.
    QString username;                ///< Логин пользователя (уникальный, используется для поиска).
    QString displayName;             ///< Публичное отображаемое имя (никнейм).
    QString lastSeen;                ///< Время последней активности (ISO 8601) или статус "hidden".
    QString avatarUrl;               ///< URL или путь к файлу аватара.
    QString statusMessage;           ///< Текстовый статус ("О себе").

    bool isTyping = false;           ///< Флаг: пользователя в данный момент печатает сообщение.
    bool isOnline = false;           ///< Флаг: пользователь находится в сети.
};

/**
 * @struct ChatMessage
 * @brief Основная структура сообщения чата.
 * @details Содержит всю информацию о единичном сообщении, включая метаданные, статус доставки и контент.
 */
struct ChatMessage {

    /**
     * @enum MessageType
     * @brief Тип содержимого сообщения.
     */
    enum MessageType {
        Text,    ///< Обычное текстовое сообщение.
        Image,   ///< Сообщение, содержащее изображение.
        File,    ///< Сообщение с прикрепленным файлом.
        Sticker, ///< Стикер или эмодзи (особое отображение).
        System   ///< Служебное сообщение (например, "Пользователь создал группу").
    };

    /**
     * @enum MessageStatus
     * @brief Текущий жизненный цикл сообщения.
     */
    enum MessageStatus {
        Sending,    ///< Сообщение в очереди на отправку или отправляется.
        Sent,       ///< Сообщение успешно ушло на сервер (одна галочка).
        Delivered,  ///< Сообщение доставлено получателю (две серые галочки).
        Read,       ///< Сообщение прочитано получателем (две синие галочки).
        Error       ///< Произошла ошибка при отправке.
    };

    qint64 id = 0;                   ///< Глобальный ID сообщения на сервере (0, если еще не отправлено).
    QString tempId;                  ///< Временный локальный ID (UUID), генерируется клиентом для отслеживания до присвоения серверного ID.
    QString fromUser;                ///< Username отправителя.
    QString toUser;                  ///< Username получателя.
    QString payload;                 ///< Текст сообщения или описание файла.
    QString timestamp;               ///< Время создания сообщения (ISO 8601).

    bool isEdited = false;           ///< Флаг: сообщение было изменено.
    qint64 replyToId = 0;            ///< ID сообщения, на которое это сообщение является ответом.
    User forwardedFrom;              ///< Информация об авторе оригинала (если сообщение переслано).
    MessageType messageType;         ///< Тип контента (Text, Image и т.д.).
    MessageStatus status;            ///< Текущий статус доставки.
    QString mediaUrl;                ///< Локальный путь или URL для медиа-файлов.
    bool isOutgoing;                 ///< Флаг: сообщение исходящее (от текущего пользователя).
    bool isFailed = false;           ///< Флаг: отправка окончательно не удалась.
    QString fileId;    // <-- новое поле
    QString fileName;  // <-- новое поле
    QString fileUrl;
};

/**
 * @brief Оператор сравнения сообщений.
 * @details Сравнение производится только по уникальному ID.
 */
inline bool operator==(const ChatMessage& lhs, const ChatMessage& rhs) {
    return lhs.id == rhs.id;
}

/**
 * @struct ChatCache
 * @brief Структура для кэширования истории сообщений в памяти.
 * @details Используется виджетом чата для хранения подгруженных сообщений и управления пагинацией.
 */
struct ChatCache {
    QList<ChatMessage> messages;      ///< Список загруженных сообщений (обычно отсортирован по времени).
    qint64 oldestMessageId = -1;      ///< ID самого старого сообщения в текущем кэше (курсор для пагинации вверх).
    bool allMessagesLoaded = false;   ///< Флаг: достигнуто начало истории (больше сообщений нет).
};

/**
 * @struct CallItem
 * @brief Структура записи в журнале звонков.
 * @details Используется для отображения истории звонков на клиенте.
 */
struct CallItem {
    QString callId;          ///< Уникальный идентификатор сессии звонка (UUID).
    QString caller;          ///< Инициатор звонка (username).
    QString callee;          ///< Принимающая сторона (username).
    QString status;          ///< Итог звонка ("completed", "missed", "rejected", "busy").
    QString callType;        ///< Направление относительно пользователя ("incoming" или "outgoing").
    QString startTime;       ///< Время начала соединения.
    QString endTime;         ///< Время завершения.
    int durationSeconds;     ///< Длительность разговора в секундах (0, если не состоялся).
};

/**
 * @struct CallInfo
 * @brief Серверная структура для отслеживания активной сессии звонка.
 * @details Хранит состояние сигнализации между двумя абонентами. Эта структура
 *          используется только на стороне сервера в `m_activeCalls`.
 */
struct CallInfo {
    QString callId;          ///< Уникальный ID звонка (совпадает с тем, что в CallItem).
    QString from;            ///< Username инициатора звонка.
    QString to;              ///< Username получателя звонка.
    
    /**
     * @brief Указатель на сокет инициатора.
     * @warning Используется для быстрой пересылки SDP/ICE пакетов. Не владеет объектом (weak reference).
     */
    QObject* fromSocket = nullptr; 
    
    /**
     * @brief Указатель на сокет получателя.
     * @warning Не владеет объектом. При отключении клиента указатель может стать невалидным, требуется проверка.
     */
    QObject* toSocket = nullptr;   
    
    quint16 callerPort = 0;  ///< Внешний порт инициатора (для помощи в NAT traversal / логирования).
    QString callerIp;        ///< Внешний IP инициатора.
};

#endif // STRUCTURES_H
