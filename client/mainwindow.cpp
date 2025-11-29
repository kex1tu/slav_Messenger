#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>
#include <QJsonArray>
#include <QDataStream>
#include <QScrollBar>
#include <QMenu>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QStackedLayout>
#include <QTextEdit>
#include <QSplitter>
#include <QResizeEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QShortcut>
#include <QKeySequence>
#include <QToolButton>
#include <QFile>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QHttpPart>
#include <QHttpMultiPart>
#include <QStandardPaths>
#include <QDesktopServices>
#include "searchresultspopup.h"
#include "chatfilterproxymodel.h"
#include "loginwidget.h"
#include "chatviewwidget.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "chatmessagedelegate.h"
#include "contactlistmodel.h"
#include "contactlistdelegate.h"
#include "chatmessagemodel.h"
#include "profileviewwidget.h"
#include "chatmessagedelegate.h"
#include "smoothlistview.h"
#include "dataservice.h"
#include "incomingrequestswidget.h"
#include "tokenmanager.h"
#include "bottomsheetdialog.h"
#include "cryptoutils.h"


DataService* MainWindow::m_dataService = nullptr;

MainWindow::MainWindow(DataService* dataService, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    qDebug()<<"[CLIENT] creating mainwindow";

    // Инициализируем UI из Designer формы
    ui->setupUi(this);

    // Создаем горячие клавиши для полноэкранного режима (F11)
    QShortcut *fullScreenShortcut = new QShortcut(this);
    fullScreenShortcut->setKey(QKeySequence(Qt::Key_F11));
    connect(fullScreenShortcut, &QShortcut::activated, this, &MainWindow::toggleFullScreen);

    // Сохраняем указатели на сервисы данных и сети
    m_dataService = dataService;
    m_networkService = new NetworkService(this);

    // Синхронизируем криптографические ключи текущего пользователя
    m_dataService->updateOrAddCurrentUserCrypto(m_networkService->getCurrentUserCrypto());
    
    // Инициализируем VoIP сервис и виджет звонков
    m_callService = new CallService(m_networkService, m_dataService, this);
    m_callWidget = new CallWidget(m_callService, this);
    m_callWidget->hide();
    qDebug() << "[MainWindow] CallWidget created:" << (m_callWidget ? "OK" : "FAILED");

    // Создаем модель и прокси для фильтрации сообщений чата
    m_chatModel = new ChatMessageModel(this);
    m_chatFilterProxy = new ChatFilterProxyModel(this);
    m_chatFilterProxy->setSourceModel(m_chatModel);
    m_chatFilterProxy->setFilterRole(Qt::UserRole);
    m_chatFilterProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    // Строим основной интерфейс и настраиваем стек виджетов
    buildMainUI();
    ui->rootStackedWidget->addWidget(m_loginWidget);
    ui->rootStackedWidget->addWidget(m_mainChatWidget);
    ui->rootStackedWidget->setCurrentWidget(m_loginWidget);

    // Создаем всплывающее окно результатов поиска
    m_searchResultsPopup = new SearchResultsPopup(this);

    // Устанавливаем глобальный фильтр событий для приложения
    qApp->installEventFilter(this);

    // Настраиваем страницу меню
    setupMenuPage();

    qDebug() << "[MainWindow] setupConnections() completed";

    // Подключаем все сигналы/слоты
    setupConnections();

    // Загружаем конфигурацию сервера из INI файла
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    qDebug() << "Current path:" << QDir::currentPath();
    qDebug() << "INI keys =" << settings.allKeys();
    QString serv_Ip = settings.value("network/servIp", "127.0.0.1").toString();
    
    // Включаем контекстное меню для списка пользователей
    m_userListView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // Подключаемся к серверу по IP и порту из конфигурации
    m_networkService->connectToServer(serv_Ip, 1234);
}


void MainWindow::attemptAutoLogin()
{
    // Проверяем наличие сохраненного токена
    if (!TokenManager::hasToken()) {
        qDebug() << "[MainWindow] No saved token, showing login screen";
        return;
    }

    // Проверяем валидность токена (30 секунд)
    if (!TokenManager::isTokenValid(30)) {
        qDebug() << "[MainWindow] Token expired, clearing...";
        TokenManager::clearToken();
        return;
    }

    // Получаем данные токена и пользователя
    QString token = TokenManager::getToken();
    QString username = TokenManager::getUsername();

    qInfo() << "[MainWindow] Attempting auto-login for user:" << username;

    // Формируем JSON запрос для авторизации по токену
    QJsonObject request;
    request["type"] = "token_login";
    request["token"] = token;
    request["username"] = username;

    // Отправляем запрос на сервер
    m_networkService->sendJson(request);
}

void MainWindow::setupMenuPage()
{
    // Создаем страницу меню с вертикальным layout
    m_menuPage = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(m_menuPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Создаем заголовок с кнопкой "Назад"
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QPushButton* backBtn = new QPushButton("← Back", this);
    backBtn->setFixedWidth(80);
    backBtn->setStyleSheet(
        "QPushButton { background-color: #f0f0f0; border: none; padding: 8px; }"
        "QPushButton:hover { background-color: #e0e0e0; }"
        );
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::onBackFromMenu);
    QLabel* titleLabel = new QLabel("Menu", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; padding: 10px;");
    headerLayout->addWidget(backBtn);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    // Добавляем разделитель
    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(separator);

    // Создаем layout для пунктов меню
    QVBoxLayout* menuItemsLayout = new QVBoxLayout();
    menuItemsLayout->setSpacing(5);
    menuItemsLayout->setContentsMargins(10, 10, 10, 10);

    // Кнопка "Контакты"
    QPushButton* contactsBtn = createMenuButton("Contacts", "");
    connect(contactsBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "[MENU] Contacts clicked";
    });
    menuItemsLayout->addWidget(contactsBtn);

    // Кнопка "Вызовы"
    QPushButton* callsBtn = createMenuButton("Calls", "");
    connect(callsBtn, &QPushButton::clicked, this, &MainWindow::onCallsButtonClicked);
    menuItemsLayout->addWidget(callsBtn);

    // Кнопка "Входящие заявки"
    QPushButton* incomingrequestsBtn = createMenuButton("incomingrequests", "");
    menuItemsLayout->addWidget(incomingrequestsBtn);
    connect(incomingrequestsBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "[CLIENT] Showing Incoming Requests";

        // Показываем заявки в bottom sheet диалоге
        BottomSheetDialog* dlg = new BottomSheetDialog(m_incomingRequestsWidget, this);
        m_incomingRequestsWidget->show();
        dlg->exec();
    });

    // Кнопка "Мой профиль"
    QPushButton* myProfileBtn = createMenuButton("my profile", "");
    menuItemsLayout->addWidget(myProfileBtn);
    connect(myProfileBtn, &QPushButton::clicked, this, &MainWindow::onMyProfileClicked);

    // Добавляем layout пунктов меню и растяжку
    layout->addLayout(menuItemsLayout);
    layout->addStretch();

    // Кнопка выхода
    m_logoutButton = new QPushButton("Выйти", this);
    m_logoutButton->setObjectName("logoutButton");
    layout->addWidget(m_logoutButton);

    // Финальные отступы
    layout->setContentsMargins(10, 0, 10, 10);

    // Применяем layout и добавляем страницу в панель
    m_menuPage->setLayout(layout);
    m_leftMainPanel->addWidget(m_menuPage);
}

void MainWindow::onMyProfileClicked() {
    // Получаем данные текущего пользователя и показываем профиль
    const User* user = m_dataService->getCurrentUser();
    if (user) {
        showProfileView(*user);
        return;
    }
}

QPushButton* MainWindow::createMenuButton(const QString& text, const QString& badge)
{
    // Создаем кнопку меню фиксированной высоты
    QPushButton* btn = new QPushButton(this);
    btn->setFixedHeight(45);

    // Добавляем бейдж если есть
    QString btnText = text;
    if (!badge.isEmpty()) {
        btnText += QString(" [%1]").arg(badge);
    }
    btn->setText(btnText);

    // Применяем стиль кнопки меню
    btn->setStyleSheet(
        "QPushButton { "
        "  background-color: #f5f5f5; "
        "  border: 1px solid #e0e0e0; "
        "  border-radius: 8px; "
        "  padding: 10px 15px; "
        "  font-size: 13px; "
        "  text-align: left; "
        "} "
        "QPushButton:hover { "
        "  background-color: #e8e8e8; "
        "} "
        "QPushButton:pressed { "
        "  background-color: #d0d0d0; "
        "}"
        );
    return btn;
}

void MainWindow::onMenuButtonClicked()
{
    qDebug() << "[UI] Menu button clicked";
    
    // Переключаем на страницу меню
    m_leftMainPanel->setCurrentIndex(PAGE_MENU);
}

void MainWindow::onCallsButtonClicked()
{
    qDebug() << "[UI] Calls button clicked - showing Call History";
}

