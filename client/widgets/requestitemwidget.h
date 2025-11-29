#ifndef REQUESTITEMWIDGET_H
#define REQUESTITEMWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QJsonObject>

/**
 * @brief Виджет элемента списка входящих заявок.
 *
 * Представляет собой одну строку в списке заявок (IncomingRequestsWidget).
 * Отображает имя пользователя и кнопки "Принять"/"Отклонить".
 */
class RequestItemWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Конструктор элемента заявки.
     *
     * Создает лейаут с именем пользователя и кнопками управления.
     *
     * @param req JSON-объект с данными заявки (username, displayName и т.д.).
     * @param parent Родительский виджет (обычно QListWidget).
     */
    RequestItemWidget(const QJsonObject& req, QWidget* parent);


signals:
    /**
     * @brief Сигнал нажатия кнопки "Принять".
     * @param request Исходные данные заявки.
     */
    void accepted(const QJsonObject& request);

    /**
     * @brief Сигнал нажатия кнопки "Отклонить".
     * @param request Исходные данные заявки.
     */
    void rejected(const QJsonObject& request);

private:
    /** @brief Хранит данные заявки для передачи в сигналы при действиях пользователя. */
    QJsonObject m_request;
};

#endif // REQUESTITEMWIDGET_H
