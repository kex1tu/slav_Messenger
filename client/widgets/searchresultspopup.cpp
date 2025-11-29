#include "searchresultspopup.h"
#include <QVBoxLayout>     
#include <QJsonObject>     
#include <QListWidgetItem> 
#include <QFontMetrics>

SearchResultsPopup::SearchResultsPopup(QWidget *parent) : QWidget(parent)
{
    // Настраиваем флаги всплывающего окна без рамки и фокуса
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // Создаем список результатов поиска
    m_listWidget = new QListWidget(this);

    // Создаем layout без отступов
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);  
    layout->addWidget(m_listWidget);
    setLayout(layout);

    // Применяем темную тему с закругленными углами и hover эффектами
    setStyleSheet(
        "QListWidget {"
        "background-color: #2A2A2A;"     
        "border-radius: 10px;"
        "padding: 4px;"
        "border: none;"
        "}"
        "QListWidget::item {"
        "border-radius: 8px;"
        "padding: 5px;"
        "margin: 1px 0px;"
        "color: white;"
        "}"
        "QListWidget::item:hover {"
        "background-color: #444444;"
        "}"
        "QListWidget::item:selected {"
        "background-color: #00557F;"
        "}"
        );

    // Обработчик клика по элементу - испускаем сигнал и скрываем popup
    connect(m_listWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem *item){
        emit userSelected(item->data(Qt::UserRole).toString());
        hide();
    });
}

void SearchResultsPopup::showResults(const QJsonArray &users)
{
    // Очищаем предыдущие результаты
    m_listWidget->clear();

    // Парсим JSON массив пользователей и создаем элементы списка
    for (const QJsonValue &value : users) {
        QJsonObject userObj = value.toObject();
        QString displayName = userObj["displayname"].toString();
        QString username = userObj["username"].toString();

        // Формируем отображаемый текст "Имя (@username)"
        QListWidgetItem *item = new QListWidgetItem(displayName + " (@" + username + ")", m_listWidget);
        item->setData(Qt::UserRole, username);      
        m_listWidget->addItem(item);
    }

    // Если результатов нет - скрываем popup
    int count = m_listWidget->count();
    if (count == 0) {
        hide();
        return;
    }

    // Вычисляем оптимальную высоту popup
    QFontMetrics fm(m_listWidget->font());
    int itemHeight = fm.height() + 37;       
    const int maxVisibleItems = 4;           
    int targetHeight = 0;
    
    if (count > maxVisibleItems) {
        // Показываем больше 4 элементов - включаем скроллбар
        targetHeight = itemHeight * maxVisibleItems + m_listWidget->frameWidth() * 2;
        m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    } else {
        // Меньше или равно 4 - скрываем скроллбар
        targetHeight = itemHeight * count + m_listWidget->frameWidth() * 2;
        m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    // Устанавливаем фиксированную высоту popup
    this->setFixedHeight(targetHeight);

    // Показываем popup если он скрыт
    if (!isVisible()) {
        show();
    }
}
