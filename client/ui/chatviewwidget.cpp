#include "chatviewwidget.h"
#include "chatmessagedelegate.h"
#include "ui_chatviewwidget.h"
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QLineEdit>
#include <QSpacerItem>
#include <QDateTime>
#include <QListView>
#include <QMenu>
#include <QAction>
#include <QScrollBar>
#include <QResizeEvent>
#include <QTextEdit>
#include <QEvent>
#include <QKeyEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <QMouseEvent>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <algorithm>


ChatViewWidget::ChatViewWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatViewWidget())
{
    ui->setupUi(this);

    // Заголовок/инфо по чату
    setupHeaderUI();

    // Анимация раскрытия/скрытия reply-блока
    m_replyAnimation = new QPropertyAnimation(ui->replyWidget, "maximumHeight", this);
    m_replyAnimation->setDuration(200);
    m_replyAnimation->setEasingCurve(QEasingCurve::OutCubic);
    ui->replyWidget->hide();

    connect(m_replyAnimation, &QPropertyAnimation::finished, this, [this]() {
        qDebug() << "Reply widget visibility:" << ui->replyWidget->isVisible();
        updateScrollToBottomButton();
    });

    // Авто-ресайз поля ввода по высоте содержимого
    ui->messageTextEdit->setFixedHeight(std::max(ui->sendButton->height(), 40));
    ui->messageTextEdit->installEventFilter(this);
    connect(ui->messageTextEdit, &QTextEdit::textChanged, this, [this]() {
        int contentHeight = ui->messageTextEdit->document()->size().height();
        int minH = ui->sendButton->height();
        int maxH = 200;
        int newHeight = std::min(std::max(contentHeight, minH), maxH);
        ui->messageTextEdit->setFixedHeight(newHeight);
    });

    // Плавающая кнопка "проскроллить вниз"
    m_scrollToBottomButton = new QToolButton(this);
    m_scrollToBottomButton->setObjectName("scrollToBottomButton");
    m_scrollToBottomButton->setIcon(QIcon(":/icons/icons/down_arrow.png"));
    m_scrollToBottomButton->setIconSize(QSize(24, 24));
    m_scrollToBottomButton->setFixedSize(40, 40);
    m_scrollToBottomButton->hide();

    // Бейдж количества непрочитанных над кнопкой скролла
    m_unreadCountLabel = new QLabel(this);
    m_unreadCountLabel->setObjectName("unreadCountLabel");
    m_unreadCountLabel->setAlignment(Qt::AlignCenter);
    m_unreadCountLabel->setFixedSize(22, 22);
    m_unreadCountLabel->hide();

    // Закрытие reply-панели
    m_closeReplyButton = ui->closeReplyButton;
    connect(m_closeReplyButton, &QToolButton::clicked,
            this, &ChatViewWidget::hideReplyUI);

    // Отправка сообщения по клику на кнопку
    connect(ui->sendButton, &QPushButton::clicked, this, [this]() {
        QString text = ui->messageTextEdit->toPlainText().trimmed();
        emit sendMessageRequested(text);
        ui->messageTextEdit->clear();
    });

    // Контекстное меню по истории чата
    ui->chatHistoryView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->chatHistoryView, &QWidget::customContextMenuRequested,
            this, &ChatViewWidget::onChatContextMenuRequested);

    // Скролл вниз кнопкой + реакция на прокрутку
    connect(m_scrollToBottomButton, &QToolButton::clicked,
            this, &ChatViewWidget::onScrollDownButtonClicked);
    connect(ui->chatHistoryView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ChatViewWidget::onChatScrolled);

    // Прикрепление файла и поиск по сообщениям
    connect(ui->attachButton, &QPushButton::clicked,
            this, [this]() { emit attachFileRequested(); });
    connect(m_searchButton, &QToolButton::clicked,
            this, &ChatViewWidget::showSearchUI);

    // Плавный скролл истории через QPropertyAnimation
    m_scrollAnimation = new QPropertyAnimation(this);
    m_scrollAnimation->setTargetObject(ui->chatHistoryView->verticalScrollBar());
    m_scrollAnimation->setPropertyName("value");
    m_scrollAnimation->setDuration(400);
    m_scrollAnimation->setEasingCurve(QEasingCurve::InOutCubic);

    // Для hover-эффектов и кастомной обработки мыши
    ui->chatHistoryView->setMouseTracking(true);
    ui->chatHistoryView->viewport()->setMouseTracking(true);

    qDebug() << "[ChatViewWidget] constructed and UI initialized";
}

