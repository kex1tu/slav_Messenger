#include "dataservice.h"
#include <QJsonArray>
#include <algorithm>
#include <QDebug>
#include <QTimer>
#include <QListView>
#include <QScrollBar>
#include "tokenmanager.h"
#include "cryptoutils.h"

class MainWindow;

DataService::DataService(QObject *parent) : QObject(parent)
{
    // Создаем сервис работы с базой данных и делаем его дочерним объектом
    m_dbService = new DatabaseService(this);

    // Инициализируем БД (создание/миграция таблиц)
    if (!m_dbService->initialize("database.db")) {
        qDebug() << "[DataService] WARNING: Database initialization failed";
    }

    // Таймер для глобального поиска (debounce 300 мс)
    // Запускается при вводе текста, чтобы не дергать поиск на каждый символ
    m_globalSearchTimer = new QTimer(this);
    m_globalSearchTimer->setSingleShot(true);  // Срабатывает один раз после паузы
    m_globalSearchTimer->setInterval(300);     // 300 мс задержки

    // Таймер для отправки статуса "печатает..." (typing)
    m_typingSendTimer = new QTimer(this);
    m_typingSendTimer->setSingleShot(true);    // Один раз после последнего ввода
    m_typingSendTimer->setInterval(2000);      // 2 секунды тишины => перестали печатать

    // Кэш аватаров пользователей (загрузка и хранение локальных путей)
    m_avatarCache = new AvatarCache(this);
    connect(m_avatarCache, &AvatarCache::avatarDownloaded,
            this, &DataService::onAvatarDownloaded);
    
    // Регистрируем обработчики ответов от сервера (маршрутизация JSON-сообщений)
    initResponseHandlers();
}

void DataService::onAvatarDownloaded(const QString& username, const QString& localPath) {
    qDebug() << "[DataService] Avatar downloaded for:" << username << "at path:" << localPath;

    // Уведомляем UI, что у пользователя обновился аватар (можно обновить список чатов/контактов)
    emit avatarUpdated(username, localPath);
}

bool validateCredentials(const QString& username,
                         const QString& password,
                         QString& errorMsg)
{
    // Проверка имени пользователя на пустоту
    if (username.isEmpty()) {
        errorMsg = "Имя пользователя не может быть пустым";
        return false;
    }

    // Минимальная длина имени пользователя
    if (username.length() < 3) {
        errorMsg = "Имя пользователя должно содержать минимум 3 символа";
        return false;
    }

    // Максимальная длина имени пользователя
    if (username.length() > 20) {
        errorMsg = "Имя пользователя не должно превышать 20 символов";
        return false;
    }

    // Разрешаем только латинские буквы, цифры, "_" и "-"
    QRegularExpression usernameRegex("^[a-zA-Z0-9_-]+$");
    if (!usernameRegex.match(username).hasMatch()) {
        errorMsg = "Имя пользователя может содержать только латинские буквы, цифры, '_' и '-'";
        return false;
    }

    // Проверки для пароля начинаются здесь

    // Пароль не должен быть пустым
    if (password.isEmpty()) {
        errorMsg = "Пароль не может быть пустым";
        return false;
    }

    // Минимальная длина пароля
    if (password.length() < 8) {
        errorMsg = "Пароль должен содержать минимум 8 символов";
        return false;
    }

    // Максимальная длина пароля (ограничение для защиты от DoS и чрезмерных ресурсов)
    if (password.length() > 64) {
        errorMsg = "Пароль не должен превышать 64 символа";
        return false;
    }

    // Должна быть хотя бы одна цифра
    if (!password.contains(QRegularExpression("[0-9]"))) {
        errorMsg = "Пароль должен содержать хотя бы одну цифру";
        return false;
    }

    // Должна быть хотя бы одна заглавная латинская буква
    if (!password.contains(QRegularExpression("[A-Z]"))) {
        errorMsg = "Пароль должен содержать хотя бы одну заглавную букву";
        return false;   
    }

    // Должна быть хотя бы одна строчная латинская буква
    if (!password.contains(QRegularExpression("[a-z]"))) {
        errorMsg = "Пароль должен содержать хотя бы одну строчную букву";
        return false;
    }

    // Должен быть хотя бы один спецсимвол из указанного набора
    if (!password.contains(QRegularExpression("[!@#$%^&*(),.?\":{}|<>]"))) {
        errorMsg = "Пароль должен содержать хотя бы один спецсимвол";
        return false;
    }

    // Дополнительная защита: проверка имени пользователя на SQL-ключевые слова
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

    // Если все проверки пройдены — учетные данные валидны
    return true;
}
void DataService::loginProcess(const QString& username, const QString& password){
    // Пытаемся валидировать логин/пароль перед отправкой на сервер
    QString error;
    if (!validateCredentials(username, password, error) ) {
        emit loginFailed(error);  // Сообщаем UI причину ошибки
        return;
    }

    // Формируем JSON-запрос для авторизации пользователя
    QJsonObject request;
    request["type"] = "login";
    request["username"] = username;
    request["password"] = password;

    // Отправляем запрос наружу (например, в NetworkService через сигнал)
    emit sendJson(request);
}

void DataService::registerProcess(const QString& username, const QString& displayName, const QString& password){
    // Валидация логина и пароля перед регистрацией
    QString error;
    if (!validateCredentials(username, password, error)) {
        emit registerFailed(error);  // Сообщаем UI текст ошибки
        return;
    }

    // Формируем JSON-запрос для регистрации нового пользователя
    QJsonObject request;
    request["type"] = "register";
    request["username"] = username;
    request["displayname"] = displayName;
    request["password"] = password;
    emit sendJson(request);
}

void DataService::processResponse(const QJsonObject& response)
{
    // Определяем тип пришедшего сообщения от сервера
    QString type = response["type"].toString();

    // Ищем соответствующий обработчик в карте
    if (m_responseHandlers.contains(type)) {
        ResponseHandler handler = m_responseHandlers[type];    

        // Вызываем метод-обработчик по указателю на член класса
        (this->*handler)(response);
    } else {
        // Если обработчик не найден — логируем предупреждение
        qDebug() << "[DataService] WARNING: No handler found for message type:" << type;
    }
}

void DataService::initResponseHandlers()
{
    // Авторизация/регистрация/выход
    m_responseHandlers["login_success"]      = &DataService::handleLoginSuccess;
    m_responseHandlers["login_failure"]      = &DataService::handleLoginFailure;
    m_responseHandlers["register_success"]   = &DataService::handleRegisterSuccess;
    m_responseHandlers["register_failure"]   = &DataService::handleRegisterFailure;
    m_responseHandlers["logout_success"]     = &DataService::handleLogoutSuccess;
    m_responseHandlers["logout_failure"]     = &DataService::handleLogoutFailure;

    // Контакты и поиск пользователей
    m_responseHandlers["contact_list"]           = &DataService::handleContactList;
    m_responseHandlers["user_list"]              = &DataService::handleUserList;
    m_responseHandlers["search_results"]         = &DataService::handleSearchResults;
    m_responseHandlers["add_contact_success"]    = &DataService::handleAddContactSuccess;
    m_responseHandlers["add_contact_failure"]    = &DataService::handleAddContactFailure;
    m_responseHandlers["incoming_contact_request"] = &DataService::handleIncomingContactRequest;
    m_responseHandlers["pending_requests_list"]  = &DataService::handlePendingRequestsList;

    // История сообщений и события чата
    m_responseHandlers["history_data"]       = &DataService::handleHistoryData;
    m_responseHandlers["old_history_data"]   = &DataService::handleOldHistoryData;
    m_responseHandlers["private_message"]    = &DataService::handlePrivateMessage;
    m_responseHandlers["edit_message"]       = &DataService::handleEditMessage;
    m_responseHandlers["delete_message"]     = &DataService::handleDeleteMessage;

    // Статусы доставки/прочтения и "печатает..."
    m_responseHandlers["message_delivered"]  = &DataService::handleMessageDelivered;
    m_responseHandlers["message_read"]       = &DataService::handleMessageRead;
    m_responseHandlers["typing"]             = &DataService::handleTypingResponse;
    m_responseHandlers["unread_counts"]      = &DataService::handleUnreadCounts;

    // Звонки и статистика звонков
    m_responseHandlers["call_request_sent"]  = &DataService::handleCallRequestSent;
    m_responseHandlers["call_request"]       = &DataService::handleIncomingCall;
    m_responseHandlers["call_accepted"]      = &DataService::handleCallAccepted;
    m_responseHandlers["call_rejected"]      = &DataService::handleCallRejected;
    m_responseHandlers["call_end"]           = &DataService::handleCallEnd;
    m_responseHandlers["call_history"]       = &DataService::handleCallHistory;
    m_responseHandlers["call_stats"]         = &DataService::handleCallStats;

    // Профиль и токен-логин
    m_responseHandlers["update_profile_result"] = &DataService::handleUpdateProfileResult;
    m_responseHandlers["token_login_failure"]   = &DataService::handleTokenLoginFailure;

    // Рукопожатие/инициализация соединения
    m_responseHandlers["handshake"]         = &DataService::handleHandshake;

    qDebug() << "[DataService] Response handlers initialized:" << m_responseHandlers.size();
}

