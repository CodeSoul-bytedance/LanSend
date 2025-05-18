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
    },

    methods: {
        // 发送文件到设备
        sendFilesToDevice(device) {
            this.$emit("send-files", device);
        },

        // 接受传输请求
        acceptTransfer(transferId) {
            this.$emit("accept-transfer", transferId);
        },

        // 拒绝传输请求
        rejectTransfer(transferId) {
            this.$emit("reject-transfer", transferId);
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
    },

    template: `
    <div>
      <h1 class="text-h4 mb-4">发现的设备</h1>
      
      <!-- 使用 DeviceList 组件替换原有的 v-row 和 v-col -->
      <device-list 
        :devices="devices" 
        @select-device="sendFilesToDevice"
      ></device-list>
      
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
      
      <!-- 上传区域 -->
      <div class="mt-8">
        <h2 class="text-h5 mb-4">发送文件</h2>
        
        <div class="file-drop-area" @click="$refs.fileInput.click()">
          <input ref="fileInput" type="file" style="display: none" multiple @change="handleFileSelection">
          <v-icon size="large" color="grey">mdi-upload</v-icon>
          <p class="mt-4">点击选择要发送的文件<br>或将文件拖放到这里</p>
        </div>
      </div>
    </div>
  `,
};
