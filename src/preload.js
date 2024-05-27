const { ipcRenderer } = require('electron');

window.addEventListener('DOMContentLoaded', () => {
    ipcRenderer.on('receive-message', (event, message) => {
        const messagesDiv = document.getElementById('messages');
        const messageElement = document.createElement('div');
        messageElement.textContent = `${message}`;
        messagesDiv.appendChild(messageElement);
        messagesDiv.scrollTop = messagesDiv.scrollHeight;
    });

    const input = document.getElementById('input');
    if (input) {
        input.addEventListener('keypress', (event) => {
            if (event.key === 'Enter') {
                const message = input.value;
                ipcRenderer.send('send-message', message);
                input.value = '';
            }
        });
    }
});

