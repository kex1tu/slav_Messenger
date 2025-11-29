#ifndef PROFILEVIEWWIDGET_H
#define PROFILEVIEWWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QVBoxLayout>
#include <QToolButton>
#include <QFileDialog>
#include <QMouseEvent>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QPainter>
#include "structures.h"
#include "dataservice.h"
#include "networkservice.h"
namespace Ui {
class ProfileViewWidget;
}

/**
 * @brief Виджет просмотра и редактирования профиля пользователя.
 *
 * Поддерживает два режима работы:
 * 1. Просмотр чужого профиля (ReadOnly): показывает информацию, кнопки блокировки/удаления чата.
 * 2. Просмотр своего профиля: позволяет включить режим редактирования (Edit Mode) для изменения
 *    отображаемого имени, статуса ("О себе") и аватара.
 *
 * Содержит UI элементы: аватар, поля имени/ника/био, кнопки действий.
 */
class ProfileViewWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Переопределение отрисовки фона.
     * Заливает виджет серым цветом для визуального отделения от основного контента.
     * @param event Событие отрисовки.
     */
    void paintEvent(QPaintEvent *event)
    {
        QPainter painter(this);
        painter.fillRect(this->rect(), Qt::gray);
        QWidget::paintEvent(event);
    }

    /**
     * @brief Конструктор виджета профиля.
     *
     * Инициализирует весь UI программно (без .ui файла): лейауты, поля ввода, кнопки.
     * Устанавливает фиксированный размер окна и подключает сигналы.
     *
     * @param netService Указатель на сервис сети для отправки обновленных данных профиля.
     * @param parent Родительский виджет.
     */
    explicit ProfileViewWidget(NetworkService* netService, QWidget* parent = nullptr)
    {
        m_isMyProfile = false;
        m_isEditing = false;
        m_netService = netService;
        qDebug() << "Profile view создан";
        setFixedSize(400, 500);
        setObjectName("ProfileViewWidget");

        // --- Создание UI разметки ---

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(24, 24, 24, 24);
        mainLayout->setSpacing(10);

        // Хедер с кнопкой редактирования
        auto* headerLayout = new QHBoxLayout();

        auto* headerLabel = new QLabel("Профиль", this);
        headerLabel->setObjectName("ProfileHeader");
        headerLabel->setAlignment(Qt::AlignCenter);

        m_editProfileButton = new QToolButton(this);
        m_editProfileButton->setText("EDIT");
        m_editProfileButton->setFixedHeight(32);

        headerLayout->addStretch();
        headerLayout->addWidget(headerLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(m_editProfileButton, 0, Qt::AlignRight);

        mainLayout->addLayout(headerLayout);

        // Блок с аватаром и основным инфо
        auto* baseLayout = new QHBoxLayout();
        m_avatarLabel = new QLabel("👤", this);
        m_avatarLabel->setObjectName("profileAvatar");
        m_avatarLabel->setFixedSize(80,80);
        m_avatarLabel->setAlignment(Qt::AlignCenter);

        auto* infoLayout = new QVBoxLayout();
        m_displayName = new QLineEdit(this);
        m_displayName->setPlaceholderText("Имя пользователя");
        m_lastSeen = new QLabel("offline", this);
        m_lastSeen->setObjectName("lastSeenLabel");
        infoLayout->addWidget(m_displayName);
        infoLayout->addWidget(m_lastSeen);

        baseLayout->addWidget(m_avatarLabel);
        baseLayout->addSpacing(16);
        baseLayout->addLayout(infoLayout);

        mainLayout->addLayout(baseLayout);

        // Разделитель
        auto* line1 = new QFrame(this);
        line1->setFrameShape(QFrame::HLine);
        line1->setFrameShadow(QFrame::Sunken);
        line1->setFixedHeight(1);
        mainLayout->addWidget(line1);

        // Детальная информация (Username, About)
        auto* detailsLayout = new QVBoxLayout();
        auto* labelUsername = new QLabel("Имя пользователя", this);
        labelUsername->setProperty("role", "secondary");
        m_username = new QLineEdit(this);
        m_username->setPlaceholderText("@username");

        auto* labelAbout = new QLabel("О себе", this);
        labelAbout->setProperty("role", "secondary");
        m_about = new QLineEdit(this);
        m_about->setPlaceholderText("О себе");

        detailsLayout->addWidget(labelUsername);
        detailsLayout->addWidget(m_username);
        detailsLayout->addWidget(labelAbout);
        detailsLayout->addWidget(m_about);
        mainLayout->addLayout(detailsLayout);

        // По умолчанию поля только для чтения
        m_displayName->setReadOnly(true);
        m_lastSeen->setProperty("role", "secondary");
        m_username->setReadOnly(true);
        m_about->setReadOnly(true);

        // Разделитель
        auto* line2 = new QFrame(this);
        line2->setFrameShape(QFrame::HLine);
        line2->setFrameShadow(QFrame::Sunken);
        line2->setFixedHeight(1);
        mainLayout->addWidget(line2);

        mainLayout->addStretch();

        // Кнопки действий (для чужого профиля)
        auto* actionsLayout = new QVBoxLayout();
        m_editContactButton = new QToolButton(this);
        m_blockContactButton = new QToolButton(this);
        m_blockContactButton->setText("Заблокировать");
        m_blockContactButton->setObjectName("blockContactButton");
        m_blockContactButton->setFixedHeight(32);

        m_deleteContactButton = new QToolButton(this);
        m_deleteContactButton->setText("Удалить чат");
        m_deleteContactButton->setObjectName("deleteContactButton");
        m_deleteContactButton->setFixedHeight(32);

        actionsLayout->addWidget(m_editContactButton);
        actionsLayout->addWidget(m_blockContactButton);
        actionsLayout->addWidget(m_deleteContactButton);
        mainLayout->addLayout(actionsLayout);

        connect(m_editProfileButton, &QToolButton::clicked, this, &ProfileViewWidget::onEditButtonClicked);
    }

    /** @brief Сбрасывает состояние виджета к начальному (очистка полей). */
    void reset();

    /** @brief Деструктор. */
    ~ProfileViewWidget();

    bool m_isEditing;     ///< Флаг активного режима редактирования
    bool m_isMyProfile;   ///< Флаг: просматривается свой собственный профиль

public slots:
    /**
     * @brief Загружает данные пользователя в UI.
     *
     * Настраивает видимость кнопок в зависимости от того, свой это профиль или чужой.
     * @param user Объект с данными пользователя.
     * @param isMyProfile true, если это профиль текущего авторизованного пользователя.
     */
    void setUserProfile(const User& user, bool isMyProfile = false);

    /**
     * @brief Обработчик нажатия кнопки "EDIT" / "SAVE".
     *
     * Если режим редактирования выключен: Включает поля ввода (ReadOnly = false).
     * Если режим включен: Собирает данные из полей и отправляет запрос на обновление профиля, затем блокирует поля.
     */
    void onEditButtonClicked();

private:
    User m_currentUser;            ///< Данные текущего отображаемого пользователя

    NetworkService* m_netService;  ///< Сервис для отправки запросов API

    // Элементы UI
    QToolButton* m_editProfileButton;
    QLineEdit* m_displayName;
    QLabel* m_lastSeen;
    QLineEdit* m_username;
    QLineEdit* m_about;
    QLabel* m_avatarLabel;
    QToolButton* m_closeButton;
    QToolButton* m_editContactButton;
    QToolButton* m_deleteContactButton;
    QToolButton* m_blockContactButton;
};

#endif // PROFILEVIEWWIDGET_H
