#include "smoothtextedit.h"
#include <QWheelEvent>
#include <QScrollBar>
#include <QEasingCurve>

SmoothTextEdit::SmoothTextEdit(QWidget *parent)
    : QTextEdit(parent)
{
    // Создаем анимацию для плавной прокрутки
    m_scrollAnimation = new QPropertyAnimation(this);

    // Устанавливаем целью вертикальный скролбар
    m_scrollAnimation->setTargetObject(verticalScrollBar());

    // Анимируем свойство "value" скролбара
    m_scrollAnimation->setPropertyName("value");

    // Устанавливаем длительность анимации 200мс
    m_scrollAnimation->setDuration(200);

    // Применяем плавную кривую замедления OutCubic
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void SmoothTextEdit::wheelEvent(QWheelEvent *e)
{
    // Останавливаем текущую анимацию прокрутки
    m_scrollAnimation->stop();

    // Получаем текущую позицию вертикального скролбара
    int currentValue = verticalScrollBar()->value();

    // Вычисляем дельту прокрутки (приглушенная в 0.4 раза)
    int delta = e->angleDelta().y() * -0.4;

    // Настраиваем анимацию от текущей к целевой позиции
    m_scrollAnimation->setStartValue(currentValue);
    m_scrollAnimation->setEndValue(currentValue + delta);

    // Запускаем плавную прокрутку
    m_scrollAnimation->start();

    // Принимаем событие колеса мыши (блокируем стандартную прокрутку)
    e->accept();
}