AvatarCache* DataService::getAvatarCache(){
    // Геттер для доступа к кэшу аватаров из других компонентов
    return m_avatarCache;
}

const Chat& DataService::getChatMetadata(const QString& username) const
{
    // Статический пустой объект, возвращается если чат не найден
    static const Chat emptyChat;

    // Пытаемся найти метаданные чата по ключу username
    auto it = m_chatMetadataCache.constFind(username);
    if (it == m_chatMetadataCache.constEnd()) {
        return emptyChat;   // Чат еще не известен
    }

    return it.value();      // Возвращаем найденные метаданные
}

void DataService::updateOrAddChatMetadata(const Chat& chat)
{
    // Если чат уже есть в кэше — просто обновляем запись
    if (m_chatMetadataCache.contains(chat.username)) {
        m_chatMetadataCache[chat.username] = chat;
    } else {
        // Если чата нет, создаем новую запись на основе переданных данных
        Chat newChat = chat;

        // Если displayName пустой, но есть данные в кэше пользователей — подставляем оттуда
        if (newChat.displayName.isEmpty() && m_userCache.contains(chat.username)) {
            newChat.displayName = m_userCache[chat.username].displayName;
        }

        m_chatMetadataCache[chat.username] = newChat;
    }

    // Синхронизируем метаданные чата с базой, если она доступна
    if (m_dbService && m_dbService->isConnected()) {
        m_dbService->addOrUpdateChat(m_chatMetadataCache[chat.username]);
    }

    // Уведомляем UI о том, что метаданные чата изменились (для обновления списка чатов)
    emit chatMetadataChanged(chat.username);
}

void DataService::updateLastMessage(const QString& username, const ChatMessage& message)
{
    // Получаем текущие метаданные чата для пользователя
    const Chat& existingChat = getChatMetadata(username);

    // Создаем изменяемую копию
    Chat updatedChat = existingChat;

    // Гарантируем, что username в метаданных заполнен
    updatedChat.username = username;
    // Если текст сообщения пуст (вероятно, файл), показываем имя файла как превью
    updatedChat.lastMessagePayload = (message.payload == "" ? message.fileName : message.payload);
    updatedChat.lastMessageTimestamp = message.timestamp;
    updatedChat.lastMessageId = message.id;
    updatedChat.isLastMessageOutgoing = message.isOutgoing;

    // Обновляем/добавляем метаданные и синхронизируем с БД/UI
    updateOrAddChatMetadata(updatedChat);
}

void DataService::incrementUnreadCount(const QString& username)
{
    // Получаем текущие метаданные чата
    const Chat& existingChat = getChatMetadata(username);

    // Делаем копию, чтобы изменить счетчик
    Chat updatedChat = existingChat;

    // Увеличиваем число непрочитанных сообщений для этого чата
    updatedChat.unreadCount++;

    // Сохраняем изменения в кэше и базе, уведомляем UI
    updateOrAddChatMetadata(updatedChat);
}

void DataService::resetUnreadCount(const QString& username)
{
    // Получаем текущие метаданные чата
    const Chat& existingChat = getChatMetadata(username);

    Chat updatedChat = existingChat;

    // Сбрасываем счетчик непрочитанных, только если он был > 0
    if (existingChat.unreadCount > 0) {
        updatedChat.unreadCount = 0;
        updateOrAddChatMetadata(updatedChat);
    }
}

QStringList DataService::getSortedChatList() const
{
    // Берем все метаданные чатов из кэша
    QList<Chat> chats = m_chatMetadataCache.values();

    // Фильтруем архивированные чаты (они не попадают в основной список)
    chats.erase(
        std::remove_if(chats.begin(), chats.end(),
                       [](const Chat& c) { return c.isArchived; }),
        chats.end()
    );

    // Сортировка по оператору <, определенному в Chat
    // Обычно учитывает isPinned, время последнего сообщения и т.д.
    std::sort(chats.begin(), chats.end());

    // Формируем список имен пользователей в отсортированном порядке
    QStringList result;
    for (const Chat& chat : chats) {
        result.append(chat.username);
    }

    return result;
}

void DataService::setPinned(const QString& username, bool pinned)
{
    // Получаем текущие метаданные чата
    const Chat& existingChat = getChatMetadata(username);

    Chat updatedChat = existingChat;

    // Обновляем флаг закрепления
    updatedChat.isPinned = pinned;

    // Сохраняем и синхронизируем изменения
    updateOrAddChatMetadata(updatedChat);
}

void DataService::setArchived(const QString& username, bool archived)
{
    // Получаем текущие метаданные чата
    const Chat& existingChat = getChatMetadata(username);

    Chat updatedChat = existingChat;

    // Обновляем флаг архивирования
    updatedChat.isArchived = archived;

    // Сохраняем и синхронизируем изменения
    updateOrAddChatMetadata(updatedChat);
}

void DataService::saveDraft(const QString& username, const QString& draftText)
{
    // Получаем текущие метаданные чата
    const Chat& existingChat = getChatMetadata(username);

    Chat updatedChat = existingChat;

    // Сохраняем текст черновика в метаданные
    updatedChat.draftText = draftText;

    // Обновляем кэш и базу
    updateOrAddChatMetadata(updatedChat);
}

QString DataService::getDraft(const QString& username) const
{
    // Ищем чат в кэше
    auto it = m_chatMetadataCache.constFind(username);
    if (it == m_chatMetadataCache.constEnd()) {
        // Если чата нет — черновика тоже нет
        return QString();
    }
    // Возвращаем сохраненный черновик
    return it.value().draftText;
}

void DataService::loadDraftsFromDatabase()
{
    // Проверяем доступность БД
    if (!m_dbService || !m_dbService->isConnected()) {
        return;
    }

    // Для каждого чата пытаемся загрузить черновик из БД
    for (const QString& username : m_chatMetadataCache.keys()) {
        QString draft = m_dbService->loadDraft(username, m_currentUser.username);

        // Если черновик не пустой — сохраняем его в метаданные чата
        if (!draft.isEmpty()) {
            saveDraft(username, draft);
        }
    }

    qDebug() << "[DataService] Drafts loaded from database";
}

void DataService::handleTokenLoginFailure(const QJsonObject& response)
{
    // Получаем причину отказа в логине по токену
    QString reason = response.value("reason").toString();

    qWarning() << "[DataService] Token login failed:" << reason;

    // Очищаем сохраненный токен (чтобы не пытаться использовать недействительный)
    TokenManager::clearToken();

    // Уведомляем UI для показа сообщения и возврата к обычному логину
    emit tokenLoginFailed(reason);
}

