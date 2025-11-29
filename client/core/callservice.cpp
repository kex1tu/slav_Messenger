#include "callservice.h"
#include "networkservice.h"
#include "dataservice.h"
#include <QMediaDevices>
#include <QAudioFormat>
#include <QJsonObject>
#include <QJsonDocument>
#include <QThread>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <opus.h>


const int SAMPLE_RATE = 16000;
const int CHANNELS = 1;
const int FRAME_SIZE = 320;
quint64 m_sequenceNumber = 0;


CallService::CallService(NetworkService* networkService, DataService* dataService, QObject* parent)
    : QObject(parent),
      m_networkService(networkService),
      m_dataService(dataService),
      m_callState(Idle),
      m_currentCallId(QString()),
      m_remoteUsername(QString()),
      m_remoteIp(QString()),
      m_remotePort(0),
      m_remoteAddress(QHostAddress()),
      m_udpSocket(nullptr),
      m_localPort(0),
      m_audioSource(nullptr),
      m_audioSink(nullptr),
      m_audioInput(nullptr),
      m_audioOutput(nullptr),
      m_callTimer(new QTimer(this)),
      m_callDuration(0),
      m_audioBytesSent(0),
      m_audioPacketsSent(0),
      m_audioBytesReceived(0),
      m_audioPacketsReceived(0),
      m_opusEncoder(nullptr),
      m_opusDecoder(nullptr)
{
    // Логируем текущую рабочую директорию (для отладки путей и загрузки файлов)
    qDebug() << "[CallService] " << "CurrentPath:" << QDir::currentPath();

    // Загружаем настройки приложения из config.ini (например, IP-адрес для сети)
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    // Отладочная информация: путь и перечень ключей из INI файла
    qDebug() << "[CallService] " << "Current path:" << QDir::currentPath();
    qDebug() << "[CallService] " << "INI keys =" << settings.allKeys();

    // Инициализация IP-адреса устройства (используется для UDP bind)
    m_myIp = settings.value("network/myIp", "127.0.0.1").toString();
    qDebug() << "[CallService] " << "[Config] m_myIp =" << m_myIp;

    // Таймер для отслеживания длительности звонка, обновляет счётчик каждую секунду
    connect(m_callTimer, &QTimer::timeout, this, &CallService::onCallTimerTimeout);

    // Таймер джиттер-буфера с периодом 20мс: регулярно вызывается процесс обработки аудио загрузки
    m_jitterTimer = new QTimer(this);
    connect(m_jitterTimer, &QTimer::timeout, this, &CallService::processJitterBuffer);
    m_jitterTimer->start(20);

    // Инициализация UDP сокета для передачи и приёма аудио данных
    initializeUdpSocket();
}

CallService::~CallService()
{
    // Остановка передачи/приёма аудио, освобождение ресурсов
    stopAudioStreaming();

    // Корректное удаление UDP сокета, если был создан
    if (m_udpSocket) {
        m_udpSocket->deleteLater();
    }
}

void CallService::initializeUdpSocket()
{
    // Если сокет уже создан — корректно очищаем ресурсы и закрываем порт
    if (m_udpSocket) {
        disconnect(m_udpSocket, nullptr, this, nullptr);    // Отключаем все сигналы от старого сокета
        m_udpSocket->close();                              // Закрываем старый сокет
        m_udpSocket->deleteLater();                        // Удаляем объект асинхронно
    }

    // Создаем новый UDP сокет как дочерний объект CallService
    m_udpSocket = new QUdpSocket(this);

    // Пробуем привязать на указанный IP (из конфига), порт 0 — автоназначение от ОС
    // Флаги позволяют разделять адрес с другими приложениями, удобно для теста на одной машине
    bool ok = m_udpSocket->bind((QHostAddress)m_myIp, 0, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

    // Если не удалось забиндить — сигнализируем об ошибке и выходим
    if (!ok) {
        qWarning() << "[UDP] ❌ Failed to bind UDP socket";
        emit callError("UDP binding failed");
        return;
    }

    // Запоминаем локальный порт, назначенный ОС после bind (он потребуется для сигнализации peer'у)
    m_localPort = m_udpSocket->localPort();
    qDebug() << "[CallService] " << "[UDP] ✅ Socket bound to port:" << m_localPort;

    // Подключаем обработчик получения UDP данных (на каждое событие readyRead)
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &CallService::onAudioDataReceived);
    qDebug() << "[CallService] " << "[UDP] ✅ readyRead connected";
}

