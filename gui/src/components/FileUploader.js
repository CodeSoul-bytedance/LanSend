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

// const FileUploader = {
//   name: 'FileUploader',
//   props: {
//     devices: {
//       type: Array,
//       default: () => [],
//     },
//   },
//   data() {
//     return {
//       isDragging: false,
//       files: [], // 存储 File 对象，在 Electron 中，File 对象会包含 path 属性
//     };
//   },
//   mounted() {
//     console.log('User Agent:', navigator.userAgent);
//     console.log('Is process defined?', typeof window.process);
//     if (window.process && window.process.versions) {
//       console.log('Electron version:', window.process.versions.electron);
//     } else {
//       console.log('Not an Electron main-like process object found on window.');
//     }
//   },
//   methods: {
//     // 处理文件选择（通过点击）
//     handleFileSelection(event) {
//       const selectedFiles = event.target.files || [];
//       if (selectedFiles.length > 0) {
//         // 在 Electron 中，File 对象会有一个 'path' 属性
//         // console.log('Selected files with paths:', Array.from(selectedFiles).map(f => ({name: f.name, path: f.path})));
//         this.processFiles(Array.from(selectedFiles));
//       }
//     },

//     // 处理拖拽进入
//     handleDragEnter(event) {
//       // event.preventDefault(); // 已在模板中处理
//       // event.stopPropagation(); // 已在模板中处理
//       this.isDragging = true;
//     },

//     // 处理拖拽悬停
//     handleDragOver(event) {
//       // event.preventDefault(); // 已在模板中处理
//       // event.stopPropagation(); // 已在模板中处理
//       this.isDragging = true; // 保持高亮
//     },

//     // 处理拖拽离开
//     handleDragLeave(event) {
//       // event.preventDefault(); // 已在模板中处理
//       // event.stopPropagation(); // 已在模板中处理
//       this.isDragging = false;
//     },

//     // 处理文件放置
//     handleDrop(event) {
//       // event.preventDefault(); // 已在模板中处理
//       // event.stopPropagation(); // 已在模板中处理
//       this.isDragging = false;
//       const droppedFiles = event.dataTransfer.files;
//       if (droppedFiles.length > 0) {
//         // 在 Electron 中，File 对象会有一个 'path' 属性
//         // console.log('Dropped files with paths:', Array.from(droppedFiles).map(f => ({name: f.name, path: f.path})));
//         this.processFiles(Array.from(droppedFiles));
//       }
//     },

//     // 处理选择的文件列表
//     processFiles(fileList) {
//       // fileList 中的每个 File 对象在 Electron 环境下都应该有 .path 属性
//       this.files = fileList.map(file => {
//         // 你可以在这里验证 file.path 是否存在
//         if (typeof file.path !== 'string') {
//           console.warn(`文件 '${file.name}' 缺少 'path' 属性。这可能不是一个标准的 Electron 文件对象，或者运行环境不是 Electron 渲染进程。`);
//         }
//         return file;
//       });

//       if (this.files.length > 0) {
//         // 发出事件，包含完整的 File 对象（包括路径）
//         this.$emit("files-selected", this.files);
//       }
//     },

//     // 触发文件选择对话框
//     triggerFileSelection() {
//       if (this.$refs.fileInput) {
//         this.$refs.fileInput.click();
//       }
//     },

//     // 格式化文件大小
//     formatFileSize(bytes) {
//       if (bytes === 0) return "0 B";
//       const k = 1024;
//       const sizes = ["B", "KB", "MB", "GB", "TB"];
//       const i = Math.floor(Math.log(bytes) / Math.log(k));
//       return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + " " + sizes[i];
//     },

//     // 根据文件类型获取图标
//     getFileIcon(fileType) {
//       if (fileType) {
//         if (fileType.startsWith('image/')) return 'mdi-file-image';
//         if (fileType.startsWith('video/')) return 'mdi-file-video';
//         if (fileType.startsWith('audio/')) return 'mdi-file-music';
//       }
//       return 'mdi-file-document';
//     },

