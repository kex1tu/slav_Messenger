#ifndef SMOOTHLISTVIEW_H
#define SMOOTHLISTVIEW_H

#include <QListView>
#include <QPropertyAnimation>

/**
 * @brief Кастомный QListView с плавной прокруткой.
 *
 * Переопределяет стандартное поведение колесика мыши для реализации
 * инерционной или плавной (интерполированной) прокрутки списка,
 * что делает интерфейс более отзывчивым и приятным визуально.
 */
class SmoothListView : public QListView
{
    Q_OBJECT
public slots:
    /**
     * @brief Останавливает текущую анимацию прокрутки.
     *
     * Вызывается при взаимодействии пользователя со списком (клик, нажатие клавиш),
     * чтобы предотвратить конфликт автоматической прокрутки и действий пользователя.
     */
    void stopScrollAnimation();

public:
    /**
     * @brief Конструктор списка.
     * @param parent Родительский виджет.
     */
    explicit SmoothListView(QWidget *parent = nullptr);

protected:
    /**
     * @brief Обработчик события прокрутки колесика мыши.
     *
     * Перехватывает стандартное событие, вычисляет целевую позицию скролла
     * и запускает QPropertyAnimation для плавного перехода к ней.
     * @param e Событие колесика.
     */
    void wheelEvent(QWheelEvent *e) override;

    /**
     * @brief Обработчик изменения размеров виджета.
     *
     * При изменении размера обычно останавливает анимацию, чтобы избежать
     * некорректного отображения элементов во время пересчета layout.
     * @param e Событие ресайза.
     */
    void resizeEvent(QResizeEvent *e) override;

    /**
     * @brief Обработчик событий вьюпорта.
     *
     * Может использоваться для обработки жестов или оптимизации перерисовки.
     * @param event Событие Qt.
     * @return Результат обработки события (true/false).
     */
    bool viewportEvent(QEvent *event) override;

private:
    /**
     * @brief Объект анимации для интерполяции значения прокрутки.
     * Управляет изменением значения QScrollBar::value.
     */
    QPropertyAnimation* m_scrollAnimation;
};

#endif // SMOOTHLISTVIEW_H
