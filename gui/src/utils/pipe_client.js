const PipeClient = {
    methods: {
        formatMessage(type, data = {}) {
            return {
                type,
                ...data,
                timestamp: new Date().toISOString(),
            };
        },

        // 发送消息到后端（通过管道通信）
        async sendToBackend(type, data = {}) {
            try {
                const message = this.formatMessage(type, data);
                const result = await window.electronAPI.sendToBackend(message);

                if (!result.success) {
                    console.error("Failed to send message:", result.error);
                    throw new Error(
                        result.error || "Failed to communicate with backend"
                    );
                }

                return result.data;
            } catch (error) {
                console.error("Failed to send message to backend:", error);
                throw error;
            }
        },

        // 发送文件到设备
        send_files_to_devices(deviceIds, filePaths) {
            return this.sendToBackend("send_request", {
                target_devices: deviceIds,
                files: filePaths,
            });
        },

        // 取消等待确认
        cancelWaitForConfirmation(transferId) {
            return this.sendToBackend("cancel_wait_for_confirmation", {
                transfer_id: transferId,
            });
        },

        // 取消传输
        cancelTransfer(transferId) {
            return this.sendToBackend("cancel_send", {
                transfer_id: transferId,
            });
        },

        //回应传输请求
        respondToReceiveRequest(transferId, accept) {
            return this.sendToBackend("respond_to_receive_request", {
                transfer_id: transferId,
                accept,
            });
        },

        // 取消接收
        cancelReceive(transferId) {
            return this.sendToBackend("cancel_receive", {
                transfer_id: transferId,
            });
        },

        // 更新设置
        updateSettings(settings) {
            return this.sendToBackend("update_settings", {
                settings,
            });
        },

        // 连接设备
        connectToDevice(deviceId) {
            return this.sendToBackend("connect_to_device", {
                device_id: deviceId,
            });
        },

        //退出
        exit() {
            return this.sendToBackend("exit");
        },
    },
};

// 导出用于混入到Vue组件中
if (typeof module !== "undefined" && module.exports) {
    module.exports = PipeClient;
}
