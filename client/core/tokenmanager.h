#ifndef TOKENMANAGER_H
#define TOKENMANAGER_H

#include <QString>
#include <QSettings>
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>

/**
 * @brief Класс для управления токенами авторизации (JWT или аналоги).
 *
 * Отвечает за сохранение, загрузку, проверку валидности и удаление токенов
 * из локального хранилища (INI-файл). Позволяет реализовать функционал
 * "Запомнить меня" и автоматический вход при запуске.
 */
class TokenManager
{
public:
    /**
     * @brief Получает путь к файлу конфигурации сессии.
     *
     * Файл `session.ini` располагается в директории исполняемого файла приложения.
     * @return Абсолютный путь к файлу конфигурации.
     */
    static QString getConfigPath()
    {
        QString appDir = QCoreApplication::applicationDirPath();
        QString configPath = appDir + "/session.ini";
        return configPath;
    }

    /**
     * @brief Сохраняет токен авторизации и данные пользователя.
     *
     * Записывает токен, имя пользователя и текущую временную метку в INI-файл.
     * Вызывает `settings.sync()` для гарантированной записи на диск.
     *
     * @param token Строка токена (например, JWT)
     * @param username Имя пользователя, которому принадлежит токен
     */
    static void saveToken(const QString& token, const QString& username)
    {
        QString configPath = getConfigPath();

        // Используем QSettings с форматом IniFormat для кроссплатформенности
        QSettings settings(configPath, QSettings::IniFormat);

        settings.setValue("auth/token", token);
        settings.setValue("auth/username", username);
        settings.setValue("auth/timestamp", QDateTime::currentDateTime().toString(Qt::ISODate));

        settings.sync(); // Принудительная запись на диск

        qDebug() << "[TokenManager] Token saved to:" << configPath;
    }

    /**
     * @brief Загружает сохраненный токен.
     * @return Строка токена или пустая строка, если токен не найден.
     */
    static QString getToken()
    {
        QString configPath = getConfigPath();
        QSettings settings(configPath, QSettings::IniFormat);
        return settings.value("auth/token", "").toString();
    }

    /**
     * @brief Загружает имя пользователя, связанное с сохраненной сессией.
     * @return Имя пользователя или пустая строка.
     */
    static QString getUsername()
    {
        QString configPath = getConfigPath();
        QSettings settings(configPath, QSettings::IniFormat);
        return settings.value("auth/username", "").toString();
    }

    /**
     * @brief Проверяет наличие сохраненного токена.
     * @return true, если токен существует (не пустая строка).
     */
    static bool hasToken()
    {
        return !getToken().isEmpty();
    }

    /**
     * @brief Удаляет данные сессии (токен, имя, метку времени).
     *
     * Используется при выходе пользователя из системы (Logout).
     */
    static void clearToken()
    {
        QString configPath = getConfigPath();
        QSettings settings(configPath, QSettings::IniFormat);

        settings.remove("auth/token");
        settings.remove("auth/username");
        settings.remove("auth/timestamp");

        settings.sync();

        qDebug() << "[TokenManager] Token cleared";
    }

    /**
     * @brief Проверяет срок действия токена.
     *
     * Сравнивает дату сохранения токена с текущей датой.
     *
     * @param maxDays Максимальное время жизни токена в днях (по умолчанию 30).
     * @return true, если токен существует и не просрочен.
     */
    static bool isTokenValid(int maxDays = 30)
    {
        QString configPath = getConfigPath();
        QSettings settings(configPath, QSettings::IniFormat);

        QString timestampStr = settings.value("auth/timestamp", "").toString();

        if (timestampStr.isEmpty()) {
            return false;
        }

        QDateTime savedTime = QDateTime::fromString(timestampStr, Qt::ISODate);
        QDateTime now = QDateTime::currentDateTime();

        qint64 daysPassed = savedTime.daysTo(now);

        return daysPassed <= maxDays;
    }

    /**
     * @brief Выводит в отладку текущий путь хранения токенов.
     */
    static void printStorageLocation()
    {
        qDebug() << "[TokenManager] Token storage location:" << getConfigPath();
    }
};

#endif // TOKENMANAGER_H
