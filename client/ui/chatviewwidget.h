#ifndef CHATVIEWWIDGET_H
#define CHATVIEWWIDGET_H

#include <QWidget>
#include "structures.h"

class QStackedWidget;
class QLabel;
class QToolButton;
class QLineEdit;
class QListView;
class QTextEdit;
class QPropertyAnimation;

namespace Ui {
class ChatViewWidget;
}

/**
 * @brief Виджет, отображающий содержимое активного чата.
 *
 * Является основным компонентом UI переписки. Содержит:
 * - Заголовок с информацией о собеседнике (имя, статус, аватар)
 * - Список сообщений (ChatHistoryView)
 * - Панель поиска по сообщениям
 * - Поле ввода сообщения с кнопкой отправки и аттачем файлов
 * - Панель ответа/редактирования (Reply UI)
 * - Кнопку быстрой прокрутки вниз (Scroll-to-bottom)
 */
class ChatViewWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Конструктор виджета чата.
     * @param parent Родительский виджет.
     */
    explicit ChatViewWidget(QWidget *parent = nullptr);

    /** @brief Деструктор. */
    ~ChatViewWidget();

    /** @brief Возвращает указатель на QListView с историей сообщений. */
    QListView* chatHistoryView() const;

    /** @brief Возвращает указатель на поле ввода текста сообщения. */
    QTextEdit* messageTextEdit() const;

    /**
     * @brief Проверяет, находится ли список сообщений в самом низу.
     * Используется для управления видимостью кнопки "Вниз".
     * @return true, если скролл в нижней позиции.
     */
    bool isScrolledToBottom() const;

signals:
    /** @brief Запрос на скачивание прикрепленного файла. */
    void fileDownloadRequested(const QString &fileId, const QString &url, const QString &fileName);

    /** @brief Запрос на прикрепление файла (открытие диалога выбора). */
    void attachFileRequested();

    /** @brief Пользователь нажал кнопку отправки сообщения. */
    void sendMessageRequested(const QString& text);

    /** @brief Клик по заголовку чата (для просмотра профиля). */
    void headerClicked();

    /** @brief Нажата кнопка поиска (лупа). */
    void searchButtonClicked();

    /** @brief Запрос на ответ конкретному сообщению (через контекстное меню). */
    void replyToMessageRequested(qint64 messageId);

    /** @brief Отмена режима ответа (нажат крестик в Reply UI). */
    void replyCancelled();

    /** @brief Введен текст в поисковую строку. */
    void searchTextEntered(const QString& text);

    /** @brief Запрос прокрутки к первому непрочитанному сообщению. */
    void scrollToUnreadRequested();

    /** @brief Запрос принудительной прокрутки в конец чата. */
    void scrollToBottomRequested();

    /** @brief Нажата кнопка аудиозвонка. */
    void callRequested();

    /** @brief Запрос на редактирование сообщения. */
    void editMessageRequested(qint64 messageId, const QString& oldText);

    /** @brief Запрос на удаление сообщения. */
    void deleteMessageRequested(qint64 messageId);

public slots:
    /** @brief Очищает UI чата (историю, поле ввода, статус). */
    void clearAll();

    /** @brief Слот обработки поиска (при вводе текста). */
    void onSearchTriggered(const QString& text);

    /** @brief Отображение контекстного меню сообщения по координатам. */
    void onChatContextMenuRequested(const QPoint &pos);

    /** @brief Обработка двойного клика по сообщению. */
    void onMessageDoubleClicked(const QModelIndex &index);

    /** @brief Обработчик прокрутки списка сообщений (для пагинации и кнопки "Вниз"). */
    void onChatScrolled(int value);

    /** @brief Показывает панель поиска в заголовке. */
    void showSearchUI();

    /** @brief Скрывает панель поиска и возвращает обычный заголовок. */
    void hideSearchUI();

    /**
     * @brief Обновляет информацию в заголовке чата.
     * @param chatPartner Данные собеседника (имя, статус, аватар).
     */
    void updateHeader(const User& chatPartner);

    /**
     * @brief Включает или выключает режим редактирования сообщения.
     * @param enabled true для включения.
     * @param text Исходный текст редактируемого сообщения.
     */
    void setEditMode(bool enabled, const QString& text = QString());

    /** @brief Скрывает и очищает панель ответа (Reply UI). */
    void clearReplyUI();

    /**
     * @brief Показывает панель ответа над полем ввода.
     * @param name Имя автора исходного сообщения.
     * @param text Текст исходного сообщения (цитата).
     */
    void showReplyUI(const QString& name, const QString& text);

    /** @brief Скрывает панель ответа (алиас для clearReplyUI). */
    void hideReplyUI();

    /**
     * @brief Обновляет счетчик непрочитанных на кнопке "Вниз".
     * @param unreadCount Число новых сообщений.
     */
    void onUnreadMessageCount(qint64 unreadCount);

    /** @brief Прокручивает список сообщений в самый низ. */
    void scrollToBottom();

    /**
     * @brief Прокручивает список к конкретному сообщению.
     * @param index Индекс сообщения в модели.
     */
    void scrollToMessage(const QModelIndex& index);

    /** @brief Слот нажатия кнопки плавающей прокрутки вниз. */
    void onScrollDownButtonClicked();

    /** @brief Слот нажатия кнопки звонка в заголовке. */
    void onCallButtonClicked();