void MainWindow::onBackFromMenu()
{
    qDebug() << "[UI] Back from Menu";
    m_leftMainPanel->setCurrentIndex(PAGE_CHATS);
}

void MainWindow::buildMainUI()
{
    // Создаем виджет входа
    m_loginWidget = new LoginWidget(this);
    if (!m_loginWidget) {
        qWarning() << "[MainWindow] Failed to create m_loginWidget";
        return;
    }

    // Создаем основной виджет чата
    m_mainChatWidget = new QWidget(this);
    if (!m_mainChatWidget) {
        qDebug() << "[MainWindow] Failed to create m_mainChatWidget";
        return;
    }

    // Создаем горизонтальный layout для левой/правой панелей
    auto* mainLayout = new QHBoxLayout(m_mainChatWidget);
    if (!mainLayout) {
        qDebug() << "[MainWindow] Failed to create mainLayout";
        return;
    }
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(1);

    // Создаем левую панель со списком чатов/контактов
    m_chatListPanel = new QWidget(this);
    m_chatListPanel->setObjectName("chatListPanel");
    
    auto* leftLayout = new QVBoxLayout(m_chatListPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);

    // Создаем поле поиска контактов
    m_searchLineEdit = new QLineEdit(this);
    m_searchLineEdit->setPlaceholderText("Поиск контактов...");
    
    // Создаем список пользователей/чатов
    m_userListView = new QListView(this);

    // Настраиваем модель и делегат списка контактов
    m_contactModel = new ContactListModel(m_dataService, this);
    m_userListView->setModel(m_contactModel);
    m_userListView->setItemDelegate(new ContactListDelegate(this));

    // Оптимизируем отображение списка
    m_userListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_userListView->setResizeMode(QListView::Adjust);
    m_userListView->setUniformItemSizes(false);

    // Создаем стековый виджет для левой панели (чаты/меню)
    m_leftMainPanel = new QStackedWidget(this);
    m_leftMainPanel->addWidget(m_chatListPanel);

    // Создаем кнопку меню
    m_menuButton = new QToolButton(m_chatListPanel);
    m_menuButton->setIcon(QIcon(":/icons/icons/menu.png"));
    m_menuButton->setToolTip("Меню");
    m_menuButton->setFixedSize(32,32);

    // Создаем верхний layout с меню и поиском
    QHBoxLayout* topMenuLayout = new QHBoxLayout();
    topMenuLayout->setContentsMargins(0,0,0,0);
    topMenuLayout->addWidget(m_menuButton);
    topMenuLayout->addStretch();
    leftLayout->insertLayout(0, topMenuLayout);

    topMenuLayout->addWidget(m_searchLineEdit);
    leftLayout->addWidget(m_userListView);

    // Создаем правую панель (чат/профиль)
    m_rightSideContainer = new QWidget(this);
    m_rightSideContainer->setObjectName("rightSideContainer");
    m_rightSideLayout = new QVBoxLayout(m_rightSideContainer);
    m_rightSideLayout->setContentsMargins(0, 0, 0, 0);

    // Создаем placeholder для пустого чата
    m_placeholderWidget = new QWidget(m_rightSideContainer);
    auto* placeholderLayout = new QVBoxLayout(m_placeholderWidget);
    auto* placeholderLabel = new QLabel("Выберите чат, чтобы начать общение", m_placeholderWidget);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setObjectName("placeholderLabel");
    placeholderLayout->addWidget(placeholderLabel);

    // Создаем виджет чата с историей сообщений
    m_chatViewWidget = new ChatViewWidget(m_rightSideContainer);
    QListView* chatView = m_chatViewWidget->chatHistoryView();
    chatView->setModel(m_chatFilterProxy);
    m_chatDelegate = new ChatMessageDelegate(m_chatModel, chatView);
    chatView->setItemDelegate(m_chatDelegate);
    
    // Подключаем сигналы делегата сообщений
    if (m_chatDelegate) {
        connect(m_chatDelegate, &ChatMessageDelegate::replyRequested, m_chatViewWidget, &ChatViewWidget::onMessageDoubleClicked);
        connect(m_chatDelegate, &ChatMessageDelegate::fileDownloadRequested, m_chatViewWidget, &ChatViewWidget::fileDownloadRequested); 

        qDebug() << "Delegate connections setup";
    }

    // Создаем стековый layout для правой панели и виджет профиля
    m_rightSideStackedLayout = new QStackedLayout(this);
    m_profileViewWidget = new ProfileViewWidget(m_networkService, this);
    m_rightSideContainer->setLayout(m_rightSideLayout);

    // Добавляем виджеты в стек правой панели
    m_rightSideStackedLayout->addWidget(m_chatViewWidget);
    m_incomingRequestsWidget = new IncomingRequestsWidget(this);
    m_rightSideStackedLayout->addWidget(m_placeholderWidget);

    // Финальная сборка layout'ов
    m_rightSideLayout->addLayout(m_rightSideStackedLayout);
    m_rightSideStackedLayout->setCurrentWidget(m_placeholderWidget);

    mainLayout->addWidget(m_leftMainPanel, 0);
    mainLayout->addWidget(m_rightSideContainer, 1);
}

void MainWindow::onContactsUpdated(const QStringList& sortedUsernames) {
    // Запрашиваем историю чата для всех контактов из кэша
    auto userCache = m_dataService->getUserCache()->keys();
    for(auto contact: userCache){
        QJsonObject request;
        request["type"] = "get_history";
        request["with_user"] = contact;

        m_networkService->sendJson(request);
    }
    m_contactModel->updateContacts(sortedUsernames);
}

void MainWindow::onOnlineStatusUpdated() {
    // Обновляем заголовок чата с данными текущего собеседника
    m_chatViewWidget->updateHeader(m_dataService->getCurrentChatPartner() != nullptr ? *m_dataService->getCurrentChatPartner(): User());
    
    if (m_dataService->getUserCache() == nullptr) {
        return;
    }
    m_contactModel->updateContacts(m_dataService->getUserCache()->keys());
}

void MainWindow::onOlderHistoryChunkPrepended(const QString& chatPartner, const QList<ChatMessage>& messages) {
    // Игнорируем если не текущий чат или сообщений нет
    if (chatPartner != m_dataService->getCurrentChatPartner()->username || messages.isEmpty()) {
        m_dataService->updateOrAddIsLoadingHistory(false);
        m_expectingRangeChange = false;
        return;
    }
    
    // Добавляем старые сообщения в начало истории
    m_chatModel->prependMessages(messages);
}

void MainWindow::onScrollBarRangeChanged()
{
    // Игнорируем если не ожидаем изменения диапазона
    if (!m_expectingRangeChange) {
        return;
    }

    m_expectingRangeChange = false;
    qDebug() << "[SCROLL] Range changed as expected.";

    // Получаем smooth list view и скролбар
    SmoothListView* view = qobject_cast<SmoothListView*>(m_chatViewWidget->chatHistoryView());
    QScrollBar* scrollBar = view->verticalScrollBar();

    // Вычисляем добавленную высоту и прокручиваем к ней
    int newMaximum = scrollBar->maximum();
    int heightAdded = newMaximum - m_oldScrollMax;

    if (heightAdded > 0) {
        view->stopScrollAnimation();
        m_programmaticScrollInProgress = true;
        scrollBar->setValue(heightAdded);
        qDebug() << "[SCROLL] Manually setting scroll value to:" << heightAdded;

        // Сбрасываем флаг программной прокрутки через 50мс
        QTimer::singleShot(50, this, [this]() {
            m_programmaticScrollInProgress = false;
        });
    }

    // Сбрасываем флаг загрузки истории
    m_dataService->updateOrAddIsLoadingHistory(false);
    qDebug() << "[SCROLL] Loading finished, flag reset.";
}

void MainWindow::onHistoryLoaded(const QString& chatPartner, const QList<ChatMessage>& messages) {
    qDebug() << "onHistoryLoaded";
    
    // Проверяем валидность текущего собеседника
    if(m_dataService->getCurrentChatPartner() == nullptr){
        qDebug() << "_dataService->getCurrentChatPartner() == nullptr";
        return;
    }
    
    // Игнорируем если история не для текущего чата
    if (chatPartner != m_dataService->getCurrentChatPartner()->username) {
        qDebug() << "(chatPartner != m_dataService->getCurrentChatPartner()->username";
        return;
    }
    
    // Очищаем и загружаем новую историю сообщений
    m_chatModel->clearMessages();
    m_chatModel->addMessages(messages);

    // Прокручиваем к непрочитанным сообщениям
    onScrollToUnreadFast();
    
    // Обрабатываем видимые сообщения через 50мс (для стабилизации layout)
    QTimer::singleShot(50, this, &MainWindow::processVisibleMessages);
}

