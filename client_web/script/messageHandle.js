export const message_history_load_html = function(message_array, new_user = false, current_chat_username){
    const message_list_element = document.getElementById('message_list');
    const menu = document.getElementById('message_contextmenu');
    if (new_user === true){ //Удаляем все элементы из контейнера для сообщений на странице
        while(message_list_element.firstChild){
            message_list_element.removeChild(message_list_element.firstChild);
        }
    }

    for (const item of new_user === true? message_array : message_array.slice().reverse()){
        const message = create_message_html(item, current_chat_username);

        if (item["fromUser"] === sessionStorage.getItem('my_username')){
            message.classList.add('my_message');
        }
        if (new_user === false){
            message_list_element.prepend(message); 
        }
        else{
            message_list_element.appendChild(message); 
        }
        
        //TODO: Тут еще много параметров, которые понадобятся позже.
    }
}

export const create_message_html = function(message, current_chat_username){ //Создает html элемент - сообщение для чата
    //Создаем элементы
    const messageDiv = document.createElement('div');
    const messagePayloadP = document.createElement('p');
    const messageInformationDiv = document.createElement('div');
    const messageIsEditP = document.createElement('p');
    const messageTimeP = document.createElement('p');
    const messageStampP = document.createElement('p');

    //Добавляем классы и id
    messageDiv.classList.add('message');
    messageDiv.id = '#' + message['id'];
    messagePayloadP.classList.add('message_payload');
    messageInformationDiv.classList.add('message_information');
    messageIsEditP.classList.add('message_is_edit');
    messageTimeP.classList.add('message_time');
    messageStampP.classList.add('message_delivery_stamp');

    //Используем значения полей из сообщения от сервера
    messagePayloadP.textContent = message['payload'];
    messageIsEditP.textContent = message['is_edited'] === 1 ? 'ред.' : '';
    messageStampP.textContent = message['is_delivered'] === 1 ? '✓✓' : '✓';

    //Парсим время отправления
    const date = new Date(message['timestamp']);
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    messageTimeP.textContent = `${hours}:${minutes}`;

    //Отдельно создаем шапку, если сообщение является ответом на другое
    if (message['reply_to_id']){
        const source_message = document.getElementById('#' + message['reply_to_id']);
        let source_text;
        if (source_message){
            if (source_message.children[0].classList.contains('reply_area')){
                source_text = source_message.children[1].textContent;
            }
            else{
                source_text = source_message.children[0].textContent;
            }
        }
        
        const replyDiv = document.createElement('div');
        const replyUserP = document.createElement('p');
        const replyTextP = document.createElement('p');

        replyDiv.classList.add('reply_area');
        replyUserP.classList.add('reply_username');
        replyTextP.classList.add('reply_payload');

        if (source_message){
            if (source_text.length > 40){
                replyTextP.textContent = source_text.slice(0, 40) + '...';
            }
            else{
                replyTextP.textContent = source_text;
            }
            
            replyUserP.textContent = source_message.classList.contains('my_message') ? sessionStorage.getItem('my_displayname') : current_chat_username;
        }
        else{
            replyTextP.textContent = "*Сообщение не загружено*";
        }
        replyDiv.appendChild(replyUserP);
        replyDiv.appendChild(replyTextP);
        messageDiv.appendChild(replyDiv);
    }

    messageInformationDiv.appendChild(messageIsEditP);
    messageInformationDiv.appendChild(messageTimeP);
    messageInformationDiv.appendChild(messageStampP);
    messageDiv.appendChild(messagePayloadP);
    messageDiv.appendChild(messageInformationDiv);

    return messageDiv;
}

export const prepare_message_hat = function(targeted_message, current_chat_username){
    if (!targeted_message){return;}
    const message_top_area = document.getElementById('message_top_area');
    const message_header_field = document.getElementById('message_header_field');
    const to_message = document.getElementById('to_message');
    if (targeted_message.classList.contains('my_message')){
        to_message.textContent = "В ответ на сообщение от " + sessionStorage.getItem('my_username');
    }
    else{
        to_message.textContent = "В ответ на сообщение от " + current_chat_username;
    }
    let text;
    if (targeted_message.children[0].classList.contains('reply_area')){
        text = targeted_message.children[1].textContent;
    }
    else{
        text = targeted_message.children[0].textContent;
    }

    if (text.length > 40){
        message_header_field.textContent = text.slice(0, 40) + '...';
    }
    else{
        message_header_field.textContent = text;
    }
    
    message_top_area.classList.remove('hidden');
}


export const cancel_message_hat = function(){
    const message_top_area = document.getElementById('message_top_area');
    const message_header_field = document.getElementById('message_header_field');
    const to_message = document.getElementById('to_message');
    message_header_field.textContent = '';
    to_message.textContent = '';
    message_top_area.classList.add('hidden');
}