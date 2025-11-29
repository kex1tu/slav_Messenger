#ifndef AVATARCACHE_H
#define AVATARCACHE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QDir>

/**
 * @brief Класс для кеширования аватаров пользователей
 *
 * AvatarCache управляет локальным кешем изображений аватаров,
 * скачивает их с сервера при необходимости и предоставляет
 * доступ к локальным файлам. Кеш хранится в директории avatars_cache.
 */
class AvatarCache : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Конструктор класса AvatarCache
     *
     * Инициализирует сетевой менеджер и создает директорию для кеша аватаров,
     * если она еще не существует. Директория создается в текущем рабочем каталоге.
     *
     * @param parent Родительский QObject для управления временем жизни
     */
    explicit AvatarCache(QObject* parent = nullptr)
        : QObject(parent),
        m_networkManager(new QNetworkAccessManager(this))
    {
        // Создаем директорию для кеша в текущей рабочей директории
        m_avatarDir = QDir(QDir::currentPath() + "/avatars_cache");
        if (!m_avatarDir.exists())
            QDir().mkpath(m_avatarDir.path());
    }

    /**
     * @brief Получает аватар пользователя из кеша или скачивает с сервера
     *
     * Проверяет наличие аватара в локальном кеше. Если файл существует и не пуст,
     * немедленно возвращает его через сигнал avatarDownloaded. В противном случае
     * инициирует асинхронную загрузку с сервера.
     *
     * @param username Имя пользователя (используется как имя файла)
     * @param avatarUrl Относительный URL аватара на сервере
     *
     * @note Функция асинхронная - результат возвращается через сигнал avatarDownloaded
     * @note Пустые параметры игнорируются без генерации сигнала
     */
    void ensureAvatar(const QString& username, const QString& avatarUrl) {
        // Проверяем валидность входных данных
        if (username.isEmpty() || avatarUrl.isEmpty())
            return;

        QString localPath = m_avatarDir.filePath(username + ".jpg");

        QFile localFile(localPath);
        if (localFile.exists() && localFile.size() > 0) {
            // Аватар уже есть в кеше - возвращаем немедленно
            emit avatarDownloaded(username, localPath);
            return;
        }

        // Формируем полный URL для загрузки с локального сервера
        QUrl fileUrl(avatarUrl);
        fileUrl = "http://localhost:9090/files/download/" + avatarUrl;
        QNetworkRequest req(fileUrl);
        QNetworkReply* reply = m_networkManager->get(req);

        // Обрабатываем завершение загрузки асинхронно
        connect(reply, &QNetworkReply::finished, this, [=]() {
            QByteArray data = reply->readAll();
            if (!data.isEmpty()) {
                QFile outFile(localPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    // Записываем скачанные данные в файл кеша
                    outFile.write(data);
                    outFile.close();
                    emit avatarDownloaded(username, localPath);
                    qDebug() << "[AvatarCache] Скачан аватар для" << username << localPath;
                } else {
                    qDebug() << "[AvatarCache] Не удалось создать файл для" << username;
                }
            } else {
                qDebug() << "[AvatarCache] Пустой ответ при скачивании аватара" << username;
            }
            // Освобождаем память reply после обработки
            reply->deleteLater();
        });
    }

signals:
    /**
     * @brief Сигнал испускается когда аватар готов к использованию
     *
     * Сигнал генерируется как при успешной загрузке с сервера,
     * так и при обнаружении аватара в локальном кеше.
     *
     * @param username Имя пользователя, чей аватар готов
     * @param localPath Абсолютный путь к локальному файлу аватара
     */
    void avatarDownloaded(QString username, QString localPath);

private:
    /**
     * @brief Менеджер для выполнения HTTP-запросов
     *
     * Используется для асинхронной загрузки аватаров с сервера.
     * Владение передано QObject через parent, автоматически удаляется.
     */
    QNetworkAccessManager* m_networkManager;

    /**
     * @brief Директория для хранения кешированных аватаров
     *
     * Путь: <current_dir>/avatars_cache/
     * Файлы именуются как <username>.jpg
     */
    QDir m_avatarDir;
};

#endif // AVATARCACHE_H
