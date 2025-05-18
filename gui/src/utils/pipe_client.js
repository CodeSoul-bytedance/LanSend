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

        // 扫描局域网设备
        scanDevices() {
            return this.sendToBackend("scan_devices");
        },

        // 发送文件到设备
        sendFilesToDevice(deviceId, filePaths) {
            return this.sendToBackend("send_request", {
                target_device: deviceId,
                files: filePaths,
            });
        },

        // 接受传输请求
        acceptTransfer(transferId) {
            return this.sendToBackend("accept_transfer", {
                transfer_id: transferId,
            });
        },

        // 拒绝传输请求
        rejectTransfer(transferId) {
            return this.sendToBackend("reject_transfer", {
                transfer_id: transferId,
            });
        },

        // 取消传输
        cancelTransfer(transferId) {
            return this.sendToBackend("cancel_transfer", {
                transfer_id: transferId,
            });
        },

        // 获取传输状态
        getTransferStatus(transferId) {
            return this.sendToBackend("get_transfer_status", {
                transfer_id: transferId,
            });
        },

        // 获取活跃传输列表
        getActiveTransfers() {
            return this.sendToBackend("get_active_transfers");
        },

        // 打开文件所在位置 (依赖后端实现 'open_file_location' 消息类型)
        openFileLocation(transferId) {
            return this.sendToBackend("open_file_location", {
                transfer_id: transferId,
            });
        },

        // 删除传输记录 (依赖后端实现 'delete_transfer_record' 消息类型)
        deleteTransferRecord(transferId) {
            return this.sendToBackend("delete_transfer_record", {
                transfer_id: transferId,
            });
        },

        // 更新设置
        updateSettings(settings) {
            return this.sendToBackend("update_settings", {
                settings,
            });
        },
    },
};

// 导出用于混入到Vue组件中
if (typeof module !== "undefined" && module.exports) {
    module.exports = PipeClient;
}
