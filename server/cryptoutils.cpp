#include "cryptoutils.h"
#include "monocypher.h"
#include <QRandomGenerator>
#include <QDebug>

/**
 * @name Настройки сложности Argon2i
 * @brief Константы, определяющие затраты ресурсов на хеширование.
 * @{
 */

/**
 * @brief Объем памяти, используемый алгоритмом (в Килобайтах).
 * @details Значение 1000 = ~1 МБ памяти на один хеш.
 * - Для мобильных клиентов/десктопа: 1-16 МБ (нормально).
 * - Для сервера: Рекомендуется 64 МБ+ (NB_BLOCKS >= 64000), чтобы затруднить атаку на GPU/ASIC.
 */
const uint32_t NB_BLOCKS = 1000;

/**
 * @brief Количество проходов (итераций) по памяти.
 * @details Большее количество итераций линейно увеличивает время хеширования.
 * Рекомендуется минимум 3 для защиты от time-memory trade-off атак.
 */
const uint32_t NB_ITERATIONS = 3;



QString CryptoUtils::hashPasswordArgon2(const QString& password, const QByteArray& salt)
{
    // Проверка безопасности: соль должна быть достаточно длинной
    if (salt.size() < 8) {
        qWarning() << "Salt is too short! Security risk.";
        return QString();
    }

    QByteArray passwordBytes = password.toUtf8();

    const int hashSize = 32; ///< Длина выходного хеша в байтах.
    uint8_t hash[hashSize];

    // Выделяем рабочую область памяти для алгоритма Argon2.
    // ВАЖНО: Monocypher требует, чтобы мы сами управляли этой памятью.
    void* work_area = malloc(NB_BLOCKS * 1024);
    if (!work_area) {
        qCritical() << "Failed to allocate memory for Argon2 hashing!";
        return QString();
    }

    // --- ЗАПОЛНЯЕМ СТРУКТУРЫ (Monocypher API) ---

    // 1. Конфигурация сложности алгоритма
    crypto_argon2_config config;
    config.algorithm = CRYPTO_ARGON2_I; // Argon2i оптимизирован против Side-Channel атак (лучше для паролей)
    config.nb_blocks = NB_BLOCKS;       // Память в КБ
    config.nb_passes = NB_ITERATIONS;   // Число проходов
    config.nb_lanes  = 1;               // Количество параллельных потоков (1 для последовательного выполнения)

    // 2. Входные данные (пароль и соль)
    crypto_argon2_inputs inputs;
    inputs.pass = (const uint8_t*)passwordBytes.constData();
    inputs.pass_size = passwordBytes.size();
    inputs.salt = (const uint8_t*)salt.constData();
    inputs.salt_size = salt.size();

    // 3. Дополнительные параметры (секреты, ключи и т.д. - не используются для обычных паролей)
    crypto_argon2_extras extras = {}; // Инициализация нулями

    // --- ВЫЗОВ ФУНКЦИИ ХЕШИРОВАНИЯ ---
    crypto_argon2(
        hash, hashSize,  // Буфер для результата и его размер
        work_area,       // Указатель на выделенную память
        config,          // Конфигурация
        inputs,          // Данные
        extras           // Пустые доп. параметры
        );

    // Очистка выделенной памяти (критично для предотвращения утечек)
    free(work_area);

    // Возвращаем результат в Hex-формате для удобного хранения в текстовом поле БД
    return QString::fromLatin1(QByteArray((char*)hash, hashSize).toHex());
}


QByteArray CryptoUtils::generateSalt(int length)
{
    QByteArray salt(length, Qt::Uninitialized);

    // QRandomGenerator::system() использует энтропию ОС (/dev/urandom на Linux, CryptGenRandom на Windows).
    // Это криптографически стойкий генератор.
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(salt.data()), length / 4);

    return salt;
}


bool CryptoUtils::verifyPassword(const QString& inputPassword, const QByteArray& storedSalt, const QString& storedHashHex)
{
    // Алгоритм проверки:
    // 1. Берем введенный пароль.
    // 2. Хешируем его с той же солью и теми же параметрами (NB_BLOCKS/ITERATIONS), что и при регистрации.
    QString newHash = hashPasswordArgon2(inputPassword, storedSalt);

    // 3. Сравниваем полученный хеш с тем, что лежит в базе.
    // TODO: В идеале использовать crypto_verify32() или аналог для comparison in constant time,
    // чтобы избежать атак по времени (timing attacks). Для Argon2 это менее критично, чем для проверки подписей,
    // но является хорошим тоном.
    return (newHash == storedHashHex);
}