protected:
    /** @brief Обработчик изменения размеров виджета (для пересчета лейаутов). */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Фильтр событий для перехвата клавиш (Enter, Esc) в поле ввода.
     *
     * Реализует отправку по Enter и перенос строки по Shift+Enter.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

    /** @brief Инициализация компонентов заголовка (кнопок, лейблов). */
    void setupHeaderUI();

    /** @brief Управляет видимостью кнопки "Вниз" и бейджем непрочитанных. */
    void updateScrollToBottomButton();

protected:
    QStackedWidget* m_headerStack;        ///< Стек для переключения между обычным заголовком и поиском
    QWidget* m_normalHeaderWidget;        ///< Виджет обычного заголовка (инфо о юзере)
    QWidget* m_searchHeaderWidget;        ///< Виджет заголовка поиска (поле ввода)
    Ui::ChatViewWidget *ui;               ///< Указатель на UI, сгенерированный из .ui файла

    QLabel* m_nameLabel;                  ///< Отображение имени собеседника
    QLabel* m_statusLabel;                ///< Отображение статуса ("в сети", "был недавно")
    QToolButton* m_searchButton;          ///< Кнопка открытия поиска
    QToolButton* m_callButton;            ///< Кнопка аудиозвонка
    QToolButton* m_videoCallButton;       ///< Кнопка видеозвонка
    QToolButton* m_moreOptionsButton;     ///< Кнопка "Три точки" (меню действий)
    QToolButton* m_closeReplyButton;      ///< Кнопка закрытия панели ответа
    QLineEdit* m_searchLineEdit;          ///< Поле ввода поискового запроса
    QToolButton* m_closeSearchButton;     ///< Кнопка закрытия поиска
    QToolButton* m_scrollToBottomButton;  ///< Плавающая кнопка прокрутки вниз
    QLabel* m_unreadCountLabel;           ///< Бейдж количества непрочитанных на кнопке прокрутки
    int m_unreadMessageCount = 0;         ///< Локальный счетчик непрочитанных

    QPropertyAnimation* m_replyAnimation; ///< Анимация появления панели ответа
    QWidget* m_userInfoWidget;            ///< Контейнер для инфо о пользователе (для кликабельности)
    QPropertyAnimation* m_scrollAnimation;///< Анимация плавной прокрутки
};

#endif // CHATVIEWWIDGET_H

/**
 * @brief Склоняет слово в зависимости от числа (1 сообщение, 2 сообщения, 5 сообщений).
 * @param n Число.
 * @param form1 Форма для 1 (сообщение).
 * @param form2 Форма для 2-4 (сообщения).
 * @param form5 Форма для 5-0 (сообщений).
 * @return Правильная словоформа.
 */
QString pluralize(int n, const QString& form1, const QString& form2, const QString& form5);

/**
 * @brief Форматирует строку времени "был в сети...".
 * @param user Объект пользователя с данными о lastSeen.
 * @return Строка статуса (напр. "был в сети 5 минут назад" или "Онлайн").
 */
QString formatLastSeen(const User &user);
