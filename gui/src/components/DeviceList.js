// 设备列表组件
const DeviceList = {
    props: {
        devices: {
            type: Array,
            default: () => [],
        },
    },

    methods: {
        // 选择设备
        selectDevice(device) {
            this.$emit("select-device", device);
        },

        // 根据设备类型获取图标
        getDeviceIcon(deviceType) {
            switch (deviceType) {
                case "desktop":
                    return "mdi-desktop-classic";
                case "laptop":
                    return "mdi-laptop";
                case "mobile":
                    return "mdi-cellphone";
                case "tablet":
                    return "mdi-tablet";
                default:
                    return "mdi-devices";
            }
        },
    },

    template: `
    <div>
      <div v-if="devices.length === 0" class="empty-state">
        <v-icon size="x-large" color="grey">mdi-radar</v-icon>
        <h3 class="mt-4">正在搜索设备</h3>
        <p class="text-subtitle-1">请确保其他设备与您在同一网络中且已开启LanSend</p>
      </div>
      
      <v-row v-else>
        <v-col v-for="device in devices" :key="device.device_id" cols="12" sm="6" md="4" lg="3">
          <v-card class="device-card" @click="selectDevice(device)">
            <v-card-title>
              <v-icon class="mr-2">{{ getDeviceIcon(device.device_model) }}</v-icon>
              {{ device.device_name }}
              <!-- 添加连接状态标识 -->
              <v-chip v-if="device.connected" color="success" size="small" class="ml-2">
                <v-icon size="x-small" start>mdi-link</v-icon>
                已连接
              </v-chip>
            </v-card-title>
            <v-card-subtitle>{{ device.ip }}</v-card-subtitle>
            <v-card-text>
              <div class="d-flex align-center">
                <span class="text-body-2">型号: {{ device.device_model || '未知' }}</span>
              </div>
              <div class="d-flex align-center mt-1">
                <span class="text-body-2">系统: {{ device.device_platform || '未知' }}</span>
              </div>
            </v-card-text>
            <v-card-actions>
              <v-btn color="primary" block>
                <v-icon class="mr-2">mdi-send</v-icon>
                发送文件
              </v-btn>
            </v-card-actions>
          </v-card>
        </v-col>
      </v-row>
    </div>
  `,
};