void CallService::initiateCall(const QString& toUser)
{
    // Логируем попытку начать звонок
    qDebug() << "[CallService] " << "[CALL] >>> INITIATING CALL TO:" << toUser;

    // Не даем инициировать звонок, если уже есть текущий (можно расширить на очередь)
    if (m_callState != Idle) {
        qWarning() << "[CALL] ❌ Already in a call";
        emit callError("Already in a call");
        return;
    }

    // Сохраняем параметры звонка для логики сигнализации и контроля
    m_remoteUsername = toUser;
    m_currentCallId = QUuid::createUuid().toString(); // Генерируем новый уникальный идентификатор звонка
    m_callState = Calling;                            // Переводим стейт в "звоню"
    m_callDuration = 0;                               // Обнуляем счетчик времени звонка

    // Отправляем сигнал (JSON) peer'у через сетевой сервис
    sendCallRequest(toUser);

    // Показываем UI для исходящего звонка
    emit outgoingCallShow();

    qDebug() << "[CallService] " << "[CALL] ✅ Outgoing call initiated";
}

void CallService::sendCallRequest(const QString& toUser)
{
    // Получаем имя текущего пользователя из DataService (если сервис доступен)
    QString fromUser = m_dataService ? m_dataService->getCurrentUser()->username : QString();

    // Проверяем, что пользователь авторизован — без этого нельзя инициировать звонок
    if (fromUser.isEmpty()) {
        qWarning() << "[CALL] ❌ User not logged in";
        emit callError("Not logged in");
        return;
    }

    // Формируем JSON запрос для сигнализации звонка
    // Важно: передаем свой IP и UDP порт, чтобы собеседник знал, куда отправлять аудио
    QJsonObject request;
    request["type"] = "call_request";
    request["from"] = fromUser;
    request["to"] = toUser;
    request["call_id"] = m_currentCallId;      // Уникальный идентификатор звонка
    request["caller_ip"] = m_myIp;             // Наш IP для UDP соединения
    request["caller_port"] = (int)m_localPort; // Наш UDP порт для приема аудио

    // Отправляем сигнальное сообщение через TCP канал (WebSocket/TCP)
    m_networkService->sendJson(request);

    qDebug() << "[CallService] " << "[CALL] ✅ Call request sent with port:" << m_localPort;
}

void CallService::onCallRequestReceived(const QString& from, const QString& callId,
                                        const QString& ip, quint16 port)
{
    // Логируем информацию о входящем звонке
    qDebug() << "[CallService] " << "[CALL] <<< INCOMING CALL FROM:" << from << "IP:" << ip << "PORT:" << port;

    // Защита от входящих звонков, если мы уже заняты (можно добавить логику "занято")
    if (m_callState != Idle) {
        qWarning() << "[CALL] ❌ Not in Idle state";
        // TODO: Отправить сообщение "busy" вызывающей стороне
        return;
    }

    // Сохраняем данные о вызывающем абоненте для дальнейшей коммуникации
    m_remoteUsername = from;
    m_currentCallId = callId;
    m_remoteIp = ip;
    m_remotePort = port;
    m_remoteAddress = QHostAddress(ip);  // Парсим IP в объект QHostAddress для UDP
    qDebug() << "[CallService] " << "m_remoteAddress" << m_remoteAddress;
    m_callState = Ringing;               // Переводим состояние в "звонит"

    // Сигнализируем UI о входящем звонке (показываем окно с кнопками "Принять/Отклонить")
    emit incomingCallShow(from);

    qDebug() << "[CallService] " << "[CALL] ✅ Incoming call signal emitted";
}

