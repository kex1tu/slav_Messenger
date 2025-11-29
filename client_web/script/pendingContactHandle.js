//Обработчик списка запросов на чат и добавление прослушек

export const pending_contact_list_load_html = function(contact_array){
    const pending_contact_list_html = document.getElementById('pending_contact_list');
    const contact_list_html = document.getElementById('contact_list');
    for (const item of contact_array){
        if (document.getElementById('@' + item['username'])){
            continue;
        }
        const contact = document.createElement('li');
        const accept_button = document.createElement('p');
        const refuse_button = document.createElement('p');
        
        contact.textContent = item['fromDisplayname'];
        accept_button.textContent = '+';
        refuse_button.textContent = '-';
        
        contact.classList.add('pending_contact');
        accept_button.classList.add('pending_contact_accept_button');
        refuse_button.classList.add('pending_contact_accept_button');

        contact.id = '@' + item['fromUsername'];
        contact.appendChild(accept_button);
        contact.appendChild(refuse_button);

        accept_button.addEventListener('click', () => { //Кнопка, принимающая запрос на добавление в контакты
            const parent_element = accept_button.parentElement;
            const request = {
                'type': 'contact_request_response',
                'fromUsername': item['fromUsername'],
                'response': 'accepted'
            }
            socket.send(JSON.stringify(request));
            parent_element.remove();
        });

        refuse_button.addEventListener('click', () => { //Кнопка отказа
            const parent_element = refuse_button.parentElement;
            const request = {
                'type': 'contact_request_response',
                'fromUsername': item['fromUsername'],
                'response': "declined"
            }
            socket.send(JSON.stringify(request));
            parent_element.remove();
        });

        pending_contact_list_html.appendChild(contact);
    }
}