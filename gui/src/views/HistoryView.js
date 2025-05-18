// 历史记录视图组件
const HistoryView = {
    props: {
        transfers: Array,
    },

    data() {
        return {
            search: "",
            headers: [
                { title: "时间", key: "time", sortable: true },
                { title: "文件", key: "filename", sortable: true },
                { title: "大小", key: "size", sortable: true },
                { title: "设备", key: "device", sortable: true },
                { title: "方向", key: "direction", sortable: true },
                { title: "状态", key: "status", sortable: true },
                { title: "操作", key: "actions", sortable: false },
            ],
        };
    },

    computed: {
        // 获取已完成或失败的传输记录
        completedTransfers() {
            return this.transfers
                .filter(
                    (t) => t.status === "completed" || t.status === "failed"
                )
                .map((t) => {
                    const isIncoming = t.targetDevice === "this-device";
                    return {
                        id: t.id,
                        time: new Date(t.startTime).toLocaleString(),
                        filename:
                            t.files[0].name +
                            (t.files.length > 1
                                ? ` 等${t.files.length}个文件`
                                : ""),
                        size: this.formatFileSize(t.totalSize),
                        device: isIncoming ? t.sourceDevice : t.targetDevice,
                        direction: isIncoming ? "接收" : "发送",
                        status: t.status === "completed" ? "已完成" : "失败",
                        statusColor:
                            t.status === "completed" ? "success" : "error",
                        error: t.error,
                    };
                })
                .sort((a, b) => new Date(b.time) - new Date(a.time));
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

        // 打开文件所在位置
        async openFileLocation(transferId) {
            try {
                await PipeClient.methods.openFileLocation(transferId);
                // 可以在这里添加成功后的操作，如果需要
            } catch (error) {
                console.error("打开文件位置失败:", error);
                alert("打开文件位置失败: " + (error.message || "未知错误"));
            }
        },

        // 重新发送文件
        async resendFile(transfer) {
            const index = this.transfers.findIndex((t) => t.id === transfer.id);
            if (index >= 0) {
                const originalTransfer = this.transfers[index];
                try {
                    await PipeClient.methods.sendFilesToDevice(
                        originalTransfer.targetDevice,
                        originalTransfer.files.map((f) => f.path)
                    );
                    alert("文件已重新发送"); // 或其他用户提示
                } catch (error) {
                    console.error("重新发送文件失败:", error);
                    alert("重新发送文件失败: " + (error.message || "未知错误"));
                }
            }
        },

        // 删除传输记录
        async deleteTransferRecord(transferId) {
            if (confirm("确定要删除此传输记录吗？")) {
                try {
                    await PipeClient.methods.deleteTransferRecord(transferId);
                    // 可以在这里添加成功后的操作，例如从视图中移除该记录或刷新列表
                    // 注意：目前props中的transfers是父组件管理的，直接在这里修改可能无效
                    // 可能需要 $emit 事件给父组件处理删除
                    alert("传输记录已删除");
                } catch (error) {
                    console.error("删除传输记录失败:", error);
                    alert("删除传输记录失败: " + (error.message || "未知错误"));
                }
            }
        },
    },

    template: `
    <div>
      <h1 class="text-h4 mb-4">传输历史</h1>
      
      <v-text-field
        v-model="search"
        label="搜索"
        prepend-inner-icon="mdi-magnify"
        single-line
        hide-details
        class="mb-4"
      ></v-text-field>
      
      <v-data-table
        :headers="headers"
        :items="completedTransfers"
        :search="search"
        class="elevation-1"
        :no-data-text="'暂无传输记录'"
      >
        <template v-slot:item.status="{ item }">
          <v-chip :color="item.statusColor" size="small">
            {{ item.status }}
          </v-chip>
        </template>
        
        <template v-slot:item.actions="{ item }">
          <div class="d-flex">
            <v-tooltip text="查看文件位置">
              <template v-slot:activator="{ props }">
                <v-btn
                  v-bind="props"
                  icon
                  size="small"
                  variant="text"
                  @click="openFileLocation(item.id)"
                  :disabled="item.direction !== '接收' || item.status !== '已完成'"
                >
                  <v-icon>mdi-folder-open</v-icon>
                </v-btn>
              </template>
            </v-tooltip>
            
            <v-tooltip text="重新发送">
              <template v-slot:activator="{ props }">
                <v-btn
                  v-bind="props"
                  icon
                  size="small"
                  variant="text"
                  @click="resendFile(item)"
                  :disabled="item.direction !== '发送'"
                >
                  <v-icon>mdi-replay</v-icon>
                </v-btn>
              </template>
            </v-tooltip>
            
            <v-tooltip text="删除记录">
              <template v-slot:activator="{ props }">
                <v-btn
                  v-bind="props"
                  icon
                  size="small"
                  variant="text"
                  color="error"
                  @click="deleteTransferRecord(item.id)"
                >
                  <v-icon>mdi-delete</v-icon>
                </v-btn>
              </template>
            </v-tooltip>
          </div>
        </template>
      </v-data-table>
    </div>
  `,
};