void CallService::acceptCall()
{
    // Логируем принятие входящего звонка
    qDebug() << "[CallService] " << "[CALL] >>> ACCEPTING CALL";

    // Проверяем, что мы действительно в состоянии "звонок входящий"
    if (m_callState != Ringing) {
        qWarning() << "[CALL] ❌ Not in Ringing state";
        return;
    }

    // Переводим состояние в "активный звонок"
    m_callState = Connected;

    // Отправляем ответ вызывающему абоненту с нашим IP и UDP портом
    sendCallAccepted();

    // Запускаем захват микрофона и воспроизведение звука
    startAudioStreaming();

    // Запускаем таймер для отсчета длительности разговора (обновляется каждую секунду)
    m_callTimer->start(1000);

    // Сигнализируем UI о том, что звонок установлен (скрыть окно вызова, показать окно разговора)
    emit callConnected();
    qDebug() << "[CallService] " << "[CALL] ✅ Call connected";
}

void CallService::sendCallAccepted()
{
    // Формируем JSON ответ на принятие звонка
    // Важно: передаем свой IP и UDP порт, чтобы вызывающий абонент знал, куда отправлять аудио
    QJsonObject response;
    response["type"] = "call_accepted";
    response["from"] = m_dataService ? m_dataService->getCurrentUser()->username : "";
    response["call_id"] = m_currentCallId;
    response["callee_ip"] = m_myIp;             // Наш IP для UDP соединения
    response["callee_port"] = (int)m_localPort; // Наш UDP порт для приема аудио

    // Отправляем сигнальное сообщение через TCP канал
    m_networkService->sendJson(response);

    qDebug() << "[CallService] " << "[CALL] ✅ Call accepted sent with port:" << m_localPort;
}

void CallService::rejectCall()
{
    // Логируем отклонение входящего звонка
    qDebug() << "[CallService] " << "[CALL] >>> REJECTING CALL";

    // Проверяем, что мы в состоянии "звонок входящий"
    if (m_callState != Ringing) {
        qWarning() << "[CALL] ❌ Not in Ringing state";
        return;
    }

    // Завершаем звонок (очищаем данные, останавливаем аудио, отправляем сигнал об окончании)
    endCall();

    // Сигнализируем UI об окончании звонка (закрыть окно вызова)
    emit callEnded();

    qDebug() << "[CallService] " << "[CALL] ✅ Call rejected";
}
void CallService::sendCallRejected()
{
    // Формируем JSON сообщение об отклонении звонка
    QJsonObject response;
    response["type"] = "call_rejected";
    response["call_id"] = m_currentCallId;
    response["to"] = m_dataService ? m_dataService->getCurrentUser()->username : "";

    // Отправляем уведомление вызывающему абоненту через TCP канал
    m_networkService->sendJson(response);
}

void CallService::onCallAcceptedReceived(const QString& ip, quint16 port)
{
    qDebug() << "[CallService] " << "[CALL] <<< CALL ACCEPTED FROM REMOTE USER IP:" << ip << "PORT:" << port;

    // Проверяем, что мы действительно в процессе исходящего звонка
    if (m_callState != Calling) {
        qWarning() << "[CALL] ❌ Not in Calling state";
        return;
    }

    // Сохраняем UDP координаты собеседника (куда слать аудио-пакеты)
    m_remoteIp = ip;
    m_remotePort = port;
    m_remoteAddress = QHostAddress(m_remoteIp);
    m_callState = Connected; // Переводим состояние в "активный звонок"

    // Запускаем захват микрофона и воспроизведение звука
    startAudioStreaming();

    // Запускаем таймер длительности разговора
    m_callTimer->start(1000);

    // Сигнализируем UI о том, что звонок установлен
    emit callConnected();
    qDebug() << "[CallService] " << "[CALL] ✅ Call connected";
}

