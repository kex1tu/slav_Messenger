#ifndef CALLWIDGET_H
#define CALLWIDGET_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QCloseEvent>
#include "callservice.h"

/**
 * @brief Виджет управления активным аудиозвонком.
 *
 * Представляет собой плавающее окно или панель, которая появляется при входящем
 * или исходящем звонке. Содержит информацию о собеседнике (имя, статус)
 * и кнопки управления (Принять, Отклонить, Завершить).
 */
class CallWidget : public QWidget
{
    Q_OBJECT
protected:
    /**
     * @brief Обработчик события закрытия окна.
     *
     * Перехватывает попытку закрыть окно крестиком.
     * Обычно используется для автоматического завершения звонка при закрытии UI.
     *
     * @param event Событие закрытия.
     */
    void closeEvent(QCloseEvent *event) override;

public:
    /**
     * @brief Конструктор виджета звонка.
     * @param service Указатель на сервис звонков (для связывания логики).
     * @param parent Родительский виджет.
     */
    explicit CallWidget(CallService* service, QWidget* parent = nullptr);

    /**
     * @brief Устанавливает отображаемое имя собеседника.
     * @param name Имя или DisplayName пользователя.
     */
    void setCallerName(const QString& name);

    /**
     * @brief Обновляет текстовый статус звонка.
     *
     * Примеры статусов: "Входящий звонок...", "Соединение...", "Звонок завершен".
     * @param state Строка состояния.
     */
    void setCallState(const QString& state);

    /**
     * @brief Обновляет таймер длительности разговора.
     * @param duration Отформатированная строка времени (напр. "00:42").
     */
    void onDurationChanged(const QString& duration);

signals:
    /** @brief Сигнал: пользователь нажал кнопку "Принять" (для входящего). */
    void acceptClicked();

    /** @brief Сигнал: пользователь нажал кнопку "Отклонить" (для входящего). */
    void rejectClicked();

    /** @brief Сигнал: пользователь нажал кнопку "Завершить" (для активного). */
    void endCallClicked();

private:
    CallService* m_callService;  ///< Ссылка на логику звонков

    QLabel* m_callerLabel;       ///< UI: Имя собеседника
    QLabel* m_stateLabel;        ///< UI: Текущий статус (Ringing/Connected)
    QLabel* m_durationLabel;     ///< UI: Время разговора

    QPushButton* m_acceptBtn;    ///< Кнопка приема вызова (обычно зеленая)
    QPushButton* m_rejectBtn;    ///< Кнопка сброса входящего (обычно красная)
    QPushButton* m_endBtn;       ///< Кнопка завершения разговора
};

#endif // CALLWIDGET_H
