// 传输项组件
const TransferItem = {
    props: {
        transfer: {
            type: Object,
            required: true,
        },
    },

    computed: {
        // 传输状态对应的颜色
        statusColor() {
            switch (this.transfer.status) {
                case "in_progress":
                    return "primary";
                case "completed":
                    return "success";
                case "failed":
                    return "error";
                case "pending":
                    return "warning";
                default:
                    return "grey";
            }
        },

        // 传输状态文本
        statusText() {
            switch (this.transfer.status) {
                case "in_progress":
                    return "传输中";
                case "completed":
                    return "已完成";
                case "failed":
                    return "失败";
                case "pending":
                    return "等待中";
                default:
                    return "未知";
            }
        },

        // 是否显示进度条
        showProgress() {
            return (
                this.transfer.status === "in_progress" ||
                this.transfer.status === "completed"
            );
        },

        // 文件名显示
        displayName() {
            if (!this.transfer.files || this.transfer.files.length === 0) {
                return "未知文件";
            }

            const firstFile = this.transfer.files[0];
            if (this.transfer.files.length === 1) {
                return firstFile.name;
            } else {
                return `${firstFile.name} 等 ${this.transfer.files.length} 个文件`;
            }
        },
    },

    methods: {
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

        // 计算传输速度
        calculateSpeed() {
            if (
                this.transfer.status !== "in_progress" ||
                !this.transfer.startTime
            ) {
                return "- KB/s";
            }

            const now = new Date();
            const start = new Date(this.transfer.startTime);
            const elapsedSeconds = (now - start) / 1000;

            if (elapsedSeconds <= 0) return "计算中...";

            const transferredBytes =
                this.transfer.totalSize * (this.transfer.progress / 100);
            const bytesPerSecond = transferredBytes / elapsedSeconds;

            return this.formatFileSize(bytesPerSecond) + "/s";
        },

        // 计算剩余时间
        calculateEta() {
            if (
                this.transfer.status !== "in_progress" ||
                !this.transfer.startTime ||
                this.transfer.progress <= 0
            ) {
                return "-";
            }

            const now = new Date();
            const start = new Date(this.transfer.startTime);
            const elapsedSeconds = (now - start) / 1000;

            if (elapsedSeconds <= 0) return "计算中...";

            const totalBytes = this.transfer.totalSize;
            const transferredBytes =
                totalBytes * (this.transfer.progress / 100);
            const bytesPerSecond = transferredBytes / elapsedSeconds;

            if (bytesPerSecond <= 0) return "计算中...";

            const remainingBytes = totalBytes - transferredBytes;
            const remainingSeconds = remainingBytes / bytesPerSecond;

            // 格式化剩余时间
            if (remainingSeconds < 60) {
                return `${Math.round(remainingSeconds)} 秒`;
            } else if (remainingSeconds < 3600) {
                return `${Math.round(remainingSeconds / 60)} 分钟`;
            } else {
                const hours = Math.floor(remainingSeconds / 3600);
                const minutes = Math.round((remainingSeconds % 3600) / 60);
                return `${hours} 小时 ${minutes} 分钟`;
            }
        },

        // 取消或关闭传输
        closeTransfer() {
            if (
                this.transfer.status === "in_progress" ||
                this.transfer.status === "pending"
            ) {
                if (confirm("确定要取消此传输吗？")) {
                    this.$emit("cancel-transfer", this.transfer.id);
                }
            } else {
                this.$emit("close-transfer", this.transfer.id);
            }
        },
    },

    template: `
    <v-card class="mb-4">
      <v-card-title class="d-flex justify-space-between">
        <div class="d-flex align-center">
          <v-icon :color="statusColor" class="mr-2">
            {{ transfer.status === 'in_progress' ? 'mdi-progress-upload' : 
               transfer.status === 'completed' ? 'mdi-check-circle' : 
               transfer.status === 'failed' ? 'mdi-alert-circle' : 'mdi-clock-outline' }}
          </v-icon>
          <span class="text-truncate">{{ displayName }}</span>
        </div>
        <v-btn icon size="small" @click="closeTransfer">
          <v-icon>{{ transfer.status === 'in_progress' ? 'mdi-close' : 'mdi-close' }}</v-icon>
        </v-btn>
      </v-card-title>
      
      <v-card-text>
        <div v-if="transfer.status === 'in_progress'" class="d-flex justify-space-between mb-1">
          <span>{{ formatFileSize(transfer.totalSize * transfer.progress / 100) }} / {{ formatFileSize(transfer.totalSize) }}</span>
          <span>{{ calculateSpeed() }}</span>
        </div>
        
        <v-progress-linear
          v-if="showProgress"
          :model-value="transfer.progress"
          :color="statusColor"
          height="10"
          :striped="transfer.status === 'in_progress'"
          class="mb-1"
        ></v-progress-linear>
        
        <div class="d-flex justify-space-between mt-1">
          <v-chip size="small" :color="statusColor" text-color="white">{{ statusText }}</v-chip>
          <span v-if="transfer.status === 'in_progress'">剩余: {{ calculateEta() }}</span>
        </div>
        
        <div v-if="transfer.status === 'failed'" class="mt-2 text-error">
          {{ transfer.error || '传输失败，请重试' }}
        </div>
      </v-card-text>
      
      <v-card-actions v-if="transfer.status === 'pending'">
        <v-btn color="primary" @click="$emit('accept-transfer', transfer.id)">接受</v-btn>
        <v-btn color="error" @click="$emit('reject-transfer', transfer.id)">拒绝</v-btn>
      </v-card-actions>
    </v-card>
  `,
};