void MainWindow::onLoginSuccess(const QJsonObject& response)
{
    // Парсим данные пользователя из ответа сервера
    User me;
    me.username      = response["username"].toString();
    me.displayName   = response["displayname"].toString();
    me.statusMessage = response["statusmessage"].toString();
    me.avatarUrl     = response["avatar_url"].toString();
    me.isOnline = true;
    me.isTyping = false;
    QString token = response.value("token").toString();  

    qInfo() << "[MainWindow] Login successful for user:" << me.username;
    qDebug() <<"STATUS MESSAGE: " << me.statusMessage;

    // Сохраняем токен для автовхода
    if (!token.isEmpty()) {
        TokenManager::saveToken(token,  me.username);
        qDebug() << "[MainWindow] Token saved";
    }

    // Проверяем валидность login widget
    if (m_loginWidget.isNull()) {
        qWarning() << "[MainWindow] LoginWidget was deleted!";
        return;
    }

    // Проверяем DataService
    if(m_dataService == nullptr)
        qDebug() << "m_data service is nullptr";

    // Обновляем данные текущего пользователя
    m_dataService->updateOrAddCurrentUser(me);
    qDebug() << "[DataService] Login successful for user:" << m_dataService->getCurrentUser()->username;

    // Очищаем поля формы входа
    m_loginWidget->clearFields();

    // Переключаемся на основной интерфейс чата
    if (m_mainChatWidget) {
        ui->rootStackedWidget->setCurrentWidget(m_mainChatWidget);
    }

    // Устанавливаем заголовок окна и инициализируем загрузку данных
    this->setWindowTitle(me.username);
    m_dataService->initLoad();
}

void MainWindow::onLoginFailed(const QString& error){
    // Показываем ошибку входа
    QMessageBox::warning(this, "Ошибка входа", error);
}

void MainWindow::onRegisterFailed(const QString& error){
    // Показываем ошибку регистрации
    QMessageBox::warning(this, "Ошибка входа", error);
}

void MainWindow::onLoginFailure(const QString& reason){
    // Показываем детальную ошибку входа
    QMessageBox::warning(this, "Ошибка входа", "Не удалось войти: " + reason);
}

void MainWindow::onRegisterSuccess(){
    // Уведомляем об успешной регистрации
    QMessageBox::information(this, "Регистрация успешна", "Ваш аккаунт был успешно создан. Теперь вы можете войти.");
    m_loginWidget->onRegistrationSuccess();
}

void MainWindow::onRegisterFailure(const QString& reason){
    // Показываем ошибку регистрации
    QMessageBox::warning(this, "Ошибка регистрации", reason);
}

void MainWindow::onNewMessageReceived(const ChatMessage& incomingMsg)
{
    qDebug() << "onNewMessageRecived";

    // Отправляем подтверждение доставки сообщения
    QJsonObject deliveredCmd;
    deliveredCmd["type"] = "message_delivered";
    deliveredCmd["id"] = (double)incomingMsg.id;
    m_networkService->sendJson(deliveredCmd);

    // Игнорируем если нет активного чата
    if(m_dataService->getCurrentChatPartner() == nullptr)
    {
        return;
    }
    
    // Обработка сообщения для текущего чата
    if (incomingMsg.fromUser == m_dataService->getCurrentChatPartner()->username) {
        bool wasScrolledToBottom = m_chatViewWidget->isScrolledToBottom();
        
        if (!m_chatModel) {
            qWarning() << "[MainWindow] ChatModel deleted!";
            return;
        }
        
        // Добавляем сообщение в модель
        m_chatModel->addMessage(incomingMsg);

        // Автопрокрутка если были внизу
        if (wasScrolledToBottom) {
            QMetaObject::invokeMethod(m_chatViewWidget, "scrollToBottom", Qt::QueuedConnection);
        } else {
            // Уведомляем о новом сообщении
            emit newMessageForCurrentChat();
        }

        // Обрабатываем видимые сообщения через 100мс
        QTimer::singleShot(100, this, &MainWindow::processVisibleMessages);
    } else {
        // Увеличиваем счетчик непрочитанных для другого чата
        const auto& unreadCounts = m_dataService->getUnreadCounts();
        int currentCount = unreadCounts.value(incomingMsg.fromUser, 0);
        m_dataService->updateOrAddUnreadCount(incomingMsg.fromUser, currentCount + 1);

        // Обновляем отображение контакта
        m_contactModel->refreshContact(incomingMsg.fromUser);
    }

    // Показываем системное уведомление если не текущий чат или окно не активно
    if (incomingMsg.fromUser != m_dataService->getCurrentChatPartner()->username || isMinimized() || !isActiveWindow()) {
        QApplication::alert(this);
    }
}

void MainWindow::onMessageStatusChanged(qint64 messageId, ChatMessage::MessageStatus newStatus) {
    // Обновляем статус сообщения в модели и базе данных
    m_chatModel->updateMessageStatus(messageId, newStatus);
    //m_dataService->getDatabaseService()->updateMessageStatus(messageId, newStatus);
}

void MainWindow::onUnreadCountChanged() {
    // Обновляем весь список контактов при изменении непрочитанных
    m_contactModel->refreshList();
}

void MainWindow::onMessageEdited(const QString& chatPartner, qint64 messageId, const QString& newPayload) {
    // Обновляем отредактированное сообщение только в текущем чате
    if (chatPartner == m_dataService->getCurrentChatPartner()->username) {
        m_chatModel->editMessage(messageId, newPayload);
    }
    m_contactModel->refreshContact(chatPartner);
}

void MainWindow::onMessageDeleted(const QString& chatPartner, qint64 messageId) {
    // Удаляем сообщение только из текущего чата
    if (chatPartner == m_dataService->getCurrentChatPartner()->username) {
        m_chatModel->removeMessage(messageId);
    }
    m_contactModel->refreshContact(chatPartner);
}

void MainWindow::onSearchResultsReceived(const QJsonArray& users){
    // Позиционируем popup поиска под строкой поиска
    QWidget *searchBar = m_searchLineEdit;
    m_searchResultsPopup->move(searchBar->mapToGlobal(QPoint(0, searchBar->height())));
    m_searchResultsPopup->setFixedWidth(searchBar->width());
    m_searchResultsPopup->showResults(users);
    
    // Возвращаем фокус в поле поиска
    QTimer::singleShot(0, this, [this]() {
        m_searchLineEdit->setFocus();
    });
}

void MainWindow::onAddContactSuccess(const QString& username) {
    // Уведомляем об успешной отправке запроса на добавление
    QMessageBox::information(this, "Запрос отправлен", "Запрос на добавление пользователя " + username + " был успешно отправлен.");
}

void MainWindow::onAddContactFailure(const QString& reason) {
    // Показываем ошибку добавления контакта
    QMessageBox::warning(this, "Ошибка добавления контакта", reason);
}

void MainWindow::onPendingContactRequestsUpdated(const QJsonArray & requests){
    // Показываем промпт для каждой входящей заявки на контакт
    for (const QJsonValue &value : requests) {
        QJsonObject reqObj = value.toObject();
        QString fromUsername = reqObj["fromUsername"].toString();
        QString fromDisplayName = reqObj["fromDisplayname"].toString();
        showContactRequestPrompt(fromUsername, fromDisplayName);
    }
}

void MainWindow::onLogoutSuccess(){
    qDebug() << "[CLIENT] Logout successful. Resetting application state.";
    QMessageBox::information(this, "Выход", "Вы успешно вышли из аккаунта.");
    
    // Очищаем токен и сбрасываем состояние приложения
    TokenManager::clearToken();
    resetApplicationState();
}

void MainWindow::onLogoutFailure(const QString& reason){
    // Показываем ошибку выхода
    QMessageBox::warning(this, "Ошибка", reason);
}

void MainWindow::onTypingStatusChanged(const QString& username, bool isTyping){
    Q_UNUSED(isTyping);
    
    // Обновляем статус контакта в списке
    m_contactModel->refreshContact(username);
    
    if(m_dataService->getCurrentChatPartner() == nullptr){
        return;
    }
    
    // Обновляем заголовок чата если печатает текущий собеседник
    if (m_dataService->getCurrentChatPartner()->username == username) {
        m_chatViewWidget->updateHeader(*(m_dataService->getUserFromCache(username)));
    }
}

void MainWindow::onConfirmMessageSent(QString tempId, const ChatMessage& msg){
    // Подтверждаем отправку временного сообщения реальным
    m_chatModel->confirmMessage(tempId, msg);
}



