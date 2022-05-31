var controller = require('bindings')('gridmidi-gui-native')
// Modules to control application life and create native browser window
const { app, BrowserWindow, ipcMain, dialog } = require('electron')
const path = require('path')

const { exec } = require('child_process')

gridMidiProc = exec( "\"" + path.join(__dirname, 'GridMidi-Cpp-Windows.exe\"'), (error, stdout, stderr) => {
  if (error) {
    console.error(`error: ${error.message}`);
    return;
  }

  if (stderr) {
    console.error(`stderr: ${stderr}`);
    return;
  }

  console.log(`stdout:\n${stdout}`);
});

async function handleFileOpen(){
    const {canceled, filePaths} = await dialog.showOpenDialog()
    if(canceled){
        return
    }else{
        return filePaths[0]
    }
}

async function handleFileSave(){
    const {canceled, filePath} = await dialog.showSaveDialog()
    if(canceled){
        return
    }else{
        return filePath
    }
}

async function handleGetStatusOfController(){
    if(controller.IsPipeReady()){
        if(controller.PipeMessage("isReady_") == "ready"){
            return 1;
        }else{
            return -1;
        }
    }else{
        return -2;
    }
}

async function handleSendMessageToController(env,...args){
    if(controller.IsPipeReady()){
        // console.log(args);
        let res = controller.PipeMessage(args[0])
        return res
    }
}

const createWindow = () => {
    // Create the browser window.
    mainWindow = new BrowserWindow({
        width: 1920,
        height: 1080,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js')
        },
        icon: path.join(__dirname, 'icon.png'),
        //frame:false
    })

    mainWindow.setMenuBarVisibility(false)

    // and load the index.html of the app.
    mainWindow.loadFile('index.html')
    // Open the DevTools.
    mainWindow.webContents.openDevTools()
    // mainWindow.maximize();

    // catch the close event and show a confirmation dialog
    mainWindow.on('close',(event)=>{
        var choice = dialog.showMessageBoxSync({
            type: 'question',
            buttons: ['Yes, exit now', 'No, go back'],
            title: 'Exit confirmation',
            message: 'Are you sure you want to exit? Perhaps you should save.'
        });
        if(choice == 1)event.preventDefault()
    })
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.whenReady().then(() => {
    ipcMain.handle('getStatusOfControllerFn', handleGetStatusOfController)
    ipcMain.handle('sendMessageToControllerFn', handleSendMessageToController)
    ipcMain.handle('dialog:openFile', handleFileOpen)
    ipcMain.handle('dialog:saveFile', handleFileSave)
    createWindow()

    app.on('activate', () => {
        // On macOS it's common to re-create a window in the app when the
        // dock icon is clicked and there are no other windows open.
        if (BrowserWindow.getAllWindows().length === 0) createWindow()
    })
})

// Quit when all windows are closed, except on macOS. There, it's common
// for applications and their menu bar to stay active until the user quits
// explicitly with Cmd + Q.
app.on('window-all-closed', () => {
    controller.PipeMessage('killGridMidi')
    if (process.platform !== 'darwin') app.quit()
})

/**
 * Need to add logic to detect if the controller app is running so that it isn't started twice.
 * 
 * Need to add termination to controller app so that it kills it when exiting GUI.
 */