void DataService::handleUpdateProfileResult(const QJsonObject& response)
{
    // Просто прокидываем результат обновления профиля дальше (например, в UI)
    emit profileUpdateResult(response);
}

void DataService::handleCallStats(const QJsonObject& response){
    qDebug() << "[DataService] Received call statistics";

    // Извлекаем агрегированную статистику звонков из ответа
    int outgoing      = response["outgoing"].toInt();
    int incoming      = response["incoming"].toInt();
    int completed     = response["completed"].toInt();
    int missed        = response["missed"].toInt();
    int totalDuration = response["total_duration_sec"].toInt();

    // Логируем статистику для отладки/аналитики
    qDebug() << "[DataService] STATS: Outgoing:" << outgoing
             << "| Incoming:" << incoming
             << "| Completed:" << completed
             << "| Missed:" << missed
             << "| Total duration:" << totalDuration << "s";

    // Передаем статистику дальше (например, в окно статистики звонков)
    emit callStatsReceived(response);
}

void DataService::handleCallHistory(const QJsonObject& response){
    // Извлекаем массив звонков из ответа
    QJsonArray calls = response["calls"].toArray();

    qDebug() << "[DataService] Received call history:" << calls.size() << "calls";

    // Логируем краткую информацию по каждому звонку
    for (const QJsonValue& val : calls) {
        QJsonObject call = val.toObject();
        qDebug() << "[DataService] CALL HISTORY:"
                 << (call["call_type"].toString() == "outgoing" ? "OUT" : "IN")
                 << call["caller"].toString() << "→" << call["callee"].toString()
                 << "| Status:" << call["status"].toString()
                 << "| Duration:" << call["duration_seconds"].toInt() << "s";
    }

    // Передаем историю дальше (например, в UI)
    emit callHistoryReceived(calls);
}

void DataService::requestCallHistory()
{
    // Здесь можно сформировать JSON-запрос и отправить его через sendJson
    qDebug() << "[DataService] Requesting call history from server";
}

void DataService::handleCallRequestSent(const QJsonObject& response)
{
    qDebug() << "[DataService] CALL REQUEST SENT";

    // Извлекаем информацию о том, кому был отправлен звонок
    QString toUser = response.value("to").toString();
    QString callId = response.value("call_id").toString();

    qDebug() << "[DataService] Call sent to:" << toUser << "call_id:" << callId;

    // Уведомляем остальные компоненты (например, CallService/UI)
    emit callRequestSent(toUser, callId);
}

void DataService::handleIncomingCall(const QJsonObject& response)
{
    qDebug() << "[DataService] INCOMING CALL";

    // Извлекаем параметры входящего звонка
    QString fromUser  = response.value("from").toString();
    QString callId    = response.value("call_id").toString();
    QString callerIp  = response.value("caller_ip").toString();
    quint16 callerPort = response.value("caller_port").toInt();

    qDebug() << "[DataService] Incoming call from:" << fromUser
             << "call_id:" << callId
             << "ip:" << callerIp
             << "port:" << callerPort;

    // Проксируем событие в CallService/GUI
    emit incomingCall(fromUser, callId, callerIp, callerPort);
}

void DataService::handleCallAccepted(const QJsonObject& response)
{
    qDebug() << "[DataService] CALL ACCEPTED";

    // Данные о том, кто принял звонок и по какому адресу устанавливать UDP-сессию
    QString fromUser = response.value("from").toString();
    QString calleeIp = response.value("callee_ip").toString();
    quint16 calleePort = response.value("callee_port").toInt();

    qDebug() << "[DataService] Call accepted by:" << fromUser
             << "ip:" << calleeIp
             << "port:" << calleePort;

    // Сообщаем CallService, что звонок принят и можно начинать аудио-сессию
    emit callAccepted(fromUser, calleeIp, calleePort);
}

void DataService::handleCallRejected(const QJsonObject& response)
{
    qDebug() << "[DataService] CALL REJECTED";

    // Кто отклонил звонок и по какой причине
    QString fromUser = response.value("from").toString();
    QString reason   = response.value("reason").toString();

    qDebug() << "[DataService] Call rejected by:" << fromUser
             << "reason:" << reason;

    // Уведомляем UI/CallService о том, что звонок отклонен
    emit callRejected(fromUser, reason);
}

void DataService::handleCallEnd(const QJsonObject& response)
{
    // Отправитель сигнала завершения и ID звонка (для логов/отладки)
    QString fromUser = response["from"].toString();
    QString callId   = response["call_id"].toString();

    qDebug() << "[DataService] CALL END from:" << fromUser << "call_id:" << callId;

    // Сообщаем CallService/GUI, что звонок завершен
    emit callEnded();
}

const User* DataService::getUserFromCache(const QString& username) {
    // Проверяем наличие пользователя в кэше
    if (m_userCache.contains(username)) {
        // Возвращаем указатель на объект в QMap (живет пока жив DataService)
        return &m_userCache[username];
    }
    // Если не найден — возвращаем nullptr
    return nullptr;
}

void DataService::updateOrAddUser(const User& user)
{
    // Обновляем или добавляем пользователя в локальный кэш
    m_userCache[user.username] = user;

    // Синхронизируем с базой данных, если соединение активно
    if (m_dbService && m_dbService->isConnected()) {
        m_dbService->addOrUpdateUser(user);
    }

    // Уведомляем об изменении кэша пользователей (для обновления UI)
    emit userCacheChanged(user.username);
}

const QMap<QString, ChatCache>& DataService::getChatCache() {
    // Возвращаем ссылку на кэш истории чатов
    return m_chatHistoryCache;
}

void DataService::updateOrAddChatCache(const QString& username, const ChatCache& chatCache)
{
    // Обновляем существующий кэш для пользователя или вставляем новый
    if (m_chatHistoryCache.contains(username)) {
        m_chatHistoryCache[username] = chatCache;
    } else {
        m_chatHistoryCache.insert(username, chatCache);
    }

    // Уведомляем, что история чата обновилась
    emit chatCacheUpdated(username);
}

const QMap<QString, int>& DataService::getUnreadCounts() {
    // Возвращаем ссылку на кэш количества непрочитанных сообщений
    return m_unreadCounts;
}

void DataService::updateOrAddUnreadCount(const QString& username, int count) {
    Q_UNUSED(count);
    // Актуализируем значение из БД (источником истины является база)
    m_unreadCounts[username] = m_dbService->getUnreadCountForChat(username, m_currentUser.username);

    // Обновляем поле unreadCount в метаданных чата
    m_chatMetadataCache[username].unreadCount = m_unreadCounts[username];

    // Сохраняем изменения и синхронизируем с БД/UI
    updateOrAddChatMetadata(m_chatMetadataCache[username]);

    // Уведомляем UI о смене количества непрочитанных
    emit unreadCountUpdated(username);
}

ChatCache* DataService::getChatCacheForUser(const QString& username) {
    // Возвращаем указатель на кэш истории чата, если он есть
    if (m_chatHistoryCache.contains(username)) {
        return &m_chatHistoryCache[username];
    }
    return nullptr;
}

const User* DataService::getCurrentChatPartner() {
    // Если выбран собеседник — возвращаем указатель на него
    if (!m_currentChatPartner.username.isEmpty()) {
        return &m_currentChatPartner;
    }
    return nullptr;
}

void DataService::updateOrAddCurrentChatPartner(const User& user) {
    // Обновляем текущего собеседника
    m_currentChatPartner = user;

    // Уведомляем UI о смене активного чата
    emit currentChatPartnerChanged(user.username);
}

QTimer* DataService::getGlobalSearchTimer() {
    // Доступ к debounce-таймеру глобального поиска
    return m_globalSearchTimer;
}

