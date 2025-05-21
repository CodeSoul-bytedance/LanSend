const { app, BrowserWindow, ipcMain, dialog } = require("electron");
const path = require("path");
const fs = require("fs");
const { spawn } = require("child_process");
const net = require("net");
const crypto = require("crypto");

// 全局变量保存主窗口
let mainWindow;
// 后端子进程
let backendProcess = null;
let backendStdinSocket = null;
let backendStdoutSocket = null;

// 状态变量
let isPipeConnected = false;
let isBackendLogicallyReady = false;
let isRendererReady = false;
let overallBackendReportedStatus = false; // To track what was last sent to renderer

// 消息处理队列
const messageQueue = {};
// 消息序列号
let messageId = 1;

// 后端现在会主动通知设备发现，不再需要定时扫描

// Helper function to create a named pipe server
function createPipeServer(pipeName, onConnection, onError) {
    const server = net.createServer(onConnection);
    server.on("error", onError);
    server.listen(pipeName, () =>
        console.log(`Pipe server listening on ${pipeName}`)
    );
    return server;
}

// Centralized function to update GUI status
function updateAndReportOverallBackendStatus(reasonDetails) {
    console.log(
        `updateAndReportOverallBackendStatus triggered. Reason: ${JSON.stringify(
            reasonDetails
        )}`
    );
    console.log(
        `Current states before update: pipe=${isPipeConnected}, logicReady=${isBackendLogicallyReady}, rendererReady=${isRendererReady}. Last reported to GUI: ${overallBackendReportedStatus}`
    );

    const newOverallStatus =
        isPipeConnected && isBackendLogicallyReady && isRendererReady;

    console.log(`New overall status calculated: ${newOverallStatus}`);

    if (newOverallStatus !== overallBackendReportedStatus) {
        if (
            mainWindow &&
            mainWindow.webContents &&
            !mainWindow.webContents.isDestroyed()
        ) {
            console.log(
                `Sending 'backend-connected: ${newOverallStatus}' to renderer.`
            );
            mainWindow.webContents.send("backend-connected", newOverallStatus);
            overallBackendReportedStatus = newOverallStatus;
        } else {
            console.log(
                "Cannot send status to renderer: mainWindow or webContents not available or destroyed."
            );
        }
    } else {
        console.log(
            `No change in overall status to report to GUI. Current: ${newOverallStatus}, Last reported: ${overallBackendReportedStatus}`
        );
    }
}

