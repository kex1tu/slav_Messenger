#include "smoothlistview.h"
#include <QWheelEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QScrollBar>
#include <QEasingCurve>

SmoothListView::SmoothListView(QWidget *parent)
    : QListView(parent)
{
    // Включаем пиксельную прокрутку для плавности
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // Отключаем горизонтальную прокрутку
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Оптимизируем рендеринг для большого количества элементов
    setLayoutMode(QListView::SinglePass);
    setBatchSize(100);

    // Создаем анимацию плавной прокрутки для вертикального скролбара
    m_scrollAnimation = new QPropertyAnimation(this);
    m_scrollAnimation->setTargetObject(verticalScrollBar());
    m_scrollAnimation->setPropertyName("value");
    m_scrollAnimation->setDuration(200);                

    // Применяем плавную кривую замедления
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutQuad);

    // Отключаем выделение элементов
    setSelectionMode(QAbstractItemView::NoSelection);

    // Отключаем drag&drop операции
    setDragDropMode(QAbstractItemView::NoDragDrop);
}

void SmoothListView::wheelEvent(QWheelEvent *e)
{
    // Останавливаем текущую анимацию прокрутки
    m_scrollAnimation->stop();

    // Получаем текущую позицию скролбара
    QScrollBar* bar = verticalScrollBar();
    int currentValue = bar->value();

    // Вычисляем дельту прокрутки (усиленная в 2 раза)
    int delta = - (e->angleDelta().y() * 2);

    // Ограничиваем целевую позицию границами скролбара
    int targetValue = currentValue + delta;
    if (targetValue < bar->minimum()) {
        targetValue = bar->minimum();
    }
    if (targetValue > bar->maximum()) {
        targetValue = bar->maximum();
    }

    // Запускаем анимацию к целевой позиции
    m_scrollAnimation->setStartValue(currentValue);
    m_scrollAnimation->setEndValue(targetValue);
    m_scrollAnimation->start();

    // Принимаем событие колеса мыши
    e->accept();
}

void SmoothListView::resizeEvent(QResizeEvent *e)
{
    // Вызываем базовый обработчик изменения размера
    QListView::resizeEvent(e);

    // Перестраиваем layout элементов
    doItemsLayout();
    scheduleDelayedItemsLayout();
}

void SmoothListView::stopScrollAnimation()
{
    // Останавливаем анимацию если она активна
    if (m_scrollAnimation->state() == QAbstractAnimation::Running) {
        m_scrollAnimation->stop();
    }
}

bool SmoothListView::viewportEvent(QEvent *event)
{
    // Обрабатываем события hover для оптимизации производительности
    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
    case QEvent::HoverLeave:
        event->accept();
        return true;
    default:
        break;
    }
    
    // Передаем остальные события базовому классу
    return QListView::viewportEvent(event);
}