void CallService::onCallRejectedReceived()
{
    qDebug() << "[CallService] " << "[CALL] <<< CALL REJECTED";

    // Сообщаем UI об отклонении звонка (для показа соответствующего уведомления)
    emit callError("Call rejected");

    // Завершаем звонок и очищаем все данные
    endCall();
}
void CallService::onCallEndedReceived()
{
    qDebug() << "[CallService] " << "[CALL] <<< CALL ENDED BY REMOTE USER";

    // Собеседник завершил звонок — выполняем локальную очистку
    endCall();
}

void CallService::endCall()
{
    // Если уже в состоянии Idle — нечего завершать
    if (m_callState == Idle) return;

    // Переводим состояние в "свободен"
    m_callState = Idle;

    // Останавливаем таймер длительности звонка
    m_callTimer->stop();

    // Отправляем сигнальное сообщение об окончании звонка собеседнику
    sendCallEnd();

    // Останавливаем захват микрофона, воспроизведение и освобождаем аудио ресурсы
    stopAudioStreaming();

    // Сигнализируем UI о завершении звонка (закрыть окно разговора)
    emit callEnded();

    qDebug() << "[CallService] " << "[CALL] ✅ Call ended";
}

void CallService::cancelOutgoingCall()
{
    // Если мы в процессе исходящего звонка (еще не принят) — отменяем
    if (m_callState == Calling) {
        // Отправляем сигнал об отмене вызова
        sendCallEnd();
    }
}

void CallService::sendCallEnd()
{
    // Формируем JSON сообщение о завершении звонка
    QJsonObject msg;
    msg["type"] = "call_end";
    msg["from"] = m_dataService ? m_dataService->getCurrentUser()->username : "";
    msg["call_id"] = m_currentCallId;

    // Отправляем уведомление собеседнику через TCP канал
    m_networkService->sendJson(msg);
}

void CallService::onCallTimerTimeout()
{
    // Увеличиваем счетчик длительности звонка на 1 секунду
    m_callDuration++;

    // Уведомляем UI об обновлении времени разговора (для отображения таймера)
    emit callDurationUpdated(m_callDuration);
}

