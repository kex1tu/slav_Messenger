#include "chatmessagedelegate.h"
#include "chatmessagemodel.h"
#include "structures.h"
#include <QPainter>
#include <QSvgRenderer>
#include <QPainterPath>
#include <algorithm>
#include <QTextDocument>
#include <QFileInfo>
#include <QEvent>
#include <QMouseEvent>

QString getFileIconPath(const QString& fileName) {
    QFileInfo fileInfo(fileName);
    QString extension = fileInfo.suffix().toLower();

    // Статический маппинг расширения -> путь к SVG-иконке
    static const QMap<QString, QString> iconMap = {
        // Документы
        {"pdf",  ":/icons/icons/pdf-document.svg"},
        {"doc",  ":/icons/icons/sword-document.svg"},
        {"docx", ":/icons/icons/word-document.svg"},
        {"txt",  ":/icons/icons/txt-document.svg"},
        {"rtf",  ":/icons/icons/rtf-document.svg"},

        // Таблицы
        {"xls",  ":/icons/icons/excel-document.svg"},
        {"xlsx", ":/icons/icons/excel-document.svg"},
        {"csv",  ":/icons/icons/csv-document.svg"},

        // Презентации
        {"ppt",  ":/icons/icons/ppt-document.svg"},
        {"pptx", ":/icons/icons/ppt-document.svg"},

        // Изображения
        {"jpg",  ":/icons/icons/image-document.svg"},
        {"jpeg", ":/icons/icons/image-document.svg"},
        {"png",  ":/icons/icons/image-document.svg"},
        {"gif",  ":/icons/icons/image-document.svg"},
        {"bmp",  ":/icons/icons/image-document.svg"},
        {"svg",  ":/icons/icons/image-document.svg"},

        // Видео
        {"mp4",  ":/icons/icons/mp4-document.svg"},
        {"avi",  ":/icons/icons/video-document.svg"},
        {"mkv",  ":/icons/icons/video-document.svg"},
        {"mov",  ":/icons/icons/video-document.svg"},

        // Аудио
        {"mp3",  ":/icons/icons/audio-document.svg"},
        {"wav",  ":/icons/icons/audio-document.svg"},
        {"flac", ":/icons/icons/flash-document.svg"},

        // Архивы
        {"zip",  ":/icons/icons/zip-document.svg"},
        {"rar",  ":/icons/icons/zip-document.svg"},
        {"7z",   ":/icons/icons/zip-document.svg"},
        {"tar",  ":/icons/icons/zip-document.svg"},
        {"gz",   ":/icons/icons/zip-document.svg"},

        // Веб/разметка
        {"html", ":/icons/icons/html-document.svg"},
        {"htm",  ":/icons/icons/html-document.svg"},
        {"xml",  ":/icons/icons/xml-document.svg"},
        {"json",":/icons/icons/unknown-document.svg"},

        // Прочее
        {"exe",  ":/icons/icons/exe-document.svg"},
        {"ai",   ":/icons/icons/ai-document.svg"},
        {"psd",  ":/icons/icons/psd-document.svg"},
        {"eps",  ":/icons/icons/eps-document.svg"},
        {"key",  ":/icons/icons/keynote-document.svg"},
        {"pgs",  ":/icons/icons/pages-document.svg"},
        {"vis",  ":/icons/icons/visio-document.svg"},
    };

    // Если расширение не найдено — возвращаем иконку "неизвестного" файла
    return iconMap.value(extension, ":/icons/icons/unknown-document.svg");
}

// Статические члены делегата
QMap<ChatMessage::MessageStatus, QSvgRenderer*> ChatMessageDelegate::m_statusRenderers;
QMap<QString, QPixmap> ChatMessageDelegate::m_iconCache;
bool ChatMessageDelegate::m_renderersInitialized = false;

ChatMessageDelegate::ChatMessageDelegate(const ChatMessageModel* model, QObject *parent)
    : QStyledItemDelegate(parent),
      m_model(model)
{
    // Инициализируем SVG-рендереры статуса один раз на процесс
    initRenderers(this);
}

