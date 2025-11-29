import * as contactListHandle from './contactListHandle.js';
import * as messageHandle from './messageHandle.js';
import * as pendingContactHandle from './pendingContactHandle.js';
import * as contactSearchHandle from './contactSearchHandle.js';

//--------ЭЛЕМЕНТЫ HTML СТРАНИЦЫ-----------

const loginButton = document.getElementById('login');
const username_input_html = document.getElementById('username_field');
const password_input_html = document.getElementById('password_field');
const loginWrapper = document.getElementById('Login_wrapper');
const mainWrapper = document.getElementById('Main_wrapper');
const message_list_html = document.getElementById('message_list');
const contact_list_html = document.getElementById('contact_list');
const contact_search_html = document.getElementById('contact_search');
const contact_search_list_html = document.getElementById('contact_search_list');
const right_column = document.getElementById('right_column');
const send_button_html = document.getElementById('send_button');
const message_input_html = document.getElementById('message_input');
const pending_contact_list_html = document.getElementById('pending_contact_list');
const pending_contact_button_html = document.getElementById('pending_contact_button');
const search_cancel_button_html = document.getElementById('search_cancel_button');
const pending_contact_image_html = document.getElementById('pending_contact_img');
const chat_img_html = document.getElementById('chat_image');
const message_contextmenu = document.getElementById('message_contextmenu');
const contextmenu_ans = document.getElementById('contextmenu_ans');
const contextmenu_red = document.getElementById('contextmenu_red');
const contextmenu_del = document.getElementById('contextmenu_del');
const message_top_area = document.getElementById('message_top_area');
const message_header_field = document.getElementById('message_header_field');
const last_seen_html = document.getElementById('profile_last_seen');
const profile_username_html = document.getElementById('profile_username');
const profile_wrapper_html = document.getElementById('profile_wrapper');
const user_profile_html = document.getElementById('user_profile');
const profile_field_html = document.getElementById('profile_field');
const profile_field_name_html = document.getElementById('profile_field_name');
const profile_field_last_seen = document.getElementById('profile_field_last_seen');
const profile_field_username_html = document.getElementById('profile_field_username');
const settings_button_html = document.getElementById('settings_button');
const my_profile_wrapper_html = document.getElementById('my_profile_wrapper');
const settings_menu_html = document.getElementById('settings_menu');
const my_profile_option_html = document.getElementById('my_profile_option');
const my_profile_field_html = document.getElementById('my_profile_field');
const profile_field_cancel_html = document.getElementById('profile_field_cancel');
const settings_cancel_button_html = document.getElementById('settings_cancel');
const my_profile_displayname_html = document.getElementById('my_profile_displayname');
const my_profile_username_html = document.getElementById('my_profile_username');
const about_me_html = document.getElementById('about_me');
const profile_field_edit_html = document.getElementById('profile_field_edit');
const img_edit_html = document.getElementById('profile_field_button_img_edit');
const img_complete_html = document.getElementById('profile_field_button_img_complete');
const logout_option_html = document.getElementById('logout_option');

//----------------ПЕРЕМЕННЫЕ С ОБЩЕЙ ОБЛАСТЬЮ ВИДИМОСТИ-----------------

let current_chat_username; //Отслеживаем, с каким пользователем открыт чат
let first_message_in_chat; //ID первого в списке сообщения в чате
let previous_first_message; //Запоминаем переменную строчкой выше, когда происходит вызов функции на прокрутке
let temp_id_counter = 0;
let profile_editing = false;
let targeted_message; //Выделенное пользователем сообщение, на которое будет отправлен ответ
let private_message_queue = []; //Очередь из личных сообщений с временными ID. Будем убирать сообщения отсюда, если сервер подтвердит отправку
let for_edit = false; //Флаг для отслеживания редактирования сообщения, который меняет поведение кнопки отправки
let user_array = []; //Массив с объектами user = 
// {"username": "123", 
// "displayname": "123", 
// "last_seen": ""}

// class User{
//     constructor(username, displayname, last_seen){
//         this.username = username;
//         this.displayname = displayname;
//         this.last_seen = last_seen;
//     }
// }
let old_displayname;
let old_about;
let is_trottle = true;


//-----------ФУНКЦИИ-ПРОСЛУШКИ-----------

function login(){
    const username = username_input_html.value;
    const password = password_input_html.value;
    password_input_html.value = '';

    const loginData = {
        type: "login",
        username: username,
        password: password
    };
    sessionStorage.setItem('my_username', username);
    socket.send(JSON.stringify(loginData));
}
loginButton.addEventListener('click', login);

