#ifndef LOGINWIDGET_H
#define LOGINWIDGET_H

#include "structures.h"
#include <QWidget>
#include <QLabel>

class QAction;

namespace Ui {
class LoginWidget;
}

/**
 * @brief Виджет аутентификации и регистрации пользователя.
 *
 * Предоставляет формы для входа (Логин/Пароль) и регистрации (Логин/Имя/Пароль).
 * Поддерживает переключение между режимами, валидацию ввода, отображение/скрытие пароля
 * и индикацию силы пароля.
 */
class LoginWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор виджета входа.
     * @param parent Родительский виджет.
     */
    explicit LoginWidget(QWidget *parent = nullptr);

    /**
     * @brief Возвращает текущее введенное имя пользователя.
     * @return Строка с логином.
     */
    QString username() const;

    /** @brief Деструктор. */
    ~LoginWidget();

public slots:
    /**
     * @brief Блокирует или разблокирует элементы управления UI.
     * Используется для предотвращения повторного ввода во время ожидания ответа от сервера.
     * @param enabled true - интерфейс активен, false - заблокирован.
     */
    void setUiEnabled(bool enabled);

    /** @brief Очищает все текстовые поля ввода (логин, пароль, имя). */
    void clearFields();

    /**
     * @brief Обработчик успешной регистрации.
     * Переключает виджет в режим входа и заполняет поле логина.
     */
    void onRegistrationSuccess();

signals:
    /**
     * @brief Сигнал запроса на вход.
     * @param username Введенный логин.
     * @param password Введенный пароль.
     */
    void loginRequested(const QString& username, const QString& password);

    /**
     * @brief Сигнал запроса на регистрацию.
     * @param username Желаемый логин.
     * @param displayName Отображаемое имя.
     * @param password Пароль.
     */
    void registerRequested(const QString& username, const QString& displayName, const QString& password);

private:
    QLabel* m_passwordStrengthLabel; ///< Лейбл индикатора сложности пароля

    Ui::LoginWidget *ui;             ///< Указатель на сгенерированный UI

    QAction* m_loginPasswordVisibilityAction;    ///< Экшен "глаз" для поля пароля (Вход)
    QAction* m_registerPasswordVisibilityAction; ///< Экшен "глаз" для поля пароля (Регистрация)

private slots:
    /**
     * @brief Отслеживает изменение текста пароля для проверки его сложности.
     * @param password Текущий текст пароля.
     */
    void onPasswordTextChanged(const QString& password);

    /**
     * @brief Проверяет корректность введенных учетных данных.
     * @param username Логин для проверки (не пустой, допустимые символы).
     * @param password Пароль для проверки (длина, сложность).
     * @param errorMsg [out] Сообщение об ошибке валидации.
     * @return true, если данные валидны.
     */
    bool validateCredentials(const QString& username, const QString& password, QString& errorMsg);

    /** @brief Переключает UI в режим регистрации (скрывает форму входа). */
    void ongoToRegisterButtonclicked();

    /** @brief Переключает UI в режим входа (скрывает форму регистрации). */
    void ongoToLoginButtonclicked();

    /** @brief Переключает видимость символов пароля в форме входа. */
    void onLoginTogglePasswordVisibilityTriggered();

    /** @brief Переключает видимость символов пароля в форме регистрации. */
    void onRegisterTogglePasswordVisibilityTriggered();
};

#endif // LOGINWIDGET_H