void MainWindow::setupConnections()
{
    qDebug() << "[MainWindow] setupConnections() START";

    // Проверка ключевых сервисов (без них дальше нет смысла работать)
    if (!m_callService) {
        qWarning() << "[MainWindow] ERROR: m_callService is nullptr!";
        return;
    }
    qDebug() << "[MainWindow] m_callService exists: OK";

    if (!m_callWidget) {
        qWarning() << "[MainWindow] ERROR: m_callWidget is nullptr!";
        return;
    }
    qDebug() << "[MainWindow] m_callWidget exists: OK";

    // --- Подключение сервисных и пользовательских событий (DataService/NetworkService/UI) ---
    connect(m_networkService, &NetworkService::connected, this, &MainWindow::onConnected);
    connect(m_networkService, &NetworkService::disconnected, this, &MainWindow::onDisconnected);
    connect(m_networkService, &NetworkService::jsonReceived, this, &MainWindow::onJsonReceived);
    connect(m_dataService, &DataService::encryptionEnabled, this, &MainWindow::attemptAutoLogin);


    connect(m_dataService, &DataService::contactsUpdated, this, &MainWindow::onContactsUpdated);
    connect(m_dataService, &DataService::onlineStatusUpdated, this, &MainWindow::onOnlineStatusUpdated);
    connect(m_dataService, &DataService::olderHistoryChunkPrepended, this, &MainWindow::onOlderHistoryChunkPrepended);
    connect(m_dataService, &DataService::historyLoaded, this, &MainWindow::onHistoryLoaded);
    connect(m_dataService, &DataService::loginSuccess, this, &MainWindow::onLoginSuccess);
    connect(m_dataService, &DataService::loginFailure, this, &MainWindow::onLoginFailure);
    connect(m_dataService, &DataService::registerSuccess, this, &MainWindow::onRegisterSuccess);
    connect(m_dataService, &DataService::registerFailure, this, &MainWindow::onRegisterFailure);

    connect(m_dataService, &DataService::newMessageReceived, this, &MainWindow::onNewMessageReceived);
    connect(m_dataService, &DataService::messageStatusChanged, this, &MainWindow::onMessageStatusChanged);
    connect(m_dataService, &DataService::unreadCountChanged, this, &MainWindow::onUnreadCountChanged);
    connect(m_dataService, &DataService::messageEdited, this, &MainWindow::onMessageEdited);
    connect(m_dataService, &DataService::messageDeleted, this, &MainWindow::onMessageDeleted);
    connect(m_dataService, &DataService::searchResultsReceived, this, &MainWindow::onSearchResultsReceived);
    connect(m_dataService, &DataService::addContactSuccess, this, &MainWindow::onAddContactSuccess);
    connect(m_dataService, &DataService::addContactFailure, this, &MainWindow::onAddContactFailure);

    // Входящие запросы — напрямую во view виджет заявок
    connect(m_dataService, &DataService::contactRequestReceived, this, &MainWindow::onContactRequestReceived);
    connect(m_incomingRequestsWidget, &IncomingRequestsWidget::requestRejected, this, &MainWindow::onRequestRejected);
    connect(m_incomingRequestsWidget, &IncomingRequestsWidget::requestAccepted, this, &MainWindow::onRequestAccepted);

    connect(m_dataService, &DataService::logoutSuccess, this, &MainWindow::onLogoutSuccess);
    connect(m_dataService, &DataService::logoutFailure, this, &MainWindow::onLogoutFailure);
    connect(m_dataService, &DataService::typingStatusChanged, this, &MainWindow::onTypingStatusChanged);
    connect(m_dataService, &DataService::confirmMessageSent, this, &MainWindow::onConfirmMessageSent);
    connect(m_dataService, &DataService::avatarUpdated, this, &MainWindow::onAvatarUpdated);

    // Не запускать, если логин-виджет ещё не создан
    if(m_loginWidget.isNull()){
        qDebug()<<"m_loginWidget is NULLPTR INSIDE CONNECTIONS";
        return;
    }
    connect(m_loginWidget, &LoginWidget::loginRequested, m_dataService,&DataService::loginProcess);

    connect(m_dataService, &DataService::sendJson, m_networkService, &NetworkService::sendJson);

    connect(m_loginWidget, &LoginWidget::registerRequested, m_dataService, &DataService::registerProcess);

    connect(m_dataService, &DataService::loginFailed, this, &MainWindow::onLoginFailed);

    connect(m_dataService, &DataService::registerFailed, this, &MainWindow::onRegisterFailed);


    // --- Чатовые сигналы/слоты ---
    connect(m_chatViewWidget, &ChatViewWidget::sendMessageRequested, this, &MainWindow::onSendMessageRequested);
    connect(m_chatViewWidget, &ChatViewWidget::headerClicked, this, static_cast<void(MainWindow::*)()>(&MainWindow::showProfileView));

    //connect(m_profileViewWidget, &ProfileViewWidget::backButtonClicked, this, &MainWindow::hideProfileView);
    connect(m_chatViewWidget->messageTextEdit(), &QTextEdit::textChanged, this, &MainWindow::onTypingNotificationFired);
    connect(m_chatViewWidget, &ChatViewWidget::replyToMessageRequested, this, &MainWindow::onReplyToMessage);
    connect(m_chatViewWidget, &ChatViewWidget::editMessageRequested, this, &MainWindow::onEditMessageRequested);
    connect(m_chatViewWidget, &ChatViewWidget::deleteMessageRequested, this, &MainWindow::onDeleteMessageRequested);

    // Сброс reply-состояния
    connect(m_chatViewWidget, &ChatViewWidget::replyCancelled, this, [this](){
        m_dataService->updateOrAddReplyToMessageId(0);
    });
    connect(m_chatViewWidget, &ChatViewWidget::scrollToUnreadRequested, this, &MainWindow::onScrollToUnread);
    connect(m_chatViewWidget, &ChatViewWidget::scrollToBottomRequested, this, &MainWindow::onScrollToBottom);

    QScrollBar* scrollBar = m_chatViewWidget->chatHistoryView()->verticalScrollBar();
    connect(scrollBar, &QScrollBar::valueChanged, this, &MainWindow::processVisibleMessages);
    connect(scrollBar, &QScrollBar::valueChanged, this, &MainWindow::onChatScroll);
    connect(scrollBar, &QScrollBar::rangeChanged, this, &MainWindow::onScrollBarRangeChanged);

    connect(m_userListView, &QListView::clicked, this, &MainWindow::onUserSelectionChanged);
    connect(m_userListView, &QListView::customContextMenuRequested, this, &MainWindow::onContactListContextMenu);

    // Поиск: запускает дебоунс-таймер при каждом новом вводе
    connect(m_searchLineEdit, &QLineEdit::textChanged, this, [this](const QString& text){
        if (text.isEmpty()){
            m_dataService->getGlobalSearchTimer()->stop();
        } else {
            m_dataService->getGlobalSearchTimer()->start();
        }
    });
    connect(m_dataService->getGlobalSearchTimer(), &QTimer::timeout, this, &MainWindow::onGlobalSearchTriggered);
    connect(m_searchResultsPopup, &SearchResultsPopup::userSelected, this, &MainWindow::onAddContactRequested);

    // Кнопка logout
    connect(m_logoutButton, &QPushButton::clicked, this, &MainWindow::onLogoutButtonClicked);

    // Вспомогательные сигналы/слоты
    //connect(this, &MainWindow::newMessageForCurrentChat, m_chatViewWidget, &ChatViewWidget::onNewMessageReceived);
    connect(m_chatModel, &ChatMessageModel::messageNeedsReadReceipt, this, &MainWindow::onSendMessageReadReceipt);

    // Символьный поиск по истории чата
    connect(m_chatViewWidget, &ChatViewWidget::searchTextEntered, this, [this](const QString& text){
        m_chatFilterProxy->setFilterRegularExpression(QRegularExpression::escape(text));
    });

    // --- Меню и заявки ---

    connect(m_dataService, &DataService::pendingContactRequestsUpdated, m_incomingRequestsWidget, &IncomingRequestsWidget::updateRequests);
    connect(m_menuButton, &QToolButton::clicked, this,  &MainWindow::onMenuButtonClicked);
    connect(m_chatViewWidget, &ChatViewWidget::attachFileRequested, this, &MainWindow::onAttachFileRequested);
    connect(m_chatViewWidget, &ChatViewWidget::fileDownloadRequested, this, &MainWindow::onFileDownloadRequested);

    // --- Звонки (audio/video) ---
    qDebug() << "[MW] Setting up call connections...";
    if (!m_callService || !m_callWidget) {
        qWarning() << "[MW] ❌ CallService or CallWidget not initialized";
        return;
    }

    // Запрос на вызов (из чата)
    connect(m_chatViewWidget, &ChatViewWidget::callRequested, this,
            [this]() {
                const QString& username = m_dataService->getCurrentChatPartner()->username;
                qDebug() << "[MW] >>> callRequested from ChatViewWidget, user:" << username;
                if (!m_callService) {
                    qWarning() << "[MW] ❌ CallService not initialized";
                    return;
                }
                qDebug() << "[MW] Calling m_callService->initiateCall(" << username << ")";
                m_callService->initiateCall(username);
                qDebug() << "[MW] ✅ Call initiated";
            });

    // Входящий вызов, принятый вызов, показ callWidget, обновление состояния, время, ошибки
    connect(m_dataService, &DataService::incomingCall, m_callService, &CallService::onCallRequestReceived);
    connect(m_dataService, &DataService::callAccepted, m_callService,
            [this](const QString& from, const QString& ip, quint16 port) {
                qDebug() << "[MW] Forwarding callAccepted to CallService";
                m_callService->onCallAcceptedReceived(ip, port);
            });

    connect(m_callService, &CallService::incomingCallShow, this,
            [this](const QString& fromUser) {
                qDebug() << "[MW] ========== INCOMING CALL SIGNAL RECEIVED ==========";
                qDebug() << "[MW] From:" << fromUser;
                if (!m_callWidget) {
                    qWarning() << "[MW] ❌ CallWidget is nullptr!";
                    return;
                }
                qDebug() << "[MW] Setting caller name to:" << fromUser;
                m_callWidget->setCallerName(fromUser);
                qDebug() << "[MW] Setting state to 'Входящий звонок'";
                m_callWidget->setCallState("Входящий звонок");
                m_callWidget->show();
                m_callWidget->raise();
                m_callWidget->activateWindow();
                qDebug() << "[MW] ✅ Incoming call UI SHOWN";
                qDebug() << "[MW] ================================================";
            });

    connect(m_callService, &CallService::outgoingCallShow, this,
            [this]() {
                qDebug() << "[MW] >>> OUTGOING CALL - waiting for answer";
                qDebug() << "[MW] Setting caller name to:" << m_dataService->getCurrentChatPartner()->username;
                m_callWidget->setCallerName(m_dataService->getCurrentChatPartner()->username);
                m_callWidget->setCallState("Ожидание ответа");
                m_callWidget->show();
                m_callWidget->raise();
                m_callWidget->activateWindow();
                qDebug() << "[MW] ✅ Outgoing call UI shown (waiting for remote user)";
            });

    connect(m_callService, &CallService::callConnected, this,
            [this]() {
                qDebug() << "[MW] >>> CALL CONNECTED";
                m_callWidget->setCallState("Разговор активен");
                m_callWidget->show();
                qDebug() << "[MW] ✅ Call connected";
            });

    connect(m_callService, &CallService::callDurationUpdated, this,
            [this](int seconds) {
                int mins = seconds / 60;
                int secs = seconds % 60;
                QString timeStr = QString::asprintf("%02d:%02d", mins, secs);
                m_callWidget->onDurationChanged(timeStr);
            });

    connect(m_callService, &CallService::callEnded, this,
            [this]() {
                qDebug() << "[MW] >>> CALL ENDED";
                m_callWidget->hide();
                qDebug() << "[MW] ✅ Call UI hidden";
            });

    connect(m_dataService, &DataService::callEnded, m_callService, &CallService::onCallEndedReceived);
    connect(m_callService, &CallService::callError, this,
            [this](const QString& error) {
                qWarning() << "[MW] Call error:" << error;
            });

    if (!m_callWidget) {
        qWarning() << "[MainWindow] CallWidget not available, skipping connections";
        return;
    }

    connect(m_callWidget, &CallWidget::acceptClicked, m_callService, &CallService::acceptCall);
    connect(m_callWidget, &CallWidget::rejectClicked, m_callService, &CallService::rejectCall);
    connect(m_callWidget, &CallWidget::endCallClicked, m_callService, &CallService::endCall);

    // Прочее: получение истории, обновление профиля
    connect(m_dataService, &DataService::requestServerHistory, this, &MainWindow::onRequestServerHistory);
    connect(m_dataService, &DataService::profileUpdateResult, this, &MainWindow::onProfileUpdateResult);
    connect(m_dataService, &DataService::tokenLoginFailed, this, &MainWindow::onTokenLoginFailed);



    connect(m_dataService, &DataService::chatMetadataChanged,
            m_contactModel, &ContactListModel::onChatMetadataChanged);
    connect(m_dataService, &DataService::chatMetadataChanged, this, [this](const QString& username){
        if(m_dataService->getCurrentChatPartner() == nullptr){
            qDebug() << "m_dataService->getCurrentChatPartner() == nullptr";
            return;
        }
        if(username == m_dataService->getCurrentChatPartner()->username){
            qDebug() << "getCurrentChatPartner";
            m_chatViewWidget->onUnreadMessageCount(m_dataService->getChatMetadata(username).unreadCount);
        }

    });




    connect(m_dataService, &DataService::currentUserChanged, this, [this]() {
        const User* currentUser = m_dataService->getCurrentUser();
        if (currentUser && m_profileViewWidget) {
            qDebug() << "setUserProfile in connection";
            m_profileViewWidget->setUserProfile(*currentUser, true);
        }
    });



    //connect(m_chatModel, &QAbstractListModel::rowsInserted, this, onScrollToUnread);

    qDebug() << "[MW] ✅ Call connections setup complete";
}