void CallService::startAudioStreaming()
{
    qDebug() << "[CallService] " << "[AUDIO] >>> STARTING AUDIO STREAMING";
    
    // Сброс всех счетчиков и буферов для нового звонка
    m_lastSeqNum = 0;
    m_sequenceNumber = 0;
    m_jitterBuffer.clear();
    m_nextSeqToPlay = 0;

    // Создаем кодек Opus для сжатия/декомпрессии аудио
    int error;
    m_opusEncoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &error);
    m_opusDecoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &error);
    if(error != OPUS_OK){
        qWarning() << "[CALLSERVICE Error with opus";
    }

    // Проверяем готовность к аудио-передаче (нужен UDP порт собеседника и состояние Connected)
    if (m_remotePort == 0 || m_callState != Connected) {
        qWarning() << "[AUDIO] ❌ Not ready - remotePort:" << m_remotePort << "state:" << m_callState;
        return;
    }

    // Получаем список доступных аудио устройств для отладки
    QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();

    qDebug() << "[CallService] " << "[CALL] === AVAILABLE INPUT DEVICES ===";
    for (int i = 0; i < inputs.size(); i++) {
        qDebug() << "[CallService] " << QString("[CALL] [%1]").arg(i) << inputs.at(i).description();
    }
    qDebug() << "[CallService] " << "[CALL] === END OF LIST ===";

    qDebug() << "[CallService] " << "[CALL] === AVAILABLE OUTPUT DEVICES ===";
    for (int i = 0; i < outputs.size(); i++) {
        qDebug() << "[CallService] " << QString("[CALL] [%1]").arg(i) << outputs.at(i).description();
    }
    qDebug() << "[CallService] " << "[CALL] === END OF LIST ===";

    // Проверяем наличие аудио устройств
    if (inputs.isEmpty() || outputs.isEmpty()) {
        qWarning() << "[AUDIO] ❌ No audio devices found";
        emit callError("No audio devices");
        return;
    }

    // Используем устройства по умолчанию
    QAudioDevice defaultInputDevice = QMediaDevices::defaultAudioInput();
    QAudioDevice defaultOutputDevice = QMediaDevices::defaultAudioOutput();

    // Настраиваем формат аудио: 16кГц моно, 16-бит signed int (совместимо с Opus)
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    qDebug() << format.sampleRate()  << format.sampleFormat();

    // Инициализируем источник звука (микрофон)
    m_audioSource = new QAudioSource(defaultInputDevice, format, this);

    // Запускаем захват звука и получаем QIODevice для чтения
    m_audioInput = m_audioSource->start();

    // Проверяем успешность запуска микрофона
    if (!m_audioInput) {
        qWarning() << "[AUDIO] ❌ Failed to start audio input";
        delete m_audioSource;
        m_audioSource = nullptr;
        return;
    }

    // Подключаем обработчик для чтения данных с микрофона (срабатывает при готовности данных)
    connect(m_audioInput, &QIODevice::readyRead, this, &CallService::onAudioInputReady);

    // Инициализируем приемник звука (динамики/наушники)
    m_audioSink = new QAudioSink(defaultOutputDevice, format, this);
    m_audioSink->setVolume(1.0); // Устанавливаем громкость на максимум
    m_audioOutput = m_audioSink->start();

    // Проверяем успешность запуска воспроизведения
    if (!m_audioOutput) {
        qWarning() << "[AUDIO] ❌ Failed to start audio output";
        delete m_audioSink;
        m_audioSink = nullptr;

        // Если выход не запустился — останавливаем и вход
        stopAudioStreaming();
        return;
    }

    qDebug() << "[CallService] " << "[AUDIO] ✅ AUDIO STREAMING STARTED";
    qDebug() << "[CallService] " << "[AUDIO] Remote:" << m_remoteAddress.toString() << ":" << m_remotePort;
}

void CallService::stopAudioStreaming()
{
    qDebug() << "[CallService] " << "[AUDIO] Stopping audio streaming...";

    // Останавливаем и очищаем источник звука (микрофон)
    if (m_audioInput) {
        disconnect(m_audioInput, nullptr, this, nullptr);  // Отключаем все сигналы от QIODevice
        delete m_audioSource;                             // Удаляем QAudioSource (автоматически останавливает захват)
        m_audioSource = nullptr;                          
        m_audioInput = nullptr;
    }

    // Останавливаем и очищаем приемник звука (динамики)
    if (m_audioOutput) {
        disconnect(m_audioOutput, nullptr, this, nullptr);  // Отключаем все сигналы от QIODevice
        delete m_audioSink;                                 // Удаляем QAudioSink (автоматически останавливает воспроизведение)
        m_audioSink = nullptr;
        m_audioOutput = nullptr;
    }

    qDebug() << "[CallService] " << "[AUDIO] ✅ Audio streaming stopped";
}

