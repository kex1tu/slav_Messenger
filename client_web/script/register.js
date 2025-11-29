const registerButton = document.getElementById('register');
const responseMessage = document.getElementById('response-message');
const displaynameInput = document.getElementById('displayname_field');
const usernameInput = document.getElementById('username_field');
const passwordInput = document.getElementById('password_field');

function register(){
    const displayname = displaynameInput.value;
    const username = usernameInput.value;
    const password = passwordInput.value;

    const registerData = {
        type: "register",
        display_name: displayname,
        username: username,
        password: password
    };
    socket.send(JSON.stringify(registerData));
}

socket.onmessage = function(event){
    
    const response = JSON.parse(event.data);

    if (response["type"] === "register_success"){
        alert("Регистрация успешна!");
    } 
    else{
        alert('Ошибка регистрации. Причина: ' + response['reason']);
    }
    
}

registerButton.addEventListener('click', register);