void MainWindow::onAttachFileRequested(){
    // Открываем диалог выбора файла для отправки
    QString filePath = QFileDialog::getOpenFileName(this, tr("Выберите файл для отправки"));
    if (!filePath.isEmpty()) {
        uploadFileToGo(filePath);  
    }
}

void MainWindow::onAvatarUpdated(const QString &username, const QString &avatarUrl) {
    Q_UNUSED(avatarUrl);
    
    // Обновляем аватар контакта в списке
    if (m_contactModel) {
        QModelIndex idx = m_contactModel->indexForUsername(username);  
        if (idx.isValid()) {
            emit m_contactModel->dataChanged(idx, idx, {ContactListModel::AvatarRole});
        }
    }
}

void MainWindow::onContactRequestReceived(const QJsonObject& request){
    // Загружаем аватар заявителя и добавляем заявку
    m_dataService->getAvatarCache()->ensureAvatar(request.value("fromUsername").toString(), request.value("fromAvatarUrl").toString());
    m_incomingRequestsWidget->addRequest(request);
}

void MainWindow::onFileDownloadRequested(const QString &fileId, const QString &url, const QString &fileName) {
    // Формируем URL для скачивания и путь сохранения
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QUrl fileUrl(url);
    fileUrl = "http://localhost:9090/files/download/" +  fileId;
    QString savePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/MessengerFiles/"+ fileId + "_" + fileName;
    
    QFileInfo file(savePath);
    if (file.exists() && file.isFile()) {
        // Открываем существующий файл
        QDesktopServices::openUrl(QUrl::fromLocalFile(savePath));
    } else{
        // Скачиваем новый файл
        QNetworkRequest request(fileUrl);
        QNetworkReply *reply = manager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this,reply, savePath]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray content = reply->readAll();
                QFile file(savePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(content);
                    file.close();
                    QMessageBox::information(this, "Файл скачан", "Файл сохранён в " + savePath);
                } else {
                    QMessageBox::critical(this, "Ошибка", "Не удалось сохранить файл!");
                }
            } else {
                QMessageBox::critical(this, "Ошибка сети", reply->errorString());
            }
            reply->deleteLater();
        });
    }
}

void MainWindow::uploadFileToGo(const QString& filePath)
{
    // Загружаем файл на Go сервер через multipart/form-data
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);

    QFile* file = new QFile(filePath, this);
    if (!file->open(QIODevice::ReadOnly)) {
        qDebug() << "Не удалось открыть файл для чтения";
        return;
    }

    // Формируем multipart часть с файлом
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                      QVariant("form-data; name=\"file\"; filename=\"" + QFileInfo(filePath).fileName() + "\""));
    filePart.setBodyDevice(file);

    multiPart->append(filePart);

    QNetworkRequest request(QUrl("http://localhost:9090/files/upload"));  
    QNetworkReply* reply = manager->post(request, multiPart);

    // Обрабатываем ответ сервера
    connect(reply, &QNetworkReply::finished, [this, reply, file, multiPart, filePath]() {
        QByteArray resp = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(resp);
        QJsonObject obj = doc.object();
        qDebug() << obj;

        if (obj.contains("data") && obj.value("data").isArray()) {
            QJsonArray arr = obj.value("data").toArray();
            qDebug() << arr;
            if (!arr.isEmpty()) {
                QJsonObject fileObj = arr[0].toObject();
                unsigned long long fileId = fileObj.value("id").toDouble();
                QString fileName = fileObj.value("name").toString();
                QString fileUrl = fileObj.value("storage_path").toString();
                
                qDebug() << fileId << " " << fileName;

                // Кэшируем информацию о загруженном файле
                m_dataService->cacheUploadedFile(QString::number(fileId), fileName, fileUrl);

                qDebug() << "Файл загружен, id:" << fileId << "name:" << fileName;
            } else {
                qDebug() << "Ошибка: пустой массив data";
            }
        } else {
            qDebug() << "Ошибка: нет data в ответе Go";
        }

        file->deleteLater();
        multiPart->deleteLater();
        reply->deleteLater();
    });
}

