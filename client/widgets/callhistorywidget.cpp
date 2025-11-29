#include "callhistorywidget.h"
#include <QDebug>
#include <QDateTime>
#include <QApplication>

CallHistoryWidget::CallHistoryWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    qDebug() << "[CallHistoryWidget] Виджет создан, инициализирован UI";
}

CallHistoryWidget::~CallHistoryWidget()
{
    qDebug() << "[CallHistoryWidget] Деструктор вызван, освобождаем ресурсы";
}

void CallHistoryWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 10, 0, 10);
    mainLayout->setSpacing(10);

    // Заголовок
    QLabel* titleLabel = new QLabel("☎️ Call History", this);
    mainLayout->addWidget(titleLabel);

    // Строка со сводной статистикой
    m_statsLabel = new QLabel("Loading statistics...", this);
    mainLayout->addWidget(m_statsLabel);

    // Список звонков
    m_callListWidget = new QListWidget(this);
    m_callListWidget->setAlternatingRowColors(true);
    m_callListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_callListWidget->setSpacing(3);

    connect(m_callListWidget, &QListWidget::itemDoubleClicked,
            this, &CallHistoryWidget::onItemDoubleClicked);
    connect(m_callListWidget, &QListWidget::itemClicked,
            this, &CallHistoryWidget::onItemClicked);
    mainLayout->addWidget(m_callListWidget);

    // Нижняя панель управления
    QHBoxLayout* controlLayout = new QHBoxLayout(this);
    controlLayout->setContentsMargins(10, 0, 10, 0);
    controlLayout->setSpacing(10);

    m_statusLabel = new QLabel("Ready", this);
    controlLayout->addWidget(m_statusLabel);
    controlLayout->addStretch();

    m_refreshBtn = new QPushButton("🔄 Refresh", this);
    m_refreshBtn->setFixedSize(100, 32);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &CallHistoryWidget::onRefreshClicked);
    controlLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(controlLayout);
    setLayout(mainLayout);

    qDebug() << "[CallHistoryWidget] UI полностью инициализирован";
}

void CallHistoryWidget::setCallHistory(const QJsonArray& calls)
{
    // Полная очистка текущей истории
    clearHistory();
    m_callListWidget->clear();
    m_calls.clear();

    // Заполняем внутренний список и QListWidget
    for (const QJsonValue& value : calls) {
        QJsonObject obj = value.toObject();
        CallItem item;
        item.callId          = obj["call_id"].toString();
        item.caller          = obj["caller"].toString();
        item.callee          = obj["callee"].toString();
        item.status          = obj["status"].toString();
        item.callType        = obj["call_type"].toString();
        item.startTime       = obj["start_time"].toString();
        item.endTime         = obj["end_time"].toString();
        item.durationSeconds = obj["duration_seconds"].toInt();
        m_calls.append(item);

        QListWidgetItem* listItem = new QListWidgetItem(m_callListWidget);
        listItem->setText(formatCallItem(item));
        listItem->setData(Qt::UserRole, m_calls.size() - 1);
        m_callListWidget->addItem(listItem);
    }

    m_statusLabel->setText(QString("✅ Loaded %1 calls").arg(calls.size()));

    // Подсчет агрегированной статистики
    int completed = 0, missed = 0, rejected = 0;
    int totalDuration = 0;
    for (const auto& call : m_calls) {
        if (call.status == "completed") {
            completed++;
            totalDuration += call.durationSeconds;
        } else if (call.status == "missed") {
            missed++;
        } else if (call.status == "rejected") {
            rejected++;
        }
    }
    int avgDuration = completed > 0 ? totalDuration / completed : 0;
    QString statsText = QString("✅ %1 completed | ⏭️ %2 missed | ❌ %3 rejected | ⏱️ Avg: %4s")
                            .arg(completed).arg(missed).arg(rejected).arg(avgDuration);
    m_statsLabel->setText(statsText);

    qDebug() << "[CALL_HISTORY] Loaded" << calls.size() << "calls";
    this->show();
}

void CallHistoryWidget::showLoading(bool loading)
{
    m_refreshBtn->setEnabled(!loading);
    if (loading) {
        m_statusLabel->setText("⏳ Loading...");
    }
    qDebug() << "[CALL_HISTORY] showLoading:" << loading;
}

void CallHistoryWidget::showError(const QString& errorMsg)
{
    m_statusLabel->setText(QString("❌ Error: %1").arg(errorMsg));
    qWarning() << "[CALL_HISTORY] Error:" << errorMsg;
}

void CallHistoryWidget::clearHistory()
{
    m_callListWidget->clear();
    m_calls.clear();
    qDebug() << "[CALL_HISTORY] История очищена";
}

QString CallHistoryWidget::formatCallItem(const CallItem& item) const
{
    // Стрелка направления и имя собеседника
    QString callType = item.callType == "outgoing" ? "→" : "←";
    QString contact  = item.callType == "outgoing" ? item.callee : item.caller;

    QString statusText;
    QString statusIcon;

    if (item.status == "completed") {
        statusIcon = "✅";
        statusText = QString("%1 (%2s)")
                         .arg(item.status)
                         .arg(item.durationSeconds);
    } else if (item.status == "missed") {
        statusIcon = "⏭️";
        statusText = item.status;
    } else if (item.status == "rejected") {
        statusIcon = "❌";
        statusText = item.status;
    } else {
        statusIcon = "⏳";
        statusText = item.status;
    }

    QString timeStr = "unknown";
    if (!item.startTime.isEmpty()) {
        QDateTime dt = QDateTime::fromString(item.startTime, Qt::ISODate);
        if (dt.isValid()) {
            timeStr = dt.toString("hh:mm");
        }
    }

    // Формат: "→ user        | ✅ completed (12s)      | 10:32"
    return QString("%1 %2 | %3 %4 | %5")
        .arg(callType)
        .arg(contact, -12)
        .arg(statusIcon)
        .arg(statusText, -20)
        .arg(timeStr);
}

QString CallHistoryWidget::formatDuration(int seconds) const
{
    if (seconds < 60) {
        return QString("%1s").arg(seconds);
    } else if (seconds < 3600) {
        int minutes = seconds / 60;
        int secs    = seconds % 60;
        return QString("%1m %2s").arg(minutes).arg(secs);
    } else {
        int hours   = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        return QString("%1h %2m").arg(hours).arg(minutes);
    }
}

QIcon CallHistoryWidget::getCallIcon(const CallItem& item) const
{
    // Пока иконка не используется — можно вернуть пустой QIcon
    return QIcon();
}

void CallHistoryWidget::onRefreshClicked()
{
    qDebug() << "[CALL_HISTORY] Refresh requested";
    m_statusLabel->setText("⏳ Loading...");
    emit refreshRequested();
}

void CallHistoryWidget::onItemDoubleClicked(QListWidgetItem* item)
{
    int index = item->data(Qt::UserRole).toInt();
    if (index >= 0 && index < m_calls.size()) {
        const CallItem& call = m_calls.at(index);
        qDebug() << "[CALL_HISTORY] Double clicked call:" << call.callId;
        emit callSelected(call);
    }
}

void CallHistoryWidget::onItemClicked(QListWidgetItem* item)
{
    int index = item->data(Qt::UserRole).toInt();
    if (index >= 0 && index < m_calls.size()) {
        const CallItem& call = m_calls.at(index);
        QString type = call.callType == "outgoing" ? "📤" : "📥";
        qDebug() << "[CALL_HISTORY]" << type << "Call:" << call.caller << "→" << call.callee
                 << "| Status:" << call.status
                 << "| Duration:" << call.durationSeconds << "s";
    }
}
