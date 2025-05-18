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
        async sendFilesToDevice(deviceInfo) {
            try {
                const filePaths = await window.electronAPI.openFilesDialog();
                if (!filePaths || filePaths.length === 0) return;
                const result = await PipeClient.methods.sendFilesToDevice(
                    deviceInfo.device_id,
                    filePaths
                );
            } catch (error) {
                this.showError(error.message || "发送文件请求失败");
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

            switch (message.type) {
                case "device_discovered": // 保留对单个设备发现的兼容（如果后端将来可能发送这种格式）
                    console.log(
                        "Vue app.js: Handling single device_discovered:",
                        JSON.stringify(message.device)
                    );
                    this.updateDeviceList(message.device);
                    break;

                case "scan_devices_response": // 新增对批量设备响应的处理
                    console.log(
                        "Vue app.js: Handling scan_devices_response:",
                        JSON.stringify(message.data.devices)
                    );
                    if (message.data && Array.isArray(message.data.devices)) {
                        message.data.devices.forEach((device) => {
                            this.updateDeviceList(device);
                        });
                    } else {
                        console.warn(
                            "Vue app.js: scan_devices_response received, but message.data.devices is not an array or is missing."
                        );
                    }
                    break;

                case "device_lost":
                    // 移除设备
                    this.removeDevice(message.device_id);
                    break;

                case "transfer_request":
                    // 收到传输请求
                    this.handleTransferRequest(message);
                    break;

                case "transfer_update":
                    // 传输状态更新
                    this.updateTransferStatus(message);
                    break;

                default:
                    console.log("未处理的消息类型:", message.type);
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
            const index = this.devices.findIndex(
                (d) => d.device_id === deviceId
            );
            if (index >= 0) {
                this.devices.splice(index, 1);
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
            if (index >= 0) {
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
                    message.status === "failed"
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
            @send-files="sendFilesToDevice"
            @accept-transfer="acceptTransfer"
            @reject-transfer="rejectTransfer">
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
