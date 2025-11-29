#ifndef CRYPTOUTILS_H
#define CRYPTOUTILS_H

#include <QString>
#include <QByteArray>
#include <QDebug>
#include <QRandomGenerator>
#include "monocypher.h" // Подключаем библиотеку Monocypher

/**
 * @class CryptoUtils
 * @brief Статический класс-утилита для базовых криптографических операций.
 *
 * @details Предоставляет методы для хеширования паролей (Argon2i) и генерации
 *          случайной соли. Не хранит состояние.
 */
class CryptoUtils
{
public:
    /**
     * @brief Генерирует безопасный хеш пароля с использованием Argon2i.
     *
     * @details Использует алгоритм Argon2i (встроенный в Monocypher), который устойчив
     *          к атакам по сторонним каналам (side-channel attacks) и GPU-брутфорсу.
     *
     * @param password Пароль пользователя (в открытом виде).
     * @param salt Случайная соль. Должна быть уникальной для каждого пользователя.
     *             Рекомендуемая длина: минимум 16 байт.
     * @return Хеш пароля в виде шестнадцатеричной строки (Hex), готовой к записи в БД.
     */
    static QString hashPasswordArgon2(const QString& password, const QByteArray& salt);

    /**
     * @brief Генерирует криптостойкую случайную соль.
     *
     * @details Использует системный генератор энтропии (QRandomGenerator::system).
     * @param length Длина соли в байтах (по умолчанию 16 байт).
     * @return Массив байтов со случайными данными.
     */
    static QByteArray generateSalt(int length = 16);

    /**
     * @brief Проверяет валидность введенного пароля.
     *
     * @details Повторно хеширует введенный `inputPassword` с той же `storedSalt`
     *          и сравнивает результат с `storedHashHex` в неизменном времени (constant time comparison),
     *          чтобы предотвратить атаки по времени выполнения (timing attacks).
     *
     * @param inputPassword Пароль, введенный пользователем при входе.
     * @param storedSalt Соль, извлеченная из базы данных для этого пользователя.
     * @param storedHashHex Эталонный хеш из базы данных.
     * @return `true`, если пароли совпадают, иначе `false`.
     */
    static bool verifyPassword(const QString& inputPassword, const QByteArray& storedSalt, const QString& storedHashHex);
};

/**
 * @class CryptoManager
 * @brief Управляет криптографической сессией и обменом ключами (X25519).
 *
 * @details Этот класс инкапсулирует протокол обмена ключами Диффи-Хеллмана на эллиптических кривых (ECDH)
 *          с использованием Curve25519. Он хранит пару ключей (приватный/публичный) для текущей сессии
 *          и вычисляет общий секрет (Shared Secret) после получения публичного ключа собеседника.
 *
 * @note Для каждого соединения создается свой экземпляр CryptoManager.
 */
class CryptoManager {
private:
    /**
     * @brief Приватный (секретный) ключ текущей стороны.
     * @note 32 байта случайных данных. Никогда не должен передаваться по сети.
     */
    uint8_t secretKey[32];

    /**
     * @brief Публичный ключ текущей стороны.
     * @note Вычисляется из secretKey. Передается собеседнику для вычисления общего секрета.
     */
    uint8_t publicKey[32];

    /**
     * @brief Общий секретный ключ сессии (Shared Secret).
     * @details Результат операции X25519(mySecret, theirPublic). Используется для симметричного шифрования (например, Chacha20).
     */
    uint8_t sharedKey[32];

    /**
     * @brief Флаг готовности шифрованного канала.
     * @details `true`, если рукопожатие завершено и `sharedKey` успешно вычислен.
     */
    bool ready = false;

public:
    /**
     * @brief Конструктор. Генерирует пару ключей (Private/Public).
     * @details При создании объекта сразу генерируется эфемерный приватный ключ
     *          с помощью `QRandomGenerator::system()` и вычисляется соответствующий публичный ключ.
     */
    CryptoManager() {
        // Генерация 32 байт случайности для приватного ключа
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(secretKey), 32 / 4);

        // Вычисление публичного ключа из приватного (Curve25519)
        crypto_x25519_public_key(publicKey, secretKey);
    }

    /**
     * @brief Возвращает публичный ключ для отправки собеседнику.
     * @return Указатель на массив из 32 байт.
     */
    const uint8_t* getMyPublicKey() const { return publicKey; }

    /**
     * @brief Вычисляет общий секрет на основе публичного ключа собеседника.
     *
     * @details Выполняет операцию скалярного умножения на эллиптической кривой:
     *          SharedSecret = X25519(MyPrivateKey, TheirPublicKey).
     *          После выполнения устанавливает флаг `ready = true`.
     *
     * @param theirPublicKey Публичный ключ удаленной стороны (должен быть 32 байта).
     */
    void computeSharedSecret(const QByteArray& theirPublicKey) {
        if (theirPublicKey.size() != 32) {
            qWarning() << "Invalid public key size received:" << theirPublicKey.size();
            return;
        }

        // Вычисление общего секрета
        crypto_x25519(sharedKey, secretKey, (const uint8_t*)theirPublicKey.data());
        ready = true;

        // Внимание: в продакшене не выводите ключи в лог! Оставлено для отладки.
        qDebug() << "Shared Secret Hex:" << QByteArray((char*)sharedKey, 32).toHex();
    }

    /**
     * @brief Проверяет, установлено ли защищенное соединение.
     * @return `true`, если общий секрет вычислен.
     */
    bool isEncrypted() const { return ready; }

    /**
     * @brief Возвращает вычисленный общий ключ сессии.
     * @warning Этот ключ должен использоваться для симметричного шифрования (XChaCha20-Poly1305).
     * @return Указатель на массив из 32 байт.
     */
    const uint8_t* getSessionKey() const { return sharedKey; }
};

#endif // CRYPTOUTILS_H