void MainWindow::onContactListContextMenu(const QPoint& pos)
{
    // Получаем индекс контакта под курсором
    QModelIndex index = m_userListView->indexAt(pos);
    if (!index.isValid())
        return;

    QString username = index.data(ContactListModel::UsernameRole).toString();
    QString displayName = index.data(ContactListModel::DisplayNameRole).toString();
    bool isPinned = index.data(ContactListModel::IsPinnedRole).toBool();

    // Создаем контекстное меню
    QMenu menu;
    QAction* pinAction = nullptr;
    if (isPinned)
        pinAction = menu.addAction("Открепить чат");
    else
        pinAction = menu.addAction("Закрепить чат");

    menu.addSeparator();
    QAction* profileAction = menu.addAction("Открыть профиль " + displayName);

    menu.addSeparator();
    QAction* deleteAction = menu.addAction("Удалить чат");

    // Выполняем выбранное действие
    QAction* selected = menu.exec(m_userListView->viewport()->mapToGlobal(pos));
    if (!selected)
        return;

    if (selected == pinAction) {
        // Закрепляем/открепляем чат
        m_dataService->setPinned(username, !isPinned);
    } else if (selected == profileAction) {
        // Показываем профиль контакта
        showProfileView((*m_dataService->getUserCache())[username] );
    } else if (selected == deleteAction) {
        // Подтверждаем удаление чата
        if (QMessageBox::question(this, "Удаление чата", "Удалить чат с " + displayName + "?") == QMessageBox::Yes) {
            // TODO: реализовать удаление чата
        }
    }
}

void MainWindow::onTokenLoginFailed(const QString& reason)
{
    qWarning() << "[MainWindow] Token login failed:" << reason;

    // Переключаемся на экран входа при неудачном токен-логине
    ui->rootStackedWidget->setCurrentWidget(m_loginWidget);

    // Формируем пользовательское сообщение об ошибке
    QString message;
    if (reason.contains("expired")) {
        message = "Ваша сессия истекла. Пожалуйста, войдите снова.";
    } else if (reason.contains("Invalid")) {
        message = "Необходимо войти в систему.";
    } else {
        message = "Не удалось восстановить сессию. Войдите снова.";
    }

    // TODO: Показать сообщение пользователю
}

void MainWindow::onProfileUpdateResult(const QJsonObject& response) {
    if (response.value("success").toBool()) {
        // Обновляем локальные данные профиля при успехе
        const User* currentUser = m_dataService->getCurrentUser();
        if (currentUser) {
            User updatedUser = *currentUser;
            updatedUser.displayName = response.value("displayname").toString();
            updatedUser.statusMessage = response.value("status_message").toString();
            updatedUser.avatarUrl = response.value("avatar_url").toString();
            m_dataService->updateOrAddCurrentUser(updatedUser);
        } else {
            qWarning() << "[DataService] currentUser is null, cannot update.";
        }
        
        QMessageBox::information(this, "Профиль", "Профиль успешно обновлён!");
    } else {
        // Показываем ошибку обновления профиля
        QString reason = response.value("reason").toString();
        QMessageBox::warning(this, "Ошибка", QString("Не удалось обновить профиль: %1").arg(reason));
    }
}

void MainWindow::onCallRequested() {
    qDebug() << "[MW] Call button clicked - initiating call";
    
    QString selectedUser = m_dataService->getCurrentChatPartner()->username;
    if (selectedUser.isEmpty()) {
        qWarning() << "[MW] ❌ No user selected";
        return;
    }
    
    qDebug() << "[MW] Calling user:" << selectedUser;
    if (m_callService) {
        m_callService->initiateCall(selectedUser);
    } else {
        qWarning() << "[MW] ❌ CallService is nullptr";
    }
}

void MainWindow::onScrollToUnreadFast(){
    qDebug() << "[MainWindow] Fast Scroll to unread requested.";
    
    // Быстрая прокрутка к первому непрочитанному сообщению
    QModelIndex firstUnreadIndex = m_chatModel->findFirstUnreadMessage();
    QModelIndex proxyIndex = m_chatFilterProxy->mapFromSource(firstUnreadIndex);

    if(proxyIndex.isValid()){
        m_chatViewWidget->chatHistoryView()->scrollTo(proxyIndex, QAbstractItemView::PositionAtTop);
    }
    else{
        onScrollToBottomFast();
    }
}

void MainWindow::onScrollToBottomFast(){
    qDebug() << "[MainWindow] Fast Scroll to bottom requested.";
    m_chatViewWidget->chatHistoryView()->scrollToBottom();
}

void MainWindow::onScrollToUnread() {
    qDebug() << "[MainWindow] Scroll to unread requested.";
    
    // Плавная прокрутка к первому непрочитанному сообщению
    QModelIndex firstUnreadIndex = m_chatModel->findFirstUnreadMessage();
    QModelIndex proxyIndex = m_chatFilterProxy->mapFromSource(firstUnreadIndex);
    
    if(proxyIndex.isValid()){
        m_chatViewWidget->scrollToMessage(proxyIndex);
    }
    else{
        onScrollToBottom();
    }
}

void MainWindow::onScrollToBottom() {
    qDebug() << "[MainWindow] Scroll to bottom requested.";
    // Плавная прокрутка к концу через queued connection
    QMetaObject::invokeMethod(m_chatViewWidget, "scrollToBottom", Qt::QueuedConnection);
}

void MainWindow::onConnected() {
    qDebug() << "[DEBUG] onConnected, m_loginWidget =" << m_loginWidget;
    
    // Разблокируем UI формы входа при подключении к серверу
    if (!m_loginWidget) {
        qWarning() << "m_loginWidget is nullptr in onConnected!";
        return;
    }
    m_loginWidget->setUiEnabled(true);  
}

void MainWindow::onDisconnected() {
    // Блокируем UI формы входа при отключении от сервера
    if(m_loginWidget){
        m_loginWidget->setUiEnabled(false);  
    }
}

void MainWindow::onJsonReceived(const QJsonObject& response) {
    // Передаем полученный JSON в DataService для обработки
    m_dataService->processResponse(response);  
}


void MainWindow::processVisibleMessages()
{
    // Получаем виджет истории чата
    QListView* view = m_chatViewWidget->chatHistoryView();

    // Проверяем валидность модели и текущего собеседника
    if (!view->model() || m_dataService->getCurrentChatPartner()->username.isEmpty()) {
        return;
    }

    // Вычисляем видимую область viewport
    QRect viewportRect = view->viewport()->rect();
    QAbstractItemModel* model = view->model();
    int rowCount = model->rowCount();

    // Проверяем каждое сообщение на видимость и отправляем read receipt
    for (int row = 0; row < rowCount; ++row) {
        QModelIndex index = model->index(row, 0);
        QRect rect = view->visualRect(index);    
        if (!rect.isValid() || rect.isEmpty()) continue;
        
        if (rect.bottom() >= viewportRect.top() && rect.top() <= viewportRect.bottom()) {
            ChatMessage msg = index.data(Qt::UserRole).value<ChatMessage>();
            
            // Отправляем подтверждение прочтения для входящих доставленных сообщений
            if (!msg.isOutgoing && (msg.status == ChatMessage::Delivered || msg.status == ChatMessage::Sent)) {
                emit m_chatModel->messageNeedsReadReceipt(msg.id);
            }
        }
    }
}

void MainWindow::onEditMessageRequested(qint64 messageId, const QString& oldText)
{
    qDebug() << "[MainWindow]: Caught editMessageRequested signal for ID:" << messageId;

    // Скрываем UI ответа при редактировании
    m_chatViewWidget->hideReplyUI();

    // Устанавливаем ID редактируемого сообщения
    m_dataService->updateOrAddEditingMessageId(messageId);

    // Переключаем чат в режим редактирования
    m_chatViewWidget->setEditMode(true, oldText);
}

void MainWindow::onDeleteMessageRequested(qint64 messageId)
{
    // Отправляем серверный запрос на удаление сообщения
    QJsonObject request;
    request["type"] = "delete_message";  
    request["id"] = messageId;            

    m_networkService->sendJson(request);  
}

void MainWindow::onAddContactRequested(const QString& username)
{
    // Скрываем popup результатов поиска
    if (m_searchResultsPopup) {
        m_searchResultsPopup->hide();
    }

    // Подтверждаем отправку запроса на добавление
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Добавить контакт",
                                  "Отправить запрос на добавление в контакты пользователю " + username + "?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        qDebug() << "[CLIENT] Sending 'add_contact_request' for user:" << username;

        // Отправляем запрос на сервер
        QJsonObject request;
        request["type"] = "add_contact_request";
        request["username"] = username;
        m_networkService->sendJson(request);
    }
}

void MainWindow::onTypingNotificationFired()
{
    // Проверяем наличие текущего собеседника
    if(m_dataService->getCurrentChatPartner()){
        if (m_dataService->getCurrentChatPartner()->username.isEmpty()) return;
    }

    // Игнорируем пустой текст
    if (m_chatViewWidget->messageTextEdit()->toPlainText().isEmpty()) return;

    // Избегаем спама уведомлениями (проверяем таймер)
    if( m_dataService->getTypingSendTimer()){
        if ( m_dataService->getTypingSendTimer()->isActive()) return;
    }

    // Отправляем уведомление о наборе текста
    QJsonObject typingRequest;
    typingRequest["type"] = "typing";
    typingRequest["toUser"] = m_dataService->getCurrentChatPartner()->username;
    m_networkService->sendJson(typingRequest);

    // Запускаем таймер для debounce
    m_dataService->getTypingSendTimer()->start();
}