QTimer* DataService::getTypingSendTimer() {
    // Доступ к таймеру отправки статуса "печатает"
    return m_typingSendTimer;
}

const QMap<QString, QTimer*>* DataService::getTypingRecieveTimers() {
    // Возвращаем карту таймеров "печатает" по собеседникам, если она не пуста
    if (!m_typingReceiveTimers.isEmpty()) {
        return &m_typingReceiveTimers;
    }
    return nullptr;
}

QMap<QString, User>* DataService::getUserCache() {
    // Возвращаем указатель на кэш пользователей для внешнего доступа/модификации
    return &m_userCache;
}

qint64 DataService::getReplyToMessageId() const {
    // ID сообщения, на которое пользователь отвечает
    return m_replyToMessageId;
}

void DataService::updateOrAddReplyToMessageId(qint64 replyMessageId){
    // Обновляем целевое сообщение для реплая
    m_replyToMessageId = replyMessageId;
    emit replyToMessageIdChanged(replyMessageId);
}

qint64 DataService::getEditinigMessageId() const {
    // ID сообщения, которое редактируется в данный момент
    return m_editingMessageId;
}

void DataService::updateOrAddEditingMessageId(qint64 messageId){
    // Обновляем ID редактируемого сообщения
    m_editingMessageId = messageId;
    emit editingMessageIdChanged(messageId);
}

const User* DataService::getCurrentUser() {
    // Возвращаем текущего авторизованного пользователя, если он задан
    if (!m_currentUser.username.isEmpty()) {
        return &m_currentUser;
    }
    return nullptr;
}

void DataService::updateOrAddCurrentUser(const User& user)
{
    // Обновляем данные текущего пользователя
    m_currentUser = user;

    // Синхронизируем пользователя с БД
    if (m_dbService && m_dbService->isConnected()) {
        m_dbService->addOrUpdateUser(user);
    }

    // Уведомляем об изменении текущего пользователя
    emit currentUserChanged(user.username);
}

bool DataService::getIsLoadingHistory() const {
    // Флаг, идет ли сейчас подгрузка истории сообщений
    return m_isLoadingHistory;  
}

void DataService::updateOrAddIsLoadingHistory(bool isLoadingHistory) {
    // Обновляем флаг загрузки истории
    m_isLoadingHistory = isLoadingHistory;

    // Уведомляем UI, чтобы, например, показать/скрыть индикатор загрузки
    emit historyLoadingChanged(isLoadingHistory);
}

qint64 DataService::getOldestMessageId() const{
    // ID самого старого загруженного сообщения для текущего чата
    return m_oldestMessageId;
}

void DataService::updateOrAddOldestMessageId(qint64 messageId){
    // Обновляем ID границы истории
    m_oldestMessageId = messageId;
    emit oldestMessageIdChanged(messageId);
}
void DataService::handleContactList(const QJsonObject& response)
{
    qDebug() << "[DataService] Processing contact list.";

    // Массив пользователей, полученный с сервера
    QJsonArray contactsArray = response.value("users").toArray();
    QStringList usernames;

    // Обновляем кэш пользователей и метаданные чатов
    for (const QJsonValue& val : contactsArray) {
        QJsonObject contactObj = val.toObject();

        QString username      = contactObj.value("username").toString();
        QString displayName   = contactObj.value("displayname").toString();
        QString lastSeen      = contactObj.value("last_seen").toString();
        QString statusMessage = contactObj.value("statusmessage").toString();
        QString avatarUrl     = contactObj.value("avatar_url").toString();

        qDebug() << "[DataService] Loaded contact:" << username << ":" << displayName;

        // Заполняем структуру User и добавляем в кэш/БД
        User user;
        user.username      = username;
        user.displayName   = displayName.isEmpty() ? username : displayName;
        user.statusMessage = statusMessage;
        user.avatarUrl     = avatarUrl;
        user.lastSeen      = lastSeen;
        user.isOnline      = false;   // онлайн-статус позже обновляется отдельно
        updateOrAddUser(user);
        usernames.append(username);

        // Инициализация/обновление метаданных чата для данного пользователя
        Chat chat = Chat(username);
        if (m_chatMetadataCache.contains(username)) {
            chat = m_chatMetadataCache[username];
            chat.displayName = user.displayName;   // Синхронизация имени в чате с контактами
        }
        updateOrAddChatMetadata(chat);
    }

    // Дополнительное обогащение: подтягиваем данные по чатам и непрочитанным из БД
    if (m_dbService && m_dbService->isConnected()) {
        for (const QString& username : usernames) {
            Chat dbChat = m_dbService->getChatByUsername(username);
            if (dbChat.id != 0) {
                dbChat.unreadCount = m_dbService->getUnreadCountForChat(username, m_currentUser.username);
                updateOrAddChatMetadata(dbChat);
            }
        }
    }

    // Уведомляем UI, что список контактов обновился
    emit contactsUpdated(usernames);
}

void DataService::loadChatMetadataFromDatabase()
{
    if (!m_dbService || !m_dbService->isConnected()) {
        qDebug() << "[DataService] Cannot load chat metadata: DB not connected";
        return;
    }

    qDebug() << "[DataService] Loading chat metadata from database...";

    // Проходим по всем чатам в кэше и дополняем их данными из БД
    for (auto it = m_chatMetadataCache.begin(); it != m_chatMetadataCache.end(); ++it) {
        QString username = it.key();
        Chat& chat = it.value();

        // Загружаем последнее сообщение для данного собеседника
        ChatMessage lastMessage = m_dbService->loadLastMessage(username, m_currentUser.username);

        if (lastMessage.id > 0) {
            chat.lastMessagePayload   = (lastMessage.payload == "" ? lastMessage.fileName : lastMessage.payload);
            chat.lastMessageTimestamp = lastMessage.timestamp;
            chat.lastMessageId        = lastMessage.id;
            chat.isLastMessageOutgoing = lastMessage.isOutgoing;

            qDebug() << "[DataService] Loaded last message for" << username
                     << ":" << lastMessage.payload.left(20);
        }

        // Обновляем количество непрочитанных сообщений для чата
        int unreadCount = m_dbService->getUnreadCountForChat(username, m_currentUser.username);
        chat.unreadCount = unreadCount;

        if (unreadCount > 0) {
            qDebug() << "[DataService] Chat" << username << "has" << unreadCount << "unread";
        }
    }

    qDebug() << "[DataService] Chat metadata loaded";
}

void DataService::handleHandshake(const QJsonObject& request){
    // Если уже работаем в зашифрованном режиме — повторный handshake не нужен
    if (m_crypto->isEncrypted()) {
        return;
    }

    // Получаем публичный ключ сервера в base64 и декодируем в бинарный вид
    QString serverKeyBase64 = request["key"].toString();
    QByteArray serverKey = QByteArray::fromBase64(serverKeyBase64.toLatin1());

    // Вычисляем общий секрет (ECDH/X25519) и переключаемся в зашифрованный режим
    m_crypto->computeSharedSecret(serverKey);
    qInfo() << "Handshake complete. Switching to encrypted mode.";

    // Уведомляем остальные компоненты, что шифрование включено
    emit encryptionEnabled();
}

