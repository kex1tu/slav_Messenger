#ifndef SEARCHRESULTSPOPUP_H
#define SEARCHRESULTSPOPUP_H

#include <QWidget>
#include <QListWidget>
#include <QJsonArray>

/**
 * @brief Виджет всплывающего списка результатов поиска пользователей.
 *
 * Отображается поверх интерфейса (обычно под строкой поиска) и показывает
 * список найденных пользователей по введенному запросу.
 * При выборе пользователя генерирует сигнал userSelected.
 */
class SearchResultsPopup : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор всплывающего окна.
     *
     * Настраивает флаги окна (Popup, FramelessWindowHint) для корректного отображения
     * поверх других виджетов.
     * @param parent Родительский виджет.
     */
    explicit SearchResultsPopup(QWidget *parent = nullptr);

    /**
     * @brief Отображает список найденных пользователей.
     *
     * Очищает предыдущие результаты, заполняет список новыми данными из JSON,
     * рассчитывает необходимую высоту окна и показывает его.
     * Если список пуст, скрывает окно.
     *
     * @param users JSON-массив объектов пользователей (должен содержать поле "username").
     */
    void showResults(const QJsonArray &users);

private:
    QListWidget *m_listWidget;  ///< Виджет списка для отображения результатов

signals:
    /**
     * @brief Сигнал выбора пользователя из списка.
     * @param username Имя выбранного пользователя.
     */
    void userSelected(const QString& username);
};
#endif // SEARCHRESULTSPOPUP_H
