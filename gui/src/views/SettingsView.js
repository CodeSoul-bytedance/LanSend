// 设置视图组件
const SettingsView = {
    props: {
        settings: Object,
    },

    data() {
        return {
            // 本地设置副本，用于编辑
            localSettings: {
                deviceName: this.settings.deviceName || "",
                savePath: this.settings.savePath || "",
                darkMode: this.settings.darkMode || false,
            },
        };
    },

    methods: {
        // 选择保存路径
        async selectSavePath() {
            try {
                const folderPath = await window.electronAPI.selectFolder();
                if (folderPath) {
                    this.localSettings.savePath = folderPath;
                }
            } catch (error) {
                console.error("选择文件夹失败:", error);
            }
        },

        // 保存设置
        async saveSettings() {
            // 通过事件将更新后的设置发送给父组件
            this.$emit("update-settings", { ...this.localSettings });

            // 发送设置到后端
            try {
                const settingsPayload = {
                    device_name: this.localSettings.deviceName,
                    save_path: this.localSettings.savePath,
                    // darkMode is a frontend-only setting based on current structure
                };

                // 改为调用 PipeClient.methods
                const result = await PipeClient.methods.updateSettings(
                    settingsPayload
                );

                this.showSnackbar("设置已保存");
            } catch (error) {
                this.showSnackbar(
                    "保存设置失败: " + (error.message || "未知错误"),
                    "error"
                );
            }
        },

        // 显示通知
        showSnackbar(message, color = "success") {
            // 实际应用中可以使用Vuetify的snackbar组件
            console.log(message);
            alert(message);
        },
    },

    template: `
    <div>
      <h1 class="text-h4 mb-6">设置</h1>
      
      <v-card class="mb-6">
        <v-card-title>常规设置</v-card-title>
        <v-card-text>
          <v-text-field
            v-model="localSettings.deviceName"
            label="设备名称"
            placeholder="输入您的设备名称"
            hint="此名称将在局域网中可见"
            persistent-hint
          ></v-text-field>
          
          <div class="d-flex align-center mt-4">
            <v-text-field
              v-model="localSettings.savePath"
              label="接收文件保存位置"
              placeholder="选择文件保存位置"
              hint="接收的文件将保存在此位置"
              persistent-hint
              readonly
              class="flex-grow-1 mr-2"
            ></v-text-field>
            <v-btn color="primary" @click="selectSavePath">浏览</v-btn>
          </div>
        </v-card-text>
      </v-card>
      
      <v-card class="mb-6">
        <v-card-title>外观</v-card-title>
        <v-card-text>
          <v-switch
            v-model="localSettings.darkMode"
            label="深色模式"
            hint="切换应用的亮/暗主题"
            persistent-hint
          ></v-switch>
        </v-card-text>
      </v-card>
      
      <v-card class="mb-6">
        <v-card-title>关于</v-card-title>
        <v-card-text>
          <p><strong>LanSend</strong> 是一个快速、安全的局域网文件传输工具。</p>
          <p>版本: 1.0.0</p>
          <p>作者: CodeSoul</p>
        </v-card-text>
      </v-card>
      
      <div class="d-flex justify-end">
        <v-btn color="primary" @click="saveSettings">保存设置</v-btn>
      </div>
    </div>
  `,
};