void DataService::handleUserList(const QJsonObject& response) {
    // Массив имен пользователей, которые сейчас онлайн
    QJsonArray onlineUsernamesArray = response["users"].toArray();
    QSet<QString> onlineUsers;

    // Строим множество онлайн-пользователей
    for (const QJsonValue &value : onlineUsernamesArray) {
        onlineUsers.insert(value.toString());
    }

    // Обновляем флаг isOnline для каждого пользователя в кэше
    for (auto it = m_userCache.begin(); it != m_userCache.end(); ++it) {
        it.value().isOnline = onlineUsers.contains(it.key());
    }

    // Сообщаем UI, что статусы онлайн/офлайн обновились
    emit onlineStatusUpdated();
}
void DataService::handleOldHistoryData(const QJsonObject& response)
{
    // Пользователь, для которого пришел блок старых сообщений
    QString historyForUser = response["with_user"].toString();
    QJsonArray history = response["history"].toArray();
    qDebug() << "[DataService] Received" << history.count() << "older messages for" << historyForUser;

    // Берем ссылку на кэш истории для этого пользователя
    ChatCache& cache = m_chatHistoryCache[historyForUser];

    // Если с сервера больше нечего подгружать — отмечаем, что вся история загружена
    if (history.isEmpty()) {
        cache.allMessagesLoaded = true;
        if (historyForUser == m_currentChatPartner.username) {
            m_oldestMessageId = 0;
            m_chatHistoryCache[historyForUser].allMessagesLoaded = true;
            // Сообщаем UI, что старых сообщений больше нет (пустой список)
            emit olderHistoryChunkPrepended(historyForUser, QList<ChatMessage>());
        }
        return;
    }

    // Конвертируем JSON-объекты в ChatMessage
    QList<ChatMessage> messages;
    for (int i = 0; i < history.count(); ++i) {
        const QJsonValue &value = history[i];
        ChatMessage msg;
        QJsonObject msgObj = value.toObject();

        msg.id        = msgObj["id"].toDouble();
        msg.fromUser  = msgObj["fromUser"].toString();
        msg.toUser    = msgObj["toUser"].toString();
        msg.payload   = msgObj["payload"].toString();
        msg.timestamp = msgObj["timestamp"].toString();
        msg.replyToId = msgObj["reply_to_id"].toDouble();
        msg.isEdited  = msgObj["is_edited"].toInt();
        msg.isOutgoing = (msg.fromUser == m_currentUser.username);
        msg.fileId    = msgObj["file_id"].toString();
        msg.fileName  = msgObj["file_name"].toString();
        msg.fileUrl   = msgObj["file_url"].toString();

        // Восстанавливаем статус по флагам is_delivered / is_read
        if (msgObj["is_delivered"].toInt() == 1) {
            msg.status = ChatMessage::Delivered;
        } else {
            msg.status = ChatMessage::Sent;
        }
        if (msgObj["is_read"].toInt() == 1) {
            msg.status = ChatMessage::Read;
        }

        messages.append(msg);
    }

    // Сохраняем сообщения в БД с upsert-логикой, чтобы не плодить дубликатов
    insertMessagesWithUpsertFiltered(messages, historyForUser);

    // Добавляем старые сообщения в начало локального кэша и обновляем статус в БД
    for (int i = messages.count() - 1; i >= 0; --i) {
        cache.messages.prepend(messages.at(i));
        m_dbService->updateMessageStatus(messages.at(i).id, messages.at(i).status);
    }

    // Обновляем ID самого старого сообщения для этого чата
    cache.oldestMessageId = messages.first().id;

    // Если это текущий открытый чат — уведомляем UI о подгрузке истории
    if (historyForUser == m_currentChatPartner.username) {
        qDebug() << "[DataService] Older history is for the current chat. Emitting signal.";
        m_oldestMessageId = cache.oldestMessageId;
        emit olderHistoryChunkPrepended(historyForUser, messages);
    } else {
        qDebug() << "[DataService] Older history is for a background chat. Caching silently.";
    }
}