username_input_html.addEventListener('keydown', function(event){
    if (event.key === 'ArrowDown' || event.key === 'Enter'){
        password_input_html.focus();
    }
});

password_input_html.addEventListener('keydown', function(event){
    if (event.key === 'Enter'){
        login();
        return;
    }
    if (event.key === 'ArrowUp'){
        username_input_html.focus();
    }
    
});

message_list_html.addEventListener('scroll', () => { //Обработка прокручивания списка сообщений
    if (previous_first_message === first_message_in_chat){return;} //Не отправляем запрос, если достигли начала истории или если происходит слишком частый вызов функции
    if (message_list_html.scrollTop <= 200){
        const request = {
            "type": "get_history",
            "with_user": current_chat_username,
            "before_id": first_message_in_chat
        }
        socket.send(JSON.stringify(request));
        previous_first_message = first_message_in_chat;
    }
})


const send_message = function(reply_to_id = 0){
    if (current_chat_username === undefined){return;}
    if (message_input_html.value === ''){return;}
    const temp_id = 'temp' + temp_id_counter.toString();
    ++temp_id_counter;

    const request = {
        "type": "private_message",
        "fromUser": sessionStorage.getItem('my_username'),
        "toUser": current_chat_username,
        "payload": message_input_html.value,
        "reply_to_id": reply_to_id,
        "temp_id": temp_id
    }
    socket.send(JSON.stringify(request));
    private_message_queue.push(temp_id);
}


send_button_html.addEventListener('click', () => {
    if (for_edit){
        const request = {
            'type': 'edit_message',
            'id': parseInt(targeted_message.id.slice(1)),
            'payload': message_input_html.value,
        }
        socket.send(JSON.stringify(request));
        message_input_html.value = '';
        messageHandle.cancel_message_hat();
        for_edit = false;
        return;
    }
    if (!(message_top_area.classList.contains('hidden'))){
        send_message(targeted_message.id.slice(1));
        messageHandle.cancel_message_hat();
    }
    else{
        send_message();
    } 
    message_list_html.scrollTo(0, message_list_html.scrollHeight);
    message_input_html.value = '';
});

message_input_html.addEventListener('keydown', function(event){
    if (event.key === 'Enter' && !event.shiftKey){
        event.preventDefault();
        send_button_html.click();
    }
});

message_input_html.addEventListener('input', () => {
    if (is_trottle){
        is_trottle = false;
        setTimeout(() => {is_trottle = true}, 2000); //2000мс = 2с задержки
        const request = {
            'type': 'typing',
            'toUser': current_chat_username
        };
        socket.send(JSON.stringify(request));
    }
});

pending_contact_button_html.addEventListener('click', () => { //Свап списков чатов и запросов на переписку
    if (contact_list_html.classList.contains('hidden')){
        contact_list_html.classList.remove('hidden');
        pending_contact_list_html.classList.add('hidden');
        pending_contact_image_html.classList.remove('hidden');
        chat_img_html.classList.add('hidden');
    }
    else{
        contact_list_html.classList.add('hidden');
        pending_contact_list_html.classList.remove('hidden');
        pending_contact_image_html.classList.add('hidden');
        chat_img_html.classList.remove('hidden');
    }
})

contact_search_html.addEventListener('input', () => {
    if (contact_search_html.value === ''){return;}
    const request = {
        'type': "search_users",
        'term': contact_search_html.value
    }
    socket.send(JSON.stringify(request));
})

contact_search_html.addEventListener('keydown', function(event){
    if (contact_search_html.value === '') {return;}
    if (event.key === 'Enter'){
        const request = {
            'type': 'add_contact_request',
            'username': contact_search_html.value
        }
        socket.send(JSON.stringify(request));
        contact_search_html.value = '';
        contact_search_html.blur();
    }
})

contact_search_html.addEventListener('focus', function(event){
    contact_list_html.classList.add('hidden');
    search_cancel_button_html.classList.remove('hidden');
    pending_contact_button_html.classList.add('hidden');
    pending_contact_list_html.classList.add('hidden');
    contact_search_html.placeholder = '\"Enter\" для добавления в друзья';
    contact_search_list_html.classList.remove('hidden');
})

