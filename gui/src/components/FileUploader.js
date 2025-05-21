// 文件上传组件
const FileUploader = {
    props: {
        devices: {
            type: Array,
            default: () => [],
        },
    },

    data() {
        return {
            isDragging: false,
            files: [],
        };
    },

    methods: {
        // 处理文件选择
        handleFileSelection(event) {
            const files = event.target.files || [];
            if (files.length > 0) {
                this.processFiles(Array.from(files));
            }
        },

        // 处理拖拽事件
        handleDragEnter(event) {
            event.preventDefault();
            event.stopPropagation();
            this.isDragging = true;
        },

        handleDragOver(event) {
            event.preventDefault();
            event.stopPropagation();
            this.isDragging = true;
        },

        handleDragLeave(event) {
            event.preventDefault();
            event.stopPropagation();
            this.isDragging = false;
        },

        handleDrop(event) {
            event.preventDefault();
            event.stopPropagation();
            this.isDragging = false;

            const files = event.dataTransfer.files;
            if (files.length > 0) {
                this.processFiles(Array.from(files));
            }
        },

        // 处理选择的文件
        processFiles(fileList) {
            this.files = fileList;

            // 如果没有选择目标设备，显示设备选择对话框
            if (this.files.length > 0) {
                this.$emit("files-selected", this.files);
            }
        },

        // 触发文件选择对话框
        triggerFileSelection() {
            this.$refs.fileInput.click();
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

        // 清除已选文件
        // clearFiles() {
        //     this.files = [];
        //     this.$refs.fileInput.value = "";
        // },
        clearFiles(event) {
          // 阻止事件冒泡
          if (event) {
              event.stopPropagation();
          }
          
          this.files = [];
          this.$refs.fileInput.value = "";
          
          // 添加一个小延迟，确保 DOM 更新完成后再允许新的点击事件
          setTimeout(() => {
              this.isDragging = false;  // 确保拖拽状态也被重置
          }, 50);
        },

        // 处理发送文件按钮点击
        handleSendFiles() {
          // 检查是否有已连接设备
          const connectedDevices = this.devices.filter(device => device.connected);
          
          if (connectedDevices.length === 0) {
            console.error('错误：没有已连接的设备可用于发送文件');
            // 可以在这里添加用户提示，例如使用 alert 或其他 UI 通知
            this.$emit('send-error', '没有已连接的设备可用于发送文件');
            return;
          }
          
          // 如果有已连接设备，则发出发送文件事件
          this.$emit('send_files', this.files.map(file => file.path));
        },
    },

    template: `
    <div>
      <input 
        ref="fileInput" 
        type="file" 
        style="display: none" 
        multiple 
        @change="handleFileSelection"
      >
      
      <div 
        class="file-drop-area"
        :class="{ 'active': isDragging }"
        @click="files.length === 0 ? triggerFileSelection() : null"
        @dragenter="handleDragEnter"
        @dragover="handleDragOver"
        @dragleave="handleDragLeave"
        @drop="handleDrop"
      >
        <template v-if="files.length === 0">
          <v-icon size="large" color="grey" class="mb-2">mdi-upload</v-icon>
          <p>点击选择要发送的文件<br>或将文件拖放到这里</p>
        </template>
        
        <template v-else>
          <div class="selected-files">
            <h3 class="text-h6 mb-2">已选择 {{ files.length }} 个文件</h3>
            
            <v-list density="compact" class="bg-transparent">
              <v-list-item v-for="(file, index) in files.slice(0, 5)" :key="index">
                <template v-slot:prepend>
                  <v-icon>
                    {{ file.type.startsWith('image/') ? 'mdi-file-image' :
                       file.type.startsWith('video/') ? 'mdi-file-video' :
                       file.type.startsWith('audio/') ? 'mdi-file-music' :
                       'mdi-file-document' }}
                  </v-icon>
                </template>
                <v-list-item-title class="text-truncate">{{ file.name }}</v-list-item-title>
                <v-list-item-subtitle>{{ formatFileSize(file.size) }}</v-list-item-subtitle>
              </v-list-item>
              
              <v-list-item v-if="files.length > 5">
                <v-list-item-title>还有 {{ files.length - 5 }} 个文件...</v-list-item-title>
              </v-list-item>
            </v-list>
            
            <div class="d-flex justify-space-between mt-4">
              <v-btn color="error" variant="outlined" @click="clearFiles">
                清除
              </v-btn>
              <v-btn color="primary" @click="handleSendFiles">
                发送
              </v-btn>
            </div>
          </div>
        </template>
      </div>
    </div>
  `,
};