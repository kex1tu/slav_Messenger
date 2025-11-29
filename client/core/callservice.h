#ifndef CALLSERVICE_H
#define CALLSERVICE_H

#include <QObject>
#include <QUdpSocket>
#include <QAudioSource>
#include <QAudioSink>
#include <QTimer>
#include <opus.h>
#include <QByteArray>
#include <QMap>
class NetworkService;
class DataService;

/**
 * @brief Сервис управления P2P аудиозвонком с потоковой обработкой аудио
 *
 * Сервис устанавливает, принимает и завершает вызовы, управляет UDP соединением,
 * кодированием Opus и буферизацией аудиоданных. Поддерживает музыкальное тестирование и работу с интервалами частот, а также сбор статистики передачи.
 */
class CallService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Возможные состояния вызова.
     */
    enum CallState {
        Idle,         ///< Нет активного звонка
        Calling,      ///< Исходящий звонок
        Ringing,      ///< Входящий звонок сигнализируется
        Connected     ///< Соединение установлено
    };

    /**
     * @brief Конструктор CallService.
     *
     * Создает сервис и связывает его с сетевыми и пользовательскими компонентами.
     * @param networkService Компонент сетевого взаимодействия
     * @param dataService Компонент для доступа к данным
     * @param parent Родительский QObject
     */
    explicit CallService(NetworkService* networkService, DataService* dataService, QObject* parent = nullptr);

    /**
     * @brief Деструктор.
     */
    ~CallService();

    /**
     * @brief Инициировать исходящий вызов.
     * @param toUser Имя пользователя-получателя
     */
    void initiateCall(const QString& toUser);
    /** @brief Принять входящий вызов. */
    void acceptCall();
    /** @brief Отклонить входящий вызов. */
    void rejectCall();
    /** @brief Завершить активный вызов. */
    void endCall();
    /**
     * @brief Обработка входящего запроса на вызов.
     * @param from Отправитель вызова
     * @param callId Идентификатор вызова
     * @param ip IP адрес собеседника
     * @param port Порт для UDP
     */
    void onCallRequestReceived(const QString& from, const QString& callId, const QString& ip, quint16 port);
    /**
     * @brief Подтверждение принятия вызова.
     * @param ip IP адрес
     * @param port UDP порт
     */
    void onCallAcceptedReceived(const QString& ip, quint16 port);
    /** @brief Вызов отклонён собеседником. */
    void onCallRejectedReceived();
    /** @brief Сообщение о завершении вызова. */
    void onCallEndedReceived();

    /** @brief Проиграть музыкальную гамму. */
    void playMusicalScale();
    /** @brief Протестировать диапазон частот воспроизведения/записи. */
    void testFrequencyRange();
    /**
     * @brief Передать синусоидальный тон на заданной частоте и длительности.
     * @param frequencyHz Частота сигнала (Гц)
     * @param durationMs Длительность сигнала (мс)
     */
    void sendSineWaveTone(int frequencyHz, int durationMs);
    /** @brief Прервать исходящий вызов до соединения. */
    void cancelOutgoingCall();
    /** @brief Сбросить текущие параметры и состояние вызова. */
    void resetCallData();

signals:
    /** @brief Показать UI для входящего звонка. @param from Собеседник */
    void incomingCallShow(const QString& from);
    /** @brief Показать UI для исходящего звонка. */
    void outgoingCallShow();
    /** @brief Сигнал: соединение установлено. */
    void callConnected();
    /** @brief Сигнал: вызов завершён. */
    void callEnded();
    /** @brief Ошибка вызова. @param error Описание ошибки */
    void callError(const QString& error);
    /** @brief Обновление длительности звонка (сек). @param seconds Длительность */
    void callDurationUpdated(int seconds);

private slots:
    /** @brief Готовность входных аудиоданных к обработке и отправке. */
    void onAudioInputReady();
    /** @brief Приём и обработка поступающих аудиоданных. */
    void onAudioDataReceived();
    /** @brief Таймер контроля событий вызова (длительность и переотправка). */
    void onCallTimerTimeout();
    /** @brief Обработка пакетов в jitter buffer. */
    void processJitterBuffer();

private:
    /** @brief Инициализация UDP сокета для обмена медиа. */
    void initializeUdpSocket();
    /** @brief Запуск аудиопотока. */
    void startAudioStreaming();
    /** @brief Остановка аудиопотока. */
    void stopAudioStreaming();
    /** @brief Отправка инициирующего запроса вызова. */
    void sendCallRequest(const QString& toUser);
    /** @brief Отправить подтверждение принятия вызова. */
    void sendCallAccepted();
    /** @brief Отправка отказа на вызов. */
    void sendCallRejected();
    /** @brief Сообщить о завершении вызова. */
    void sendCallEnd();

    NetworkService* m_networkService; /*!< Сервис сетевого взаимодействия */
    DataService* m_dataService;       /*!< Сервис работы с данными */
    CallState m_callState;            /*!< Текущее состояние вызова */
    QString m_currentCallId;          /*!< Идентификатор активного звонка */
    QString m_remoteUsername;         /*!< Имя пользователя-собеседника */
    QString m_remoteIp;               /*!< Строка IP для peer */
    quint16 m_remotePort;             /*!< UDP порт peer */
    QHostAddress m_remoteAddress;     /*!< IP адрес peer в виде QHostAddress */
    QUdpSocket* m_udpSocket;          /*!< UDP сокет передачи аудио данных */
    quint16 m_localPort;              /*!< Локальный UDP порт */
    QAudioSource* m_audioSource;      /*!< Захват устройства (микрофон) */
    QAudioSink* m_audioSink;          /*!< Воспроизведение (динамик) */
    QIODevice* m_audioInput;          /*!< Входной поток аудио */
    QIODevice* m_audioOutput;         /*!< Выходной поток аудио */
    QTimer* m_callTimer;              /*!< Таймер длительности вызова */
    int m_callDuration;               /*!< Текущая длительность вызова */
    qint64 m_audioBytesSent;          /*!< Отправлено байт аудиоданных */
    qint64 m_audioPacketsSent;        /*!< Отправлено аудиопакетов */
    qint64 m_audioBytesReceived;      /*!< Принято байт аудиоданных */
    qint64 m_audioPacketsReceived;    /*!< Принято аудиопакетов */
    QString m_myIp;                   /*!< IP адрес этого клиента */
    OpusEncoder* m_opusEncoder;       /*!< Экземпляр Opus-энкодера (кодирование PCM->Opus) */
    OpusDecoder* m_opusDecoder;       /*!< Экземпляр Opus-декодера (Opus->PCM) */
    quint64 m_lastSeqNum = 0;         /*!< Последний обработанный seq номер */
    quint64 m_sequenceNumber = 0;     /*!< Следующий seq номер для передачи */
    QMap<quint64, QByteArray> m_jitterBuffer; /*!< Jitter buffer для сглаживания сети */
    quint64 m_nextSeqToPlay = 0;              /*!< seq номер следующего буфера на воспроизведение */
    QTimer* m_jitterTimer = nullptr;          /*!< Таймер обслуживания jitter buffer */
};

#endif
