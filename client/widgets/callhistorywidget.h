#ifndef CALLHISTORYWIDGET_H
#define CALLHISTORYWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "structures.h"

/**
 * @brief Виджет отображения истории звонков.
 *
 * Показывает список входящих, исходящих и пропущенных звонков.
 * Позволяет обновлять историю вручную и просматривать детали звонка.
 * Отображает статус, длительность и время каждого вызова.
 */
class CallHistoryWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Конструктор виджета истории.
     * @param parent Родительский виджет.
     */
    explicit CallHistoryWidget(QWidget* parent = nullptr);

    /** @brief Деструктор. */
    ~CallHistoryWidget();

    /**
     * @brief Заполняет список звонков данными из JSON.
     *
     * Парсит массив JSON, преобразует его в список объектов CallItem
     * и обновляет элементы QListWidget. Также обновляет статистику звонков.
     *
     * @param calls JSON-массив с историей звонков от сервера.
     */
    void setCallHistory(const QJsonArray& calls);

    /**
     * @brief Управляет отображением индикатора загрузки.
     *
     * @param loading Если true, блокирует интерфейс и меняет текст статуса на "Загрузка...".
     * Если false, возвращает интерфейс в обычное состояние.
     */
    void showLoading(bool loading);

    /**
     * @brief Отображает сообщение об ошибке в UI.
     * @param errorMsg Текст ошибки.
     */
    void showError(const QString& errorMsg);

    /**
     * @brief Очищает список звонков и сбрасывает статистику.
     */
    void clearHistory();

signals:
    /** @brief Сигнал: пользователь нажал кнопку "Обновить". */
    void refreshRequested();

    /**
     * @brief Сигнал: выбран конкретный звонок (для просмотра деталей или перезвона).
     * @param call Структура с данными выбранного звонка.
     */
    void callSelected(const CallItem& call);

private slots:
    /** @brief Обработчик клика по кнопке обновления. */
    void onRefreshClicked();

    /**
     * @brief Обработчик двойного клика по элементу списка.
     * @param item Элемент, по которому кликнули.
     */
    void onItemDoubleClicked(QListWidgetItem* item);

    /**
     * @brief Обработчик одиночного клика (выделение).
     * @param item Выбранный элемент.
     */
    void onItemClicked(QListWidgetItem* item);

private:
    /** @brief Инициализация графического интерфейса и layout'ов. */
    void setupUI();

    /**
     * @brief Форматирует строку описания звонка для списка.
     * @param item Данные звонка.
     * @return Строка вида "User (Status) - Time".
     */
    QString formatCallItem(const CallItem& item) const;

    /**
     * @brief Форматирует длительность в человекочитаемый вид.
     * @param seconds Количество секунд.
     * @return Строка формата "MM:SS" или "HH:MM:SS".
     */
    QString formatDuration(int seconds) const;

    /**
     * @brief Подбирает иконку в зависимости от типа звонка.
     *
     * @param item Данные звонка.
     * @return QIcon (стрелка вверх/вниз, красная/зеленая).
     */
    QIcon getCallIcon(const CallItem& item) const;

    QListWidget* m_callListWidget;  ///< Основной список отображения
    QPushButton* m_refreshBtn;      ///< Кнопка запроса обновления с сервера
    QLabel* m_statusLabel;          ///< Лейбл для ошибок и статусов загрузки
    QLabel* m_statsLabel;           ///< Лейбл общей статистики (всего, длительность)
    QList<CallItem> m_calls;        ///< Локальное хранилище данных истории
};

#endif // CALLHISTORYWIDGET_H
