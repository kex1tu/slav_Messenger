#include "incomingrequestswidget.h"
#include "requestitemwidget.h"
#include <QVBoxLayout>
#include <QListWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

IncomingRequestsWidget::IncomingRequestsWidget(QWidget* parent)
    : QWidget(parent)
{
    // Устанавливаем фиксированный размер виджета (400x500)
    setFixedSize(400, 500);
    
    // Создаем основной вертикальный layout
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Создаем и настраиваем лейбл с количеством заявок
    QLabel* count = new QLabel(this);
    count->setText("ЗАЯВКИ");
    layout->addWidget(count, Qt::AlignTop);

    // Создаем список для отображения заявок
    m_listWidget = new QListWidget(this);
    layout->addWidget(m_listWidget, Qt::AlignVCenter);

    // Применяем layout к виджету и скрываем по умолчанию
    setLayout(layout);
    this->hide();
}

void IncomingRequestsWidget::updateRequests(const QJsonArray& requests)
{
    // Очищаем текущий список заявок
    m_listWidget->clear();
    
    // Логируем начало обновления и количество заявок
    qDebug() << "start update requests, count:" << requests.size();
    
    // Добавляем каждую заявку из массива
    for (const auto& val : requests)
    {
        QJsonObject req = val.toObject();
        addRequest(req);
    }
}

void IncomingRequestsWidget::addRequest(const QJsonObject& request)
{
    // Создаем виджет элемента заявки с данными JSON
    auto* itemWidget = new RequestItemWidget(request, m_listWidget);
    
    // Создаем элемент списка с фиксированным размером (300x70)
    auto* listItem = new QListWidgetItem(m_listWidget);
    listItem->setSizeHint(QSize(300, 70));
    m_listWidget->addItem(listItem);

    // Устанавливаем виджет в элемент списка
    m_listWidget->setItemWidget(listItem, itemWidget);

    // Подключаем сигнал принятия заявки
    connect(itemWidget, &RequestItemWidget::accepted, this, [this, listItem, itemWidget](const QJsonObject& req){
        // Удаляем элемент из списка
        m_listWidget->takeItem(m_listWidget->row(listItem));
        qDebug() <<"Accept emited";
        
        // Испускаем сигнал принятия заявки
        emit requestAccepted(req);
        
        // Отложенное удаление виджета
        itemWidget->deleteLater();
    });
    
    // Подключаем сигнал отклонения заявки
    connect(itemWidget, &RequestItemWidget::rejected, this, [this, listItem, itemWidget](const QJsonObject& req){
        // Удаляем элемент из списка
        m_listWidget->takeItem(m_listWidget->row(listItem));
        
        // Испускаем сигнал отклонения заявки
        emit requestRejected(req);
        
        // Отложенное удаление виджета
        itemWidget->deleteLater();
    });
}
