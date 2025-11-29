#include "requestitemwidget.h"
#include <QPainter>
#include <QPainterPath>

RequestItemWidget::RequestItemWidget(const QJsonObject& req, QWidget* parent)
    : QWidget(parent), m_request(req)
{
    // Извлекаем данные пользователя из JSON запроса
    QString name = req.value("fromDisplayname").toString();
    QString username = req.value("fromUsername").toString();

    // Создаем горизонтальный layout без отступов
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);     
    layout->setSpacing(12);                   

    // Создаем аватар размером 32x32
    int avatarSize = 32;
    QLabel* avatarLabel = new QLabel(this);
    avatarLabel->setFixedSize(avatarSize, avatarSize);

    // Загружаем аватар по умолчанию или создаем placeholder
    QPixmap avatar;
    avatar.load(":/icons/icons/default_avatar.png");
    if (avatar.isNull()) {
        // Создаем серый фон с инициалами если аватар не загрузился
        avatar = QPixmap(avatarSize, avatarSize);
        avatar.fill(Qt::gray);
        QPainter p(&avatar);
        p.setPen(Qt::white);
        p.setFont(QFont("Segoe UI", 14, QFont::Bold));
        p.drawText(avatar.rect(), Qt::AlignCenter, name.isEmpty() ? "?" : name.left(1).toUpper());
    }

    // Создаем закругленный аватар с антиалиасингом
    QPixmap rounded(avatarSize, avatarSize);
    rounded.fill(Qt::transparent);
    {
        QPainter p(&rounded);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        path.addEllipse(0, 0, avatarSize, avatarSize);
        p.setClipPath(path);
        p.drawPixmap(0, 0, avatar.scaled(avatarSize, avatarSize,
                                        Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation));
    }
    avatarLabel->setPixmap(rounded);

    // Создаем лейбл с именем и username
    QLabel* label = new QLabel(QString("%1 (%2)").arg(name, username), this);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    label->setFont(QFont("Arial", 13));
    label->setStyleSheet("color: white;");

    // Создаем кнопки принятия/отклонения
    QToolButton* acceptBtn = new QToolButton(this);
    QToolButton* rejectBtn = new QToolButton(this);
    acceptBtn->setText("Y");
    rejectBtn->setText("N");

    // Подключаем сигналы кнопок к слотам
    connect(acceptBtn, &QToolButton::clicked, this, [this] {
        emit accepted(m_request);
    });
    connect(rejectBtn, &QToolButton::clicked, this, [this] {
        emit rejected(m_request);
    });

    // Устанавливаем фиксированный размер кнопок 32x32
    acceptBtn->setFixedSize(32,32);
    rejectBtn->setFixedSize(32,32);

    // Добавляем элементы в layout: аватар, текст, растяжка, кнопки
    layout->addWidget(avatarLabel);
    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(acceptBtn);
    layout->addWidget(rejectBtn);
    setLayout(layout);
}