void CallService::onAudioInputReady()
{
    // Проверяем готовность к отправке: есть микрофон, известен порт собеседника, звонок активен
    if (!m_audioInput || m_remotePort == 0 || m_callState != Connected) {
        qDebug() << "[CallService] " << "ERROR m_audioInput is nullptr orm_remotePort == 0 or m_callState != Connected ";
        return;
    }

    // Буфер для накопления PCM данных между вызовами
    // static сохраняет данные между вызовами функции (остатки незавершенного фрейма)
    static QByteArray audioBuffer;

    // Читаем все доступные данные с микрофона и добавляем в буфер
    audioBuffer.append(m_audioInput->readAll());
    
    // Обрабатываем полные фреймы (320 сэмплов * 2 байта = 640 байт на фрейм)
    while (audioBuffer.size() >= FRAME_SIZE * sizeof(short)) {
        // Извлекаем один фрейм из буфера
        QByteArray pcmData = audioBuffer.left(FRAME_SIZE * sizeof(short));
        audioBuffer.remove(0, FRAME_SIZE * sizeof(short));

        // Приводим байты к массиву 16-битных сэмплов
        short* pcm = reinterpret_cast<short*>(pcmData.data());
        unsigned char opusPacket[4000]; // Буфер для сжатых Opus данных

        // Кодируем PCM в Opus (сжатие аудио)
        int opusLen = opus_encode(m_opusEncoder, pcm, FRAME_SIZE, opusPacket, sizeof(opusPacket));
        if (opusLen > 0) {
            // Формируем UDP пакет: номер последовательности + сжатые аудио данные
            QByteArray packet;
            QDataStream stream(&packet, QIODevice::WriteOnly);
            stream << m_sequenceNumber;  // Записываем номер пакета (для джиттер-буфера на приемнике)
            packet.append(reinterpret_cast<char*>(opusPacket), opusLen);
            m_sequenceNumber++;
            m_audioBytesSent += packet.size();
            
            // Отправляем пакет по UDP на адрес и порт собеседника
            qint64 sent = m_udpSocket->writeDatagram(packet, QHostAddress(m_remoteIp), m_remotePort);
            ++m_audioPacketsSent;
            
            if (sent < 0) {
                qWarning() << "UDP write failure:" << m_udpSocket->errorString();
            }
            
            // Периодически логируем статистику отправки (каждые 50 пакетов)
            if (m_audioPacketsSent % 50 == 0) {
                qDebug() << "[CallService] " << "[AUDIO] Sent" << m_audioPacketsSent << "packets"
                         << "(" << m_audioBytesSent / 1024 << "KB) from" << m_udpSocket->localPort() << " to " << m_remotePort;
            }
        } else {
            qWarning() << "OPUS encode Error";
        }
    }
}

void CallService::onAudioDataReceived()
{
    // Обрабатываем все UDP дейтаграммы, находящиеся в очереди приема
    while (m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        // Читаем одну дейтаграмму (один аудио пакет)
        m_udpSocket->readDatagram(datagram.data(), datagram.size(),
                                  &senderAddress, &senderPort);

        // Обновляем статистику приема
        m_audioBytesReceived += datagram.size();
        m_audioPacketsReceived++;

        // Проверяем готовность декодера
        if (!m_opusDecoder)
            return;

        // Проверяем готовность устройства воспроизведения
        if (!m_audioOutput)
            return;

        // Разбираем пакет: первые 8 байт — номер последовательности, остальное — Opus данные
        QDataStream stream(datagram);
        quint64 seqNum;
        stream >> seqNum;  // Извлекаем номер пакета
        QByteArray opusData = datagram.mid(sizeof(quint64));  // Извлекаем сжатые аудио данные
        
        // Периодически логируем статистику приема (каждые 50 пакетов)
        if (m_audioPacketsReceived % 50 == 0) {
            qDebug() << "[CallService] [AUDIO] Received" << m_audioPacketsReceived << "packets"
                     << "(" << m_audioBytesReceived / 1024 << "KB)";
        }

        // Помещаем пакет в джиттер-буфер (QMap автоматически сортирует по seqNum)
        // Воспроизведение будет позже в processJitterBuffer()
        m_jitterBuffer[seqNum] = opusData;
    }
}

