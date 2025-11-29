#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include "chatfilterproxymodel.h"
#include "structures.h"
#include "networkservice.h"
#include "callservice.h"
#include "callwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QStackedWidget;
class QTcpSocket;
class QJsonObject;
class QLineEdit;
class QPushButton;
class QStackedLayout;
class QTimer;
class QPoint;
class QListView;
class QToolButton;
class QMenu;
class QAction;
class QVBoxLayout;
class QWidget;
class ContactListModel;
class ContactListDelegate;
class ChatMessageDelegate;
class LoginWidget;
class ChatViewWidget;
class ProfileViewWidget;
class ChatMessageModel;
class ChatFilterProxyModel;
class SearchResultsPopup;
class DataService;
class IncomingRequestsWidget;
QT_END_NAMESPACE

/**
 * @brief Главное окно приложения.
 *
 * MainWindow является центральным контроллером UI. Он управляет:
 * - Переключением между экранами (Вход/Чат/Меню/Профиль).
 * - Связыванием сигналов бизнес-логики (Services) с UI-виджетами.
 * - Обработкой событий клавиатуры и глобальных горячих клавиш.
 * - Инициализацией и компоновкой всех основных виджетов приложения.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /** @brief Глобальный доступ к сервису данных (используется с осторожностью). */
    static DataService* m_dataService;

    /**
     * @brief Конструктор главного окна.
     * @param dataService Ссылка на сервис данных.
     * @param parent Родительский виджет.
     */
    MainWindow(DataService* dataService, QWidget *parent = nullptr);

    /** @brief Деструктор. */
    ~MainWindow();

    /** @brief Возвращает ссылку на виджет активного чата. */
    ChatViewWidget& getChatViewWidget();

    /** @brief Возвращает ссылку на модель сообщений текущего чата. */
    ChatMessageModel& getChatMessageModel();

public slots:
    /** @brief Обработка входящего запроса на добавление в друзья (показывает попап). */
    void onContactRequestReceived(const QJsonObject& request);

    /** @brief Переключение полноэкранного режима (F11). */
    void toggleFullScreen();

    /** @brief Обработка нажатия кнопки "Скрепка" (открыть диалог файла). */
    void onAttachFileRequested();

    /** @brief Запуск скачивания файла по клику в чате. */
    void onFileDownloadRequested(const QString &fileId, const QString &url, const QString &fileName);

    /** @brief Обновление аватара в интерфейсе при завершении загрузки. */
    void onAvatarUpdated(const QString &username, const QString &avatarUrl);

protected:
    /**
     * @brief Фильтр событий для перехвата кликов вне попапов.
     * Используется для закрытия поиска или меню при клике мимо.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

    /** @brief Обработка нажатий клавиш (Esc для закрытия поиска/меню). */
    void keyPressEvent(QKeyEvent *event);

private slots:
    /** @brief Отправка выбранного файла на сервер. */
    void uploadFileToGo(const QString& filePath);

    // --- Слоты аутентификации ---
    void onLoginFailed(const QString& error);
    void onRegisterFailed(const QString& error);
    void onTokenLoginFailed(const QString& reason);
    void attemptAutoLogin();
    void onLoginSuccess(const QJsonObject& response);
    void onLoginFailure(const QString& reason);
    void onRegisterSuccess();
    void onRegisterFailure(const QString& reason);
    void onLogoutSuccess();
    void onLogoutFailure(const QString& reason);

    // --- UI События ---
    void onContactListContextMenu(const QPoint& pos);
    void onJsonReceived(const QJsonObject& response);
    void onContactsUpdated(const QStringList& sortedUsernames);
    void onOnlineStatusUpdated();
    void onOlderHistoryChunkPrepended(const QString& chatPartner, const QList<ChatMessage>& messages);
    void onHistoryLoaded(const QString& chatPartner, const QList<ChatMessage>& messages);
    void onNewMessageReceived(const ChatMessage& incomingMsg);
    void onMessageStatusChanged(qint64 messageId, ChatMessage::MessageStatus newStatus);
    void onUnreadCountChanged();
    void onMessageEdited(const QString& chatPartner, qint64 messageId, const QString& newPayload);
    void onMessageDeleted(const QString& chatPartner, qint64 messageId);
    void onConfirmMessageSent(QString tempId, const ChatMessage& msg);

    // --- Поиск и Контакты ---
    void onSearchResultsReceived(const QJsonArray& users);
    void onAddContactSuccess(const QString& username);
    void onAddContactFailure(const QString& reason);
    void onPendingContactRequestsUpdated(const QJsonArray& requests);
    void onAddContactRequested(const QString& username);
    void onChatSearchTriggered(const QString &text);
    void onGlobalSearchTriggered();
    void onRequestAccepted(const QJsonObject& request);
    void onRequestRejected(const QJsonObject& request);

    // --- Взаимодействие с чатом ---
    void onSendMessageRequested(const QString& text);
    void onUserSelectionChanged(const QModelIndex &current);
    void onEditMessageRequested(qint64 messageId, const QString& oldText);
    void onDeleteMessageRequested(qint64 messageId);
    void onReplyToMessage(qint64 messageId);
    void onSendMessageReadReceipt(qint64 messageId);
    void onChatScroll(int value);
    void onTypingNotificationFired();
    void onTypingStatusChanged(const QString& username, bool isTyping);
    void onScrollToUnread();
    void onScrollToBottom();
    void onScrollBarRangeChanged();
    void processVisibleMessages();
    void onRequestServerHistory(const QString& chatPartner, int afterId);
    void onScrollToUnreadFast();
    void onScrollToBottomFast();

    // --- Состояние сети ---
    void onConnected();
    void onDisconnected();

    // --- Звонки ---
    void onCallRequested();
    void onCallsButtonClicked();

    // --- Меню и Навигация ---
    void onMenuButtonClicked();
    void onLogoutButtonClicked();
    void onBackFromMenu();
    void showProfileView();
    void showProfileView(User Us);
    void hideProfileView();
    void onMyProfileClicked();
    void onProfileUpdateResult(const QJsonObject& response);