void MainWindow::onReplyToMessage(qint64 messageId)
{
    qDebug() << "[CLIENT] Setting reply context to message ID:" << messageId;
    ChatMessage msg;

    // Получаем сообщение для контекста ответа
    if (m_chatModel->getMessageById(messageId, msg)) {
        m_dataService->updateOrAddReplyToMessageId(messageId);

        // Показываем UI ответа с текстом/именем файла
        m_chatViewWidget->showReplyUI(msg.fromUser, (msg.payload == "")? msg.fileName :msg.payload );
    }
    m_chatViewWidget->scrollToBottom();
}
void MainWindow::onChatSearchTriggered(const QString &text)
{
    Q_UNUSED(text);  
}

void MainWindow::resetApplicationState()
{
    // Сбрасываем состояние VoIP сервиса
    if (m_callService) {
        m_callService->resetCallData();
    }

    // Очищаем все данные сервиса
    m_dataService->clearAllData();
    qDebug() << "DataService cleared data";

    // Очищаем модели чата и контактов
    if (m_chatModel) {
        m_chatModel->clearMessages();
    }
    m_contactModel->clear();  

    // Очищаем UI элементы
    m_searchLineEdit->clear();
    m_chatViewWidget->clearAll();
    qDebug() <<"ui cleared";

    // Скрываем popup поиска
    if (m_searchResultsPopup) {
        m_searchResultsPopup->hide();
    }

    // Возвращаем UI в начальное состояние
    m_rightSideStackedLayout->setCurrentWidget(m_placeholderWidget);
    m_leftMainPanel->setCurrentWidget(m_chatListPanel);
    ui->rootStackedWidget->setCurrentWidget(m_loginWidget);

    // Переподключаемся к серверу из конфигурации
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    qDebug() << "Current path:" << QDir::currentPath();
    qDebug() << "INI keys =" << settings.allKeys();
    QString serv_Ip = settings.value("network/servIp", "192.168.0.101").toString();
    m_networkService->connectToServer(serv_Ip, 1234);
    
    qDebug() << "[MainWindow] Application UI and models have been reset.";
}

void MainWindow::onSendMessageRequested(const QString& text)
{
    // Проверяем наличие собеседника
    if (m_dataService->getCurrentChatPartner()->username.isEmpty()) {
        return;
    }
    
    qint64 editingMessageId = m_dataService->getEditinigMessageId();
    if (editingMessageId > 0) {
        // РЕДАКТИРОВАНИЕ СООБЩЕНИЯ
        m_dataService->updateOrAddEditingMessageId(0);
        m_chatViewWidget->setEditMode(false);
        if(text.isEmpty()){
            return;
        }
        qDebug() << "[CLIENT] Sending 'edit_message' request for ID:" <<editingMessageId;

        QJsonObject request;
        request["type"] = "edit_message";                  
        request["id"] = editingMessageId;     
        request["payload"] = text;                              
        m_networkService->sendJson(request);
    } else {
        // ОТПРАВКА НОВОГО СООБЩЕНИЯ
        if(text.isEmpty() && (!m_dataService->getCachedFiles() || m_dataService->getCachedFiles()->isEmpty()) ){
            return;
        }
        
        qint64 replyToMessageId =  m_dataService->getReplyToMessageId();
        
        // Создаем локальное сообщение
        ChatMessage msg;
        msg.fromUser = (m_dataService->getCurrentUser()->username);
        msg.toUser = m_dataService->getCurrentChatPartner()->username;
        msg.payload = text;
        msg.status = ChatMessage::Sending;           
        msg.isOutgoing = true;
        msg.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        msg.tempId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        msg.replyToId =replyToMessageId;
        msg.isEdited = false;
        msg.fileId = "";
        msg.fileName = "";
        msg.fileUrl = "";
        
        // Добавляем прикрепленный файл если есть
        if(m_dataService->getCachedFiles()){
            if(!m_dataService->getCachedFiles()->isEmpty()){
                msg.fileId = m_dataService->getCachedFiles()->last().fileId;
                msg.fileName = m_dataService->getCachedFiles()->last().fileName;
                msg.fileUrl = m_dataService->getCachedFiles()->last().fileUrl;
                m_dataService->getCachedFiles()->remove(msg.fileId);
            }
        }

        // Сохраняем в локальную БД
        if (m_dataService->getDatabaseService() && m_dataService->getDatabaseService()->isConnected()) {
            m_dataService->getDatabaseService()->saveMessage(msg, m_dataService->getCurrentUser()->username);
        }

        // Добавляем в UI модель (оптимистично)
        m_chatModel->addMessage(msg);
        QMetaObject::invokeMethod(m_chatViewWidget, "scrollToBottom", Qt::QueuedConnection);

        // Кэшируем в чат кэш
        QString chatPartner = m_dataService->getCurrentChatPartner()->username;
        if (m_dataService->getChatCacheForUser(chatPartner) != nullptr) {
            m_dataService->getChatCacheForUser(chatPartner)->messages.append(msg);
        }

        // Отправляем на сервер
        QJsonObject request;
        request["type"] = "private_message";
        request["fromUser"] = msg.fromUser;
        request["toUser"] = msg.toUser;
        request["payload"] = msg.payload;
        request["reply_to_id"] = msg.replyToId;
        request["temp_id"] = msg.tempId;
        request["file_id"] = msg.fileId;
        request["file_name"] = msg.fileName;
        request["file_url"] = msg.fileUrl;

        m_networkService->sendJson(request);

        // Сбрасываем контекст ответа
        if (replyToMessageId > 0) {
            m_dataService->updateOrAddReplyToMessageId(0);
            m_chatViewWidget->hideReplyUI();
        }
    }
    
    // Обновляем отображение контакта
    m_contactModel->refreshContact(m_dataService->getCurrentChatPartner()->username);
}

void MainWindow::onLogoutButtonClicked()
{
    qDebug() << "[CLIENT] MainWindow: Logout button clicked.";

    // Отправляем запрос на выход с сервера
    QJsonObject logoutRequest;
    logoutRequest["type"] = "logout_request";
    logoutRequest["username"] = m_dataService->getCurrentUser()->username;  

    m_networkService->sendJson(logoutRequest);
}

