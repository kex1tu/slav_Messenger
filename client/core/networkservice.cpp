#include "networkservice.h"
#include <QTcpSocket>
#include <QDataStream>
#include <QJsonDocument>
#include <QDebug>
#include <QRandomGenerator>

NetworkService::NetworkService(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this)), m_nextBlockSize(0), m_crypto(new CryptoManager())
{
    // Связь: подключение, чтение и отключение сокета — на внутренние обработчики
    connect(m_socket, &QTcpSocket::connected, this, &NetworkService::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkService::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkService::onDisconnected);
}


void NetworkService::connectToServer(const QString& host, quint16 port) {
    qDebug() << "[NetworkService] Attempting to connect to" << host << ":" << port;
    m_socket->connectToHost(host, port);
}


void NetworkService::sendJson(const QJsonObject& json)
{
    qDebug() << "---------------------------------";
    qDebug() << "[NetworkService] Preparing to send JSON of type:" << json["type"].toString();
    qDebug() << "[NetworkService]" << json;
    qDebug() << "[NetworkService] Full JSON content:" << json;
    qDebug() << "---------------------------------";

    // Сериализация объекта в QByteArray.
    QByteArray jsonData = QJsonDocument(json).toJson(QJsonDocument::Compact);
    if (!m_crypto->isEncrypted()) {
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_2);
        // Формируем пакет "размер (4 байта) + данные".
        out << (quint32)0; // Резервируем место для размера.
        out << jsonData;
        out.device()->seek(0); // Возвращаемся в начало.
        out << (quint32)(block.size() - sizeof(quint32)); // Записываем реальный размер.
        m_socket->write(block);
        qDebug() << "JSON send" << jsonData.size();
        qWarning() << "Encryption not ready! Sending plain text (UNSAFE unless handshake)";
        return;
    }
    // ================= ШИФРОВАНИЕ (XChaCha20) =================

    // 3. Генерируем случайный Nonce (24 байта)
    uint8_t nonce[24];// Приводим к quint32* (поскольку 24 байта = 6 штук по 4 байта)
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(nonce), 24 / 4);


    // 4. Подготовка буфера: MAC (16 байт) + Ciphertext (N байт)
    QByteArray encryptedData;
    encryptedData.resize(16 + jsonData.size());

    // 5. Шифруем
    // crypto_lock(mac, ciphertext, key, nonce, jsonData, size)
    crypto_aead_lock(
        reinterpret_cast<uint8_t*>(encryptedData.data()) + 16, // Ciphertext (выход)
        reinterpret_cast<uint8_t*>(encryptedData.data()),      // MAC (выход, 16 байт)
        m_crypto->getSessionKey(),                               // Key (32 байта)
        nonce,                                                 // Nonce (24 байта)
        nullptr, 0,                                            // Associated Data (нет)
        reinterpret_cast<const uint8_t*>(jsonData.constData()), // Message (вход)
        jsonData.size()                                      // Message length
        );


    // ================= УПАКОВКА (TCP Framing) =================

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);


    // Структура: [Length 4b] [Nonce 24b] [MAC 16b + Ciphertext]

    // 1. Резервируем место под длину (4 байта)
    out << (quint32)0;

    // 2. Пишем Nonce
    out << QByteArray(reinterpret_cast<const char*>(nonce), 24);
    qDebug() << "Client nonce" <<  QByteArray((char*)nonce, 24).toHex();
    // 3. Пишем зашифрованные данные (включая MAC в начале)
    out << encryptedData;

    // 4. Возвращаемся в начало и пишем реальную длину пакета
    out.device()->seek(0);
    // Размер = Nonce(24) + MAC(16) + TextLen
    quint32 totalSize = packet.size() - sizeof(quint32);
    out << totalSize;

    // Отправляем в сеть
    m_socket->write(packet);
    qDebug() << totalSize;

}

void NetworkService::sendClientPublicKey() {
    QJsonObject json;
    json["type"] = "handshake";
    // Преобразуем ключ (32 байта) в Base64 или Hex строку для JSON
    json["key"] = QString::fromLatin1(QByteArray((const char*)m_crypto->getMyPublicKey(), 32).toBase64());
    sendJson(json);
}

