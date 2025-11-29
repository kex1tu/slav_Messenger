#ifndef CRYPTOUTILS_H
#define CRYPTOUTILS_H

#include <QString>
#include <QByteArray>
#include <QDebug>
#include <QRandomGenerator>
#include "monocypher.h"

/**
 * @brief Утилитный класс для работы с криптографическими примитивами.
 *
 * Предоставляет статические методы для хеширования паролей (Argon2),
 * генерации случайной соли и верификации паролей.
 */
class CryptoUtils
{
public:
    /**
     * @brief Хеширует пароль с использованием алгоритма Argon2.
     *
     * Использует библиотеку Monocypher для создания безопасного хеша пароля,
     * устойчивого к атакам по времени и памяти.
     *
     * @param password Исходный пароль пользователя
     * @param salt Случайная соль (рекомендуется 16 байт)
     * @return Хеш пароля в шестнадцатеричном формате (Hex QString)
     */
    static QString hashPasswordArgon2(const QString& password, const QByteArray& salt);

    /**
     * @brief Генерирует криптографически стойкую случайную соль.
     *
     * @param length Длина соли в байтах (по умолчанию 16)
     * @return QByteArray, содержащий сгенерированные случайные байты
     */
    static QByteArray generateSalt(int length = 16);

    /**
     * @brief Проверяет правильность введенного пароля.
     *
     * Хеширует введенный пароль с той же солью и сравнивает результат
     * с сохраненным хешем.
     *
     * @param inputPassword Пароль, введенный пользователем
     * @param storedSalt Сохраненная соль, использованная при хешировании
     * @param storedHashHex Сохраненный хеш пароля для сравнения
     * @return true, если пароли совпадают, иначе false
     */
    static bool verifyPassword(const QString& inputPassword, const QByteArray& storedSalt, const QString& storedHashHex);
};

/**
 * @brief Менеджер сквозного шифрования на основе X25519.
 *
 * Класс управляет генерацией пары ключей (публичный/приватный) и
 * вычислением общего секрета (Shared Secret) для безопасного обмена данными.
 * Использует эллиптическую кривую Curve25519 (через Monocypher).
 */
class CryptoManager {
    uint8_t secretKey[32];  ///< Собственный приватный ключ (должен храниться в секрете)
    uint8_t publicKey[32];  ///< Собственный публичный ключ (передается собеседнику)
    uint8_t sharedKey[32];  ///< Вычисленный общий ключ сессии
    bool ready = false;     ///< Флаг готовности шифрования (ключ вычислен)

public:
    /**
     * @brief Конструктор менеджера шифрования.
     *
     * При создании объекта автоматически генерирует случайный приватный ключ
     * и вычисляет соответствующий ему публичный ключ.
     */
    CryptoManager() {
        // Генерируем 32 байта случайных данных для приватного ключа
        // Используем системный генератор для криптографической стойкости
        QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(secretKey), 32 / 4);

        // Вычисляем публичный ключ из приватного (функция Monocypher)
        crypto_x25519_public_key(publicKey, secretKey);
    }

    /**
     * @brief Получить собственный публичный ключ.
     * @return Указатель на массив байтов публичного ключа (32 байта)
     */
    const uint8_t* getMyPublicKey() const { return publicKey; }

    /**
     * @brief Вычисляет общий секрет (Shared Secret) на основе публичного ключа собеседника.
     *
     * Использует протокол Диффи-Хеллмана на эллиптических кривых (X25519).
     * Результирующий sharedKey будет одинаковым у обоих участников обмена.
     *
     * @param theirPublicKey Публичный ключ собеседника (должен быть 32 байта)
     */
    void computeSharedSecret(const QByteArray& theirPublicKey) {
        // Проверка длины ключа - критично для безопасности и предотвращения сбоев
        if (theirPublicKey.size() != 32) return;

        // Выполняем скалярное умножение X25519: shared = secret * their_public
        crypto_x25519(sharedKey, secretKey, (const uint8_t*)theirPublicKey.data());

        ready = true; // Помечаем, что канал готов к шифрованию

        // Логируем для отладки (В ПРОДАКШЕНЕ УДАЛИТЬ ВЫВОД КЛЮЧА!)
        qDebug() << "Shared Secret Hex:" << QByteArray((char*)sharedKey, 32).toHex();
    }

    /**
     * @brief Проверяет, установлен ли общий ключ шифрования.
     * @return true, если handshake завершен и ключ вычислен
     */
    bool isEncrypted() const { return ready; }

    /**
     * @brief Получить текущий сессионный ключ.
     * @return Указатель на массив байтов общего ключа
     */
    const uint8_t* getSessionKey() const { return sharedKey; }
};
#endif // CRYPTOUTILS_H
