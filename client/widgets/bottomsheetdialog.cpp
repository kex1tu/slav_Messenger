#include "bottomsheetdialog.h"
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include "profileviewwidget.h"
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>

BottomSheetDialog::BottomSheetDialog(QWidget* contentWidget, QWidget* parent)
    : QDialog(parent), m_contentWidget(contentWidget)
{
    // Настраиваем флаги безрамочного модального диалога с прозрачным фоном
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);

    // Создаем полупрозрачный оверлей для затемнения фона
    m_overlay = new QWidget(this);
    m_overlay->setStyleSheet("background-color: rgba(0, 0, 0, 100);");  
    m_overlay->installEventFilter(this);  

    // Создаем вертикальный layout для содержимого
    QVBoxLayout* contentLayout = new QVBoxLayout(this);

    // Добавляем кнопку закрытия
    QToolButton* clsBtn = new QToolButton(this);
    clsBtn->setText("X");
    contentLayout->addWidget(clsBtn, 0, Qt::AlignHCenter);
    contentLayout->addWidget(m_contentWidget, 0, Qt::AlignHCenter);
    contentLayout->setAlignment(Qt::AlignHCenter);
    connect(clsBtn, &QToolButton::clicked, this, &QWidget::close);

    // Применяем эффект тени к содержимому
    auto* shadow = new QGraphicsDropShadowEffect(m_contentWidget);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 8);
    shadow->setColor(QColor(0, 0, 0, 80));  
    m_contentWidget->setGraphicsEffect(shadow);
}

void BottomSheetDialog::resizeEvent(QResizeEvent *event)
{
    // Синхронизируем геометрию оверлея с диалогом
    if (m_overlay) {
        m_overlay->setGeometry(this->rect());
    }
    QDialog::resizeEvent(event);
}

void BottomSheetDialog::showEvent(QShowEvent* event)
{
    // Вызываем базовый обработчик показа
    QDialog::showEvent(event);

    // Определяем ширину от родителя или используем 400px по умолчанию
    QWidget* p = parentWidget();
    int w = p ? p->width() : 400;

    // Вычисляем оптимальную высоту (половина высоты родителя или экрана)
    int h = 300;  
    if (p) {
        h = p->height() / 2;
    } else {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (screen)
            h = screen->geometry().height() / 2;
    }

    // Устанавливаем геометрию диалога по центру родителя
    setGeometry(p ? p->geometry() : QRect(0, 0, w, h * 2));
    
    // Логируем размеры для отладки
    qDebug() << "Dialog size:" << size();
    qDebug() << "Content size:" << m_contentWidget->size();
}

bool BottomSheetDialog::eventFilter(QObject* obj, QEvent* ev)
{
    // Пропускаем события viewport скроллареи
    if (obj->objectName() == QStringLiteral("qt_scrollarea_viewport"))  
        return false;
        
    // Закрываем диалог по клику мыши на оверлей
    if (ev->type() == QEvent::MouseButtonPress) {
        close();
        return true;
    }
    
    // Передаем остальные события базовому классу
    return QDialog::eventFilter(obj, ev);
}