void NetworkService::onConnected() {
    qDebug() << "[NetworkService] Socket connected.";
    sendClientPublicKey();
    emit connected();
}


void NetworkService::onDisconnected() {
    qDebug() << "[NetworkService] Socket disconnected.";
    emit disconnected();
}


void NetworkService::onReadyRead(){
    QDataStream in(m_socket);
    in.setVersion(QDataStream::Qt_6_2);

    while(true) {

        // Если пока не знаем размер блока — проверяем его наличие
        if (m_nextBlockSize == 0) {
            if (m_socket->bytesAvailable() < (qint64)sizeof(qint32)) {
                break;  // Ещё не пришёл весь размер блока
            }
            // Получаем размер следующего блока
            in >> m_nextBlockSize;
        }

        // Ждём появления полного тела сообщения
        if (m_socket->bytesAvailable() < m_nextBlockSize) {
            break;  // Данных пока недостаточно — ждём
        }

        if (!m_crypto->isEncrypted()) {
            QByteArray jsonData;
            in >> jsonData;
            m_nextBlockSize = 0; // Сбрасываем размер для следующей итерации.

            // Фаза 3: Парсинг и маршрутизация.
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);

            if (!doc.isNull() && doc.isObject()) {
                emit jsonReceived(doc.object());
                //processJsonRequest(doc.object(), socket);
                continue;
            } else {
                QString type = doc.object()["type"].toString();
                qWarning() << "[CLIENT] Unknown request type received:" << type;
                //sendJson({{"type", "error"}, {"reason", "Unknown command: " + type}});
            }
            return;
            //continue;  // НЕ return! Продолжаем обработку буфера
        }


        //QByteArray data = socket->read(nextBlockSize);
        QByteArray encryptedData;
        QByteArray nonceArray;
        in >> nonceArray;          // Читает [size:4b][24 bytes]
        in >> encryptedData;                // Читает [size:4b][MAC+cipher]


        if (nonceArray.size() != 24) {
            qCritical() << "Invalid nonce size:" << nonceArray.size();
            m_socket->abort();
            return;
        }

        if (encryptedData.size() < 16) {
            qCritical() << "Encrypted data too short!";
            m_socket->abort();
            return;
        }

        // Извлекаем части
        const uint8_t* nonce = reinterpret_cast<const uint8_t*>(nonceArray.constData());
        const uint8_t* mac   = reinterpret_cast<const uint8_t*>(encryptedData.constData());
        const uint8_t* cipherText = mac + 16;

        qDebug() << "Server Nonce:" << QByteArray((char*)nonce, 24).toHex();


        int textLen = encryptedData.size() - 16;
        QByteArray decrypted(textLen, Qt::Uninitialized);

        // crypto_unlock(plaintext, key, nonce, mac, ciphertext, size)
        // Возвращает 0 при успехе, ненулевое при ошибке (подделка)
        int status = crypto_aead_unlock(
            reinterpret_cast<uint8_t*>(decrypted.data()), // Plaintext (выход)
            mac,                                          // MAC (вход, 16 байт)
            m_crypto->getSessionKey(),                      // Key (32 байта)
            nonce,                                        // Nonce (24 байта)
            nullptr, 0,                                   // Associated Data (нет)
            cipherText,                                   // Ciphertext (вход)
            textLen                                      // Ciphertext length
            );

        if (status != 0) {
            qCritical() << "DECRYPTION FAILED! MAC mismatch. Possible hacking attempt.";
            m_socket->disconnect();
            return;
        }



        // Сброс значения размера для следующего сообщения
        m_nextBlockSize = 0;

        // Парсим JSON из прочитанных байт
        QJsonDocument doc = QJsonDocument::fromJson(decrypted);
        if (doc.isNull() || !doc.isObject()) {
            qDebug() << "[NetworkService] Failed to parse JSON or it's not an object.";
            continue;  // Ошибка парсинга, игнорируем фрейм
        }

        QJsonObject response = doc.object();
        QString type = response["type"].toString();
        qDebug() << "[NetworkService] Processing message of type" << type;

        // Передаём событие на все подписанные компоненты (логика/слоты)
        emit jsonReceived(doc.object());
    }
}
