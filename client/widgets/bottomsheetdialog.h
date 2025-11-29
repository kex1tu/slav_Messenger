#ifndef BOTTOMSHEETDIALOG_H
#define BOTTOMSHEETDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QPropertyAnimation>

/**
 * @brief Диалог, реализующий UI-паттерн "Bottom Sheet" (шторка снизу).
 * Виджет появляется снизу экрана с анимацией и затемняет фон (overlay).
 * Часто используется в мобильных интерфейсах для контекстных меню, выбора действий
 * или отображения дополнительной информации, не перекрывая полностью контекст.
 */
class BottomSheetDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief Конструктор диалога.
     *
     * Настраивает безрамочное окно (FramelessWindowHint) и прозрачный фон.
     *
     * @param contentWidget Виджет с содержимым, который будет отображаться внутри шторки.
     * Владение виджетом передается диалогу.
     * @param parent Родительский виджет (обычно главное окно приложения).
     */
    explicit BottomSheetDialog(QWidget* contentWidget, QWidget* parent = nullptr);

protected:
    /**
     * @brief Событие отображения диалога.
     *
     * Переопределяется для запуска анимации выезда шторки снизу вверх
     * и плавного появления затемнения фона.
     * @param event Событие показа.
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief Событие изменения размера.
     *
     * Гарантирует, что оверлей перекрывает всё окно, а шторка остается
     * прижатой к нижней части экрана при ресайзе родителя.
     * @param event Событие изменения размера.
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Фильтр событий для обработки кликов вне области контента.
     *
     * Перехватывает клики мыши по оверлею (m_overlay) для автоматического
     * закрытия диалога (dismiss), как это принято в мобильных UI.
     *
     * @param obj Объект, которому предназначено событие.
     * @param ev Событие.
     * @return true, если событие обработано и не должно распространяться дальше.
     */
    bool eventFilter(QObject* obj, QEvent* ev);

private:
    QWidget* m_contentWidget;   ///< Виджет с полезным содержимым (меню, инфо)
    QWidget* m_overlay;         ///< Полупрозрачный виджет для затемнения фона
};

#endif // BOTTOMSHEETDIALOG_H
