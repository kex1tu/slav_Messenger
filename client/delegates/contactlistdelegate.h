#ifndef CONTACTLISTDELEGATE_H
#define CONTACTLISTDELEGATE_H

#include <QStyledItemDelegate>

/**
 * @brief Делегат для отрисовки элементов списка контактов/чатов.
 *
 * Отвечает за кастомное отображение каждого контакта в списке, включая:
 * - Аватар пользователя
 * - Имя (DisplayName)
 * - Текст последнего сообщения (с обрезкой)
 * - Время последнего сообщения
 * - Счетчик непрочитанных сообщений (badge)
 * - Индикаторы статуса (онлайн, пин, архив, Mute)
 */
class ContactListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор делегата.
     * @param parent Родительский объект.
     */
    explicit ContactListDelegate(QObject *parent = nullptr);

protected:
    /**
     * @brief Отрисовывает элемент списка контактов.
     *
     * Рисует фон (с учетом выделения/ховера), аватар в круге,
     * текстовые поля и дополнительные индикаторы (счетчик, время).
     *
     * @param painter Объект рисования
     * @param option Параметры стиля
     * @param index Индекс модели с данными контакта
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /**
     * @brief Возвращает фиксированный размер элемента списка.
     *
     * Обычно высота элемента в списке контактов фиксирована (например, 72px)
     * для единообразия UI.
     *
     * @param option Параметры стиля
     * @param index Индекс элемента
     * @return QSize Размер прямоугольника контакта
     */
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // CONTACTLISTDELEGATE_H