// 启动C++后端进程
function startBackendProcess() {
    const pipeId = crypto.randomBytes(8).toString("hex");
    const stdinPipeName = `\\\\.\\pipe\\lansend-stdin-${pipeId}`;
    const stdoutPipeName = `\\\\.\\pipe\\lansend-stdout-${pipeId}`;
    console.log(`Generated stdin pipe name: ${stdinPipeName}`);
    console.log(`Generated stdout pipe name: ${stdoutPipeName}`);

    const isDev = process.argv.includes("--dev");
    let backendPath;
    const appBasePath = path.join(app.getAppPath(), "..", "build"); // Base path for build artifacts

    if (isDev) {
        // Development: try Debug first, then Release
        backendPath = path.join(appBasePath, "Debug", "lansend-backend");
        if (process.platform === "win32") backendPath += ".exe";
        if (!fs.existsSync(backendPath)) {
            backendPath = path.join(appBasePath, "Release", "lansend-backend");
            if (process.platform === "win32") backendPath += ".exe";
        }
    } else {
        // Production: primarily use Release
        backendPath = path.join(appBasePath, "Release", "lansend");
        if (process.platform === "win32") backendPath += ".exe";
        // Fallback to Debug only if Release absolutely not found (should not happen in a proper build)
        if (!fs.existsSync(backendPath)) {
            console.warn(
                `Production: Release build not found at ${backendPath}. Attempting Debug build.`
            );
            backendPath = path.join(appBasePath, "Debug", "lansend");
            if (process.platform === "win32") backendPath += ".exe";
        }
    }

    if (process.platform === "win32" && !backendPath.endsWith(".exe")) {
        if (fs.existsSync(backendPath + ".exe")) {
            backendPath += ".exe";
        } else {
            // If we are here, neither backendPath nor backendPath + '.exe' exists.
            // The fs.existsSync(backendPath) check below will handle the error reporting.
        }
    }

    console.log(`Attempting to start backend process from: ${backendPath}`);
    if (!fs.existsSync(backendPath)) {
        console.error(`Backend executable not found: ${backendPath}`);
        isPipeConnected = false;
        isBackendLogicallyReady = false;
        updateAndReportOverallBackendStatus({
            message: `后端可执行文件不存在: ${backendPath}`,
        });
        return null;
    }

    let individualStdinConnected = false;
    let individualStdoutConnected = false;
    let connectionAttemptReported = false;

    const updatePipeConnectionStatus = (specificErrorDetails = null) => {
        const newPipeStatus =
            individualStdinConnected && individualStdoutConnected;
        if (
            newPipeStatus !== isPipeConnected ||
            (!newPipeStatus &&
                specificErrorDetails &&
                !connectionAttemptReported)
        ) {
            if (!newPipeStatus && specificErrorDetails)
                connectionAttemptReported = true;
            else if (newPipeStatus) connectionAttemptReported = true;

            console.log(
                `Pipe overall connection status changed to: ${newPipeStatus}`
            );
            isPipeConnected = newPipeStatus;
            updateAndReportOverallBackendStatus(
                isPipeConnected ? null : specificErrorDetails
            );
        }
    };

    const backendArgs = [
        "--stdin-pipe-name",
        stdinPipeName,
        "--stdout-pipe-name",
        stdoutPipeName,
    ];
    if (isDev) {
        backendArgs.push("--dev");
    }

    const newBackendProcess = spawn(backendPath, backendArgs, {
        stdio: ["ignore", "pipe", "pipe"],
    });
    backendProcess = newBackendProcess; // Assign to global immediately after spawn

    if (backendProcess && backendProcess.stdout) {
        backendProcess.stdout.on("data", (data) => {
            console.log(`BACKEND_PROCESS_STDOUT: ${data.toString().trim()}`);
        });
    } else {
        console.error("Failed to attach stdout listener to backend process.");
    }

    if (backendProcess && backendProcess.stderr) {
        backendProcess.stderr.on("data", (data) => {
            console.error(`BACKEND_PROCESS_STDERR: ${data.toString().trim()}`);
        });
    } else {
        console.error("Failed to attach stderr listener to backend process.");
    }

    backendProcess.on("error", (err) => {
        console.error("Failed to start backend process:", err);
        individualStdinConnected = false;
        individualStdoutConnected = false;
        updatePipeConnectionStatus({
            message: `启动后端进程失败: ${err.message}`,
        });
        backendProcess = null;
    });

    backendProcess.on("exit", (code, signal) => {
        console.log(
            `Backend process exited with code ${code} and signal ${signal}`
        );
        if (backendStdinSocket) backendStdinSocket.destroy();
        if (backendStdoutSocket) backendStdoutSocket.destroy();
        individualStdinConnected = false;
        individualStdoutConnected = false;
        isBackendLogicallyReady = false;
        updatePipeConnectionStatus({
            message: `后端进程已退出 (code ${code}, signal ${signal})`,
        });
        backendProcess = null;
    });

    // Setup STDIN pipe server
    const stdinServer = createPipeServer(
        stdinPipeName,
        (socket) => {
            console.log("Backend client connected to STDIN pipe server.");
            backendStdinSocket = socket;
            individualStdinConnected = true;
            connectionAttemptReported = false;
            updatePipeConnectionStatus();

            socket.on("error", (err) => {
                console.error("STDIN pipe socket error:", err);
                individualStdinConnected = false;
                updatePipeConnectionStatus({
                    message: `STDIN pipe socket error: ${err.message}`,
                });
                backendStdinSocket = null;
            });
            socket.on("close", () => {
                console.log("STDIN pipe socket closed by backend.");
                individualStdinConnected = false;
                updatePipeConnectionStatus({ message: "STDIN pipe socket closed by backend." });
                backendStdinSocket = null;
            });
        },
        (err) => {
            console.error("STDIN pipe server error:", err);
            individualStdinConnected = false;
            updatePipeConnectionStatus({
                message: `STDIN pipe server error: ${err.message}`,
            });
        }
    );

    // Setup STDOUT pipe server
    const stdoutServer = createPipeServer(
        stdoutPipeName,
        (socket) => {
            console.log("Backend process connected to STDOUT pipe.");
            backendStdoutSocket = socket;
            individualStdoutConnected = true;
            connectionAttemptReported = false;
            updatePipeConnectionStatus();

            socket.on("data", (data) => processReceivedDataInternal(data));
            socket.on("error", (err) => {
                console.error("STDOUT pipe socket error:", err);
                individualStdoutConnected = false;
                updatePipeConnectionStatus({
                    message: `STDOUT pipe socket error: ${err.message}`,
                });
                backendStdoutSocket = null;
            });
            socket.on("close", () => {
                console.log("STDOUT pipe socket closed by backend.");
                individualStdoutConnected = false;
                updatePipeConnectionStatus({ message: "STDOUT pipe socket closed by backend." });
                backendStdoutSocket = null;
            });
        },
        (err) => {
            console.error("STDOUT pipe server error:", err);
            individualStdoutConnected = false;
            updatePipeConnectionStatus({
                message: `STDOUT pipe server error: ${err.message}`,
            });
        }
    );

    const cleanupPipes = () => {
        if (stdinServer && stdinServer.listening)
            stdinServer.close(() => console.log("STDIN pipe server closed."));
        if (stdoutServer && stdoutServer.listening)
            stdoutServer.close(() => console.log("STDOUT pipe server closed."));
        if (backendStdinSocket) backendStdinSocket.destroy();
        if (backendStdoutSocket) backendStdoutSocket.destroy();
    };
    backendProcess.on("exit", cleanupPipes);
    app.on("before-quit", cleanupPipes);

    return backendProcess;
}