void CallService::processJitterBuffer()
{
    if (!m_opusDecoder || !m_audioOutput)
        return;

    const int FRAME_SIZE = 320;
    short decodedPcm[FRAME_SIZE];

    // Отладочная информация о состоянии буфера
    qDebug() << "[JITTER] Buffer size:" << m_jitterBuffer.size()
             << "NextSeq:" << m_nextSeqToPlay;
    if(m_jitterBuffer.size() == 0){
        return;
    }
    // 1. Режим "догонялки" (catch-up): если буфер переполнен (>3 пакетов), значит отстаем
    // Проигрываем несколько пакетов подряд, чтобы уменьшить задержку
    while (m_jitterBuffer.size() > 3 && m_jitterBuffer.contains(m_nextSeqToPlay)) {
        QByteArray opusData = m_jitterBuffer.take(m_nextSeqToPlay);
        // Декодируем Opus обратно в PCM
        int samples = opus_decode(m_opusDecoder,
                                  reinterpret_cast<unsigned char*>(opusData.data()),
                                  opusData.size(),
                                  decodedPcm,
                                  FRAME_SIZE,
                                  0);
        qDebug() << "[JITTER] Fast play seqNum:" << m_nextSeqToPlay << "samples:" << samples;
        QByteArray play(reinterpret_cast<char*>(decodedPcm), samples * sizeof(short));
        if(m_audioOutput->isWritable()){
            m_audioOutput->write(play);  // Отправляем декодированный звук в динамики
        }

        m_nextSeqToPlay++;
    }

    // 2. Обычный режим: если следующий ожидаемый пакет есть в буфере — проигрываем его
    if (m_jitterBuffer.contains(m_nextSeqToPlay)) {
        QByteArray opusData = m_jitterBuffer.take(m_nextSeqToPlay);
        int samples = opus_decode(m_opusDecoder,
                                  reinterpret_cast<unsigned char*>(opusData.data()),
                                  opusData.size(),
                                  decodedPcm,
                                  FRAME_SIZE,
                                  0);
        qDebug() << "[JITTER] Regular play seqNum:" << m_nextSeqToPlay << "samples:" << samples;
        QByteArray play(reinterpret_cast<char*>(decodedPcm), samples * sizeof(short));
        m_audioOutput->write(play);
    } else {
        // 3. Packet Loss Concealment (PLC): пакет потерян или еще не пришел
        // Opus генерирует заполнитель на основе предыдущих данных (передаем nullptr, 0)
        int samples = opus_decode(m_opusDecoder, nullptr, 0, decodedPcm, FRAME_SIZE, 0);
        qDebug() << "[JITTER] PLC fill for missing seqNum:" << m_nextSeqToPlay << "samples:" << samples;
        QByteArray play(reinterpret_cast<char*>(decodedPcm), FRAME_SIZE * sizeof(short));
        m_audioOutput->write(play);
    }

    // Переходим к следующему ожидаемому пакету
    m_nextSeqToPlay++;
}

void CallService::playMusicalScale()
{
    if (m_callState != Connected) {
        qWarning() << "[MUSIC] ❌ Not connected";
        return;
    }

    qDebug() << "[CallService] " << "\n[MUSIC] 🎵 Playing scale...";

    // Частоты нот музыкальной гаммы (в Герцах) и их названия
    int notes[] = {262, 294, 329, 349, 392};  // До, Ре, Ми, Фа, Соль
    QString names[] = {"ДО", "РЕ", "МИ", "ФА", "СОЛЬ"};

    // Последовательно отправляем тональные сигналы для каждой ноты
    for (int i = 0; i < 5; ++i) {
        sendSineWaveTone(notes[i], 500);     // Генерируем синусоиду длительностью 500мс
        qDebug() << "[CallService] " << "[MUSIC]" << names[i];
        QThread::msleep(600);                // Пауза 600мс между нотами
    }

    qDebug() << "[CallService] " << "[MUSIC] ✅ Scale finished\n";
}