ChatMessageDelegate::~ChatMessageDelegate()
{
    // Чистим кеш QTextDocument для сообщений
    qDeleteAll(m_documentCache);
    m_documentCache.clear();
}

void ChatMessageDelegate::initRenderers(QObject* parent)
{
    // Защита от повторной инициализации статиков
    if (m_renderersInitialized) return;

    qDebug() << "[Delegate] Инициализация SVG-рендереров...";

    // Иконки статуса сообщения (отправка/отправлено/доставлено/прочитано)
    m_statusRenderers[ChatMessage::Sending]   =
        new QSvgRenderer(QString(":/icons/icons/clock_icon.svg"), parent);
    m_statusRenderers[ChatMessage::Sent]      =
        new QSvgRenderer(QString(":/icons/icons/message_send_icon.svg"), parent);
    m_statusRenderers[ChatMessage::Delivered] =
        new QSvgRenderer(QString(":/icons/icons/message_read_icon.svg"), parent);
    // При необходимости сюда можно добавить статус Read и др.

    m_renderersInitialized = true;
    qDebug() << "[Delegate] Рендереры созданы.";
}



void ChatMessageDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    // Базовая подготовка
    QStyleOptionViewItem opt = option;
    QStyledItemDelegate::paint(painter, opt, index);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Шрифт для метаданных (время, "изм.")
    QFont metaFont = option.font;
    metaFont.setPointSizeF(metaFont.pointSizeF() * 0.75);
    QFontMetrics metaFm(metaFont);

    // Достаем ChatMessage из UserRole
    ChatMessage message = index.data(Qt::UserRole).value<ChatMessage>();
    const QRect& originalRect = option.rect;
    QFontMetrics fm(painter->font());

    // Базовые константы геометрии
    const int margin          = 10;
    const int padding         = 10;
    const int borderRadius    = 15;
    const int verticalSpacing = 6;
    const int minBubbleWidth  = 100;

    int currentY   = originalRect.top() + verticalSpacing / 2;
    int iconSize   = 32 + padding;
    if (message.fileId.isEmpty())
        iconSize = 0;  // Нет файла – нет иконки

    // --------- Расчет блока цитаты (reply) ---------
    int quoteHeight = 0;
    int quoteTextWidth = 0;
    if (message.replyToId > 0) {
        ChatMessage repliedMsg;
        m_model->getMessageById(message.replyToId, repliedMsg);

        QString fromUser = repliedMsg.fromUser.isEmpty()
                               ? "НЕ ЗАГРУЖЕНО"
                               : repliedMsg.fromUser;

        QString payload;
        if (repliedMsg.payload.isEmpty()) {
            if (!repliedMsg.fileName.isEmpty())
                payload = repliedMsg.fileName;
            else
                payload = "НЕ ЗАГРУЖЕНО";
        } else {
            payload = repliedMsg.payload;
        }

        int fromUserWidth = fm.horizontalAdvance(fromUser);
        int payloadWidth  = fm.horizontalAdvance(payload);
        quoteTextWidth = std::min(fromUserWidth, payloadWidth)
                         + 3 * padding + 5;
        if (quoteTextWidth > 400)
            quoteTextWidth = 400;
        quoteHeight = fm.height() * 2 + 15;
    }

    // --------- Ограничение ширины текста ---------
    int textMaxWidth = originalRect.width() * 0.75 - 2 * padding;
    if (textMaxWidth <= 0)  textMaxWidth = 400;
    if (textMaxWidth > 400) textMaxWidth = 400;

    // Ключ кэша QTextDocument: (id или -row, ширина)
    QPair<qint64, int> cacheKey(message.id > 0 ? message.id : -index.row(),
                                textMaxWidth);
    QTextDocument* doc = m_documentCache.value(cacheKey, nullptr);

    // Высота и фактическая ширина текста
    qreal textHeight = doc
                           ? doc->size().height()
                           : fm.boundingRect(QRect(0, 0, textMaxWidth, 0),
                                             Qt::TextWrapAnywhere,
                                             message.payload).height();

    qreal textActualWidth = doc
                                ? doc->idealWidth()
                                : fm.boundingRect(QRect(0, 0, textMaxWidth, 0),
                                                  Qt::TextWrapAnywhere,
                                                  message.payload).width();

    // Ширина блока имени файла (если есть вложение)
    qreal fileNameWidth = iconSize + padding * 3
                          + fm.horizontalAdvance(
                              fm.elidedText(message.fileName,
                                            Qt::ElideMiddle, 300));
    if (fileNameWidth > textMaxWidth)
        fileNameWidth = textMaxWidth;

    if (message.payload.isEmpty()) {
        textHeight      = 0;
        textActualWidth = 0;
    }

    // --------- Метаданные (время + "(изм.)") ---------
    QString metaText;
    if (message.isEdited)
        metaText += "(изм.) ";
    metaText += message.timestamp.mid(11, 5); // HH:MM

    int metaTextWidth  = metaFm.horizontalAdvance(metaText);
    int metaDataHeight = metaFm.height();
    if (message.isOutgoing)
        metaTextWidth += fm.height(); // место под иконку статуса

    // --------- Итоговые размеры пузыря ---------
    int bubbleContentWidth = std::max({
        static_cast<int>(textActualWidth),
        metaTextWidth,
        quoteTextWidth,
        static_cast<int>(fileNameWidth)
    });

    int bubbleContentHeight =
        textHeight + metaDataHeight + quoteHeight + iconSize;

    QRect bubbleRect(0, 0,
                     bubbleContentWidth + 2 * padding,
                     bubbleContentHeight + 2 * padding);

    if (bubbleRect.width() < minBubbleWidth)
        bubbleRect.setWidth(minBubbleWidth);

    // Выравнивание: outgoing справа, incoming слева
    if (message.isOutgoing)
        bubbleRect.moveTopRight(QPoint(originalRect.right() - margin, currentY));
    else
        bubbleRect.moveTopLeft(QPoint(originalRect.left() + margin, currentY));

    // Область текста внутри пузыря
    QRect textDrawRect = bubbleRect.adjusted(padding, padding,
                                             -padding, -padding);
    textDrawRect.setHeight(textHeight);
    textDrawRect.moveTop(bubbleRect.top() + quoteHeight + padding);

    // --------- Рисуем фон пузыря ---------
    QColor bubbleColor = message.isOutgoing
                             ? QColor("#753955")
                             : QColor("#3D383A");
    painter->setBrush(bubbleColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(bubbleRect, borderRadius, borderRadius);

    // --------- Вложенный файл (иконка + имя) ---------
    if (!message.fileId.isEmpty()) {
        // Прямоугольник под иконку
        QRect iconRect = bubbleRect.adjusted(
            padding,
            quoteHeight + padding,
            -bubbleRect.width() + padding + iconSize,
            -bubbleRect.height() + iconSize + padding + quoteHeight
            );
        iconRect.setSize(QSize(iconSize, iconSize));

        // Получаем и кешируем иконку по пути
        QString iconPath = getFileIconPath(message.fileName);
        if (!m_iconCache.contains(iconPath)) {
            QSvgRenderer renderer(iconPath);
            QPixmap pm(32, 32);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            renderer.render(&p);
            m_iconCache.insert(iconPath, pm);
        }

        painter->drawPixmap(iconRect, m_iconCache[iconPath]);

        // Прямоугольник под имя файла
        QRect fileNameRect = iconRect.adjusted(
            iconSize + padding, 0,
            bubbleRect.width() - iconSize - 2 * padding, 0
            );
        painter->setPen(Qt::white);
        QString fileName = message.fileName.isEmpty()
                               ? "[файл]"
                               : message.fileName;
        painter->drawText(
            fileNameRect,
            Qt::AlignVCenter | Qt::AlignLeft,
            fm.elidedText(fileName, Qt::ElideMiddle, fileNameRect.width())
            );

        // Сдвигаем область текста ниже блока файла
        textDrawRect.moveTop(textDrawRect.top() + iconSize + 5);
    }

    // --------- Блок цитаты (reply bubble) ---------
    if (quoteHeight > 0) {
        ChatMessage repliedMsg;
        QString fromUser;
        QString payload;

        if (m_model->getMessageById(message.replyToId, repliedMsg)) {
            fromUser = repliedMsg.fromUser;
            if (repliedMsg.payload.isEmpty()) {
                if (!repliedMsg.fileName.isEmpty())
                    payload = repliedMsg.fileName;
                else
                    payload = "НЕ ЗАГРУЖЕНО";
            } else {
                payload = repliedMsg.payload;
            }
        }

        QRectF quoteRect = bubbleRect.adjusted(
            padding, padding,
            -padding,
            -padding - metaDataHeight - textHeight
                - ((message.fileId.isEmpty()) ? 0 : iconSize + padding)
            );

        QColor quoteRectColor = message.isOutgoing
                                    ? QColor("#ff7fbb").darker(150)
                                    : QColor("#ff7fbb");
        painter->setBrush(quoteRectColor);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(quoteRect, 5, 5);

        // Цветная вертикальная полоска слева
        QRectF colorBarRect = quoteRect;
        colorBarRect.setWidth(5);
        painter->setBrush(QColor("#ffffff"));
        painter->drawRoundedRect(colorBarRect, 0, 0);

        // Текст цитаты (от кого и текст/имя файла)
        QRectF quoteTextRect = quoteRect.adjusted(
            colorBarRect.width() + padding, 5,
            -padding, -5
            );
        painter->setPen(QColor("#ffffff"));
        painter->drawText(
            quoteTextRect,
            Qt::AlignTop | Qt::AlignLeft,
            fm.elidedText(fromUser, Qt::ElideRight,
                          quoteRect.width() - 5 - 2 * padding)
            );

        QRectF repliedTextRect = quoteTextRect.adjusted(0, fm.height(), 0, 0);
        painter->drawText(
            repliedTextRect,
            Qt::AlignTop | Qt::AlignLeft,
            fm.elidedText(payload, Qt::ElideRight,
                          quoteRect.width() - 5 - 2 * padding)
            );
    }

    // --------- Текст сообщения ---------
    painter->setPen(Qt::white);

    if (doc) {
        painter->save();
        painter->translate(textDrawRect.topLeft());
        painter->setPen(Qt::white);
        doc->drawContents(painter);
        painter->restore();
    } else {
        QTextOption textOption(Qt::AlignLeft | Qt::AlignTop);
        textOption.setWrapMode(QTextOption::WrapAnywhere);
        painter->drawText(textDrawRect, message.payload, textOption);
    }

    // --------- Метаданные (время + статус/галочки) ---------
    QRect baseMetaRect = bubbleRect.adjusted(
        padding, padding,
        -padding, -padding
        );
    baseMetaRect.setHeight(metaDataHeight);
    baseMetaRect.moveBottom(bubbleRect.bottom() - padding);

    painter->setFont(metaFont);

    if (message.isOutgoing) {
        int statusIconSize = fm.height() - 2;

        QRect iconRect(
            baseMetaRect.right() - statusIconSize,
            baseMetaRect.top()
                + (baseMetaRect.height() - statusIconSize) / 2,
            statusIconSize, statusIconSize
            );

        QRect textMetaRect = baseMetaRect;
        textMetaRect.setRight(iconRect.left() - 3);

        // Цвет времени зависит от статуса (Read – синий)
        QPen textPen = (message.status == ChatMessage::Read)
                           ? QColor(70, 150, 255)
                           : Qt::gray;
        painter->setPen(textPen);
        painter->drawText(textMetaRect,
                          Qt::AlignRight | Qt::AlignVCenter,
                          metaText);

        // Для статуса Read используется тот же SVG, что и Delivered,
        // а цветом показывается "прочитано"
        ChatMessage::MessageStatus statusToRender = message.status;
        if (statusToRender == ChatMessage::Read)
            statusToRender = ChatMessage::Delivered;

        QSvgRenderer* renderer =
            m_statusRenderers.value(statusToRender, nullptr);

        if (renderer && renderer->isValid()) {
            QPixmap pixmap(iconRect.size());
            pixmap.fill(Qt::transparent);

            // Рендер SVG в pixmap
            QPainter pixmapPainter(&pixmap);
            renderer->render(&pixmapPainter);
            pixmapPainter.end();

            // Перекрашиваем иконку нужным цветом через CompositionMode_SourceIn
            QPainter effectPainter(&pixmap);
            effectPainter.setCompositionMode(
                QPainter::CompositionMode_SourceIn);
            QColor iconColor = (message.status == ChatMessage::Read)
                                   ? QColor(70, 150, 255)
                                   : Qt::gray;
            effectPainter.fillRect(pixmap.rect(), iconColor);
            effectPainter.end();

            painter->drawPixmap(iconRect, pixmap);
        } else {
            // Фоллбек: только текст времени
            painter->setPen(Qt::gray);
            painter->drawText(baseMetaRect,
                              Qt::AlignRight | Qt::AlignVCenter,
                              metaText);
        }
    } else {
        // Для входящих – только время, без иконки статуса
        painter->setPen(Qt::gray);
        painter->drawText(baseMetaRect,
                          Qt::AlignRight | Qt::AlignVCenter,
                          metaText);
    }

    painter->restore();
}


