#include "loginwidget.h"
#include "ui_loginwidget.h"
#include <QMessageBox>
#include <QAction>
#include <QIcon>
#include <QDebug>
#include "cryptoutils.h"
#include <QRegularExpression>

LoginWidget::LoginWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LoginWidget)
{
    ui->setupUi(this);

    // Настраиваем кнопку показа/скрытия пароля для входа
    m_loginPasswordVisibilityAction = new QAction(this);
    m_loginPasswordVisibilityAction->setIcon(QIcon(":/icons/icons/crossed_eye.png"));
    ui->loginPasswordEdit->addAction(m_loginPasswordVisibilityAction, QLineEdit::TrailingPosition);
    ui->loginPasswordEdit->setEchoMode(QLineEdit::Password);
    connect(m_loginPasswordVisibilityAction, &QAction::triggered, this, &LoginWidget::onLoginTogglePasswordVisibilityTriggered);

    // Настраиваем кнопку показа/скрытия пароля для регистрации
    m_registerPasswordVisibilityAction = new QAction(this);
    m_registerPasswordVisibilityAction->setIcon(QIcon(":/icons/icons/crossed_eye.png"));
    ui->registerPasswordEdit->addAction(m_registerPasswordVisibilityAction, QLineEdit::TrailingPosition);
    ui->registerPasswordEdit->setEchoMode(QLineEdit::Password);
    connect(m_registerPasswordVisibilityAction, &QAction::triggered, this, &LoginWidget::onRegisterTogglePasswordVisibilityTriggered);

    // Подключаем переключение между формами входа/регистрации
    connect(ui->goToRegisterButton, &QPushButton::clicked, this, &LoginWidget::ongoToRegisterButtonclicked);
    connect(ui->goToLoginButton, &QPushButton::clicked, this, &LoginWidget::ongoToLoginButtonclicked);

    // Обработчик кнопки входа - испускает сигнал с данными
    connect(ui->loginButton, &QPushButton::clicked, this, [this](){
        QString username = ui->loginUsernameEdit ->text().trimmed();
        QString password = ui->loginPasswordEdit->text();
        emit loginRequested(username, password);
    });

    // Обработчик кнопки регистрации - испускает сигнал с данными пользователя
    connect(ui->registerButton, &QPushButton::clicked, this, [this](){
        qDebug() << "LoginWidget: registerButton clicked. Emitting registerRequested signal.";
        QString username = ui->registerUsernameEdit->text().trimmed();
        QString displayName = ui->registerDisplayNameEdit->text().trimmed();
        QString password = ui->registerPasswordEdit->text();
        emit registerRequested(username, displayName, password);
    });

    // Создаем лейбл для индикации силы пароля
    m_passwordStrengthLabel = new QLabel(this);
    m_passwordStrengthLabel->setStyleSheet("font-size: 10px;");
    
    // Подключаем мониторинг изменения пароля для оценки силы
    connect(ui->registerPasswordEdit, &QLineEdit::textChanged,
            this, &LoginWidget::onPasswordTextChanged);

    qDebug() <<"LOGINWIDGET CREATED";
}

LoginWidget::~LoginWidget()
{
    qDebug() <<"LOGINWIDGET DELETED";
    delete ui;
}

void LoginWidget::onLoginTogglePasswordVisibilityTriggered()
{
    // Переключаем режим отображения пароля для формы входа
    if (ui->loginPasswordEdit->echoMode() == QLineEdit::Password) {
        ui->loginPasswordEdit->setEchoMode(QLineEdit::Normal);
        m_loginPasswordVisibilityAction->setIcon(QIcon(":/icons/icons/eye.png"));
    } else {
        ui->loginPasswordEdit->setEchoMode(QLineEdit::Password);
        m_loginPasswordVisibilityAction->setIcon(QIcon(":/icons/icons/crossed_eye.png"));
    }
}

void LoginWidget::onRegisterTogglePasswordVisibilityTriggered()
{
    // Переключаем режим отображения пароля для формы регистрации
    if (ui->registerPasswordEdit->echoMode() == QLineEdit::Password) {
        ui->registerPasswordEdit->setEchoMode(QLineEdit::Normal);
        m_registerPasswordVisibilityAction->setIcon(QIcon(":/icons/icons/eye.png"));  // ИСПРАВЛЕНО: был m_loginPasswordVisibilityAction
    } else {
        ui->registerPasswordEdit->setEchoMode(QLineEdit::Password);
        m_registerPasswordVisibilityAction->setIcon(QIcon(":/icons/icons/crossed_eye.png"));  // ИСПРАВЛЕНО: был m_loginPasswordVisibilityAction
    }
}

QString LoginWidget::username() const
{
    // Возвращаем очищенное имя пользователя из формы входа
    return ui->loginUsernameEdit->text().trimmed();
}

void LoginWidget::onRegistrationSuccess()
{
    // Показываем сообщение об успешной регистрации
    QMessageBox::information(this, "Регистрация успешна", "Теперь вы можете войти, используя свои данные.");
    
    // Очищаем поля формы регистрации
    ui->registerUsernameEdit->clear();
    ui->registerDisplayNameEdit->clear();
    ui->registerPasswordEdit->clear();
    
    // Переключаемся на форму входа
    ui->stackedWidget->setCurrentIndex(1);  
}

void LoginWidget::ongoToRegisterButtonclicked()
{
    // Переключаемся на форму регистрации (индекс 0)
    ui->stackedWidget->setCurrentIndex(0);
}

