#ifndef NETWORKSERVICE_H
#define NETWORKSERVICE_H

#include <QObject>
#include <QJsonObject>
#include "cryptoutils.h"
#include <QTcpSocket>

/**
 * @brief Сервис для управления сетевым TCP-соединением с сервером.
 *
 * Обеспечивает подключение, обмен JSON-сообщениями, обработку потока данных
 * (сборка полных пакетов из фрагментов) и базовое шифрование через CryptoManager.
 */
class NetworkService : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Конструктор сетевого сервиса.
     *
     * Инициализирует TCP-сокет и менеджер криптографии.
     * @param parent Родительский объект.
     */
    explicit NetworkService(QObject *parent = nullptr);

    /**
     * @brief Деструктор.
     *
     * Корректно разрывает соединение, отключает сигналы и освобождает ресурсы.
     */
    ~NetworkService(){
        if (m_socket) {
            disconnect(m_socket, nullptr, this, nullptr);
            m_socket->close();
            m_socket->deleteLater();
        }
        delete m_crypto;
    }

public slots:
    /**
     * @brief Устанавливает соединение с сервером.
     * @param host Адрес сервера (IP или домен)
     * @param port Порт сервера
     */
    void connectToServer(const QString& host, quint16 port);

    /**
     * @brief Отправляет JSON-объект на сервер.
     *
     * Данные сериализуются в QByteArray, снабжаются заголовком с длиной пакета
     * и отправляются через сокет.
     * @param json JSON-объект для отправки
     */
    void sendJson(const QJsonObject& json);

    /**
     * @brief Возвращает указатель на менеджер криптографии.
     * @return Указатель на CryptoManager, управляющий ключами сессии
     */
    CryptoManager* getCurrentUserCrypto(){
        return m_crypto;
    }

signals:
    /** @brief Сигнал успешного подключения к серверу. */
    void connected();

    /** @brief Сигнал разрыва соединения. */
    void disconnected();

    /**
     * @brief Сигнал получения нового валидного JSON-сообщения.
     * @param jsonDoc Десериализованный JSON объект
     */
    void jsonReceived(const QJsonObject& jsonDoc);

    /**
     * @brief Сигнал входящего запроса на звонок (P2P).
     * @param fromUser Имя звонящего
     * @param callId Уникальный ID звонка
     * @param callerIp IP адрес инициатора (для P2P соединения)
     * @param callerPort Порт инициатора
     */
    void callRequestReceived(const QString& fromUser, const QString& callId,
                             const QString& callerIp, quint16 callerPort);

    /**
     * @brief Сигнал принятия исходящего звонка собеседником.
     * @param calleeIp IP адрес ответившего
     * @param calleePort Порт ответившего
     */
    void callAcceptedReceived(const QString& calleeIp, quint16 calleePort);

    /** @brief Сигнал отклонения звонка собеседником. */
    void callRejectedReceived();

    /** @brief Сигнал завершения текущего разговора. */
    void callEndedReceived();

    /**
     * @brief Сигнал ошибки при инициации звонка.
     * @param reason Текстовое описание причины
     */
    void callRequestFailure(const QString& reason);

private slots:
    /** @brief Внутренний слот обработки успешного connect(). */
    void onConnected();

    /** @brief Внутренний слот обработки disconnect(). */
    void onDisconnected();

    /**
     * @brief Слот чтения поступающих данных из сокета.
     *
     * Реализует логику сборки пакетов: читает длину блока, ждет накопления данных,
     * затем парсит JSON и эмитит jsonReceived.
     */
    void onReadyRead();

    /**
     * @brief Отправляет публичный ключ клиента сразу после подключения.
     *
     * Часть процедуры рукопожатия (Handshake) для установления защищенного канала.
     */
    void sendClientPublicKey();

private:
    QTcpSocket *m_socket;        ///< Основной TCP сокет для связи с сервером
    quint32 m_nextBlockSize;     ///< Размер ожидаемого блока данных (для парсинга потока)
    CryptoManager *m_crypto;     ///< Менеджер шифрования (X25519)
};

#endif // NETWORKSERVICE_H
