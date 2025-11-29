#include "cryptoutils.h"
#include "monocypher.h"
#include <QRandomGenerator>
#include <QDebug>

// Параметры Argon2: количество блоков памяти и итераций
// NB_BLOCKS * 1024 байт = ~1МБ (баланс между безопасностью и производительностью)
const uint32_t NB_BLOCKS = 1000;
const uint32_t NB_ITERATIONS = 3;

QString CryptoUtils::hashPasswordArgon2(const QString& password, const QByteArray& salt)
{
    // Проверяем длину соли: минимум 8 байт для криптографической стойкости
    if (salt.size() < 8) {
        qWarning() << "Salt is too short! Security risk.";
        return QString();
    }

    // Конвертируем пароль в байты (UTF-8)
    QByteArray passwordBytes = password.toUtf8();

    // Буфер для результата хеширования (32 байта = 256 бит)
    const int hashSize = 32;
    uint8_t hash[hashSize];

    // Выделяем рабочую область для Argon2 (требуется для промежуточных вычислений)
    void* work_area = malloc(NB_BLOCKS * 1024);
    if (!work_area) return QString();

    // Настраиваем параметры Argon2
    crypto_argon2_config config;
    config.algorithm = CRYPTO_ARGON2_I;  // Argon2i: защита от time-memory trade-off атак
    config.nb_blocks = NB_BLOCKS;        // Количество блоков памяти (влияет на memory-hardness)
    config.nb_passes = NB_ITERATIONS;    // Количество итераций (влияет на время вычисления)
    config.nb_lanes  = 1;                // Количество параллельных потоков (1 = без параллелизма)

    // Заполняем входные данные: пароль и соль
    crypto_argon2_inputs inputs;
    inputs.pass = (const uint8_t*)passwordBytes.constData();
    inputs.pass_size = passwordBytes.size();
    inputs.salt = (const uint8_t*)salt.constData();
    inputs.salt_size = salt.size();

    // Дополнительные параметры (не используются, но требуются API)
    crypto_argon2_extras extras = {};

    // Выполняем хеширование пароля с использованием Argon2
    crypto_argon2(
        hash, hashSize,   // Выходной буфер и его размер
        work_area,        // Рабочая область памяти
        config,           // Конфигурация алгоритма
        inputs,           // Входные данные (пароль + соль)
        extras            // Дополнительные параметры
    );

    // Освобождаем рабочую область
    free(work_area);

    // Возвращаем хеш в виде hex-строки (для хранения в БД)
    return QString::fromLatin1(QByteArray((char*)hash, hashSize).toHex());
}

QByteArray CryptoUtils::generateSalt(int length)
{
    QByteArray salt(length, Qt::Uninitialized);
    
    // Генерируем криптографически стойкую случайную соль с помощью системного генератора
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(salt.data()), length / 4);
    return salt;
}

bool CryptoUtils::verifyPassword(const QString& inputPassword, const QByteArray& storedSalt, const QString& storedHashHex)
{
    // Хешируем введенный пароль с той же солью, что использовалась при регистрации
    QString newHash = hashPasswordArgon2(inputPassword, storedSalt);

    // Сравниваем полученный хеш с сохраненным (constant-time сравнение было бы безопаснее)
    return (newHash == storedHashHex);
}