QSize ChatMessageDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    // Достаем сообщение из UserRole
    ChatMessage message = index.data(Qt::UserRole).value<ChatMessage>();

    QFontMetrics fm(option.font);
    QFont metaFont = option.font;
    metaFont.setPointSizeF(metaFont.pointSizeF() * 0.75);
    QFontMetrics metaFm(metaFont);

    const int padding         = 10;
    const int verticalSpacing = 6;
    const int minBubbleWidth  = 100;

    // --------- Высота блока цитаты (reply) ---------
    int quoteHeight = 0;
    int quoteTextWidth = 0;

    if (message.replyToId > 0) {
        ChatMessage repliedMsg;
        m_model->getMessageById(message.replyToId, repliedMsg);

        QString fromUser = repliedMsg.fromUser.isEmpty()
                           ? "НЕ ЗАГРУЖЕНО"
                           : repliedMsg.fromUser;

        QString payload;
        if (repliedMsg.payload.isEmpty()) {
            if (!repliedMsg.fileName.isEmpty())
                payload = repliedMsg.fileName;
            else
                payload = "НЕ ЗАГРУЖЕНО";
        } else {
            payload = repliedMsg.payload;
        }

        int fromUserWidth = fm.horizontalAdvance(fromUser);
        int payloadWidth  = fm.horizontalAdvance(payload);
        quoteTextWidth = std::max(fromUserWidth, payloadWidth)
                         + 3 * padding + 5;

        if (quoteTextWidth > 400)
            quoteTextWidth = 400;

        quoteHeight = fm.height() * 2 + 15;
    }

    // --------- Максимальная ширина текста ---------
    int textMaxWidth = option.rect.width() * 0.75 - 2 * padding;
    if (textMaxWidth <= 0)  textMaxWidth = 400;
    if (textMaxWidth > 400) textMaxWidth = 400;

    // --------- Кеш QTextDocument ---------
    QTextDocument* doc = nullptr;
    QPair<qint64, int> cacheKey(
        message.id > 0 ? message.id : -index.row(),
        textMaxWidth
    );

    if (m_documentCache.contains(cacheKey)) {
        doc = m_documentCache.value(cacheKey);

        // Если текст изменился (редактирование) — обновляем документ
        if (doc->toPlainText() != message.payload) {
            doc->setPlainText(message.payload);
            doc->setTextWidth(textMaxWidth);
        }
    } else {
        // Создаем новый QTextDocument для текста
        doc = new QTextDocument();
        doc->setDefaultFont(option.font);
        doc->setPlainText(message.payload);
        doc->setTextWidth(textMaxWidth);
        m_documentCache.insert(cacheKey, doc);
    }

    qreal textHeight      = doc->size().height();
    qreal textActualWidth = doc->idealWidth();

    // --------- Метаданные (время + статус) ---------
    QString metaText;
    if (message.isEdited)
        metaText += "(изм.) ";
    metaText += message.timestamp.mid(11, 5);

    int metaTextWidth  = metaFm.horizontalAdvance(metaText);
    int metaDataHeight = metaFm.height();

    if (message.isOutgoing)
        metaTextWidth += metaFm.height();  // место под иконку статуса

    // --------- Блок файла (иконка + имя) ---------
    int iconSize = 32 + padding;
    qreal fileNameWidth = iconSize + padding * 3
                          + fm.horizontalAdvance(
                                fm.elidedText(message.fileName,
                                              Qt::ElideMiddle,
                                              textMaxWidth));

    if (fileNameWidth > textMaxWidth)
        fileNameWidth = textMaxWidth;

    if (message.fileId.isEmpty()) {
        iconSize      = 0;
        fileNameWidth = 0;
    }

    // --------- Итоговая ширина и высота пузыря ---------
    int bubbleContentWidth = std::max({
        static_cast<int>(textActualWidth),
        metaTextWidth,
        quoteTextWidth,
        static_cast<int>(fileNameWidth)
    });

    if (bubbleContentWidth < minBubbleWidth)
        bubbleContentWidth = minBubbleWidth;

    if (message.payload.isEmpty())
        textHeight = 0;

    int bubbleContentHeight = textHeight
                              + metaDataHeight
                              + (2 * padding)
                              + quoteHeight
                              + iconSize;

    int totalHeight = bubbleContentHeight + verticalSpacing;

    return QSize(bubbleContentWidth + 2 * padding, totalHeight);
}