ChatViewWidget::~ChatViewWidget()
{
    delete ui;
    qDebug() << "[ChatViewWidget] destroyed";
}

void ChatViewWidget::onScrollDownButtonClicked()
{
    if (m_unreadMessageCount > 0) {
        qDebug() << "[ChatViewWidget] Скроллим к непрочитанным сообщениям:" << m_unreadMessageCount;
        emit scrollToUnreadRequested();
    } else {
        qDebug() << "[ChatViewWidget] Скроллим в конец истории чата";
        emit scrollToBottomRequested();
    }
}

bool ChatViewWidget::eventFilter(QObject *watched, QEvent *event)
{
    // Отправка по Enter, перевод строки по Shift+Enter
    if (watched == ui->messageTextEdit &&
        event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return ||
            keyEvent->key() == Qt::Key_Enter)
        {
            if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                ui->sendButton->click();
                return true; // съедаем событие
            }
        }
    }

    // Клик по виджету с информацией о пользователе в шапке
    if (watched == m_userInfoWidget &&
        event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            qDebug() << "User info widget clicked, emitting headerClicked()";
            emit headerClicked();
            return true;
        }
    }

    // Остальные события передаем базовой реализации
    return QWidget::eventFilter(watched, event);
}

void ChatViewWidget::showReplyUI(const QString& name, const QString& text)
{
    // Подпись "В ответ <имя>"
    ui->replyNameLabel->setText("В ответ " + name);

    // Обрезаем текст цитаты по ширине лейбла
    QFontMetrics fm(ui->replyTextLabel->font());
    QString elidedText = fm.elidedText(
        text, Qt::ElideRight, ui->replyTextLabel->width());
    ui->replyTextLabel->setText(elidedText);
    ui->replyWidget->show();

    // Плавное раскрытие reply-панели
    m_replyAnimation->setStartValue(0);
    m_replyAnimation->setEndValue(50);
    m_replyAnimation->setDirection(QAbstractAnimation::Forward);
    m_replyAnimation->start();

    ui->messageTextEdit->setFocus();
    qDebug() << "[ChatViewWidget] showReplyUI for" << name;
}

void ChatViewWidget::hideReplyUI()
{
    // Сброс состояния ответа и уведомление логики
    clearReplyUI();
    emit replyCancelled();
    ui->replyWidget->hide();

    // Небольшая задержка перед перерасчётом позиции кнопки скролла
    QTimer::singleShot(50, this, &ChatViewWidget::updateScrollToBottomButton);
}

