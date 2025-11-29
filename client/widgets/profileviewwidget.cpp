#include "profileviewwidget.h"

void ProfileViewWidget::setUserProfile(const User& user, bool isMyProfile)
{
    // Сохраняем флаги и данные пользователя
    m_isMyProfile = isMyProfile;
    m_currentUser = user;

    // Обновляем текстовые поля профиля
    m_displayName->setText(user.displayName);
    m_username->setText(user.username);
    m_about->setText(user.statusMessage);
    qDebug() << user.statusMessage;

    // Устанавливаем статус последнего визита
    if(user.lastSeen == ""){
        if(m_isMyProfile){
            m_lastSeen->setText("В сети");
        } else{
            m_lastSeen->setText("unknown");
        }
    } else{
        m_lastSeen->setText(user.lastSeen);
    }

    // Загружаем аватар по умолчанию и масштабируем
    QPixmap avatarPixmap(":icons/icons/default_avatar.png");
    m_avatarLabel->setPixmap(avatarPixmap.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Настраиваем режим просмотра и видимость кнопок управления
    m_displayName->setReadOnly(true);
    m_about->setReadOnly(true);
    m_username->setReadOnly(true);
    m_deleteContactButton->setVisible(!m_isMyProfile);
    m_editContactButton->setVisible(!m_isMyProfile);
    m_blockContactButton->setVisible(!m_isMyProfile);
}

void ProfileViewWidget::reset()
{
    // Сбрасываем состояние редактирования и данные профиля
    m_isEditing = false;
    m_isMyProfile = false;
    m_currentUser = User();  

    // Очищаем все поля профиля
    m_displayName->clear();
    m_username->clear();
    m_about->clear();
    m_lastSeen->setText("не в сети");
    m_avatarLabel->setText("Нет аватар");
    m_avatarLabel->setPixmap(QPixmap());
}

void ProfileViewWidget::onEditButtonClicked() {
    if (!m_isEditing) {
        // Переключаемся в режим редактирования
        m_isEditing = true;
        m_editProfileButton->setVisible(true);
        m_editProfileButton->setText("Save");

        // Разрешаем редактирование полей (статус только для своего профиля)
        if (m_isMyProfile) {
            m_about->setReadOnly(false);
        }
        m_displayName->setReadOnly(false);
        return;
    }

    // Сохраняем изменения и выходим из режима редактирования
    m_isEditing = false;
    m_editProfileButton->setVisible(true);
    m_editProfileButton->setText("Edit");
    m_about->setReadOnly(true);
    m_displayName->setReadOnly(true);

    // Для чужого профиля просто выходим из редактирования
    if (!m_isMyProfile) {
        return;
    }

    // Собираем новые данные профиля
    QString newDisplayName = m_displayName->text();
    QString newAbout = m_about->text();

    // Проверяем наличие изменений
    bool changed = false;
    User updated = m_currentUser;
    
    if (newDisplayName != m_currentUser.displayName) {
        updated.displayName = newDisplayName;
        changed = true;
    }
    if (newAbout != m_currentUser.statusMessage) {
        updated.statusMessage = newAbout;
        changed = true;
    }
    if (!changed)
        return;

    // Формируем JSON запрос для обновления профиля на сервере
    QJsonObject req;
    req["type"]          = "update_profile";
    req["username"]      = updated.username;
    req["display_name"]  = updated.displayName;
    req["status_message"] = updated.statusMessage;
    req["avatar_url"]    = updated.avatarUrl;
    m_netService->sendJson(req);
}

ProfileViewWidget::~ProfileViewWidget()
{
    // Освобождаем память от динамически выделенных виджетов
    delete m_editProfileButton;
    delete m_displayName;
    delete m_lastSeen;
    delete m_username;
    delete m_about;
    delete m_avatarLabel;
    delete m_closeButton;
    delete m_editContactButton;
    delete m_deleteContactButton;
    delete m_blockContactButton;
}