void LoginWidget::ongoToLoginButtonclicked()
{
    // Переключаемся на форму входа (индекс 1)
    ui->stackedWidget->setCurrentIndex(1);
}

void LoginWidget::clearFields()
{
    // Очищаем все поля ввода обеих форм
    ui->loginUsernameEdit->clear();
    ui->loginPasswordEdit->clear();
    ui->registerUsernameEdit->clear();
    ui->registerPasswordEdit->clear();
    ui->registerDisplayNameEdit->clear();
}

void LoginWidget::setUiEnabled(bool enabled)
{
    // Включаем/отключаем все элементы управления форм
    ui->loginUsernameEdit->setEnabled(enabled);
    ui->loginPasswordEdit->setEnabled(enabled);
    ui->loginButton->setEnabled(enabled);
    ui->goToRegisterButton->setEnabled(enabled);
    ui->registerUsernameEdit->setEnabled(enabled);
    ui->registerPasswordEdit->setEnabled(enabled);
    ui->registerDisplayNameEdit->setEnabled(enabled);
    ui->registerButton->setEnabled(enabled);
}

bool LoginWidget::validateCredentials(const QString& username, 
                                      const QString& password, 
                                      QString& errorMsg)
{
    // Проверяем пустое имя пользователя
    if (username.isEmpty()) {
        errorMsg = "Имя пользователя не может быть пустым";
        return false;
    }
    
    // Проверяем минимальную длину имени пользователя
    if (username.length() < 3) {
        errorMsg = "Имя пользователя должно содержать минимум 3 символа";
        return false;
    }
    
    // Проверяем максимальную длину имени пользователя
    if (username.length() > 20) {
        errorMsg = "Имя пользователя не должно превышать 20 символов";
        return false;
    }
    
    // Проверяем допустимые символы в имени пользователя
    QRegularExpression usernameRegex("^[a-zA-Z0-9_-]+$");
    if (!usernameRegex.match(username).hasMatch()) {
        errorMsg = "Имя пользователя может содержать только латинские буквы, цифры, '_' и '-'";
        return false;
    }
    
    // Проверяем пустой пароль
    if (password.isEmpty()) {
        errorMsg = "Пароль не может быть пустым";
        return false;
    }
    
    // Проверяем минимальную длину пароля
    if (password.length() < 8) {
        errorMsg = "Пароль должен содержать минимум 8 символов";
        return false;
    }
    
    // Проверяем максимальную длину пароля
    if (password.length() > 64) {
        errorMsg = "Пароль не должен превышать 64 символа";
        return false;
    }
    
    // Проверяем наличие цифры в пароле
    if (!password.contains(QRegularExpression("[0-9]"))) {
        errorMsg = "Пароль должен содержать хотя бы одну цифру";
        return false;
    }
    
    // Проверяем наличие заглавной буквы
    if (!password.contains(QRegularExpression("[A-Z]"))) {
        errorMsg = "Пароль должен содержать хотя бы одну заглавную букву";
        return false;   
    }
    
    // Проверяем наличие строчной буквы
    if (!password.contains(QRegularExpression("[a-z]"))) {
        errorMsg = "Пароль должен содержать хотя бы одну строчную букву";
        return false;
    }
    
    // Проверяем наличие специального символа
    if (!password.contains(QRegularExpression("[!@#$%^&*(),.?\":{}|<>]"))) {
        errorMsg = "Пароль должен содержать хотя бы один спецсимвол";
        return false;
    }
    
    // Защита от SQL-инъекций - проверяем SQL ключевые слова
    QStringList sqlKeywords = {"SELECT", "INSERT", "UPDATE", "DELETE", "DROP", 
                               "CREATE", "ALTER", "EXEC", "--", "/*", "*/", 
                               "XOR", "UNION", "OR", "AND"};
    
    QString upperUsername = username.toUpper();
    for (const QString& keyword : sqlKeywords) {
        if (upperUsername.contains(keyword)) {
            errorMsg = "Недопустимые символы в имени пользователя";
            return false;
        }
    }
    
    // Все проверки пройдены успешно
    return true;
}

void LoginWidget::onPasswordTextChanged(const QString& password)
{
    // Очищаем индикатор если пароль пустой
    if (password.isEmpty()) {
        m_passwordStrengthLabel->setText("");
        return;
    }
    
    // Вычисляем силу пароля по критериям
    int strength = 0;
    
    if (password.length() >= 8) strength++;      // Минимум 8 символов
    if (password.length() >= 12) strength++;     // Бонус за 12+ символов
    if (password.contains(QRegularExpression("[0-9]"))) strength++;       // Цифры
    if (password.contains(QRegularExpression("[A-Z]"))) strength++;       // Заглавные
    if (password.contains(QRegularExpression("[a-z]"))) strength++;       // Строчные
    if (password.contains(QRegularExpression("[!@#$%^&*(),.?\":{}|<>]"))) strength++;  // Спецсимволы
    
    // Определяем текстовое описание и цвет по силе пароля
    QString strengthText;
    QString color;
    
    if (strength <= 2) {
        strengthText = "Слабый пароль";
        color = "red";
    } else if (strength <= 4) {
        strengthText = "Средний пароль";
        color = "orange";
    } else {
        strengthText = "Сильный пароль";
        color = "green";
    }
    
    // Обновляем индикатор силы пароля
    m_passwordStrengthLabel->setText(strengthText);
    m_passwordStrengthLabel->setStyleSheet(QString("font-size: 10px; color: %1;").arg(color));
}
