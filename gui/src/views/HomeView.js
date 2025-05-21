// 主页视图组件
const HomeView = {
    props: {
        devices: Array,
        transfers: Array,
    },

    computed: {
        // 待处理的传输请求
        pendingTransfers() {
            return this.transfers.filter((t) => t.status === "pending");
        },

        // 活跃的传输任务
        activeTransfers() {
            return this.transfers.filter(
                (t) =>
                    t.status === "in_progress" ||
                    (t.status === "completed" &&
                        new Date() - new Date(t.endTime) < 60000)
            );
        },
    },

    components: {
        // 新增：注册 DeviceList 组件
        "device-list": DeviceList,
        "file-uploader": FileUploader,
        "transfer-item": TransferItem,
    },

    methods: {

        // 处理文件发送
        handleSendFiles(filePaths) {
          console.log("HomeView: handleSendFiles")
            // 获取所有已连接的设备
            const connectedDevices = this.devices.filter(device => device.connected);
            if (connectedDevices.length > 0) {
                this.$emit("send_request", connectedDevices, filePaths);
            } else {
                console.error("没有已连接的设备可用于发送文件");
                // 可以添加用户通知逻辑
            }
        },

        // 处理发送错误
        handleSendError(errorMessage) {
          console.log("HomeView: handleSendError")
            console.error(`发送错误: ${errorMessage}`);
            // 这里可以添加错误通知UI，例如使用 alert 或显示错误消息
            // 如果有通知系统，可以在这里触发
        },

        // 接受传输请求
        acceptTransfer(transferId) {
          console.log("HomeView: acceptTransfer")
            this.$emit("respond_to_receive_request", transferId, true);
        },

        // 拒绝传输请求
        rejectTransfer(transferId) {
          console.log("HomeView: rejectTransfer")
            this.$emit("respond_to_receive_request", transferId, false);
        },

        // 格式化文件大小
        formatFileSize(bytes) {
            if (bytes === 0) return "0 B";

            const sizes = ["B", "KB", "MB", "GB", "TB"];
            const i = Math.floor(Math.log(bytes) / Math.log(1024));

            return (
                parseFloat((bytes / Math.pow(1024, i)).toFixed(2)) +
                " " +
                sizes[i]
            );
        },

        //连接设备
        connectToDevice(deviceId, pinCode) {
          console.log("HomeView: connectToDevice")
            this.$emit("connect_to_device", deviceId, pinCode);
        },

        // 取消传输
        cancelTransfer(transferId) {
          console.log("HomeView: cancelTransfer")
            this.$emit("cancel_send", transferId);
        },
    },

    template: `
    <div>
      <h1 class="text-h4 mb-4">发现的设备</h1>
  
      <device-list 
        :devices="devices" 
        @connect_to_device="connectToDevice"
      ></device-list>
      
      <!-- 上传区域 -->
      <div class="mt-8">
        <h2 class="text-h5 mb-4">发送文件</h2>
        <file-uploader 
          :devices="devices"
          @send_files="handleSendFiles"
          @send_error="handleSendError"
        ></file-uploader>
      </div>

      <transfer-item
        v-for="transfer in transfers"
        :key="transfer.id"
        :transfer="transfer"
        @cancel-transfer="cancelTransfer"
        @close-transfer="closeTransfer"
      ></transfer-item>

      <!-- 传输请求 -->
      <div v-if="pendingTransfers.length > 0" class="mt-8">
        <h2 class="text-h5 mb-4">传输请求</h2>
        
        <v-card v-for="transfer in pendingTransfers" :key="transfer.id" class="mb-4">
          <v-card-title>来自 {{ transfer.sourceDevice }} 的传输请求</v-card-title>
          <v-card-text>
            <div>文件数量: {{ transfer.files.length }}</div>
            <div>总大小: {{ formatFileSize(transfer.totalSize) }}</div>
            <div class="mt-3">文件列表:</div>
            <v-list density="compact">
              <v-list-item v-for="(file, index) in transfer.files" :key="index">
                <v-list-item-title>{{ file.name }}</v-list-item-title>
                <v-list-item-subtitle>{{ formatFileSize(file.size) }}</v-list-item-subtitle>
              </v-list-item>
            </v-list>
          </v-card-text>
          <v-card-actions>
            <v-btn color="primary" @click="acceptTransfer(transfer.id)">接受</v-btn>
            <v-btn color="error" @click="rejectTransfer(transfer.id)">拒绝</v-btn>
          </v-card-actions>
        </v-card>
      </div>
      
      <!-- 活跃传输 -->
      <div v-if="activeTransfers.length > 0" class="mt-8">
        <h2 class="text-h5 mb-4">活跃传输</h2>
        
        <v-card v-for="transfer in activeTransfers" :key="transfer.id" class="mb-4">
          <v-card-title>
            <v-icon class="mr-2">
              {{ transfer.status === 'completed' ? 'mdi-check-circle' : 'mdi-progress-upload' }}
            </v-icon>
            {{ transfer.files[0].name }}
            <span v-if="transfer.files.length > 1">等 {{ transfer.files.length }} 个文件</span>
            <v-spacer></v-spacer>
            <v-btn
              v-if="transfer.status === 'pending' || transfer.status === 'in_progress'"
              icon
              small
              color="error"
              @click="cancelTransfer(transfer.id)"
              title="取消传输"
            >
              <v-icon>mdi-close-circle</v-icon>
            </v-btn>
          </v-card-title>
          <v-card-text>
            <div class="d-flex justify-space-between mb-2">
              <span>{{ formatFileSize(transfer.totalSize * transfer.progress / 100) }} / {{ formatFileSize(transfer.totalSize) }}</span>
              <span>{{ Math.round(transfer.progress) }}%</span>
            </div>
            <v-progress-linear 
              :model-value="transfer.progress" 
              :color="transfer.status === 'completed' ? 'success' : 'primary'"
              class="transfer-progress"
              :indeterminate="transfer.status === 'pending'"
              :striped="transfer.status === 'in_progress'">
            </v-progress-linear>
            
            <div v-if="transfer.status === 'completed'" class="text-success mt-2">
              传输已完成
            </div>
            <div v-if="transfer.status === 'failed'" class="text-error mt-2">
              传输失败: {{ transfer.error || '未知错误' }}
            </div>
          </v-card-text>
        </v-card>
      </div>
    </div>
  `,
};