void ChatMessageDelegate::clearSizeHintCache()
{
    // Кеш sizeHint больше не используется (был m_sizeHintCache),
    // поэтому этот метод — пустышка или можно убрать
    m_sizeHintCache.clear();
    qDebug() << "[Delegate] Кеш размеров sizeHint очищен";
}

void ChatMessageDelegate::clearCaches()
{
    // Очищаем все QTextDocument из кеша (освобождаем память)
    qDeleteAll(m_documentCache);
    m_documentCache.clear();
    qDebug() << "[Delegate] Все кеши QTextDocument очищены";
}

bool ChatMessageDelegate::editorEvent(QEvent *event,
                                      QAbstractItemModel *model,
                                      const QStyleOptionViewItem &option,
                                      const QModelIndex &index)
{
    ChatMessage message = index.data(Qt::UserRole).value<ChatMessage>();

    // --------- Двойной клик по сообщению => запрос реплая ---------
    if (event->type() == QEvent::MouseButtonDblClick) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton &&
            option.rect.contains(mouseEvent->pos()))
        {
            qDebug() << "emit replyRequested(index);";
            emit replyRequested(index);
            return true; // событие обработано делегатом
        }
    }

    // --------- Клик по вложению (иконка / имя файла) ---------
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);

        if (option.rect.contains(mouseEvent->pos()) &&
            !message.fileId.isEmpty() &&
            mouseEvent->button() == Qt::LeftButton)
        {
            QPoint pos = mouseEvent->pos();
            const QRect& originalRect = option.rect;

            // Дублируем геометрию из paint/sizeHint для точного хита
            const int margin          = 10;
            const int padding         = 10;
            const int borderRadius    = 15;
            const int verticalSpacing = 6;
            const int minBubbleWidth  = 100;

            QFontMetrics fm(option.font);
            QFont metaFont = option.font;
            metaFont.setPointSizeF(metaFont.pointSizeF() * 0.75);
            QFontMetrics metaFm(metaFont);

            int currentY   = originalRect.top() + verticalSpacing / 2;
            int iconSize   = 32 + padding;
            int quoteHeight = 0;
            int quoteTextWidth = 0;

            // Высота/ширина блока цитаты
            if (message.replyToId > 0) {
                ChatMessage repliedMsg;
                m_model->getMessageById(message.replyToId, repliedMsg);

                QString fromUser = repliedMsg.fromUser.isEmpty()
                                   ? "НЕ ЗАГРУЖЕНО"
                                   : repliedMsg.fromUser;

                QString payload;
                if (repliedMsg.payload.isEmpty()) {
                    if (!repliedMsg.fileName.isEmpty())
                        payload = repliedMsg.fileName;
                    else
                        payload = repliedMsg.payload.isEmpty()
                                  ? "НЕ ЗАГРУЖЕНО"
                                  : repliedMsg.payload;
                } else {
                    payload = repliedMsg.payload;
                }

                int fromUserWidth = fm.horizontalAdvance(fromUser);
                int payloadWidth  = fm.horizontalAdvance(payload);
                quoteTextWidth = std::min(fromUserWidth, payloadWidth)
                                 + 3 * padding + 5;
                if (quoteTextWidth > 400)
                    quoteTextWidth = 400;
                quoteHeight = fm.height() * 2 + 15;
            }

            // Макс. ширина текста
            int textMaxWidth = originalRect.width() * 0.75 - 2 * padding;
            if (textMaxWidth <= 0)  textMaxWidth = 400;
            if (textMaxWidth > 400) textMaxWidth = 400;

            // Берем QTextDocument из кеша (как в paint/sizeHint)
            QPair<qint64, int> cacheKey(
                message.id > 0 ? message.id : -index.row(),
                textMaxWidth
            );
            QTextDocument* doc = m_documentCache.value(cacheKey, nullptr);

            qreal textHeight = doc
                ? doc->size().height()
                : fm.boundingRect(QRect(0, 0, textMaxWidth, 0),
                                  Qt::TextWrapAnywhere,
                                  message.payload).height();

            qreal textActualWidth = doc
                ? doc->idealWidth()
                : fm.boundingRect(QRect(0, 0, textMaxWidth, 0),
                                  Qt::TextWrapAnywhere,
                                  message.payload).width();

            qreal fileNameWidth = iconSize + padding * 3
                                  + fm.horizontalAdvance(
                                        fm.elidedText(message.fileName,
                                                      Qt::ElideMiddle,
                                                      textMaxWidth));
            if (fileNameWidth > textMaxWidth)
                fileNameWidth = textMaxWidth;

            if (message.payload.isEmpty())
                textHeight = 0;

            // Метаданные (время/статус)
            QString metaText;
            if (message.isEdited)
                metaText += "(изм.) ";
            metaText += message.timestamp.mid(11, 5);
            int metaTextWidth  = metaFm.horizontalAdvance(metaText);
            int metaDataHeight = metaFm.height();
            if (message.isOutgoing)
                metaTextWidth += metaFm.height();

            // Итоговый размер пузыря
            int bubbleContentWidth = std::max({
                static_cast<int>(textActualWidth),
                metaTextWidth,
                quoteTextWidth,
                static_cast<int>(fileNameWidth)
            });
            int bubbleContentHeight =
                textHeight + metaDataHeight + quoteHeight + iconSize;

            QRect bubbleRect(0, 0,
                             bubbleContentWidth + 2 * padding,
                             bubbleContentHeight + 2 * padding);
            if (bubbleRect.width() < minBubbleWidth)
                bubbleRect.setWidth(minBubbleWidth);

            if (message.isOutgoing)
                bubbleRect.moveTopRight(
                    QPoint(originalRect.right() - margin, currentY));
            else
                bubbleRect.moveTopLeft(
                    QPoint(originalRect.left() + margin, currentY));

            // Области иконки и текста файла
            QRect iconRect = bubbleRect.adjusted(
                padding,
                quoteHeight + padding,
                -bubbleRect.width() + padding + iconSize,
                -bubbleRect.height() + iconSize + padding + quoteHeight
            );
            QRect fileNameRect = iconRect.adjusted(
                iconSize + padding, 0,
                bubbleRect.width() - iconSize - 2 * padding, 0
            );

            // Если клик попал по иконке или имени файла — запрашиваем скачивание
            if (iconRect.contains(pos) || fileNameRect.contains(pos)) {
                qDebug() << "emit fileDownloadRequested(message.fileId, "
                            "message.fileUrl, message.fileName);";
                emit fileDownloadRequested(
                    message.fileId,
                    message.fileUrl,
                    message.fileName
                );
                return true;
            }
        }
    }

    // Остальные события обрабатываются базовой реализацией
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