search_cancel_button_html.addEventListener('click', function(event){
    contact_search_html.placeholder = 'Поиск контактов...';
    contact_list_html.classList.remove('hidden');
    chat_img_html.classList.add('hidden');
    pending_contact_image_html.classList.remove('hidden');
    contact_search_list_html.classList.add('hidden');
    pending_contact_button_html.classList.remove('hidden');
    search_cancel_button_html.classList.add('hidden');
    while (contact_search_list_html.firstChild){
        contact_search_list_html.removeChild(contact_search_list_html.firstChild);
    }
})

contact_search_list_html.addEventListener('click', function(event){
    contact_search_list_html.classList.remove('hidden');
    const found_contacts = contact_search_list_html.children;
    let hitbox;
    for (const item of found_contacts){
        hitbox = item.getBoundingClientRect();
        if (hitbox.top <= event.clientY && event.clientY <= hitbox.bottom){
            contact_search_html.value = item.children[1].textContent.slice(1); //Содержание элемента p "username"
            break;
        }
    }
    contact_search_html.focus();
})

message_list_html.addEventListener('contextmenu', function(event){
    if (current_chat_username === undefined){return;}
    messageHandle.cancel_message_hat();
    event.preventDefault();
    if (message_contextmenu.classList.contains('hidden')){
        
        const messages = message_list_html.children; //Список всех сообщений в контейнере 
        let s_message; //Это будет ближайшим сообщением к точке клика по оси Y
        let hitbox; //Информация о коллизии элемента-сообщения
        for (const item of messages){ //Перебираем все сообщения
            hitbox = item.getBoundingClientRect(); 
            if (hitbox.top <= event.clientY && event.clientY <= hitbox.bottom){
                s_message = item;
                break;
            }
        }
        message_contextmenu.style.left = hitbox.left + hitbox.width + 5 + 'px'; //Ставим меню относительно левого края сообщения
        message_contextmenu.style.top = event.clientY + 'px'; //Ставим меню отсносительно середины сообщения
        message_contextmenu.classList.remove('hidden'); //Делаем меню видимым
        targeted_message = s_message;
        //Здесь нужно сделать кнопки "редактировать" и "удалить" невидимыми, если targeted_message не наше (не содержит класса my_message)
    }
    else{
        message_contextmenu.classList.add('hidden');
    }
});

contextmenu_ans.addEventListener('click', () => {
    message_contextmenu.classList.add('hidden');
    messageHandle.prepare_message_hat(targeted_message, current_chat_username);
});

contextmenu_red.addEventListener('click', () => {
    message_contextmenu.classList.add('hidden');
    if (!(targeted_message.classList.contains('my_message'))){
        return;
    }
    messageHandle.prepare_message_hat(targeted_message);
    message_input_html.value = targeted_message.querySelector('.message_payload').textContent;
    for_edit = true; //Помечаем следующее действие кнопки отправки, как истину
});

contextmenu_del.addEventListener('click', () => {
    message_contextmenu.classList.add('hidden');
    if (targeted_message.classList.contains('my_message')){
        const request = {
            'type': 'delete_message',
            'id': parseInt(targeted_message.id.slice(1))
        }
        socket.send(JSON.stringify(request));
    }
    else{ 
        return;
     }
});

message_header_field.addEventListener('click', () => {
    messageHandle.cancel_message_hat();
    if (for_edit){
        message_input_html.value = '';
    }
    for_edit = false; //Больше не редактируем сообщение
});

const prepare_profile = function(){
    profile_field_username_html.textContent = '@' + current_chat_username;
    profile_field_name_html.textContent = user_array[current_chat_username]['displayname'];
    profile_field_last_seen.textContent = last_seen_html.textContent;
}

user_profile_html.addEventListener('click', function(event) {
    event.preventDefault();
    profile_wrapper_html.classList.remove('hidden');
    prepare_profile();
});

profile_wrapper_html.addEventListener('click', function(event) {
    const hitbox = profile_field_html.getBoundingClientRect();
    if (!(hitbox.top <= event.clientY && event.clientY <= hitbox.bottom && hitbox.left <= event.clientX && event.clientX <= hitbox.right)){
        profile_wrapper_html.classList.add('hidden');
    }
});

settings_button_html.addEventListener('click', () => {
    my_profile_wrapper_html.classList.remove('hidden');
});

settings_cancel_button_html.addEventListener('click', () => {
    my_profile_wrapper_html.classList.add('hidden');
});

