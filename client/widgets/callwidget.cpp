#include "callwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include "callservice.h"

CallWidget::CallWidget(CallService* service, QWidget* parent)
    : QWidget(parent)
    , m_callService(service)
    , m_callerLabel(new QLabel("Входящий звонок", this))
    , m_stateLabel(new QLabel("Ожидание...", this))
    , m_durationLabel(new QLabel("00:00", this))
    , m_acceptBtn(new QPushButton("Принять", this))
    , m_rejectBtn(new QPushButton("Отклонить", this))
    , m_endBtn(new QPushButton("Завершить", this))
{
    setWindowTitle("Звонок");
    setGeometry(100, 100, 500, 250);

    // Отдельное верхнее окно, поверх остальных
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowModal, false);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 20, 20, 20);

    // Оформление текста
    m_callerLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    m_stateLabel->setStyleSheet("font-size: 14px;");
    m_durationLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: blue;");

    layout->addWidget(m_callerLabel);
    layout->addWidget(m_stateLabel);
    layout->addWidget(m_durationLabel);
    layout->addSpacing(20);

    // Кнопки управления
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);

    m_acceptBtn->setMinimumSize(100, 50);
    m_acceptBtn->setStyleSheet("background-color: green; color: white; font-weight: bold; font-size: 12px;");

    m_rejectBtn->setMinimumSize(100, 50);
    m_rejectBtn->setStyleSheet("background-color: red; color: white; font-weight: bold; font-size: 12px;");

    m_endBtn->setMinimumSize(100, 50);
    m_endBtn->setStyleSheet("background-color: orange; color: white; font-weight: bold; font-size: 12px;");

    btnLayout->addWidget(m_acceptBtn);
    btnLayout->addWidget(m_rejectBtn);
    btnLayout->addWidget(m_endBtn);

    layout->addLayout(btnLayout);
    setLayout(layout);

    // Сигналы кнопок наружу
    connect(m_acceptBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "[CW] >>> ACCEPT BUTTON CLICKED";
        emit acceptClicked();
        qDebug() << "[CW] ✅ acceptClicked() emitted";
    });
    connect(m_rejectBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "[CW] >>> REJECT BUTTON CLICKED";
        emit rejectClicked();
        qDebug() << "[CW] ✅ rejectClicked() emitted";
    });
    connect(m_endBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "[CW] >>> END CALL BUTTON CLICKED";
        emit endCallClicked();
        qDebug() << "[CW] ✅ endCallClicked() emitted";
    });

    // По умолчанию режим входящего вызова
    m_endBtn->hide();
    m_acceptBtn->show();
    m_rejectBtn->show();

    qDebug() << "[CW] CallWidget created and ready";
}

void CallWidget::setCallerName(const QString& name)
{
    m_callerLabel->setText("Звонок: " + name);
    qDebug() << "[CW] Caller name set to:" << name;
}

void CallWidget::setCallState(const QString& state)
{
    m_stateLabel->setText(state);
    qDebug() << "[CW] State changed to:" << state;

    // Управление видимостью кнопок по состоянию
    if (state.contains("Входящий") || state.contains("Ringing")) {
        qDebug() << "[CW] INCOMING mode: showing accept/reject, hiding end";
        m_acceptBtn->show();
        m_rejectBtn->show();
        m_endBtn->hide();
    } else if (state.contains("Ожидание")) {
        qDebug() << "[CW] OUTGOING mode: hiding all buttons (waiting for answer)";
        m_acceptBtn->hide();
        m_rejectBtn->hide();
        m_endBtn->show();
    } else if (state.contains("активен") || state.contains("Разговор")) {
        qDebug() << "[CW] CONNECTED mode: showing end, hiding accept/reject";
        m_acceptBtn->hide();
        m_rejectBtn->hide();
        m_endBtn->show();
    }
}

void CallWidget::onDurationChanged(const QString& duration)
{
    m_durationLabel->setText(duration);
}

void CallWidget::closeEvent(QCloseEvent *event)
{
    // При закрытии окна гарантированно завершаем звонок
    if (m_callService)
        m_callService->endCall();
    event->accept();
}