void MainWindow::onUserSelectionChanged(const QModelIndex &current)
{
    // Сохраняем черновик текущего чата перед переключением
    if (m_dataService->getCurrentChatPartner()) {
        QString currentDraft = m_chatViewWidget->messageTextEdit()->toPlainText();
        if (!currentDraft.isEmpty()) {
            m_dataService->saveDraft(m_dataService->getCurrentChatPartner()->username, currentDraft);
        }
    }

    // Сбрасываем UI если выбор недействителен
    if (!current.isValid()) {
        qDebug() << "Current item is null, resetting view.";
        m_rightSideStackedLayout->setCurrentWidget(m_placeholderWidget);

        m_dataService->updateOrAddCurrentChatPartner(User());
        if (m_chatDelegate) {
            m_chatDelegate->clearSizeHintCache();
            m_chatDelegate->clearCaches();
        }
        m_chatModel->clearMessages();
        m_userListView->clearSelection();
        qDebug() << "--- onUserSelectionChanged END (reset) ---";
        return;
    }

    qDebug() << "--- onUserSelectionChanged START ---";
    
    // Сбрасываем счетчик непрочитанных и поиск
    m_chatViewWidget->onUnreadMessageCount(0);
    m_chatViewWidget->hideSearchUI();

    // Сбрасываем контекст ответа
    qint64 replyToMessageId =  m_dataService->getReplyToMessageId();
    if (replyToMessageId > 0) {
        m_dataService->updateOrAddReplyToMessageId(0);
        m_chatViewWidget->hideReplyUI();
    }

    // Получаем выбранного пользователя
    QString selectedUsername = current.data(ContactListModel::UsernameRole).toString();
    qDebug() << "Selected username from model:" << selectedUsername;

    // Проверяем наличие пользователя в кэше
    if (m_dataService->getUserCache() != nullptr){
        if (selectedUsername.isEmpty() || !(*m_dataService->getUserCache()).contains(selectedUsername)) {
            qWarning() << "CRITICAL: Selected user not found in cache or username is empty!";
            return;
        }
    }

    // Устанавливаем текущего собеседника
    const auto* userCache = m_dataService->getUserCache();  
    if (userCache && userCache->contains(selectedUsername)) {
        m_dataService->updateOrAddCurrentChatPartner(userCache->value(selectedUsername));
    }

    qDebug() << "Current chat partner set to:" << m_dataService->getCurrentChatPartner()->displayName;
    m_contactModel->refreshContact(selectedUsername);

    // Настраиваем пагинацию истории
    m_dataService->updateOrAddIsLoadingHistory(true);
    qint64 oldestMessageId = -1;
    if(m_dataService->getChatCacheForUser(selectedUsername) != nullptr){
        oldestMessageId = m_dataService->getChatCacheForUser(selectedUsername)->oldestMessageId;
    }
    m_dataService->updateOrAddOldestMessageId(oldestMessageId);

    qDebug() << "Pagination state reset.";

    // Проверяем валидность основных виджетов
    if (!m_chatViewWidget || !m_chatModel || !m_rightSideLayout) {
        qWarning() << "CRITICAL: A core widget is a nullptr!";
        return;
    }

    // Очищаем кэш делегата и модель сообщений
    if (m_chatDelegate) {
        m_chatDelegate->clearSizeHintCache();
        m_chatDelegate->clearCaches();
    }
    m_chatModel->clearMessages();
    if (!m_chatViewWidget) {
        qWarning() << "[MainWindow] ChatViewWidget was deleted!";
        return;
    }
    
    // Обновляем заголовок чата
    m_chatViewWidget->updateHeader((*(m_dataService->getCurrentChatPartner())));

    // Переключаемся на виджет чата
    m_rightSideStackedLayout->setCurrentWidget(m_chatViewWidget);
    qDebug() << "Switched to ChatViewWidget.";
    
    // Синхронизируем историю чата с сервером
    m_dataService->syncChatHistory(selectedUsername);

    // Восстанавливаем черновик
    QString draft = m_dataService->getDraft(selectedUsername);
    m_chatViewWidget->messageTextEdit()->setPlainText(draft);

    // Очищаем черновик из метаданных чата
    const Chat& chat  = m_dataService->getChatMetadata(selectedUsername);
    Chat chatUpdated = chat;
    chatUpdated.draftText = "";
    m_dataService->updateOrAddChatMetadata(chatUpdated);

    // Сбрасываем счетчик непрочитанных
    m_dataService->resetUnreadCount(selectedUsername);

    // Обрабатываем видимые сообщения через 50мс
    QTimer::singleShot(50, this, &MainWindow::processVisibleMessages);

    qDebug() << "--- onUserSelectionChanged END (success) ---";
}
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Горячие клавиши для тестирования VoIP (F5 - гамма, F6 - частоты)
    if (event->key() == Qt::Key_F5) {
        m_callService->playMusicalScale();
    }
    else if (event->key() == Qt::Key_F6) {
        m_callService->testFrequencyRange();
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::toggleFullScreen()
{
    // Переключение полноэкранного режима (F11)
    if (isFullScreen()) {
        showNormal();       
    } else {
        showFullScreen();   
    }
}

void MainWindow::showProfileView(User Us)
{
    qDebug() << "[CLIENT] Showing profile for" << Us.username;

    // Показываем профиль в bottom sheet (с флагом "свой профиль")
    m_profileViewWidget->setUserProfile(Us, Us.username == m_dataService->getCurrentUser()->username);

    BottomSheetDialog* dlg = new BottomSheetDialog(m_profileViewWidget, this);
    dlg->exec();

    qDebug() << "showProfileView";
}

void MainWindow::showProfileView()
{
    // Перегруженная версия для текущего собеседника
    if(m_dataService->getCurrentChatPartner()){
        qDebug() << "[CLIENT] Showing profile for" <<m_dataService->getCurrentChatPartner()->username;
        m_profileViewWidget->setUserProfile(*m_dataService->getCurrentChatPartner(), false);
        BottomSheetDialog* dlg = new BottomSheetDialog(m_profileViewWidget, this);
        dlg->exec();
    }
}

void MainWindow::hideProfileView()
{
    qDebug() << "[CLIENT] Hiding profile, returning to chat view.";

    // Возвращаемся к чату или placeholder
    if(m_dataService->getCurrentChatPartner() != nullptr && !m_dataService->getCurrentChatPartner()->username.isEmpty()){
        qDebug() << m_dataService->getCurrentChatPartner()->username;
        m_rightSideStackedLayout->setCurrentWidget(m_chatViewWidget);
    } else{
        m_rightSideStackedLayout->setCurrentWidget(m_placeholderWidget);
    }
}

void MainWindow::showContactRequestPrompt(const QString& fromUsername, const QString& fromDisplayName)
{
    // Показываем диалог подтверждения заявки на контакт
    QString questionText = QString("Пользователь %1 (@%2) хочет добавить вас в список контактов. Принять запрос?")
                              .arg(fromDisplayName, fromUsername);

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Запрос на добавление в контакты", questionText,
                                  QMessageBox::Yes | QMessageBox::No);

    // Отправляем ответ на сервер
    QJsonObject contactResponse;
    contactResponse["type"] = "contact_request_response";
    contactResponse["fromUsername"] = fromUsername;  

    if (reply == QMessageBox::Yes) {
        contactResponse["response"] = "accepted";
    } else {
        contactResponse["response"] = "declined";
    }

    m_networkService->sendJson(contactResponse);
}

void MainWindow::onRequestAccepted(const QJsonObject& request) {
    // Автоматическое принятие заявки из IncomingRequestsWidget
    QJsonObject contactResponse;
    contactResponse["type"] = "contact_request_response";     
    contactResponse["fromUsername"] = request["fromUsername"];  

    contactResponse["response"] = "accepted";  
    m_networkService->sendJson(contactResponse);  
}

void MainWindow::onRequestRejected(const QJsonObject& request) {
    // Автоматическое отклонение заявки из IncomingRequestsWidget
    QJsonObject contactResponse;
    contactResponse["type"] = "contact_request_response";
    contactResponse["fromUsername"] = request["fromUsername"];

    contactResponse["response"] = "declined";
    m_networkService->sendJson(contactResponse);
}

void MainWindow::onGlobalSearchTriggered()
{
    QString query = m_searchLineEdit->text().trimmed();

    // Скрываем popup если пустой поиск
    if (query.isEmpty()) {
        if (m_searchResultsPopup && m_searchResultsPopup->isVisible()) {
            m_searchResultsPopup->hide();
        }
        return;
    }

    qDebug() << "[CLIENT] Triggered global user search for:" << query;

    // Отправляем запрос поиска пользователей на сервер
    QJsonObject request;
    request["type"] = "search_users";
    request["term"] = query;
    m_networkService->sendJson(request);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Скрываем popup поиска при клике вне его области
    if (event->type() == QEvent::MouseButtonPress && m_searchResultsPopup->isVisible()) {
        if (!m_searchResultsPopup->geometry().contains(QCursor::pos())) {
            m_searchResultsPopup->hide();
        }
    }
    
    // Завершаем VoIP звонок при закрытии окна
    if (event->type() == QEvent::Close) {
        m_callService->endCall();
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onChatScroll(int value)
{
    // Игнорируем программную прокрутку и загрузку истории
    if (m_programmaticScrollInProgress || m_dataService->getIsLoadingHistory()) {
        return;
    }

    // Загружаем старые сообщения при прокрутке к верху
    if (value == 0 && m_dataService->getOldestMessageId() != 0) {
        QScrollBar* scrollBar = m_chatViewWidget->chatHistoryView()->verticalScrollBar();

        m_expectingRangeChange = true;        
        m_oldScrollMax = scrollBar->maximum();  

        qDebug() << "[SCROLL] Scrolled to top. Expecting range change. Old max:" << m_oldScrollMax;

        m_dataService->updateOrAddIsLoadingHistory(true);  

        QJsonObject request;
        request["type"] = "get_history";
        request["with_user"] = m_dataService->getCurrentChatPartner()->username;
        request["before_id"] = m_dataService->getOldestMessageId();
        m_networkService->sendJson(request);
    }
}

void MainWindow::onSendMessageReadReceipt(qint64 messageId)
{
    qDebug() << "[CLIENT] Received receipt signal for message ID:" << messageId << ". Sending to server.";

    // Формируем подтверждение прочтения
    QJsonObject readCmd;
    readCmd["type"] = "message_read";
    readCmd["id"] = (double)messageId;
    
    // Локально обновляем статус сообщения
    m_dataService->updateMessageStatus(messageId, ChatMessage::MessageStatus::Read);;

    m_networkService->sendJson(readCmd);
}

void MainWindow::onRequestServerHistory(const QString& chatPartner, int afterId) {
    qDebug() << "[MainWindow] Requesting server history for" << chatPartner << "after id" << afterId;

    // Запрашиваем историю сообщений после определенного ID
    QJsonObject request;
    request["type"] = "get_history";
    request["with_user"] = chatPartner;
    request["after_server_id"] = afterId;

    m_networkService->sendJson(request);
}

MainWindow::~MainWindow()
{
    qDebug() << "[DEBUG] MainWindow destructor called";
    delete ui;
}
