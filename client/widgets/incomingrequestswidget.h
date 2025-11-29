#ifndef INCOMINGREQUESTSWIDGET_H
#define INCOMINGREQUESTSWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>

/**
 * @brief Виджет для отображения входящих заявок в друзья.
 *
 * Показывает список пользователей, которые отправили запрос на добавление в контакты.
 * Для каждого запроса предоставляет кнопки "Принять" и "Отклонить".
 * Обычно используется как часть интерфейса настроек или списка контактов.
 */
class IncomingRequestsWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Конструктор виджета.
     * @param parent Родительский виджет.
     */
    explicit IncomingRequestsWidget(QWidget* parent = nullptr);

    /**
     * @brief Переопределение события отрисовки.
     *
     * Заливает фон серым цветом (Qt::gray) перед стандартной отрисовкой.
     * Используется для визуального выделения области заявок.
     *
     * @param event Событие перерисовки.
     */
    void paintEvent(QPaintEvent *event)
    {
        QPainter painter(this);
        painter.fillRect(this->rect(), Qt::gray);
        QWidget::paintEvent(event);
    }

public slots:
    /**
     * @brief Обновляет полный список заявок.
     *
     * Очищает текущий список и заполняет его новыми данными из JSON-массива.
     * Каждая заявка отображается как отдельный элемент списка с кнопками действий.
     *
     * @param requests JSON-массив объектов заявок.
     */
    void updateRequests(const QJsonArray& requests);

    /**
     * @brief Добавляет одну новую заявку в существующий список.
     * @param request JSON-объект новой заявки.
     */
    void addRequest(const QJsonObject& request);

signals:
    /**
     * @brief Сигнал: пользователь принял заявку.
     * @param request Исходные данные заявки (включая username отправителя).
     */
    void requestAccepted(const QJsonObject& request);

    /**
     * @brief Сигнал: пользователь отклонил заявку.
     * @param request Исходные данные заявки.
     */
    void requestRejected(const QJsonObject& request);

private:
    /** @brief Виджет списка для отображения элементов заявок. */
    QListWidget* m_listWidget;

    /**
     * @brief Локальное хранилище данных текущих заявок.
     * Используется для доступа к данным при клике по кнопкам списка.
     */
    QVector<QJsonObject> m_pendingRequests;
};

#endif // INCOMINGREQUESTSWIDGET_H
