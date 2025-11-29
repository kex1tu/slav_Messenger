const cancel_message_hat = function(){
    const message_top_area = document.getElementById('message_top_area');
    const message_header_field = document.getElementById('message_header_field');
    const to_message = document.getElementById('to_message');
    message_header_field.textContent = '';
    to_message.textContent = '';
    message_top_area.classList.add('hidden');
}


export const update_html_contact_list = function(user_array){
    const contact_list_element = document.getElementById('contact_list'); //HTML элемент, представляющий список пользователей
    for (const item of user_array){
        if (document.getElementById('@' + item["username"])){
            continue;
        } //Если элемент уже есть, не добавляем его
        const contact = create_contact_html(item);
        const request = {
            "type": "get_history",
            "with_user": item["username"]
        } //Формируем запрос для отправки через прослушку
        
        
        contact.addEventListener('click', () => { //Добавляем прослушку каждой кнопке. Она будет делать нажатую кнопку выбранной, отправлять запрос на сервер, забирать статус выбранной у другой кнопки, а так же проверять, не нажали ли одну и ту же кнопку дважды
            cancel_message_hat();
            for (const elem of contact_list_element.children){ //Пробегаемся по всем контактам
                if (elem.classList.contains('current_contact')){
                    if (elem.id === contact.id){
                        return; //Если пользователь повторно запросит у сервера историю с уже выбранным пользователем, игнорируем запрос
                    }
                    else{
                        elem.classList.remove('current_contact'); //Нужно для отслеживания выбранного пользователя а так же для изменения стиля
                    }
                }
            }
            const counter = contact.querySelector('.contact_unread_count');
            counter.textContent = '';
            counter.classList.add('hidden');
            contact.classList.add('current_contact');
            socket.send(JSON.stringify(request));
        });
        contact_list_element.appendChild(contact);
    }
}

export const create_contact_html = function(user){
    const contact_div = document.createElement('div');
    const contact_f1 = document.createElement('div');
    const contact_f2 = document.createElement('div');
    const contact_name = document.createElement('p');
    const contact_p1 = document.createElement('p');
    const contact_unread = document.createElement('p');
    const contact_p2 = document.createElement('p');

    contact_div.classList.add('contact');
    contact_f1.classList.add('contact_f1');
    contact_f2.classList.add('contact_f2');
    contact_name.classList.add('contact_name'); // Имя человека
    contact_p1.classList.add('contact_p'); //Текст последнего сообщения
    contact_p2.classList.add('contact_p'); //Время отправки последнего сообщения
    contact_unread.classList.add('contact_unread_count', 'hidden'); 

    contact_div.id = '@' + user['username'];
    contact_div.appendChild(contact_f1);
    contact_div.appendChild(contact_f2);
    contact_f1.appendChild(contact_name);
    contact_f1.appendChild(contact_p1);
    contact_f2.appendChild(contact_unread);
    contact_f2.appendChild(contact_p2);

    if (user['displayname'].length > 40){
        contact_name.textContent = user['displayname'].slice(0, 40);
    }
    else{
        contact_name.textContent = user['displayname'];
    }
    return contact_div;
}

export const update_contact_html = function(contact_html, last_message, count_inc = false){
    if (!last_message){return;}
    const message_payload = last_message.querySelector('.message_payload').textContent;
    const message_time = last_message.querySelector('.message_time').textContent;
    const contact_p_list = contact_html.querySelectorAll('.contact_p');
    if (message_payload.length > 15){
        contact_p_list[0].textContent = message_payload.slice(0, 15);
    }
    else{
        contact_p_list[0].textContent = message_payload;
    }
    contact_p_list[1].textContent = message_time;
    if (count_inc){
        const counter = contact_html.querySelector('.contact_unread_count');
        
        if (counter.textContent === ''){
            counter.textContent = '1';
            counter.classList.remove('hidden');
        }
        else{
            counter.textContent = (parseInt(counter.textContent) + 1).toString();
        }
        
    }
}