void ChatViewWidget::setupHeaderUI()
{
    // Стек: обычный заголовок / строка поиска
    m_headerStack       = new QStackedWidget(this);
    m_normalHeaderWidget = new QWidget(this);
    m_searchHeaderWidget = new QWidget(this);

    // Блок с именем и статусом собеседника
    m_userInfoWidget = new QWidget(this);
    QVBoxLayout* userInfoLayout = new QVBoxLayout(m_userInfoWidget);
    userInfoLayout->setSpacing(0);
    userInfoLayout->setContentsMargins(8, 0, 0, 0);

    QHBoxLayout* normalHeaderLayout = new QHBoxLayout();
    m_normalHeaderWidget->setLayout(normalHeaderLayout);
    normalHeaderLayout->setContentsMargins(0, 5, 5, 5);

    m_nameLabel = new QLabel("Имя собеседника", this);
    m_nameLabel->setObjectName("chatPartnerNameLabel");

    m_statusLabel = new QLabel("статус", this);
    m_statusLabel->setObjectName("chatPartnerStatusLabel");

    userInfoLayout->addWidget(m_nameLabel);
    userInfoLayout->addWidget(m_statusLabel);
    userInfoLayout->setSpacing(0);
    userInfoLayout->setContentsMargins(0, 0, 0, 0);

    // Клик по области userInfo => сигнал headerClicked()
    m_userInfoWidget->installEventFilter(this);
    m_userInfoWidget->setCursor(Qt::PointingHandCursor);

    // Кнопки в заголовке
    m_searchButton = new QToolButton(this);
    m_searchButton->setObjectName("searchInChatButton");
    m_searchButton->setIcon(QIcon(":/icons/icons/search.png"));

    m_callButton = new QToolButton(this);
    m_callButton->setObjectName("callButton");
    m_callButton->setIcon(QIcon(":/icons/icons/audioCall.png"));

    m_videoCallButton = new QToolButton(this);
    m_videoCallButton->setObjectName("videoCallButton");
    m_videoCallButton->setIcon(QIcon(":/icons/icons/videoCall.png"));

    m_moreOptionsButton = new QToolButton(this);
    m_moreOptionsButton->setObjectName("moreOptionsButton");
    m_moreOptionsButton->setIcon(QIcon(":/icons/icons/dotsVertical.png"));

    // Обычный хедер: инфо слева, кнопки справа
    normalHeaderLayout->addWidget(m_userInfoWidget);
    normalHeaderLayout->addSpacerItem(
        new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    normalHeaderLayout->addWidget(m_searchButton);
    normalHeaderLayout->addWidget(m_callButton);
    normalHeaderLayout->addWidget(m_videoCallButton);
    normalHeaderLayout->addWidget(m_moreOptionsButton);

    // Хедер поиска: поле + кнопка закрытия
    QHBoxLayout* searchHeaderLayout = new QHBoxLayout(m_searchHeaderWidget);
    searchHeaderLayout->setContentsMargins(5, 5, 5, 5);

    m_searchLineEdit = new QLineEdit(this);
    m_searchLineEdit->setPlaceholderText("Поиск в чате...");

    m_closeSearchButton = new QToolButton(this);
    m_closeSearchButton->setIcon(QIcon(":/icons/icons/cross.png"));

    searchHeaderLayout->addWidget(m_searchLineEdit);
    searchHeaderLayout->addWidget(m_closeSearchButton);

    // Добавляем оба варианта в стек
    m_headerStack->addWidget(m_normalHeaderWidget);
    m_headerStack->addWidget(m_searchHeaderWidget);

    // Встраиваем стек в ui->headerWidget
    if (ui->headerWidget->layout()) {
        ui->headerWidget->layout()->addWidget(m_headerStack);
    } else {
        QHBoxLayout* mainHeaderLayout = new QHBoxLayout(ui->headerWidget);
        mainHeaderLayout->setContentsMargins(0, 0, 0, 0);
        mainHeaderLayout->addWidget(m_headerStack);
    }

    // Сигналы управления поиском и звонками
    connect(m_closeSearchButton, &QToolButton::clicked,
            this, &ChatViewWidget::hideSearchUI);
    connect(m_searchLineEdit, &QLineEdit::textChanged,
            this, &ChatViewWidget::searchTextEntered);
    connect(m_searchButton, &QToolButton::clicked,
            this, &ChatViewWidget::showSearchUI);
    connect(m_callButton, &QToolButton::clicked,
            this, &ChatViewWidget::onCallButtonClicked);

    qDebug() << "[ChatViewWidget] Header UI инициализирован";
}
void ChatViewWidget::showSearchUI() {
    // Переключаем стековый виджет заголовка на панель поиска
    m_headerStack->setCurrentWidget(m_searchHeaderWidget);
    
    // Устанавливаем фокус на поле ввода поиска для удобства пользователя
    m_searchLineEdit->setFocus();
    
    // Логируем событие для отладки
    qDebug() << "[ChatViewWidget] showSearchUI triggered";
}

void ChatViewWidget::onCallButtonClicked() {
    // Логируем клик по кнопке для отладки
    qDebug() << "Call button clicked";
    
    // Испускаем сигнал для инициации звонка через родительский компонент
    emit callRequested();
}

void ChatViewWidget::hideSearchUI() {
    // Очищаем текст в поле поиска
    m_searchLineEdit->clear();
    
    // Возвращаем обычный заголовок чата
    m_headerStack->setCurrentWidget(m_normalHeaderWidget);
    
    // Логируем восстановление интерфейса
    qDebug() << "[ChatViewWidget] hideSearchUI, header restored";
}

void ChatViewWidget::clearReplyUI()
{
    // Восстанавливаем стандартный плейсхолдер для ввода сообщений
    ui->messageTextEdit->setPlaceholderText("Напишите сообщение...");
}

void ChatViewWidget::onMessageDoubleClicked(const QModelIndex &index)
{
    // Логируем начало обработки двойного клика
    qDebug() <<"onMessageDoubleClicked 1";
    
    // Проверяем валидность индекса, выходим если сообщение не найдено
    if (!index.isValid()) return;
    
    // Логируем успешное получение индекса
    qDebug() <<"onMessageDoubleClicked 2";
    
    // Извлекаем данные сообщения из модели
    ChatMessage msg = index.data(Qt::UserRole).value<ChatMessage>();
    
    // Активируем режим ответа с данными сообщения
    showReplyUI(msg.fromUser, msg.payload);
    
    // Уведомляем о необходимости ответа на конкретное сообщение
    emit replyToMessageRequested(msg.id);
}

void ChatViewWidget::onChatContextMenuRequested(const QPoint &pos)
{
    // Получаем индекс сообщения по координатам клика
    QModelIndex index = ui->chatHistoryView->indexAt(pos);
    
    // Выходим если клик не по сообщению
    if (!index.isValid()) return;
    
    // Извлекаем данные выбранного сообщения
    ChatMessage msg = index.data(Qt::UserRole).value<ChatMessage>();
    
    // Создаем контекстное меню
    QMenu contextMenu(this);
    
    // Добавляем действия меню
    QAction *copyAction = contextMenu.addAction("Копировать текст");
    QAction *replyAction = contextMenu.addAction("Ответить");
    QAction *editAction = contextMenu.addAction("Редактировать");
    QAction *deleteAction = contextMenu.addAction("Удалить");

    // Отключаем редактирование/удаление для входящих сообщений
    if (!msg.isOutgoing) {
        editAction->setEnabled(false);
        deleteAction->setEnabled(false);
    }
    
    // Отключаем копирование для пустых сообщений
    if (msg.payload.isEmpty()) {
        copyAction->setEnabled(false);
    }
    
    // Показываем меню и получаем выбранное действие
    QAction *selectedAction = contextMenu.exec(ui->chatHistoryView->viewport()->mapToGlobal(pos));
    
    // Копируем текст в буфер обмена
    if (selectedAction == copyAction) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(msg.payload);
    } 
    // Переходим в режим ответа
    else if (selectedAction == replyAction) {
        onMessageDoubleClicked(index);
    } 
    // Запрашиваем редактирование сообщения
    else if (selectedAction == editAction) {
        emit editMessageRequested(msg.id, msg.payload);
    } 
    // Запрашиваем удаление сообщения
    else if (selectedAction == deleteAction) {
        emit deleteMessageRequested(msg.id);
    }
}

