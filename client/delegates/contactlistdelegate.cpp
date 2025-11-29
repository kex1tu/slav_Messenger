#include "contactlistdelegate.h"
#include "contactlistmodel.h"
#include <QPainter>
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QPainterPath>
ContactListDelegate::ContactListDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{}

void ContactListDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    painter->save();

    // Фон при выделении/hover
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect,
                          option.palette.color(QPalette::AlternateBase));
    }

    // Данные из модели
    QString displayName   = index.data(ContactListModel::DisplayNameRole).toString();
    QString lastMessage   = index.data(ContactListModel::LastMessageRole).toString();
    bool isOnline         = index.data(ContactListModel::IsOnlineRole).toBool();
    bool isTyping         = index.data(ContactListModel::IsTypingRole).toBool();
    int unreadCount       = index.data(ContactListModel::UnreadCountRole).toInt();
    bool isLastOutgoing   = index.data(ContactListModel::IsLastMessageOutgoingRole).toBool();
    QString lastMessageTime = index.data(ContactListModel::LastMessageTimestampRole).toString();
    bool isPinned         = index.data(ContactListModel::IsPinnedRole).toBool();
    QString avatarUrl     = index.data(ContactListModel::AvatarRole).toString();

    const int padding = 10;
    const int spacing = 10;
    const int pinWidth = 5;
    int avatarSize = 32;

    // Загрузка аватара
    QPixmap avatar;
    if (!avatarUrl.isEmpty()) {
        qDebug() << "[ContactListDelegate] Avatar url not empty!";
        avatar.load(avatarUrl);
        if (avatar.isNull()) {
            qDebug() << "[ContactListDelegate] WARNING: can't load avatarUrl, fallback to default";
            avatar.load(":/icons/icons/default_avatar.png");
        }
    } else {
        avatar.load(":/icons/icons/default_avatar.png");
    }

    if (avatar.isNull()) {
        qDebug() << "[ContactListDelegate] ERROR: default avatar missing, drawing placeholder";
        avatar = QPixmap(avatarSize, avatarSize);
        avatar.fill(Qt::gray);
        QPainter pIcon(&avatar);
        pIcon.setPen(Qt::white);
        pIcon.setFont(QFont("Segoe UI", 16, QFont::Bold));
        pIcon.drawText(avatar.rect(), Qt::AlignCenter, "?");
    }

    // Округляем аватар в круг
    QPixmap rounded(avatarSize, avatarSize);
    rounded.fill(Qt::transparent);
    {
        QPainter p(&rounded);
        QPainterPath path;
        path.addEllipse(0, 0, avatarSize, avatarSize);
        p.setClipPath(path);
        p.drawPixmap(0, 0,
                     avatar.scaled(avatarSize, avatarSize,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation));
    }

    int avatarX = option.rect.x() + padding / 2;
    int avatarY = option.rect.y()
                  + (option.rect.height() - avatarSize) / 2;
    painter->drawPixmap(avatarX, avatarY, rounded);

    // Область контента справа от аватара
    QRect contentRect = option.rect.adjusted(
        avatarSize + padding + (isPinned ? pinWidth : 0),
        padding / 2,
        -padding,
        -padding / 2
    );

    // Нижняя строка: typing / "Вы: msg" / msg
    QString bottomText = lastMessage;
    QColor bottomTextColor = Qt::gray;
    if (isTyping) {
        bottomText = "печатает...";
        bottomTextColor = QColor("#F4ABC4");
    } else {
        if (isLastOutgoing && !lastMessage.isEmpty())
            bottomText = "Вы: " + lastMessage;
        else
            bottomText = lastMessage;
    }

    // Метка времени HH:mm
    QString timeLabel;
    if (!lastMessageTime.isEmpty()) {
        QDateTime dt = QDateTime::fromString(lastMessageTime, Qt::ISODate);
        if (dt.isValid()) {
            timeLabel = dt.toLocalTime().toString("HH:mm");
        }
    }

    QFont timeFont = painter->font();
    QFontMetrics timeFm(timeFont);
    int timeWidth = timeFm.horizontalAdvance(timeLabel);

    // Верх/низ элемента
    QRect topRect(contentRect.left(),
                  contentRect.top(),
                  contentRect.width(),
                  contentRect.height() / 2);
    QRect bottomRect(contentRect.left(),
                     contentRect.top() + contentRect.height() / 2,
                     contentRect.width(),
                     contentRect.height() / 2);

    QRect topLeftRect = topRect.adjusted(
        0, 0,
        -timeWidth - spacing, 0
    );
    QRect topRightRect(
        topRect.right() - timeWidth,
        topRect.top(),
        timeWidth,
        topRect.height()
    );

    // Имя контакта (жирное, если online)
    QFont nameFont = painter->font();
    nameFont.setBold(isOnline);
    painter->setFont(nameFont);
    painter->setPen(isOnline ? Qt::white : QColor("#EAEAEA"));
    painter->drawText(topLeftRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      displayName);

    // Время последнего сообщения
    painter->setFont(timeFont);
    painter->setPen(Qt::gray);
    painter->drawText(topRightRect,
                      Qt::AlignCenter | Qt::AlignVCenter,
                      timeLabel);

    // Текст последнего сообщения / "печатает..."
    int badgeWidth = 20;

    QFont bottomFont = option.font;
    bottomFont.setPointSize(bottomFont.pointSize() - 2);
    bottomFont.setBold(false);
    QFontMetrics fm(bottomFont);
    painter->setFont(bottomFont);
    painter->setPen(bottomTextColor);

    QRect msgRect = bottomRect.adjusted(
        0, 0,
        -topRightRect.width() - spacing, 0
    );
    QString elided = fm.elidedText(bottomText, Qt::ElideRight, msgRect.width());
    painter->drawText(msgRect,
                      Qt::AlignLeft | Qt::AlignTop,
                      elided);

    // Бейдж непрочитанных
    if (unreadCount > 0) {
        int badgeCx = topRightRect.center().x();
        int badgeCy = bottomRect.center().y();
        QRect badgeRect(badgeCx - badgeWidth / 2,
                        badgeCy - badgeWidth / 2,
                        badgeWidth, badgeWidth);

        painter->setRenderHint(QPainter::Antialiasing);
        painter->setBrush(QColor(30, 144, 255));
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(badgeRect);

        painter->setPen(Qt::white);
        bottomFont.setPointSize(bottomFont.pointSize() - 4);
        bottomFont.setBold(true);
        painter->setFont(bottomFont);

        QString badgeText = unreadCount > 99
                            ? "99+"
                            : QString::number(unreadCount);
        painter->drawText(badgeRect, Qt::AlignCenter, badgeText);
    }

    // Вертикальная полоса закрепленного чата
    if (isPinned) {
        painter->save();
        QColor pinColor("#ff004c");
        QRect pinRect(option.rect.x(),
                      option.rect.y(),
                      pinWidth,
                      option.rect.height());
        painter->setBrush(pinColor);
        painter->setPen(Qt::NoPen);
        painter->drawRect(pinRect);
        painter->restore();
    }

    painter->restore();
}

QSize ContactListDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    Q_UNUSED(index);

    // Высота строки имени
    QFont nameFont = option.font;
    int nameHeight = QFontMetrics(nameFont).height();

    // Высота строки сообщения (чуть меньше)
    QFont messageFont = option.font;
    messageFont.setPointSize(messageFont.pointSize() - 2);
    int messageHeight = QFontMetrics(messageFont).height();

    const int totalVerticalPadding = 10;
    const int spacingBetweenLines  = 4;

    int totalHeight = nameHeight
                      + messageHeight
                      + totalVerticalPadding
                      + spacingBetweenLines;

    // Ширина вычисляется видом (возвращаем -1)
    return QSize(-1, totalHeight);
}
