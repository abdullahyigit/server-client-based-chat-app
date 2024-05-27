const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const { spawn } = require('child_process');

let mainWindow;
let clientProcess = null;
let username = '';
let room = '';

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 800,
        height: 1000,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: false,
            nodeIntegration: true
        }
    });

    mainWindow.loadFile(path.join(__dirname, 'index.html'));

    ipcMain.on('set-username', (event, user) => {
        console.log('Kullanıcı adı alındı:', user);
        username = user;
        mainWindow.loadFile(path.join(__dirname, 'lobby.html'));
    });

    ipcMain.on('set-room', (event, selectedRoom) => {
        console.log('Oda numarası alındı:', selectedRoom);
        room = selectedRoom;
        startClientProcess(username, room);
        mainWindow.loadFile(path.join(__dirname, 'chat.html')).then(() => {
            mainWindow.webContents.send('set-room-name', `Room ${parseInt(room) + 1}`);
        });
    });

    ipcMain.on('send-message', (event, message) => {
        console.log('Gönderilen mesaj:', message);
        if (clientProcess && !clientProcess.killed) {
            clientProcess.stdin.write(message + '\n');
        }
    });

    ipcMain.on('leave-room', () => {
        if (clientProcess) {
            clientProcess.kill();
            clientProcess = null;
            mainWindow.loadFile(path.join(__dirname, 'lobby.html'));
        }
    });
}

function startClientProcess(username, room) {
    const clientPath = path.join(__dirname, '../client');
    console.log('Client path:', clientPath);

    clientProcess = spawn(clientPath, ['127.0.0.1', '8080']);

    clientProcess.stdin.write(username + '\n');
    clientProcess.stdin.write(room + '\n');

    clientProcess.stdout.on('data', (data) => {
        console.log('Alınan mesaj:', data.toString());
        mainWindow.webContents.send('receive-message', data.toString());
    });

    clientProcess.stderr.on('data', (data) => {
        console.error('stderr:', data.toString());
    });

    clientProcess.on('exit', (code, signal) => {
        console.log('Client process exited with code:', code, 'and signal:', signal);
        clientProcess = null;
    });
}

app.on('ready', createWindow);
app.on('window-all-closed', () => {
    if (clientProcess) {
        clientProcess.kill();
    }
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
    }
});