QListView* ChatViewWidget::chatHistoryView() const { 
    // Возвращаем виджет списка истории чата
    return ui->chatHistoryView; 
}

QTextEdit* ChatViewWidget::messageTextEdit() const {
    // Возвращаем поле ввода сообщений
    return ui->messageTextEdit;
}

void ChatViewWidget::clearAll(){
    // Безопасно очищаем поле ввода сообщений
    if (ui && ui->messageTextEdit){
        ui->messageTextEdit->clear();
    }

    // Скрываем режим ответа
    hideReplyUI();
    
    // Отключаем режим редактирования
    setEditMode(false);

    // Скрываем интерфейс поиска
    hideSearchUI();

    // Логируем полную очистку состояния
    qDebug() << "[ChatViewWidget] All fields, reply/edit state and attachments cleared.";
}

void ChatViewWidget::updateHeader(const User& chatPartner)
{
    // Логируем обновление заголовка
    qDebug() << "Header update" << chatPartner.username << " : " << chatPartner.isOnline;
    
    // Обновляем отображаемое имя пользователя
    m_nameLabel->setText(chatPartner.displayName);

    // Показываем статус "печатает..." если собеседник набирает текст
    if (chatPartner.isTyping) {
        m_statusLabel->setText("печатает...");
        m_statusLabel->setStyleSheet("color: #F4ABC4;");
    } 
    // Иначе показываем статус онлайн/оффлайн с временем последнего визита
    else {
        QString statusText = formatLastSeen(chatPartner);
        m_statusLabel->setText(statusText);
        m_statusLabel->setStyleSheet(chatPartner.isOnline ? "color: #4CAF50;" : "color: #a0a0a0;");
    }
}