void DataService::handleHistoryData(const QJsonObject& response) {
    // Пользователь, для которого пришла начальная/основная история
    QString historyForUser = response["with_user"].toString();
    QJsonArray history = response["history"].toArray();

    qDebug() << "[DataService] Received" << history.count() << "messages for" << historyForUser;
    
    QList<ChatMessage> messages;
    for (const QJsonValue &value : history) {
        QJsonObject msgObj = value.toObject();
        ChatMessage msg;

        msg.id        = msgObj["id"].toDouble();
        msg.fromUser  = msgObj["fromUser"].toString();
        msg.toUser    = msgObj["toUser"].toString();
        msg.payload   = msgObj["payload"].toString();
        msg.timestamp = msgObj["timestamp"].toString();
        msg.replyToId = msgObj["reply_to_id"].toDouble();
        msg.isEdited  = msgObj["is_edited"].toInt();
        msg.isOutgoing = (msg.fromUser == m_currentUser.username);
        msg.fileId    = msgObj["file_id"].toString();
        msg.fileName  = msgObj["file_name"].toString();
        msg.fileUrl   = msgObj["file_url"].toString();

        // Определяем статус в порядке приоритета: Read > Delivered > Sent
        if (msgObj["is_read"].toInt() == 1) {
            msg.status = ChatMessage::Read;
        } else if (msgObj["is_delivered"].toInt() == 1) {
            msg.status = ChatMessage::Delivered;
        } else {
            msg.status = ChatMessage::Sent;
        }

        messages.append(msg);
    }

    if (!messages.isEmpty()) {
        // Сначала синхронизируем историю с БД (upsert)
        insertMessagesWithUpsertFiltered(messages, historyForUser);

        // Обновляем или дополняем кэш истории для данного чата
        ChatCache& cache = m_chatHistoryCache[historyForUser];
        for (const ChatMessage& msg : messages) {
            bool exists = false;
            for (ChatMessage& cached : cache.messages) {
                if (cached.id == msg.id) {
                    cached = msg;  // Обновляем существующее сообщение
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                cache.messages.append(msg);  // Добавляем новое сообщение в конец
            }
        }

        // Обновляем метаданные чата (последнее сообщение, время, направление, ID)
        const Chat& existingChat = getChatMetadata(historyForUser);
        Chat updatedChat = existingChat;

        if (!messages.isEmpty()) {
            const ChatMessage& last = messages.last();
            updatedChat.lastMessagePayload   = (last.payload == "" ? last.fileName : last.payload);
            updatedChat.lastMessageTimestamp = last.timestamp;
            updatedChat.isLastMessageOutgoing = last.isOutgoing;
            updatedChat.lastMessageId        = last.id;
        }

        // Пересчитываем количество непрочитанных по данным БД
        updatedChat.unreadCount = m_dbService->getUnreadCountForChat(historyForUser, m_currentUser.username);

        // Сохраняем обновленные метаданные
        updateOrAddChatMetadata(updatedChat);

        // Если история для текущего активного чата — обновляем границу и снимаем флаг загрузки
        if (historyForUser == m_currentChatPartner.username) {
            m_oldestMessageId = cache.messages.isEmpty() ? 0 : cache.messages.first().id;
            m_isLoadingHistory = false;
            qDebug() << "[DataService] Emitting historyLoaded for current chat";
        } else {
            qDebug() << "[DataService] History cached silently for background chat";
        }

        // Уведомляем UI, что история для данного пользователя загружена/обновлена
        emit historyLoaded(historyForUser, messages);
    }
}

void DataService::handleUnreadCounts(const QJsonObject& response)
{
    // Получаем стартовые счетчики непрочитанных с сервера
    qDebug() << "[DataService] Received initial unread counts from server.";
    QJsonArray countsArray = response["counts"].toArray();

    // Сбрасываем локальный кэш непрочитанных
    m_unreadCounts.clear();

    // Заполняем кэш только теми чатами, где есть непрочитанные сообщения
    for (const QJsonValue &value : countsArray) {
        QJsonObject countObj = value.toObject();
        QString username = countObj["username"].toString();
        int count = countObj["count"].toInt();

        if (count > 0) {
            m_unreadCounts[username] = count;
        }
    }

    // Уведомляем UI, что глобальные счетчики непрочитанных изменились
    emit unreadCountChanged();
}

void DataService::updateMessageStatus(qint64 messageId, ChatMessage::MessageStatus newStatus)
{
    // Ищем сообщение в кэше истории по всем чатам
    QString chatPartner;
    for (auto& chatCache : m_chatHistoryCache) {
        for (ChatMessage& msg : chatCache.messages) {
            if (msg.id == messageId) {
                msg.status = newStatus;
                // Определяем собеседника (не текущего пользователя)
                chatPartner = (msg.fromUser == m_currentUser.username) ? msg.toUser : msg.fromUser;
                break;
            }
        }
        if (!chatPartner.isEmpty()) break;
    }

    // Обновляем статус в базе данных
    if (m_dbService && m_dbService->isConnected()) {
        m_dbService->updateMessageStatus(messageId, newStatus);
    }

    // Пересчитываем непрочитанные и метаданные чата, если удалось найти чат
    if (!chatPartner.isEmpty()) {
        m_chatMetadataCache[chatPartner].unreadCount =
            m_dbService->getUnreadCountForChat(chatPartner, m_currentUser.username);
        emit unreadCountChanged();
        emit chatMetadataChanged(chatPartner);
    }

    // Локальный сигнал о смене статуса конкретного сообщения
    emit messageStatusChanged(messageId, newStatus);
}

void DataService::handleLoginSuccess(const QJsonObject& response)
{
    // Проксируем успешный логин дальше (например, для сохранения токена и переключения экрана)
    emit loginSuccess(response);
}

void DataService::handleLoginFailure(const QJsonObject& response)
{
    // Передаем текст причины неуспешного логина в UI
    emit loginFailure(response["reason"].toString());
}

void DataService::initLoad() {
    qDebug() << "[DataService] initialLoad: стартуем загрузку всех данных из БД...";

    // 1. Загружаем всех пользователей и прогреваем кэш аватаров
    QList<User> users = m_dbService->loadAllUsers();
    for (const User& user : users) {
        m_userCache[user.username] = user;
        m_avatarCache->ensureAvatar(user.username, user.avatarUrl);
    }

    qDebug() << "[DataService] Загружено пользователей:" << users.size();

    // 2. Загружаем все чаты и кладем их в метаданные
    QList<Chat> chats = m_dbService->loadAllChats();
    for (const Chat& chat : chats) {
        m_chatMetadataCache[chat.username] = chat;
    }

    // Обогащаем метаданные последними сообщениями и непрочитанными
    loadChatMetadataFromDatabase();
    qDebug() << "[DataService] Загружено чатов:" << chats.size();

    // 3. Для каждого чата подгружаем последние N сообщений в кэш
    const int RECENT_LIMIT = 20;
    for (const Chat& chat : chats) {
        QList<ChatMessage> history = m_dbService->loadRecentMessages(
            m_currentUser.username, chat.username, RECENT_LIMIT
        );
        m_chatHistoryCache[chat.username].messages = history;

        // Обновляем превью последнего сообщения в метаданных
        if (!history.isEmpty()) {
            const ChatMessage& last = history.last();
            m_chatMetadataCache[chat.username].lastMessagePayload =
                (last.payload == "" ? last.fileName : last.payload);
            m_chatMetadataCache[chat.username].lastMessageTimestamp = last.timestamp;
            m_chatMetadataCache[chat.username].isLastMessageOutgoing = last.isOutgoing;
            m_chatMetadataCache[chat.username].lastMessageId = last.id;
        } else {
            m_chatMetadataCache[chat.username].lastMessagePayload = "";
            m_chatMetadataCache[chat.username].lastMessageTimestamp = "";
            m_chatMetadataCache[chat.username].lastMessageId = 0;
        }
        emit chatMetadataChanged(chat.username);
    }

    qDebug() << "[DataService] initialLoad завершён! Кэш готов.";
}

void DataService::cacheUploadedFile(QString fileId, const QString& fileName, const QString& fileUrl)
{
    // Кэшируем информацию о загруженном файле по его ID
    CachedFile file;
    file.fileId = fileId;
    file.fileName = fileName;
    file.fileUrl = fileUrl;
    m_uploadedFilesCache[fileId] = file;
}

QMap<QString, CachedFile>* DataService::getCachedFiles()
{
    // Возвращаем указатель на кэш файлов
    return &m_uploadedFilesCache;
}

void DataService::removeCachedFile(QString fileId)
{
    // Удаляем отдельный файл из кэша по ID
    m_uploadedFilesCache.remove(fileId);
}

void DataService::clearUploadedFilesCache()
{
    // Полностью очищаем кэш загруженных файлов
    m_uploadedFilesCache.clear();
}

void DataService::handleRegisterSuccess(const QJsonObject& response)
{
    Q_UNUSED(response);
    // Простое уведомление об успешной регистрации
    emit registerSuccess();
}

void DataService::handleRegisterFailure(const QJsonObject& response)
{
    // Передаем причину неуспешной регистрации в UI
    emit registerFailure(response["reason"].toString());
}

void DataService::handlePrivateMessage(const QJsonObject& response) {

    // 1. Проверяем, является ли это эхо по temp_id (подтверждение отправки)
    QString tempId = response["temp_id"].toString();
    if (!tempId.isEmpty()) {
        qDebug() << "[DataService] Received ECHO for temp_id:" << tempId;

        // Собираем подтвержденное сообщение
        ChatMessage msg;
        msg.id        = response["id"].toDouble();
        msg.tempId    = tempId;
        msg.fromUser  = response["fromUser"].toString();
        msg.toUser    = response["toUser"].toString();
        msg.payload   = response["payload"].toString();
        msg.timestamp = response["timestamp"].toString();
        msg.replyToId = response["reply_to_id"].toDouble();
        msg.isOutgoing = true;
        msg.status    = ChatMessage::MessageStatus::Sent;
        msg.isEdited  = false;
        msg.fileId    = response["file_id"].toString();
        msg.fileName  = response["file_name"].toString();
        msg.fileUrl   = response["file_url"].toString();

        // Обновляем локальную запись в БД по временном ID
        if (m_dbService && m_dbService->isConnected()) {
            m_dbService->confirmSentMessageByTempId(tempId, msg);
            qDebug() << "[DataService] Echo: updated local record by tempId";
        }

        // Обновляем метаданные чата (последнее сообщение и превью)
        qDebug() << "[DataService] Emit confirmMessageSent for tempId";
        updateLastMessage(msg.toUser, msg);

        // Уведомляем UI/другие компоненты, что сообщение подтверждено сервером
        emit confirmMessageSent(tempId, msg);

        // Обновляем сообщение в кэше истории по temp_id
        QString chatPartner = msg.toUser;
        if (m_chatHistoryCache.contains(chatPartner)) {
            QList<ChatMessage>& messagesInCache = m_chatHistoryCache[chatPartner].messages;
            for (int i = 0; i < messagesInCache.size(); ++i) {
                if (messagesInCache[i].tempId == tempId) {
                    messagesInCache[i] = msg;
                    qDebug() << "[DataService] CACHE: Confirmed message with temp_id:" << tempId
                             << "in cache for" << chatPartner;
                    break;
                }
            }
        }
        return;
    }

    // 2. Обычное входящее сообщение (без temp_id)
    ChatMessage incomingMsg;
    incomingMsg.id        = response["id"].toDouble();
    incomingMsg.fromUser  = response["fromUser"].toString();
    incomingMsg.toUser    = response["toUser"].toString();
    incomingMsg.payload   = response["payload"].toString();
    incomingMsg.timestamp = response["timestamp"].toString();
    incomingMsg.replyToId = response["reply_to_id"].toDouble();
    incomingMsg.isOutgoing = false;
    incomingMsg.isEdited  = false;
    incomingMsg.fileId    = response["file_id"].toString();
    incomingMsg.fileName  = response["file_name"].toString();
    incomingMsg.fileUrl   = response["file_url"].toString();

    // Восстанавливаем статус по флагам is_read / is_delivered
    if (response["is_read"].toInt() == 1) {
        incomingMsg.status = ChatMessage::Read;
    } else if (response["is_delivered"].toInt() == 1) {
        incomingMsg.status = ChatMessage::Delivered;
    } else {
        incomingMsg.status = ChatMessage::Sent;
    }

    // Сохраняем входящее сообщение в БД
    m_dbService->saveMessage(incomingMsg, m_currentUser.username);

    // Добавляем в кэш истории соответствующего чата
    QString chatPartner = incomingMsg.fromUser;
    if (m_chatHistoryCache.contains(chatPartner)) {
        m_chatHistoryCache[chatPartner].messages.append(incomingMsg);
        m_chatHistoryCache[chatPartner].oldestMessageId = incomingMsg.id;
    }

    // Обновляем метаданные чата (последнее сообщение)
    updateLastMessage(incomingMsg.fromUser, incomingMsg);

    // Обновляем unreadCount через БД
    const Chat& existingChat = getChatMetadata(incomingMsg.fromUser);
    Chat updatedChat = existingChat;
    updatedChat.unreadCount = m_dbService->getUnreadCountForChat(incomingMsg.fromUser, m_currentUser.username);
    updateOrAddChatMetadata(updatedChat);

    qDebug() << "[DataService] Emit newMessageReceived for incomingMsg";

    // Уведомляем UI/слои выше о новом входящем сообщении
    emit newMessageReceived(incomingMsg);
}

DatabaseService* DataService::getDatabaseService() {
    // Геттер для доступа к DatabaseService
    return m_dbService;
}

const CryptoManager* DataService::getCurrentUserCrypto(){
    // Текущее крипто-состояние (ключи, шифрование) для активного пользователя
    return m_crypto;
}

void DataService::updateOrAddCurrentUserCrypto(CryptoManager* cryptoManager){
    // Обновление/установка менеджера криптографии
    m_crypto = cryptoManager;
    emit cryptoManagerChanged(m_crypto);
}

void DataService::handleMessageDelivered(const QJsonObject& response)
{
    qint64 messageId = response["id"].toDouble();
    qDebug() << "[DataService] Message" << messageId << "was delivered.";

    // Обновляем статус в БД
    if (m_dbService && m_dbService->isConnected()) {
        m_dbService->updateMessageStatus(messageId, ChatMessage::Delivered);
        qDebug() << "[DataService] Status updated in DB for message" << messageId << "-> Delivered";
    }

    // Обновляем статус в кэше истории
    ChatMessage::MessageStatus newStatus = ChatMessage::Delivered;
    for (auto it = m_chatHistoryCache.begin(); it != m_chatHistoryCache.end(); ++it) {
        QList<ChatMessage>& messages = it.value().messages;
        for (int i = 0; i < messages.size(); ++i) {
            if (messages[i].id == messageId) {
                if (messages[i].status == newStatus) return;  // Уже обновлено

                messages[i].status = newStatus;
                QString foundInChatWith = it.key();
                qDebug() << "[DataService] Status updated for message" << messageId
                         << "in chat with" << foundInChatWith;

                // В текущем открытом чате дергаем сигнал для обновления UI
                if (foundInChatWith == m_currentChatPartner.username) {
                    emit messageStatusChanged(messageId, newStatus);
                }
                return;
            }
        }
    }
}

void DataService::handleMessageRead(const QJsonObject& response)
{
    qint64 messageId = response["id"].toDouble();
    qDebug() << "[DataService] Message" << messageId << "was read.";

    // Обновляем статус в БД
    if (m_dbService && m_dbService->isConnected()) {
        m_dbService->updateMessageStatus(messageId, ChatMessage::Read);
        qDebug() << "[DataService] Status updated in DB for message" << messageId << "-> Read";
    }

    // Обновляем статус в кэше истории
    ChatMessage::MessageStatus newStatus = ChatMessage::Read;
    for (auto it = m_chatHistoryCache.begin(); it != m_chatHistoryCache.end(); ++it) {
        QList<ChatMessage>& messages = it.value().messages;
        for (int i = 0; i < messages.size(); ++i) {
            if (messages[i].id == messageId) {
                if (messages[i].status == newStatus) return;  // Уже Read

                messages[i].status = newStatus;
                QString foundInChatWith = it.key();
                qDebug() << "[DataService] Status updated for message" << messageId
                         << "in chat with" << foundInChatWith;

                if (foundInChatWith == m_currentChatPartner.username) {
                    emit messageStatusChanged(messageId, newStatus);
                }
                return;
            }
        }
    }
}

void DataService::handleEditMessage(const QJsonObject& response)
{
    QString chatPartner = response["with_user"].toString();
    qint64 messageId = response["id"].toDouble();
    QString newPayload = response["payload"].toString();

    qDebug() << "[DataService] Received command to edit message" << messageId;

    // Обновляем текст сообщения в БД
    if (m_dbService && m_dbService->isConnected()) {
        m_dbService->editMessage(messageId, newPayload);
        qDebug() << "[DataService] Message" << messageId << "edited in DB";
    }

    // Обновляем сообщение в кэше чата
    if (m_chatHistoryCache.contains(chatPartner)) {
        QList<ChatMessage>& messagesInCache = m_chatHistoryCache[chatPartner].messages;
        for (int i = 0; i < messagesInCache.size(); ++i) {
            if (messagesInCache[i].id == messageId) {
                messagesInCache[i].payload = newPayload;
                messagesInCache[i].isEdited = true;
                qDebug() << "[DataService] Message" << messageId
                         << "edited in cache for" << chatPartner;
                break;
            }
        }
    }

    // Уведомляем UI о редактировании сообщения
    emit messageEdited(chatPartner, messageId, newPayload);
}

void DataService::handleDeleteMessage(const QJsonObject& response){
    qint64 messageId = response["id"].toDouble();
    QString chatUser = response["with_user"].toString();

    // Удаляем сообщение из БД
    if (m_dbService && m_dbService->isConnected()) {
        m_dbService->deleteMessage(messageId);
        qDebug() << "[DataService] Message" << messageId << "deleted from DB";
    }

    // Определяем реального собеседника (с учетом возможного совпадения с currentUser)
    QString chatPartner = (chatUser == m_currentUser.username) ? m_currentChatPartner.username : chatUser;
    qDebug() << "[DataService] Received command to delete message" << messageId
             << "in chat with user" << chatPartner;

    // Удаляем сообщение из кэша истории
    if (m_chatHistoryCache.contains(chatPartner)) {
        QList<ChatMessage>& messagesInCache = m_chatHistoryCache[chatPartner].messages;
        for (int i = 0; i < messagesInCache.size(); ++i) {
            if (messagesInCache[i].id == messageId) {
                messagesInCache.removeAt(i);
                qDebug() << "[DataService] CACHE: Message" << messageId
                         << "deleted from cache for" << chatPartner;
                break;
            }
        }
    }

    // Сигнал для обновления UI
    emit messageDeleted(chatPartner, messageId);
}

void DataService::handleSearchResults(const QJsonObject& response)
{
    // Просто прокидываем массив найденных пользователей наверх
    QJsonArray users = response["users"].toArray();
    emit searchResultsReceived(users);
}

void DataService::handleAddContactSuccess(const QJsonObject& response)
{
    // Контакт успешно добавлен — передаем username
    emit addContactSuccess(response["username"].toString());
}

void DataService::handleAddContactFailure(const QJsonObject& response)
{
    // Передаем причину ошибки добавления контакта
    emit addContactFailure(response["reason"].toString());
}

void DataService::handleIncomingContactRequest(const QJsonObject& response)
{
    // Весь JSON запроса прокидываем дальше (для отображения диалога)
    emit contactRequestReceived(response);
}

void DataService::handlePendingRequestsList(const QJsonObject& response)
{
    // Список ожидающих заявок в друзья
    QJsonArray requests = response["requests"].toArray();
    emit pendingContactRequestsUpdated(requests);
}

void DataService::handleLogoutSuccess(const QJsonObject& response)
{
    Q_UNUSED(response);
    // Успешный выход — уведомляем UI (очистка состояния и переход на экран логина)
    emit logoutSuccess();
}

void DataService::handleLogoutFailure(const QJsonObject& response)
{
    // Неуспешный выход — сообщаем текст ошибки
    emit logoutFailure(response["reason"].toString());
}
void DataService::handleTypingResponse(const QJsonObject& response)
{
    QString fromUser = response["fromUser"].toString();

    // Игнорируем, если такого пользователя нет в кэше
    if (!m_userCache.contains(fromUser)) return;

    // Помечаем пользователя как "печатает"
    m_userCache[fromUser].isTyping = true;
    emit typingStatusChanged(fromUser, true);

    // Создаем таймер авто-сброса статуса, если его ещё нет
    if (!m_typingReceiveTimers.contains(fromUser)) {
        QTimer* timer = new QTimer(this);
        timer->setInterval(2500);      // Через 2.5 секунды без новых событий — сбросить "печатает"
        timer->setSingleShot(true);

        connect(timer, &QTimer::timeout, this, [this, fromUser](){
            if (m_userCache.contains(fromUser)) {
                m_userCache[fromUser].isTyping = false;
                emit typingStatusChanged(fromUser, false);
            }
        });
        m_typingReceiveTimers[fromUser] = timer;
    }

    // Перезапускаем таймер для этого пользователя
    m_typingReceiveTimers[fromUser]->start();
}

void DataService::clearAllData()
{
    // Очищаем все кэши и состояния
    m_chatHistoryCache.clear();
    m_userCache.clear();
    m_unreadCounts.clear();
    m_chatMetadataCache.clear();

    // Сбрасываем текущие сущности и флаги
    m_currentUser = User();
    m_currentChatPartner = User();
    m_isLoadingHistory = false;
    m_oldestMessageId = 0;
    m_editingMessageId = 0;
    m_replyToMessageId = 0;
    m_isChatSearchActive = false;

    // Удаляем все таймеры статуса "печатает" и очищаем карту
    qDeleteAll(m_typingReceiveTimers);
    m_typingReceiveTimers.clear();

    qDebug() << "[DataService] All data and state has been cleared.";
}

int DataService::getLastServerIdForChat(int chatId) {
    QSqlQuery query;
    query.prepare("SELECT MAX(server_id) FROM messages WHERE chat_id = ?");
    query.addBindValue(chatId);

    // Получаем максимальный server_id для данного чата
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

void DataService::syncChatHistory(const QString& chatPartner) {
    qDebug() << "[DataService] Starting history sync for chat with:" << chatPartner;

    QList<ChatMessage> localHistory;
    QSqlQuery query;
    // Загружаем всю локальную историю для этого собеседника
    query.prepare(
        "SELECT server_id, from_user, to_user, payload, timestamp, "
        "is_edited, reply_to_id, status, file_id, file_name, file_url "
        "FROM messages WHERE (from_user = :usr2 OR to_user = :usr2) "
        "ORDER BY server_id ASC"
    );
    query.bindValue(":usr2", chatPartner);
    query.bindValue(":usr2", chatPartner);  // Дубликат, но harmless

    if (query.exec()) {
        qDebug() << "Queryexec";
        while (query.next()) {
            ChatMessage msg;
            msg.id        = query.value(0).toDouble();
            msg.fromUser  = query.value(1).toString();
            msg.toUser    = query.value(2).toString();
            msg.payload   = query.value(3).toString();
            msg.timestamp = query.value(4).toString();
            msg.isEdited  = query.value(5).toInt();
            msg.replyToId = query.value(6).toInt();
            msg.isOutgoing = (msg.fromUser == m_currentUser.username);
            msg.fileId    = query.value(8).toString();
            msg.fileName  = query.value(9).toString();
            msg.fileUrl   = query.value(10).toString();

            // Восстанавливаем статус по числовому коду
            if (query.value(7).toInt() == 1) {
                msg.status = ChatMessage::Delivered;
            }
            if (query.value(7).toInt() == 3) {
                msg.status = ChatMessage::Read;
            }

            localHistory.append(msg);
        }
    } else {
        qDebug() << query.lastError();
    }

    // Кладем историю в кэш
    m_chatHistoryCache[chatPartner].messages = localHistory;
    if (!localHistory.isEmpty()) {
        m_oldestMessageId = localHistory.first().id;
    }

    qDebug() << "[DataService] Loaded" << localHistory.size() << "messages from local cache";
    emit historyLoaded(chatPartner, localHistory);

    // Определяем lastId для догрузки с сервера
    int lastId = 0;
    if (!localHistory.isEmpty()) {
        lastId = localHistory.last().id;
        qDebug() << "Last ID" << localHistory.last().id
                 << "First:" << localHistory.first().id;
    }

    qDebug() << "[DataService] Requesting history from server after id:" << lastId;

    // Запрашиваем у сервера историю после lastId
    emit requestServerHistory(chatPartner, lastId);
}

QSet<qint64> DataService::fetchExistingServerIds(const QString& chatPartner) {
    QSet<qint64> serverIds;
    QSqlQuery query;

    // Получаем все server_id для сообщений с этим собеседником (и входящих, и исходящих)
    query.prepare(
        "SELECT server_id FROM messages "
        "WHERE (from_user = ? OR to_user = ?) AND server_id NOT NULL"
    );
    query.addBindValue(chatPartner);
    query.addBindValue(chatPartner);

    if (query.exec()) {
        while (query.next()) {
            serverIds.insert(query.value(0).toLongLong());
        }
    }
    return serverIds;
}

void DataService::insertMessagesWithUpsertFiltered(const QList<ChatMessage>& messages,
                                                   const QString& chatPartner) {
    // Если нет сообщений — делать нечего
    if (messages.isEmpty())
        return;

    // Собираем уже существующие server_id, чтобы не вставлять дубликаты
    QSet<qint64> localServerIds = fetchExistingServerIds(chatPartner);
    qDebug() << localServerIds.isEmpty();

    // Открываем транзакцию для пачечной вставки (быстрее и атомарно)
    QSqlDatabase::database().transaction();

    QSqlQuery query;
    query.prepare(
        "INSERT OR IGNORE INTO messages "
        "(server_id, temp_id, from_user, to_user, payload, timestamp, status, "
        " is_edited, reply_to_id, is_outgoing, file_id, file_name, file_url) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );

    int inserted = 0;
    for (const ChatMessage& msg : messages) {
        // Если server_id > 0 и такое сообщение уже есть локально — пропускаем
        if (msg.id > 0 && localServerIds.contains(msg.id))
            continue;

        // Биндим все поля сообщения в инсерт
        query.addBindValue(msg.id);
        query.addBindValue(msg.tempId);
        query.addBindValue(msg.fromUser);
        query.addBindValue(msg.toUser);
        query.addBindValue(msg.payload);
        query.addBindValue(msg.timestamp);
        query.addBindValue(msg.status);
        query.addBindValue(msg.isEdited);
        query.addBindValue(msg.replyToId);
        query.addBindValue(msg.isOutgoing);
        query.addBindValue(msg.fileId);
        query.addBindValue(msg.fileName);
        query.addBindValue(msg.fileUrl);

        qDebug() << msg.id << msg.tempId << msg.fromUser << msg.toUser
                 << msg.payload << msg.timestamp << msg.status
                 << msg.isEdited << msg.replyToId << msg.isOutgoing
                 << msg.fileId << msg.fileName << msg.fileUrl;

        if (query.exec()){
            inserted++;
        } else {
            qDebug() << "[ERROR] Insert failed:" << query.lastError().text();
        }
    }

    // Фиксируем транзакцию
    QSqlDatabase::database().commit();

    qDebug() << "[DataService] insertMessagesWithUpsertFiltered: inserted"
             << inserted << "filtered (was" << messages.size() << ")";
}
