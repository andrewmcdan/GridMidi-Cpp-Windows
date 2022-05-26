const { contextBridge, ipcRenderer } = require('electron')

window.addEventListener('DOMContentLoaded', () => {

})

contextBridge.exposeInMainWorld('gridMidiAPI',{
    testFn: () => {console.log("this is a test");},
    getStatusOfController:() => ipcRenderer.invoke('getStatusOfControllerFn'),
    sendMessageToController:(...args) => ipcRenderer.invoke('sendMessageToControllerFn',...args),
    openFile: () => ipcRenderer.invoke('dialog:openFile'),
    saveFile: () => ipcRenderer.invoke('dialog:saveFile')
})