my_profile_option_html.addEventListener('click', () => {
    //Подготавливаем профиль
    my_profile_displayname_html.value = sessionStorage.getItem('my_displayname');
    my_profile_username_html.value = sessionStorage.getItem('my_username');
    about_me_html.value = sessionStorage.getItem('about_me');
    settings_menu_html.classList.add('hidden');
    my_profile_field_html.classList.remove('hidden');
});

profile_field_cancel_html.addEventListener('click', () => {
    my_profile_field_html.classList.add('hidden');
    settings_menu_html.classList.remove('hidden');
});

profile_field_edit_html.addEventListener('click', () => {
    if (profile_editing === false){
        profile_editing = true;
        old_displayname = my_profile_displayname_html.value;
        old_about = about_me_html.value;
        img_edit_html.classList.add('hidden');
        img_complete_html.classList.remove('hidden');
        my_profile_displayname_html.disabled = false;
        about_me_html.disabled = false;
        my_profile_displayname_html.classList.add('input_editing');
        about_me_html.classList.add('input_editing');
    }
    else{
        profile_editing = false;
        img_edit_html.classList.remove('hidden');
        img_complete_html.classList.add('hidden');
        my_profile_displayname_html.classList.remove('input_editing');
        about_me_html.classList.remove('input_editing');
        my_profile_displayname_html.disabled = true;
        about_me_html.disabled = true;
        if (old_displayname !== my_profile_displayname_html.value || old_about !== about_me_html.value){
            const request = {
                'type': 'update_profile',
                'display_name': my_profile_displayname_html.value,
                'status_message': about_me_html.value,
                //Здесь будет новый аватар
            }
            socket.send(JSON.stringify(request));
        }
    }
});

const logout = function(){
    sessionStorage.clear();
    while(message_list_html.firstChild){
        message_list_html.removeChild(message_list_html.firstChild);
    }
    while(contact_list_html.firstChild){
        contact_list_html.removeChild(contact_list_html.firstChild);
    }
    while(pending_contact_list_html.firstChild){
        pending_contact_list_html.removeChild(pending_contact_list_html.firstChild);
    }
    while(contact_search_list_html.firstChild){
        contact_search_list_html.removeChild(contact_search_list_html.firstChild);
    }

    profile_username_html.textContent = '';
    last_seen_html.textContent = '';
    messageHandle.cancel_message_hat();
    message_contextmenu.classList.add('hidden');
    my_profile_wrapper_html.classList.add('hidden');

    current_chat_username = undefined;
    first_message_in_chat = undefined; 
    previous_first_message = undefined; 
    temp_id_counter = 0;
    profile_editing = false;
    targeted_message = undefined; 
    private_message_queue = []; 
    for_edit = false;
    user_array = [];
    old_displayname = undefined;
    old_about = undefined;
    is_trottle = true;
    const request = {
        'type': 'logout_request'
    }
    socket.send(JSON.stringify(request));

    loginWrapper.classList.remove("hidden");
    mainWrapper.classList.add("hidden");
}

logout_option_html.addEventListener('click', logout);

//----------ЕДИНСТВЕННАЯ ПРОСЛУШКА СОКЕТА----------

