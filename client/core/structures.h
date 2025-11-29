#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <QString>
#include <QList>
#include <QMetaType>
#include <QObject>

/**
 * @brief Структура, представляющая чат с пользователем.
 *
 * Содержит метаданные чата, такие как последнее сообщение, счетчик непрочитанных,
 * настройки (закрепление, архив, заглушение) и черновики.
 * Используется для отображения списка чатов в UI.
 */
struct Chat {
    int id;                           ///< Уникальный идентификатор чата в БД
    QString username;                 ///< Имя пользователя-собеседника (ключ чата)
    QString displayName;              ///< Отображаемое имя собеседника
    QString lastMessagePayload;       ///< Текст последнего сообщения (для превью)
    QString lastMessageTimestamp;     ///< Временная метка последнего сообщения
    qint64 lastMessageId = 0;         ///< ID последнего сообщения
    int unreadCount = 0;              ///< Количество непрочитанных сообщений
    bool isPinned = false;            ///< Флаг: чат закреплен в верху списка
    bool isArchived = false;          ///< Флаг: чат находится в архиве
    bool isLastMessageOutgoing = false;  ///< Флаг: последнее сообщение отправлено нами
    QString draftText;                ///< Сохраненный черновик неотправленного сообщения
    bool isMuted = false;             ///< Флаг: уведомления для чата отключены

    /** @brief Конструктор по умолчанию. Инициализирует поля пустыми значениями. */
    Chat(){
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
     * @brief Создает чат с указанным username.
     * @param user Имя пользователя чата.
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
     *
     * Приоритет сортировки:
     * 1. Закрепленные чаты всегда выше.
     * 2. Более новые сообщения выше (по timestamp).
     */
    bool operator<(const Chat& other) const {
        if (isPinned != other.isPinned) {
            return isPinned > other.isPinned;
        }
        return lastMessageTimestamp > other.lastMessageTimestamp;
    }

    Chat(const Chat& other) = default;
    Chat& operator=(const Chat& other) = default;
};

Q_DECLARE_METATYPE(Chat)

/**
 * @brief Структура профиля пользователя.
 */
struct User {
    qint64 id = 0;                    ///< Уникальный ID пользователя
    QString username;                 ///< Логин (уникальный идентификатор)
    QString displayName;              ///< Публичное имя
    QString lastSeen;                 ///< Время последней активности
    QString avatarUrl;                ///< URL или путь к аватару
    QString statusMessage;            ///< Статус ("О себе")

    bool isTyping = false;            ///< Runtime-флаг: пользователь печатает
    bool isOnline = false;            ///< Runtime-флаг: пользователь в сети
};
Q_DECLARE_METATYPE(User)

/**
 * @brief Структура единичного сообщения в чате.
 */
struct ChatMessage {

    /** @brief Тип содержимого сообщения. */
    enum MessageType {
        Text,      ///< Обычный текст
        Image,     ///< Изображение
        File,      ///< Файл для скачивания
        Sticker,   ///< Стикер
        System     ///< Системное уведомление
    };

    /** @brief Статус доставки сообщения. */
    enum MessageStatus {
        Sending,     ///< Отправляется (еще не на сервере)
        Sent,        ///< Отправлено на сервер (одна галочка)
        Delivered,   ///< Доставлено получателю (две галочки)
        Read,        ///< Прочитано получателем (две синие галочки)
        Error        ///< Ошибка отправки
    };

    qint64 id = 0;                    ///< ID сообщения на сервере
    QString tempId;                   ///< Временный GUID для локального трекинга до подтверждения сервером
    QString fromUser;                 ///< Имя отправителя
    QString toUser;                   ///< Имя получателя
    QString payload;                  ///< Тело сообщения (текст или описание)
    QString timestamp;                ///< Время создания (ISO 8601)

    bool isEdited = false;            ///< Флаг: сообщение было изменено
    qint64 replyToId = 0;             ///< ID сообщения, на которое это является ответом
    User forwardedFrom;               ///< Информация об авторе пересылаемого сообщения (если есть)
    MessageType messageType;          ///< Тип контента
    MessageStatus status;             ///< Текущий статус доставки
    QString mediaUrl;                 ///< Ссылка на медиа (для картинок/файлов)
    bool isOutgoing;                  ///< true, если сообщение отправлено текущим пользователем
    bool isFailed = false;            ///< Флаг ошибки (дублирует статус Error для удобства)
    QString fileId;                   ///< ID файла на сервере (если есть вложение)
    QString fileName;                 ///< Оригинальное имя файла
    QString fileUrl;                  ///< Полный URL файла
};

/**
 * @brief Оператор сравнения сообщений по ID.
 */
inline bool operator==(const ChatMessage& lhs, const ChatMessage& rhs) {
    return lhs.id == rhs.id;
}

/**
 * @brief Кеш истории сообщений для одного чата.
 *
 * Используется для хранения загруженных сообщений в оперативной памяти
 * для быстрого доступа без постоянных запросов к БД.
 */
struct ChatCache {
    QList<ChatMessage> messages;       ///< Список загруженных сообщений
    qint64 oldestMessageId = -1;       ///< ID самого старого сообщения в кеше (для пагинации)
    bool allMessagesLoaded = false;    ///< Флаг: вся история чата загружена полностью
};

/**
 * @brief Информация о совершенном звонке.
 */
struct CallItem {
    QString callId;            ///< Уникальный ID звонка
    QString caller;            ///< Инициатор звонка
    QString callee;            ///< Получатель звонка
    QString status;            ///< Статус завершения (принят/отклонен/пропущен)
    QString callType;          ///< Тип звонка (аудио/видео)
    QString startTime;         ///< Время начала
    QString endTime;           ///< Время окончания
    int durationSeconds;       ///< Длительность разговора в секундах
};

/**
 * @brief Информация о загруженном файле для кеширования.
 */
struct CachedFile {
    QString fileId;    ///< ID файла
    QString fileName;  ///< Имя файла
    QString fileUrl;   ///< URL для доступа
};

#endif // STRUCTURES_H
