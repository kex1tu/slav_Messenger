export const update_html_search_database = function(array){
    const contact_search_database = document.getElementById('contact_search_list');
    while (contact_search_database.firstChild){
        contact_search_database.removeChild(contact_search_database.firstChild);
    }
    for (const item of array){
        const contact = document.createElement('div');
        const username = document.createElement('p');
        const displayname = document.createElement('p');
        contact.classList.add('found_contact');
        username.classList.add('found_contact_username')

        username.textContent = '@' + item['username'];
        displayname.textContent = item['displayname'];

        contact.appendChild(displayname);
        contact.appendChild(username);
        contact_search_database.appendChild(contact);
    }
}