//     // 清除已选文件
//     clearFiles(event /* event 参数已通过 @click.stop 确保 */) {
//       this.files = [];
//       if (this.$refs.fileInput) {
//         this.$refs.fileInput.value = ""; // 重置 input，以便可以再次选择相同文件
//       }
//       this.$emit("files-cleared"); // 可以选择性地发出一个事件
//     },

//     // 处理发送文件按钮点击
//     handleSendFiles() {
//       const connectedDevices = this.devices.filter(device => device.connected);
//       if (connectedDevices.length === 0) {
//         console.error('错误：没有已连接的设备可用于发送文件');
//         this.$emit('send-error', '没有已连接的设备可用于发送文件');
//         return;
//       }

//       if (this.files.length === 0) {
//         console.warn('没有选择任何文件');
//         this.$emit('send-error', '没有选择任何文件');
//         return;
//       }

//       // 提取文件路径
//       const filePaths = this.files.map(file => {
//         if (!file.path || typeof file.path !== 'string') {
//           console.error(`文件 "${file.name}" 没有有效的本地路径。无法发送。`);
//           return null;
//         }
//         return file.path;
//       }).filter(path => path !== null);

//       if (filePaths.length === 0) {
//         console.error('所有选中的文件都没有有效的本地路径。');
//         this.$emit('send-error', '所有选中的文件都没有有效的本地路径。');
//         return;
//       }

//       console.log("准备发送的文件路径:", filePaths);

//       // **Electron IPC 通信**
//       // 在这里，你应该使用 IPC 将 filePaths 发送到主进程
//       // 例如:
//       // const { ipcRenderer } = require('electron'); // 或者 import
//       // if (typeof ipcRenderer !== 'undefined') {
//       //   ipcRenderer.send('send-files-to-main', filePaths, connectedDevices /* 你可能还需要目标设备信息 */);
//       // } else {
//       //   console.warn('ipcRenderer 未定义。此组件似乎未在 Electron 渲染进程中运行。');
//       // }
      
//       // 为了演示，我们仍然通过 Vue 事件发出路径
//       this.$emit('send_files_paths', filePaths, connectedDevices);
//     },
//   },
//   template: `
//   <div>
//     <input
//       ref="fileInput"
//       type="file"
//       style="display: none"
//       multiple
//       @change="handleFileSelection"
//     >

//     <div
//       class="file-drop-area"
//       :class="{ 'active': isDragging }"
//       @click="files.length === 0 ? triggerFileSelection() : null"
//       @dragenter.prevent.stop="handleDragEnter"
//       @dragover.prevent.stop="handleDragOver"
//       @dragleave.prevent.stop="handleDragLeave"
//       @drop.prevent.stop="handleDrop"
//     >
//       <template v-if="files.length === 0">
//         <v-icon size="large" color="grey" class="mb-2">mdi-upload</v-icon>
//         <p>点击选择要发送的文件<br>或将文件拖放到这里</p>
//       </template>

//       <template v-else>
//         <div class="selected-files">
//           <h3 class="text-h6 mb-2">已选择 {{ files.length }} 个文件</h3>

//           <v-list density="compact" class="bg-transparent">
//             <v-list-item v-for="(file, index) in files.slice(0, 5)" :key="file.path || index"> <template v-slot:prepend>
//                 <v-icon>
//                   {{ getFileIcon(file.type) }}
//                 </v-icon>
//               </template>
//               <v-list-item-title class="text-truncate" :title="file.name + (file.path ? '\\n路径: ' + file.path : '')">
//                 {{ file.name }}
//               </v-list-item-title>
//               <v-list-item-subtitle>{{ formatFileSize(file.size) }}</v-list-item-subtitle>
//             </v-list-item>

//             <v-list-item v-if="files.length > 5">
//               <v-list-item-title>还有 {{ files.length - 5 }} 个文件...</v-list-item-title>
//             </v-list-item>
//           </v-list>

//           <div class="d-flex justify-space-between mt-4">
//             <v-btn color="error" variant="outlined" @click.stop="clearFiles">
//               清除
//             </v-btn>
//             <v-btn color="primary" @click.stop="handleSendFiles">
//               发送
//             </v-btn>
//           </div>
//         </div>
//       </template>
//     </div>
//   </div>
//   `,
// };

