// 注册Vue组件
const app = Vue.createApp({
    data() {
        return {
            currentView: "home", // 默认视图
            backendConnected: false, // 后端连接状态
            devices: [], // 发现的设备列表
            transfers: [], // 传输任务列表
            settings: {
                // 设置
                deviceName: "",
                savePath: "",
                darkMode: false,
            },
        };
    },

    computed: {
        // 是否有活跃的传输任务
        hasActiveTransfers() {
            return this.transfers.some(
                (t) => t.status === "pending" || t.status === "in_progress"
            );
        },
    },

    methods: {
        // 切换视图
        changeView(view) {
            this.currentView = view;
        },

        // 发送文件到设备
        async send_files_to_devices(devices, filePaths) {
            console.log("app.js: send_files_to_devices");
            console.log("devices (original):", devices); // 打印原始 devices
            
            // --- 详细检查 devices ---
            if (devices && devices.length > 0) {
                devices.forEach((device, index) => {
                    console.log(`Device ${index}:`, device);
                    // 尝试序列化单个设备对象，看是否会提前报错
                    try {
                        const clonedDevice = JSON.parse(JSON.stringify(device)); // 简单的克隆测试
                        console.log(`Device ${index} (cloned via JSON):`, clonedDevice);
                    } catch (e) {
                        console.error(`Device ${index} cannot be JSON cloned:`, e);
                        // 打印 device 的所有 key，并检查是否有函数
                        for (const key in device) {
                            if (Object.prototype.hasOwnProperty.call(device, key)) {
                                console.log(`  Device ${index} key: ${key}, type: ${typeof device[key]}`);
                                if (typeof device[key] === 'function') {
                                    console.warn(`  Device ${index} has a function property: ${key}`);
                                }
                            }
                        }
                    }
                });
            }
            // --- 详细检查结束 ---

            console.log("filePaths:", filePaths);
            try {
                if (!filePaths || filePaths.length === 0) return;

                // --- 尝试发送简化的设备信息 ---
                const simplifiedDevices = devices.map(d => ({
                    device_id: d.device_id, // 假设设备对象有 device_id
                    name: d.name || d.alias, // 使用 d.alias 作为 d.name 的备用值
                    alias: d.alias,
                    ip: d.ip,
                    port: d.port,
                    device_model: d.device_model,
                    device_type: d.device_type,
                    connected: d.connected // 保留连接状态，后端可能需要
                    // 只包含必要的、已知可序列化的属性
                }));
                console.log("Simplified devices for IPC:", simplifiedDevices);

                const result = await PipeClient.methods.send_files_to_devices(
                    simplifiedDevices, // <--- 使用简化的设备列表
                    filePaths
                );
                // --- 结束尝试 ---

                // const result = await PipeClient.methods.send_files_to_devices(
                //     devices,
                //     filePaths
                // );
            } catch (error) {
                console.error("app.js - Error in send_files_to_devices:", error); // 打印详细错误
                this.showError(error.message || "发送文件请求失败");
            }
        },

        async respond_to_receive(transferId, accept) {
            try {
                await PipeClient.methods.respond_to_receive(transferId, accept);
            } catch (error) {
                this.showError(error.message || "响应传输请求失败");
            }
        },

        // 接受传输请求
        async acceptTransfer(transferId) {
            try {
                // 改为调用 PipeClient.methods
                await PipeClient.methods.acceptTransfer(transferId);
                // 同上，假设 PipeClient 方法的错误处理机制
            } catch (error) {
                this.showError(error.message || "接受传输失败");
            }
        },

        // 拒绝传输请求
        async rejectTransfer(transferId) {
            try {
                // 改为调用 PipeClient.methods
                await PipeClient.methods.rejectTransfer(transferId);
                // 同上，假设 PipeClient 方法的错误处理机制
            } catch (error) {
                this.showError(error.message || "拒绝传输失败");
            }
        },

        // 取消发送传输
        async cancel_send(transferId) {
            try {
                await PipeClient.methods.cancelTransfer(transferId);
                // 更新传输状态为已取消
                const index = this.transfers.findIndex(
                    (t) => t.id === transferId
                );
                if (index !== -1) {
                    const transfer = { ...this.transfers[index] };
                    transfer.status = "canceled";
                    transfer.endTime = new Date().toISOString();
                    this.transfers.splice(index, 1, transfer);
                }
            } catch (error) {
                this.showError(error.message || "取消传输失败");
            }
        },

        //连接设备
        async connect_to_device(deviceId, pinCode) {
            try {
                console.log("尝试连接设备，设备ID:", deviceId);
                // 确保传递的是字符串类型的设备ID
                if (typeof deviceId === "object" && deviceId.device_id) {
                    deviceId = deviceId.device_id;
                }

                if (!deviceId || typeof deviceId !== "string") {
                    throw new Error("无效的设备ID");
                }

                await PipeClient.methods.connectToDevice(deviceId, pinCode);

                const deviceToUpdate = this.devices.find(
                    (d) => d.device_id === deviceId
                );
                if (deviceToUpdate) {
                    deviceToUpdate.connected = true;
                    console.log(
                        `App.js: 设备 ${deviceId} 已在状态中标记为 connected.`
                    );
                } else {
                    console.warn(
                        `App.js: 设备 ${deviceId} 连接成功，但在本地设备列表中未找到，无法更新UI状态。`
                    );
                }
            } catch (error) {
                console.error("连接设备失败:", error);
                this.showError(error.message || "连接设备失败");
            }
        },

        // 显示错误信息
        showError(message) {
            // 实际应用中可以使用Vuetify的snackbar组件
            console.error(message);
            alert(message);
        },

        // 处理从后端收到的消息
        handleBackendMessage(message) {
            console.log(
                "Vue app.js: Received backend message in handleBackendMessage:",
                JSON.stringify(message)
            );

            switch (message.feedback) {
                case "Error":
                    this.showError(message.error || "未知错误");
                    break;
                case "FoundDevice":
                    console.log("处理发现设备通知:", message);
                    // 设备信息在message.data数组中
                    if (
                        message.data
                    ) {
                        this.updateDeviceList({
                            alias: message.data.device_info.alias,
                            device_id: message.data.device_info.device_id,
                            ip: message.data.device_info.ip,
                            port: message.data.device_info.port,
                        });
                        console.log("添加设备:", message.data);
                    } else {
                        console.warn(
                            "收到FoundDevice通知，但没有设备数据或格式不正确",
                            message
                        );
                    }
                    break;
                case "LostDevice":
                    console.log("处理设备离线通知:", message);
                    // 设备ID在message.data.device_id中
                    if (message.data && message.data.device_id) {
                        console.log("移除设备:", message.data.device_id);
                        this.removeDevice(message.data.device_id);
                    } else {
                        console.warn(
                            "收到LostDevice通知，但没有设备ID或格式不正确",
                            message
                        );
                    }
                    break;
                case "Settings":
                    this.settings = { ...this.settings, ...message.settings };
                    break;
                case "ConnectDeviceResult":
                    // 更新设备连接状态
                    const connectedDevice = this.devices.find(
                        (d) => d.device_id === message.device_id
                    );
                    if (connectedDevice) {
                        connectedDevice.connected = true;
                        this.updateDeviceList(connectedDevice);
                    }
                    break;
                case "RecipientAccepted":
                    // 更新传输状态为已接受
                    const transferIdx = this.transfers.findIndex(
                        (t) =>
                            t.targetDevice &&
                            t.targetDevice.device_id === message.device_id
                    );
                    if (transferIdx !== -1) {
                        const transfer = this.transfers[transferIdx];
                        this.updateTransferStatus({
                            transfer_id: transfer.id,
                            status: "accepted",
                            acceptedFiles: message.files || [],
                        });
                    }
                    break;
                case "RecipientDeclined":
                    // 更新传输状态为已拒绝
                    const declinedTransferIdx = this.transfers.findIndex(
                        (t) =>
                            t.targetDevice &&
                            t.targetDevice.device_id === message.device_id
                    );
                    if (declinedTransferIdx !== -1) {
                        const transfer = this.transfers[declinedTransferIdx];
                        this.updateTransferStatus({
                            transfer_id: transfer.id,
                            status: "declined",
                        });
                    }
                    break;
                case "FileSendingProgress":
                    // 找到对应的传输任务
                    const sendingTransfer = this.transfers.find(
                        (t) =>
                            t.targetDevice &&
                            t.targetDevice.device_id === message.device_id
                    );
                    if (sendingTransfer) {
                        // 更新文件进度
                        const fileIdx = sendingTransfer.files.findIndex(
                            (f) => f.name === message.file_name
                        );
                        if (fileIdx !== -1) {
                            sendingTransfer.files[fileIdx].progress =
                                message.progress;
                        }

                        // 计算传输的总大小
                        const transferredSize = sendingTransfer.files.reduce(
                            (sum, file) =>
                                sum +
                                ((file.progress || 0) / 100) * (file.size || 0),
                            0
                        );

                        // 使用辅助函数更新状态
                        this.updateTransferStatus({
                            transfer_id: sendingTransfer.id,
                            status: "in_progress",
                            transferred_size: transferredSize,
                        });
                    }
                    break;
                case "FileSendingCompleted":
                    // 找到对应的传输任务
                    const completedSendingTransfer = this.transfers.find(
                        (t) =>
                            t.targetDevice &&
                            t.targetDevice.device_id === message.device_id
                    );
                    if (completedSendingTransfer) {
                        // 更新文件状态
                        const fileIdx =
                            completedSendingTransfer.files.findIndex(
                                (f) => f.name === message.file_name
                            );
                        if (fileIdx !== -1) {
                            completedSendingTransfer.files[
                                fileIdx
                            ].completed = true;
                            completedSendingTransfer.files[
                                fileIdx
                            ].progress = 100;
                        }

                        // 检查所有文件是否都已完成
                        const allCompleted =
                            completedSendingTransfer.files.every(
                                (f) => f.completed
                            );
                        if (allCompleted) {
                            this.updateTransferStatus({
                                transfer_id: completedSendingTransfer.id,
                                status: "completed",
                                transferred_size:
                                    completedSendingTransfer.totalSize,
                            });
                        } else {
                            // 计算已传输的总大小
                            const transferredSize =
                                completedSendingTransfer.files.reduce(
                                    (sum, file) =>
                                        sum +
                                        ((file.progress || 0) / 100) *
                                            (file.size || 0),
                                    0
                                );

                            this.updateTransferStatus({
                                transfer_id: completedSendingTransfer.id,
                                status: "in_progress",
                                transferred_size: transferredSize,
                            });
                        }
                    }
                    break;
                case "SendSessionEnded":
                    // 找到对应的传输任务
                    const allCompletedTransfer = this.transfers.find(
                        (t) =>
                            t.targetDevice &&
                            t.targetDevice.device_id === message.device_id
                    );
                    if (allCompletedTransfer) {
                        // 将所有文件标记为已完成
                        allCompletedTransfer.files.forEach((file) => {
                            file.completed = true;
                            file.progress = 100;
                        });

                        this.updateTransferStatus({
                            transfer_id: allCompletedTransfer.id,
                            status: "completed",
                            transferred_size: allCompletedTransfer.totalSize,
                        });
                    }
                    else{
                        console.error("收到SendSessionEnded通知，但没有找到对应的传输任务", message);
                    }
                    break;
                case "RequestReceiveFiles":
                    // 处理接收文件请求
                    this.handleTransferRequest(message);
                    // 切换到主页以显示传输请求
                    this.changeView("home");
                    break;
                case "FileReceivingProgress":
                    // 更新文件接收进度
                    const receivingTransfer = this.transfers.find(
                        (t) =>
                            t.id === message.transfer_id ||
                            (t.files.some(
                                (f) => f.name === message.file_name
                            ) &&
                                t.status === "in_progress")
                    );

                    if (receivingTransfer) {
                        // 更新文件进度
                        const fileIdx = receivingTransfer.files.findIndex(
                            (f) => f.name === message.file_name
                        );
                        if (fileIdx !== -1) {
                            receivingTransfer.files[fileIdx].progress =
                                message.progress;
                        }

                        // 计算传输的总大小
                        const transferredSize = receivingTransfer.files.reduce(
                            (sum, file) =>
                                sum +
                                ((file.progress || 0) / 100) * (file.size || 0),
                            0
                        );

                        this.updateTransferStatus({
                            transfer_id: receivingTransfer.id,
                            status: "in_progress",
                            transferred_size: transferredSize,
                        });
                    }
                    break;
                case "FileReceivingCompleted":
                    // 更新单个文件接收完成状态
                    const completedReceivingTransfer = this.transfers.find(
                        (t) =>
                            t.id === message.transfer_id ||
                            (t.files.some(
                                (f) => f.name === message.file_name
                            ) &&
                                t.status === "in_progress")
                    );

                    if (completedReceivingTransfer) {
                        // 更新文件状态
                        const fileIdx =
                            completedReceivingTransfer.files.findIndex(
                                (f) => f.name === message.file_name
                            );
                        if (fileIdx !== -1) {
                            completedReceivingTransfer.files[
                                fileIdx
                            ].completed = true;
                            completedReceivingTransfer.files[
                                fileIdx
                            ].progress = 100;
                        }

                        // 检查所有文件是否都已完成
                        const allCompleted =
                            completedReceivingTransfer.files.every(
                                (f) => f.completed
                            );
                        if (allCompleted) {
                            this.updateTransferStatus({
                                transfer_id: completedReceivingTransfer.id,
                                status: "completed",
                                transferred_size:
                                    completedReceivingTransfer.totalSize,
                            });
                        } else {
                            // 计算已传输的总大小
                            const transferredSize =
                                completedReceivingTransfer.files.reduce(
                                    (sum, file) =>
                                        sum +
                                        ((file.progress || 0) / 100) *
                                            (file.size || 0),
                                    0
                                );

                            this.updateTransferStatus({
                                transfer_id: completedReceivingTransfer.id,
                                status: "in_progress",
                                transferred_size: transferredSize,
                            });
                        }
                    }
                    break;
                case "ReceiveSessionEnded":
                    // 更新所有文件接收完成状态
                    // 找到当前正在接收的传输
                    const allReceivingCompletedTransfer = this.transfers.find(
                        (t) => t.status === "in_progress" && t.sourceDevice
                    );

                    if (allReceivingCompletedTransfer) {
                        // 将所有文件标记为已完成
                        allReceivingCompletedTransfer.files.forEach((file) => {
                            file.completed = true;
                            file.progress = 100;
                        });

                        this.updateTransferStatus({
                            transfer_id: allReceivingCompletedTransfer.id,
                            status: "completed",
                            transferred_size:
                                allReceivingCompletedTransfer.totalSize,
                        });
                    }
                    else{
                        console.error("收到ReceiveSessionEnded通知，但没有找到对应的传输任务", message);
                    }
                    break;
                case "SendingCancelledByReceiver":
                    // 处理接收者取消发送
                    const cancelledByReceiverTransfer = this.transfers.find(
                        (t) =>
                            t.targetDevice &&
                            t.targetDevice.device_id === message.device_id
                    );

                    if (cancelledByReceiverTransfer) {
                        this.updateTransferStatus({
                            transfer_id: cancelledByReceiverTransfer.id,
                            status: "canceled",
                            error: "receiver cancelled",
                        });
                    }
                    break;
                case "ReceivingCancelledBySender":
                    // 处理发送者取消接收
                    // 找到当前正在接收的传输
                    const cancelledBySenderTransfer = this.transfers.find(
                        (t) => t.status === "in_progress" && t.sourceDevice
                    );

                    if (cancelledBySenderTransfer) {
                        this.updateTransferStatus({
                            transfer_id: cancelledBySenderTransfer.id,
                            status: "canceled",
                            error: "sender cancelled",
                        });
                    }
                    break;
                default:
                    console.log("Unknown message type:", message.feedback);
            }
        },

        // 更新设备列表
        updateDeviceList(device) {
            console.log(
                "Vue app.js: updateDeviceList called with device:",
                JSON.stringify(device)
            );
            console.log(
                "Vue app.js: this.devices BEFORE update:",
                JSON.stringify(this.devices)
            );
            const index = this.devices.findIndex(
                (d) => d.device_id === device.device_id
            );
            if (index >= 0) {
                console.log(
                    `Vue app.js: Updating existing device at index ${index}`
                );
                this.devices.splice(index, 1, device);
            } else {
                console.log("Vue app.js: Adding new device");
                this.devices.push(device);
            }
            console.log(
                "Vue app.js: this.devices AFTER update:",
                JSON.stringify(this.devices)
            );
        },

        // 移除设备
        removeDevice(deviceId) {
            console.log("执行removeDevice，设备ID:", deviceId);
            console.log("当前设备列表:", JSON.stringify(this.devices));

            const index = this.devices.findIndex(
                (d) => d.device_id === deviceId
            );

            console.log("找到设备索引:", index);

            // 注意：findIndex如果没有找到元素会返回-1，所以我们需要检查index >= 0
            if (index >= 0) {
                console.log("移除设备索引", index);
                this.devices.splice(index, 1);
                console.log("移除后的设备列表:", JSON.stringify(this.devices));
            } else {
                console.warn("要移除的设备未找到:", deviceId);
            }
        },

        // 处理传输请求
        handleTransferRequest(message) {
            this.transfers.push({
                id: message.transfer_id,
                sourceDevice: message.source_device,
                targetDevice: message.target_device,
                files: message.files,
                totalSize: message.total_size,
                status: "pending",
                progress: 0,
                startTime: new Date().toISOString(),
            });
        },

        // 更新传输状态
        updateTransferStatus(message) {
            const index = this.transfers.findIndex(
                (t) => t.id === message.transfer_id
            );
            if (index !== -1) {
                const transfer = this.transfers[index];

                // 更新传输信息
                transfer.status = message.status;
                if (message.transferred_size !== undefined) {
                    transfer.progress =
                        (message.transferred_size / transfer.totalSize) * 100;
                }
                if (message.error) {
                    transfer.error = message.error;
                }

                // 如果传输已完成，添加结束时间
                if (
                    message.status === "completed" ||
                    message.status === "failed" ||
                    message.status === "canceled"
                ) {
                    transfer.endTime = new Date().toISOString();
                }

                // 更新数组，触发视图更新
                this.transfers.splice(index, 1, transfer);
            }
        },
    },

    mounted() {
        console.log("app.js: mounted() hook executed.");

        if (
            window.electronAPI &&
            typeof window.electronAPI.onBackendConnected === "function"
        ) {
            window.electronAPI.onBackendConnected((status) => {
                console.log(
                    "app.js: backend-connected event received in renderer. Status from main:",
                    status
                );
                this.backendConnected = status;
                console.log(
                    "app.js: this.backendConnected in renderer is now:",
                    this.backendConnected
                );
            });
        } else {
            console.error(
                "app.js: window.electronAPI.onBackendConnected is not available. Cannot listen for backend status."
            );
        }

        // 监听来自后端的消息
        window.electronAPI.onBackendMessage((message) => {
            console.log("app.js: backend-message event received:", message);
            this.handleBackendMessage(message);
        });

        // 通知主进程，渲染器已准备好接收消息 (Vue app mounted)
        window.electronAPI.rendererReady();
        console.log(
            'app.js: Sent "renderer-mounted-ready" (via electronAPI.rendererReady) to main process.'
        );
    },

    template: `
    <v-app :theme="settings.darkMode ? 'dark' : 'light'">
      <v-navigation-drawer permanent>
        <v-list>
          <v-list-item @click="changeView('home')" :active="currentView === 'home'">
            <template v-slot:prepend>
              <v-icon>mdi-home</v-icon>
            </template>
            <v-list-item-title>主页</v-list-item-title>
          </v-list-item>
          
          <v-list-item @click="changeView('history')" :active="currentView === 'history'">
            <template v-slot:prepend>
              <v-icon>mdi-history</v-icon>
            </template>
            <v-list-item-title>历史记录</v-list-item-title>
          </v-list-item>
          
          <v-list-item @click="changeView('settings')" :active="currentView === 'settings'">
            <template v-slot:prepend>
              <v-icon>mdi-cog</v-icon>
            </template>
            <v-list-item-title>设置</v-list-item-title>
          </v-list-item>
        </v-list>
        
        <template v-slot:append>
          <v-list>
            <v-list-item>
              <div class="d-flex align-center">
                <v-icon :color="backendConnected ? 'success' : 'error'" class="mr-2">
                  {{ backendConnected ? 'mdi-check-circle' : 'mdi-alert-circle' }}
                </v-icon>
                <span>{{ backendConnected ? '已连接到后端' : '未连接到后端' }}</span>
              </div>
            </v-list-item>
          </v-list>
        </template>
      </v-navigation-drawer>
      
      <v-main>
        <v-container fluid>
          <component 
            :is="currentView + '-view'" 
            :devices="devices"
            :transfers="transfers"
            :settings="settings"
            @send_request="send_files_to_devices"
            @respond_to_receive_request="respond_to_receive"
            @connect_to_device="connect_to_device"
            @cancel_send="cancel_send">
          </component>
        </v-container>
      </v-main>
    </v-app>
  `,
});

// 注册视图组件
app.component("home-view", HomeView);
app.component("settings-view", SettingsView);
app.component("history-view", HistoryView);

// 挂载应用
const vuetify = Vuetify.createVuetify();
app.use(vuetify);
app.mount("#app");