void CallService::testFrequencyRange()
{
    if (m_callState != Connected) {
        qWarning() << "[TEST] ❌ Not connected";
        return;
    }

    qDebug() << "[CallService] " << "\n[TEST] Testing frequency range...";

    // Набор тестовых частот для проверки диапазона передачи/приема
    int testFreqs[] = {200, 440, 880, 1000, 2000, 4000};

    // Последовательно отправляем тональные сигналы каждой частоты
    for (int freq : testFreqs) {
        qDebug() << "[CallService] " << "[TEST]" << freq << "Hz...";
        sendSineWaveTone(freq, 300);         // Генерируем синусоиду длительностью 300мс
        QThread::msleep(400);                // Пауза 400мс между тестами
    }

    qDebug() << "[CallService] " << "[TEST] ✅ Range test finished\n";
}

void CallService::sendSineWaveTone(int frequencyHz, int durationMs)
{
    // Проверяем готовность к отправке тестового сигнала
    if (m_callState != Connected || m_remotePort == 0 || !m_udpSocket || !m_opusEncoder) {
        qWarning() << "[SINE] ❌ Not ready";
        return;
    }

    const int sampleRate = 16000;
    const int frameSize = 320;  // Размер фрейма Opus (20мс при 16кГц)

    // Амплитуда 30% от максимума (чтобы избежать перегрузки/клиппинга)
    const float amplitude = 32767.0f * 0.3f;

    // Вычисляем общее количество сэмплов для заданной длительности
    const int totalSamples = (sampleRate * durationMs) / 1000;

    // Генерируем синусоидальный сигнал заданной частоты
    QVector<qint16> audioData(totalSamples);
    for (int i = 0; i < totalSamples; ++i) {
        float t = (float)i / sampleRate;                // Время в секундах
        float phase = 2.0f * M_PI * frequencyHz * t;    // Фаза синусоиды
        float sampleValue = sin(phase) * amplitude;     // Значение синуса
        audioData[i] = (qint16)sampleValue;             // Преобразование в 16-бит
    }

    // Разбиваем сгенерированный сигнал на фреймы и кодируем в Opus
    for (int offset = 0; offset + frameSize <= totalSamples; offset += frameSize) {
        unsigned char opusFrame[4000];  // Буфер для сжатого Opus фрейма
        
        // Кодируем фрейм PCM данных в Opus
        int opusLen = opus_encode(m_opusEncoder, audioData.data() + offset, frameSize, opusFrame, sizeof(opusFrame));
        if (opusLen > 0) {
            // Формируем UDP пакет с номером последовательности
            QByteArray packet;
            QDataStream stream(&packet, QIODevice::WriteOnly);
            stream << m_sequenceNumber++;  // Добавляем номер пакета

            packet.append(reinterpret_cast<char*>(opusFrame), opusLen);
            m_udpSocket->writeDatagram(packet, m_remoteAddress, m_remotePort);

            // Обновляем статистику отправки
            m_audioBytesSent += packet.size();
            m_audioPacketsSent++;
        }
    }

    qDebug() << "[CallService] [SINE] ✅ Sent sinewave tone at" << frequencyHz << "Hz,"
             << (totalSamples / sampleRate) << "sec in Opus-audio packets";
}

void CallService::resetCallData()
{
    // Завершаем активный звонок (если есть), останавливаем аудио и отправляем сигнал завершения
    endCall();
    
    // Сбрасываем счетчики последовательности пакетов и очищаем джиттер-буфер
    m_lastSeqNum = 0;
    m_sequenceNumber = 0;
    m_nextSeqToPlay = 0;
    m_jitterBuffer.clear();

    // Переводим состояние сервиса в "свободен"
    m_callState = Idle;

    // Очищаем данные о звонке
    m_currentCallId.clear();
    m_remoteUsername.clear();
    m_remoteIp.clear();
    m_remotePort = 0;
    m_remoteAddress = QHostAddress();  // Сбрасываем адрес удаленного абонента
    m_callDuration = 0;                // Обнуляем длительность звонка

    // Сбрасываем статистику передачи и приема аудио
    m_audioBytesSent = 0;
    m_audioPacketsSent = 0;
    m_audioBytesReceived = 0;
    m_audioPacketsReceived = 0;
}