signals:
    /** @brief Сигнал о получении нового сообщения в текущем активном чате. */
    void newMessageForCurrentChat();

private:
    /** @brief Создание и компоновка всех UI элементов. */
    void buildMainUI();

    /** @brief Подключение сигналов и слотов между компонентами. */
    void setupConnections();

    /** @brief Инициализация обработчиков ответов (если используется). */
    void initResponseHandlers();

    /** @brief Сброс состояния UI к экрану входа (Logout). */
    void resetApplicationState();

    /** @brief Обновление списка пользователей (deprecated?). */
    void updateUserList();

    /** @brief Показ диалога входящего запроса в друзья. */
    void showContactRequestPrompt(const QString& fromUsername, const QString& fromDisplayName);

    /** @brief Настройка страницы меню (бургер-меню). */
    void setupMenuPage();

    /** @brief Хелпер для создания кнопок меню. */
    QPushButton* createMenuButton(const QString& text, const QString& badge);

private:
    Ui::MainWindow *ui;

    // Сервисы (QPointer для безопасности при удалении)
    QPointer<NetworkService> m_networkService;
    QPointer<CallService> m_callService;

    // Виджеты экранов
    QPointer<LoginWidget> m_loginWidget;
    QPointer<QWidget> m_mainChatWidget;
    QPointer<QWidget> m_chatListPanel;
    QPointer<QWidget> m_rightSideContainer;
    QPointer<QWidget> m_placeholderWidget; // Заглушка "Выберите чат"

    // Элементы списка контактов
    QPointer<QLineEdit> m_searchLineEdit;
    QPointer<QListView> m_userListView;
    QPointer<QPushButton> m_logoutButton;
    QPointer<QToolButton> m_menuButton;
    QPointer<QMenu> m_mainMenu;
    QPointer<QAction> m_actionIncomingRequests;

    // Лейауты
    QPointer<QVBoxLayout> m_rightSideLayout;
    QPointer<QStackedLayout> m_rightSideStackedLayout;

    // Активные виджеты
    QPointer<ChatViewWidget> m_chatViewWidget;
    QPointer<ProfileViewWidget> m_profileViewWidget;
    QPointer<IncomingRequestsWidget> m_incomingRequestsWidget;
    QPointer<SearchResultsPopup> m_searchResultsPopup;
    QPointer<CallWidget> m_callWidget;

    // Модели
    QPointer<ChatMessageModel> m_chatModel;
    QPointer<ChatFilterProxyModel> m_chatFilterProxy;
    QPointer<ContactListModel> m_contactModel;
    QPointer<ChatMessageDelegate> m_chatDelegate;

    // Состояние скролла
    qint64 m_scrollAnchorId = 0;
    int m_oldScrollMax = 0;
    bool m_programmaticScrollInProgress = false;
    bool m_expectingRangeChange = false;

    // Навигация (левая панель)
    QStackedWidget* m_leftMainPanel;
    enum PageIndex {
        PAGE_CHATS = 0,
        PAGE_MENU = 1
    };
    QWidget* m_chatsPage;
    QWidget* m_menuPage;
};

#endif // MAINWINDOW_H