QString pluralize(int n, const QString& form1, const QString& form2, const QString& form5) {
    // Приводим число к диапазону 0-99 для правильного склонения
    n = abs(n) % 100;
    int n1 = n % 10;
    
    // Русские правила склонения: 11-19 -> form5, 2-4 -> form2, 1 -> form1, иначе form5
    if (n > 10 && n < 20) return form5;
    if (n1 > 1 && n1 < 5) return form2;
    if (n1 == 1) return form1;
    return form5;
}

QString formatLastSeen(const User &user)
{
    // Возвращаем статус "в сети" для онлайн пользователя
    if (user.isOnline) return "в сети";
    
    // Нет данных о последнем визите
    if (user.lastSeen.isEmpty()) return "не в сети";
    
    // Парсим время последнего визита
    QDateTime lastSeenTime = QDateTime::fromString(user.lastSeen, Qt::ISODate);
    if (!lastSeenTime.isValid()) return "не в сети";
    
    // Текущее время для вычисления разницы
    QDateTime now = QDateTime::currentDateTime();
    qint64 diffSeconds = lastSeenTime.secsTo(now);

    // Только что (менее минуты)
    if (diffSeconds < 60) {
        return "был(а) только что";
    } 
    // Минуты назад
    else if (diffSeconds < 3600) {
        int minutes = diffSeconds / 60;
        return QString("был(а) %1 %2 назад").arg(minutes).arg(pluralize(minutes, "минуту", "минуты", "минут"));
    } 
    // Сегодня
    else if (lastSeenTime.date() == now.date()) {
        return "был(а) сегодня в " + lastSeenTime.toString("HH:mm");
    } 
    // Вчера
    else if (lastSeenTime.date() == now.date().addDays(-1)) {
        return "был(а) вчера в " + lastSeenTime.toString("HH:mm");
    } 
    // Давно - показываем дату
    else {
        return "был(а) " + QLocale::system().toString(lastSeenTime, QLocale::ShortFormat);
    }
}



void ChatViewWidget::onUnreadMessageCount(qint64 unreadCount)
{
    // Обновляем счетчик непрочитанных только если не находимся внизу списка
    if (!isScrolledToBottom()) {
        qDebug() << "m_unreadMessageCount"<< unreadCount;
        m_unreadMessageCount = unreadCount;
        updateScrollToBottomButton();
    }
}

void ChatViewWidget::scrollToBottom()
{
    // Принудительно обновляем модель для перерисовки элементов
    emit ui->chatHistoryView->model()->dataChanged(QModelIndex(), QModelIndex());
    
    // Останавливаем текущую анимацию прокрутки
    m_scrollAnimation->stop();
    
    // Настраиваем анимацию от текущей позиции до максимума (низ списка)
    m_scrollAnimation->setStartValue(ui->chatHistoryView->verticalScrollBar()->value());
    m_scrollAnimation->setEndValue(ui->chatHistoryView->verticalScrollBar()->maximum());
    m_scrollAnimation->start();

    // Обновляем видимость кнопки прокрутки вниз
    updateScrollToBottomButton();
}