let receivedDataBuffer = Buffer.alloc(0);
function processReceivedDataInternal(newDataChunk) {
    receivedDataBuffer = Buffer.concat([receivedDataBuffer, newDataChunk]);
    while (receivedDataBuffer.length >= 4) {
        const messageLength = receivedDataBuffer.readUInt32BE(0);
        if (receivedDataBuffer.length >= 4 + messageLength) {
            const messageJson = receivedDataBuffer
                .slice(4, 4 + messageLength)
                .toString("utf8");
            receivedDataBuffer = receivedDataBuffer.slice(4 + messageLength);
            try {
                const messageObj = JSON.parse(messageJson);
                console.log("Received from backend:", messageObj);

                if (messageObj.feedback === "backend_started") {
                    console.log(
                        "Backend has started and sent backend_started signal."
                    );
                    if (!isBackendLogicallyReady) {
                        isBackendLogicallyReady = true;
                        updateAndReportOverallBackendStatus();
                    }
                } else if (messageObj.feedback === "log_message") {
                    // console.log(`[Backend Log - ${messageObj.level}]: ${messageObj.payload}`);
                } else if (messageObj.data.msgId && messageQueue[messageObj.data.msgId]) {
                    messageQueue[messageObj.data.msgId].resolve(messageObj);
                    delete messageQueue[messageObj.data.msgId];
                } else {
                    if (
                        mainWindow &&
                        !mainWindow.isDestroyed() &&
                        mainWindow.webContents &&
                        !mainWindow.webContents.isDestroyed()
                    ) {
                        mainWindow.webContents.send(
                            "backend-message",
                            messageObj
                        );
                    }
                }
            } catch (e) {
                console.error(
                    "Error parsing JSON from backend:",
                    e,
                    "Raw JSON:",
                    messageJson
                );
            }
        } else {
            break;
        }
    }
}

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1024,
        height: 768,
        minWidth: 800,
        minHeight: 600,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: true,
            preload: path.join(__dirname, "preload.js"),
            devTools: process.argv.includes("--dev"),
        },
        icon: path.join(__dirname, "assets", "icon.png"),
    });

    ipcMain.on("renderer-mounted-ready", () => {
        console.log("Main process received 'renderer-mounted-ready'.");
        if (!isRendererReady) {
            isRendererReady = true;
            updateAndReportOverallBackendStatus();
        }
    });

    mainWindow.loadFile(path.join(__dirname, "index.html"));
    mainWindow.on("closed", () => {
        mainWindow = null;
    });
}

app.whenReady().then(() => {
    createWindow();
    if (!backendProcess) {
        backendProcess = startBackendProcess();
    }

    app.on("activate", () => {
        if (BrowserWindow.getAllWindows().length === 0) {
            createWindow();
        }
    });
});

app.on("window-all-closed", () => {
    if (process.platform !== "darwin") {
        app.quit();
    }
});

app.on("before-quit", () => {
    app.isQuitting = true;
    isPipeConnected = false;
    isBackendLogicallyReady = false;
    isRendererReady = false; // Explicitly set all to false
    updateAndReportOverallBackendStatus({ message: "Application is quitting" });

    if (backendProcess && !backendProcess.killed) {
        console.log("Attempting to kill backend process on quit.");
        backendProcess.kill();
    }
});

async function sendToBackend(message) {
    if (
        !backendProcess ||
        !backendStdinSocket ||
        backendStdinSocket.destroyed
    ) {
        console.error(
            "Cannot send to backend: Backend process or stdin pipe not available."
        );
        return Promise.reject(new Error("Backend not connected for sending."));
    }

    const id = messageId++;
    message.data.msgId = id;

    const messageData = JSON.stringify(message);
    const messageLength = Buffer.byteLength(messageData);
    const buffer = Buffer.alloc(4 + messageLength);
    buffer.writeUInt32BE(messageLength, 0);
    buffer.write(messageData, 4);

    return new Promise((resolve, reject) => {
        const timeoutId = setTimeout(() => {
            delete messageQueue[id];
            reject(
                new Error(
                    `Backend response timeout for message type ${message.type}`
                )
            );
        }, 30000);

        messageQueue[id] = {
            resolve: (response) => {
                clearTimeout(timeoutId);
                resolve(response);
            },
            reject: (error) => {
                clearTimeout(timeoutId);
                reject(error);
            },
        };

        if (!backendStdinSocket.write(buffer)) {
            backendStdinSocket.once("drain", () =>
                backendStdinSocket.write(buffer)
            );
        }
    });
}

ipcMain.handle("send-to-backend", async (event, message) => {
    try {
        const response = await sendToBackend(message);
        return { success: true, data: response.data, type: response.type };
    } catch (error) {
        console.error("Error in ipcMain.handle send-to-backend:", error);
        return { success: false, error: error.message };
    }
});

ipcMain.handle("open-file-dialog", async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog({
        properties: ["openFile"],
    });
    if (!canceled && filePaths.length > 0) return filePaths[0];
    return null;
});

ipcMain.handle("open-files-dialog", async () => {
    const { canceled, filePaths } = await dialog.showOpenDialog({
        properties: ["openFile", "multiSelections"],
    });
    if (!canceled && filePaths.length > 0) return filePaths;
    return [];
});
