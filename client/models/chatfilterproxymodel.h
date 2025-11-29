#ifndef CHATFILTERPROXYMODEL_H
#define CHATFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QRegularExpressionMatch>

/**
 * @brief Прокси-модель для фильтрации и сортировки списка чатов.
 *
 * Позволяет фильтровать чаты по имени пользователя или тексту последнего сообщения,
 * а также поддерживает кастомную сортировку (например, закрепленные сверху).
 * Используется в связке с ContactListModel и QListView.
 */
class ChatFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор модели фильтрации.
     *
     * Настраивает регистронезависимую фильтрацию по умолчанию.
     * @param parent Родительский объект.
     */
    explicit ChatFilterProxyModel(QObject *parent = nullptr);

protected:
    /**
     * @brief Определяет, должен ли чат отображаться в списке.
     *
     * Проверяет соответствие строки (source_row) текущему регулярному выражению фильтра.
     * Поиск обычно ведется по имени пользователя (UserRole) и отображаемому имени (DisplayNameRole).
     *
     * @param source_row Номер строки в исходной модели
     * @param source_parent Родительский индекс (обычно неважен для плоского списка)
     * @return true, если строка соответствует критериям поиска
     */
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

#endif // CHATFILTERPROXYMODEL_H
