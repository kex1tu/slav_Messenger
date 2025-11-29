#ifndef CHATMESSAGEDELEGATE_H
#define CHATMESSAGEDELEGATE_H

#include <QObject>
#include <QStyledItemDelegate>
#include <QMap>
#include <QStaticText>
#include <QTextDocument>
#include "structures.h"

class ChatMessageModel;
class QSvgRenderer;

/**
 * @brief Делегат для отрисовки сообщений в списке чата.
 *
 * Отвечает за визуализацию различных типов сообщений (текст, файлы, системные уведомления),
 * отрисовку "пузырей" (bubbles), статусов доставки, времени и обработку взаимодействий
 * (клики по файлам, кнопкам ответа). Оптимизирован с использованием кеширования
 * размеров и текстовых документов.
 */
class ChatMessageDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    /**
     * @brief Конструктор делегата.
     * @param model Указатель на модель данных (для доступа к raw данным при отрисовке)
     * @param parent Родительский объект
     */
    explicit ChatMessageDelegate(const ChatMessageModel* model, QObject *parent = nullptr);

    /** @brief Деструктор. Очищает статические ресурсы и кеши. */
    ~ChatMessageDelegate();

    /** @brief Принудительная очистка всех кешей (размеров, документов, иконок). */
    void clearCaches();

    /**
     * @brief Основной метод отрисовки элемента списка.
     *
     * Рисует фон сообщения, текст, время, статус, аватар (для входящих) и
     * дополнительные элементы (иконки файлов, превью).
     *
     * @param painter Объект рисования
     * @param option Параметры стиля (прямоугольник, состояние)
     * @param index Индекс отрисовываемого элемента
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief Вычисляет необходимый размер элемента.
     *
     * Использует кеширование результатов для повышения производительности при прокрутке.
     *
     * @param option Параметры стиля
     * @param index Индекс элемента
     * @return QSize Размер прямоугольника, занимаемого сообщением
     */
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief Обрабатывает события ввода (мышь, клавиатура).
     *
     * Реализует логику кликов по кнопкам скачивания файлов и контекстным действиям.
     *
     * @param event Событие Qt
     * @param model Модель данных
     * @param option Параметры стиля
     * @param index Индекс элемента
     * @return true, если событие обработано делегатом
     */
    bool editorEvent(QEvent *event,
                     QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

public slots:
    /** @brief Слот для очистки кеша размеров (вызывается при изменении шрифта или ресайзе). */
    void clearSizeHintCache();

signals:
    /**
     * @brief Сигнал запроса на скачивание файла.
     * @param fileId ID файла
     * @param fileUrl URL для скачивания
     * @param fileName Имя файла
     */
    void fileDownloadRequested(const QString &fileId, const QString &fileUrl, const QString &fileName);

    /**
     * @brief Сигнал запроса на ответ (Reply) на сообщение.
     * @param index Индекс сообщения, на которое отвечают
     */
    void replyRequested(const QModelIndex& index);

private:
    QModelIndex m_hoveredIndex;        ///< Индекс элемента под курсором мыши
    QRect m_hoveredFileRect;           ///< Область кнопки файла под курсором

    const ChatMessageModel* m_model;   ///< Ссылка на модель данных

    /**
     * @brief Кеш вычисленных размеров сообщений.
     * Ключ: ID сообщения, Значение: QSize.
     * mutable позволяет менять кеш внутри константного метода sizeHint.
     */
    mutable QMap<qint64, QSize> m_sizeHintCache;

    /** @brief Статический кеш SVG-рендереров для иконок статусов (галочки, часы). */
    static QMap<ChatMessage::MessageStatus, QSvgRenderer*> m_statusRenderers;

    /** @brief Флаг инициализации статических рендереров. */
    static bool m_renderersInitialized;

    /**
     * @brief Кеш текстовых документов (QTextDocument) для сложной верстки.
     * Ключ: пара <ID сообщения, ширина зоны отрисовки>.
     */
    mutable QMap<QPair<qint64, int>, QTextDocument*> m_documentCache;

    /** @brief Инициализирует SVG рендереры (загружает ресурсы один раз). */
    static void initRenderers(QObject* parent);

    /** @brief Кеш загруженных иконок файлов. */
    static QMap<QString, QPixmap> m_iconCache;
};

#endif // CHATMESSAGEDELEGATE_H