socket.onmessage = function(event){
    let response = JSON.parse(event.data);
    let tmp_array = [];
    
    switch (response["type"]){
        case "login_success":
            alert("Выполнен вход");
            loginWrapper.classList.add("hidden");
            mainWrapper.classList.remove("hidden");
            sessionStorage.setItem('my_displayname', response['displayname']);
            sessionStorage.setItem('about_me', response['statusmessage']);
            break;
        case "login_failure":
            alert("login failure. Reason: " + response["reason"]);
            break;
        case "contact_list":
            tmp_array = response['users'];
            contactListHandle.update_html_contact_list(tmp_array);
            for (const elem of tmp_array){
                const {username, ...new_contact} = elem; //Убираем поле username, так как оно теперь является ключем значения
                user_array[elem['username']] = new_contact;
            }
            break;
        case "history_data":
            current_chat_username = response["with_user"];
            tmp_array = response["history"];
            messageHandle.message_history_load_html(tmp_array, true, user_array[current_chat_username]['displayname']);
            if (tmp_array.length !== 0){
                first_message_in_chat = tmp_array[0]['id'];
            }
            // last_message_id = message_list_html.lastChild.id;
            if (current_chat_username.length > 40){
                profile_username_html.textContent = current_chat_username.slice(0, 40) + '...';
            }
            else{
                profile_username_html.textContent = user_array[current_chat_username]['displayname'];
            }
            
            const date = new Date(user_array[current_chat_username]['last_seen']);
            const hours = String(date.getHours()).padStart(2, '0');
            const minutes = String(date.getMinutes()).padStart(2, '0');
            if (hours !== 'NaN' && minutes !== 'NaN'){
                last_seen_html.textContent = `Был(а) в сети в ${hours}:${minutes}`;
            }
            else{
                last_seen_html.textContent = 'Не в сети';
            }
            var contact_html = document.getElementById('@' + current_chat_username);
            var last_message = message_list_html.lastChild;  
            contactListHandle.update_contact_html(contact_html, last_message);

            message_list_html.scrollTo(0, message_list_html.scrollHeight);
            break;
        case "old_history_data":
            tmp_array = response["history"];
            messageHandle.message_history_load_html(tmp_array, false, user_array[current_chat_username]['displayname']);
            if (tmp_array.length !== 0){
                first_message_in_chat = tmp_array[0]['id'];
            }
            break;
        case "private_message":
            if (response['temp_id']){ //Наличие поля temp_id говорит о том, что сервер отправил эхо нашего же сообщения
                let temp_id = response['temp_id'];
                if (!(private_message_queue.includes(temp_id))){ //Если такого сообщения мы не отправляли, игнорируем его
                    break;
                }
                private_message_queue.splice(private_message_queue.indexOf(temp_id), 1); //Удаляем временный ID из очереди, так как сервер отчитался о его доставке
                
                const message_html = messageHandle.create_message_html(response, user_array[current_chat_username]['displayname']);
                message_html.classList.add('my_message');
                message_list_html.appendChild(message_html);
                contactListHandle.update_contact_html(document.getElementById('@' + response['toUser']), message_html);
            }
            else{
                const message_html = messageHandle.create_message_html(response, user_array[current_chat_username]['displayname'])
                if (current_chat_username === response['fromUser']){
                    message_list_html.appendChild(message_html);
                    contactListHandle.update_contact_html(document.getElementById('@' + response['fromUser']), message_html);
                }
                else{
                    contactListHandle.update_contact_html(document.getElementById('@' + response['fromUser']), message_html, true);
                }
            }

            

            message_list_html.scrollTo(0, message_list_html.scrollHeight);
            break;
        case "pending_requests_list":
            tmp_array = response['requests'];
            pendingContactHandle.pending_contact_list_load_html(tmp_array);
            break;
        case "incoming_contact_request":
            tmp_array = [];
            tmp_array.push(response);
            pendingContactHandle.pending_contact_list_load_html(tmp_array);
            break;
        case "search_results":
            tmp_array = response['users'];
            contactSearchHandle.update_html_search_database(tmp_array);
            break;
        case "add_contact_success":
            alert(response['reason']);
            break;
        case "add_contact_failure":
            alert(response['reason']);
            break;
        case "delete_message":
            if (current_chat_username === response['with_user']){
                const msg_to_del = document.getElementById('#' + response['id']);
                if (msg_to_del){msg_to_del.remove();}
            }
            break;
        case "edit_message":
            const edited_message = document.getElementById('#' + response['id']);
            if (edited_message){
                edited_message.querySelector('.message_payload').textContent = response['payload'];
                edited_message.querySelector('.message_is_edit').textContent = 'ред.';
            }
            break;
        case 'user_list':
            //Здесь будет обработка онлайн-списка
            break;
        case 'typing':
            if (response['fromUser'] === current_chat_username){
                const tmp = last_seen_html.textContent;
                last_seen_html.textContent = 'Печатает...';
                const timer = setTimeout(() => {last_seen_html.textContent = tmp}, 2000);
            }
            break;
        case 'update_profile_result':
            if (response['success'] === true){
                alert('Профиль успешно обновлен');
                sessionStorage.setItem('my_displayname', response['display_name']);
                sessionStorage.setItem('about_me', response['status_message']);
            }
            else{
                alert('Оишбка изенения профиля. Причина ' + response['reason']);
                about_me_html.value = old_about;
                my_profile_displayname_html.value = old_displayname;
            }
            break;
        case 'unread_counts':
            tmp_array = response['counts'];
            for (const elem of response['counts']){
                const contact = document.getElementById('@' + elem['username']);
                if (contact){
                    const counter = contact.querySelector('.contact_unread_count');
                    counter.textContent = elem['count'].toString();
                    counter.classList.remove('hidden');
                }
            }
            break;
    }
}