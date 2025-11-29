#ifndef SMOOTHTEXTEDIT_H
#define SMOOTHTEXTEDIT_H

#include <QTextEdit>
#include <QPropertyAnimation>

/**
 * @brief Кастомный QTextEdit с плавной прокруткой.
 *
 * Аналогично SmoothListView, добавляет инерционную или плавную прокрутку
 * для текстовых полей (например, для просмотра длинных сообщений или логов).
 */
class SmoothTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор.
     * @param parent Родительский виджет.
     */
    explicit SmoothTextEdit(QWidget *parent = nullptr);

protected:
    /**
     * @brief Обработчик колесика мыши.
     *
     * Перехватывает событие прокрутки, вычисляет дельту и запускает
     * анимацию вертикального скроллбара.
     * @param e Событие колесика.
     */
    void wheelEvent(QWheelEvent *e) override;

private:
    /**
     * @brief Анимация для управления значением вертикального скроллбара.
     */
    QPropertyAnimation *m_scrollAnimation;
};

#endif // SMOOTHTEXTEDIT_H
