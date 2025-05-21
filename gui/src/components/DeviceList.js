// 设备列表组件
const DeviceList = {
    props: {
        devices: {
            type: Array,
            default: () => [],
        },
    },
    data() {
        return {
            pinDialogVisible: false,
            selectedDeviceForPin: null,
            pinCode: "",
        };
    },
    methods: {
        // 选择设备
        selectDevice(device) {
            console.log("DeviceList: selectDevice");
            this.$emit("select-device", device);
        },

        // // 连接设备 (旧方法，将被 showPinDialog 和 submitPinAndConnect 替代)
        // connectToDevice(deviceId) {
        //     console.log("DeviceList: 尝试连接设备", deviceId);
        //     this.$emit("connect_to_device", deviceId);
        // },

        showPinDialog(device) {
            this.selectedDeviceForPin = device;
            this.pinCode = ""; // 重置PIN码
            this.pinDialogVisible = true;
        },

        cancelPinDialog() {
            this.pinDialogVisible = false;
            this.selectedDeviceForPin = null;
            this.pinCode = "";
        },

        submitPinAndConnect() {
            console.log("DeviceList: submitPinAndConnect");
            if (
                this.pinCode &&
                this.pinCode.trim() !== "" &&
                this.selectedDeviceForPin
            ) {
                console.log(
                    `DeviceList: 尝试使用PIN连接设备 ${this.selectedDeviceForPin.device_id}, PIN: ${this.pinCode}`
                );
                this.$emit(
                    "connect_to_device",
                    this.selectedDeviceForPin.device_id,
                    this.pinCode.trim()
                );
                this.cancelPinDialog(); // 关闭对话框并重置
            } else {
                // 可选: 如果PIN码为空，显示错误或警告
                console.warn("PIN code cannot be empty.");
            }
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
              <v-btn color="primary" block @click.stop="showPinDialog(device)">
                <v-icon class="mr-2">mdi-link</v-icon>
                连接设备
              </v-btn>
            </v-card-actions>
          </v-card>
        </v-col>
      </v-row>

      <v-dialog v-model="pinDialogVisible" persistent max-width="350px">
        <v-card>
          <v-card-title>
            <span class="headline">输入认证码</span>
          </v-card-title>
          <v-card-text>
            <p v-if="selectedDeviceForPin">请输入连接到 <strong>{{ selectedDeviceForPin.device_name }}</strong> ({{ selectedDeviceForPin.ip }}) 的认证码。</p>
            <v-text-field
              v-model="pinCode"
              label="认证码 (PIN)"
              required
              autofocus
              @keyup.enter="submitPinAndConnect"
            ></v-text-field>
          </v-card-text>
          <v-card-actions>
            <v-spacer></v-spacer>
            <v-btn color="blue darken-1" text @click="cancelPinDialog">取消</v-btn>
            <v-btn color="blue darken-1" text @click="submitPinAndConnect" :disabled="!pinCode || !pinCode.trim()">确认连接</v-btn>
          </v-card-actions>
        </v-card>
      </v-dialog>
    </div>
  `,
};
