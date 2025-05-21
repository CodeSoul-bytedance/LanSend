const { ipcRenderer, contextBridge } = require("electron");

contextBridge.exposeInMainWorld("electronAPI", {
    sendToBackend: (message) => {
        console.log(
            "Preload: window.electronAPI.sendToBackend called with:",
            message
        );
        return ipcRenderer.invoke("send-to-backend", message);
    },
    onBackendConnected: (callback) => {
        console.log(
            "Preload: window.electronAPI.onBackendConnected listener being set up (app.js might not use this)."
        );
        const listener = (_, status) => {
            console.log(
                'Preload: (window.electronAPI) ipcRenderer received "backend-connected", status:',
                status
            );
            callback(status);
        };
        ipcRenderer.on("backend-connected", listener);
        return () => ipcRenderer.removeListener("backend-connected", listener);
    },
    onBackendError: (callback) => {
        console.log(
            "Preload: window.electronAPI.onBackendError listener being set up (app.js might not use this)."
        );
        const listener = (_, error) => {
            console.log(
                'Preload: (window.electronAPI) ipcRenderer received "backend-error", error:',
                error
            );
            callback(error);
        };
        ipcRenderer.on("backend-error", listener);
        return () => ipcRenderer.removeListener("backend-error", listener);
    },
    // Signal from renderer that its core (Vue app) is mounted and ready
    rendererReady: () => ipcRenderer.send("renderer-mounted-ready"),
    openFileDialog: () => {
        console.log("Preload: window.electronAPI.openFileDialog called");
        return ipcRenderer.invoke("open-file-dialog");
    },
    openFilesDialog: () => {
        console.log("Preload: window.electronAPI.openFilesDialog called");
        return ipcRenderer.invoke("open-files-dialog");
    },
    showTransferRequest: (callback) => {
        console.log(
            "Preload: window.electronAPI.showTransferRequest listener being set up (app.js might not use this)."
        );
        const listener = (_, ...args) => {
            console.log(
                'Preload: (window.electronAPI) ipcRenderer received "show-transfer-request", args:',
                args
            );
            callback(...args);
        };
        ipcRenderer.on("show-transfer-request", listener);
        return () =>
            ipcRenderer.removeListener("show-transfer-request", listener);
    },
    transferStatusChanged: (callback) => {
        console.log(
            "Preload: window.electronAPI.transferStatusChanged listener being set up (app.js might not use this)."
        );
        const listener = (_, ...args) => {
            console.log(
                'Preload: (window.electronAPI) ipcRenderer received "transfer-status-changed", args:',
                args
            );
            callback(...args);
        };
        ipcRenderer.on("transfer-status-changed", listener);
        return () =>
            ipcRenderer.removeListener("transfer-status-changed", listener);
    },
    onDevicesUpdated: (callback) => {
        console.log(
            "Preload: window.electronAPI.onDevicesUpdated listener being set up (app.js might not use this)."
        );
        const listener = (_, ...args) => {
            console.log(
                'Preload: (window.electronAPI) ipcRenderer received "devices-updated", args:',
                args
            );
            callback(...args);
        };
        ipcRenderer.on("devices-updated", listener);
        return () => ipcRenderer.removeListener("devices-updated", listener);
    },
    onBackendMessage: (callback) => {
        console.log(
            "Preload: window.electronAPI.onBackendMessage listener being set up (app.js might not use this)."
        );
        const listener = (_, message) => {
            console.log(
                'Preload: (window.electronAPI) ipcRenderer received "backend-message", message:',
                message
            );
            callback(message);
        };
        ipcRenderer.on("backend-message", listener);
        return () => ipcRenderer.removeListener("backend-message", listener);
    },
    requestBackendStatus: () =>
        ipcRenderer.invoke("send-to-backend", {
            type: "REQUEST_BACKEND_STATUS",
        }),
});

// Send a signal that the renderer's JavaScript context (this preload script) is ready
// This is different from the Vue app being mounted.
ipcRenderer.send("renderer-ready");

console.log(
    "Preload: electronAPI attached to window object. window.electronAPI is:",
    window.electronAPI
);
window.myCustomPreloadTest = "Preload for contextIsolation:false was here";