void ChatViewWidget::onChatScrolled(int value)
{
    // Реакция на прокрутку - обновляем состояние кнопки "вниз"
    updateScrollToBottomButton();
}

void ChatViewWidget::scrollToMessage(const QModelIndex& index)
{
    // Если индекс невалиден - прокручиваем к низу чата
    if (!index.isValid()) {
        scrollToBottom();
        return;
    }

    // Вычисляем целевую позицию прокрутки для центрирования сообщения
    QRect messageRect = ui->chatHistoryView->visualRect(index);
    int targetValue = ui->chatHistoryView->verticalScrollBar()->value() + messageRect.top();

    // Останавливаем предыдущую анимацию и запускаем новую
    m_scrollAnimation->stop();
    m_scrollAnimation->setStartValue(ui->chatHistoryView->verticalScrollBar()->value());
    m_scrollAnimation->setEndValue(targetValue);
    m_scrollAnimation->start();

    // Обновляем кнопку прокрутки
    updateScrollToBottomButton();
}

bool ChatViewWidget::isScrolledToBottom() const
{
    // Проверяем нахождение внизу списка (с допуском 5px)
    QScrollBar* scrollBar = ui->chatHistoryView->verticalScrollBar();
    return scrollBar->value() >= scrollBar->maximum() - 5;
}

void ChatViewWidget::resizeEvent(QResizeEvent *event)
{
    // Вызываем базовый обработчик изменения размера
    QWidget::resizeEvent(event);

    // При изменении ширины перестраиваем layout сообщений
    if (event->oldSize().width() != event->size().width()) {
        auto delegate = qobject_cast<ChatMessageDelegate*>(ui->chatHistoryView->itemDelegate());
        if (delegate) {
            // Очищаем кэш размеров для пересчета под новую ширину
            delegate->clearSizeHintCache();
            delegate->clearCaches();
        }
        ui->chatHistoryView->doItemsLayout();
    }
    updateScrollToBottomButton();
}

void ChatViewWidget::updateScrollToBottomButton()
{
    // Получаем текущую прокрутку
    QScrollBar* scrollBar = ui->chatHistoryView->verticalScrollBar();
    const int scrollThreshold = 1000;
    
    // Показываем кнопку если есть непрочитанные или далеко от низа
    bool showButton = (m_unreadMessageCount > 0) || (scrollBar->maximum() - scrollBar->value() > scrollThreshold);

    if (showButton) {
        // Вычисляем позицию кнопки в правом нижнем углу
        int margin = 15;
        QPoint buttonPos(width() - m_scrollToBottomButton->width() - margin, 
                        height() - m_scrollToBottomButton->height() - ui->messageInputWidget->height() - margin);
        m_scrollToBottomButton->move(buttonPos);

        // Показываем бейдж с количеством непрочитанных
        if (m_unreadMessageCount > 0) {
            m_unreadCountLabel->setText(QString::number(m_unreadMessageCount));
            QPoint labelPos(buttonPos.x() + (m_scrollToBottomButton->width() / 2), 
                          buttonPos.y() - m_unreadCountLabel->height() / 2);
            m_unreadCountLabel->move(labelPos);
            m_unreadCountLabel->show();
        } else {
            m_unreadCountLabel->hide();
        }
        m_scrollToBottomButton->show();
    } else {
        // Скрываем кнопку и бейдж если не нужны
        m_scrollToBottomButton->hide();
        m_unreadCountLabel->hide();
    }
}

void ChatViewWidget::onSearchTriggered(const QString& text)
{
    // Логируем запуск поиска по чату
    qDebug() << "Search in chat triggered:" << text;
}

void ChatViewWidget::setEditMode(bool enabled, const QString& text)
{
    if (enabled) {
        // Переключаем в режим редактирования
        ui->sendButton->setText("Сохранить");
        ui->messageTextEdit->setText(text);
        ui->messageTextEdit->setFocus();
    } else {
        // Возвращаемся в режим отправки нового сообщения
        ui->sendButton->setText("Отправить");
        ui->messageTextEdit->clear();
        ui->messageTextEdit->setPlaceholderText("Напишите сообщение...");
    }
}
