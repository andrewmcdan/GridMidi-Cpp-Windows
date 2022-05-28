const { contextBridge, ipcRenderer } = require('electron')
window.addEventListener('DOMContentLoaded', () => {})
contextBridge.exposeInMainWorld('gridMidiAPI',{
    getStatusOfController:() => ipcRenderer.invoke('getStatusOfControllerFn'),
    sendMessageToController:(...args) => ipcRenderer.invoke('sendMessageToControllerFn',...args),
    openFile: () => ipcRenderer.invoke('dialog:openFile'),
    saveFile: () => ipcRenderer.invoke('dialog:saveFile')
})
