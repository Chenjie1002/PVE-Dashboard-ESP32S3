# ESP32-S3 Proxmox VE 8.1 物理监控仪表盘 (PVE & iKuai 状态屏)

基于 **ESP32-S3 Super Mini** 与 **ST7789 LCD (320×240 横屏)** 的 Proxmox VE & iKuai 路由器硬件监控仪表盘。

采用 **LVGL 7** 现代极简设计，支持 **4 套不同风格主题无感实时轮换**，内置 Web 后台控制台与 SoftAP 配网，直连 PVE 与爱快路由器官方 API，实现零侵入、高刷新率的极客硬件桌面小屏。

---

## ✨ 核心特性

- **四套个性化仪表主题**（短按板载 `BOOT` 键即时无感切换）：
  1. **风格一：Apple Bento Grid** - 现代极简卡片，纯黑底色，加粗指标条，16px 粗体数字与无网格平滑水流曲线。
  2. **风格二：Cyberpunk HUD** - 赛博科幻机甲风，荧光青与霓虹粉对比，HUD 指标框体。
  3. **风格三：Industrial Telemetry** - 工业遥测终端，硬核黑金仪表，高对比刻度条。
  4. **风格四：Monochrome Telemetry** - 纯黑白极简科研仪器，22px 醒目网速大字，紧凑 60px 示波窗。
- **高精度硬件指标采集**：
  - **CPU 封装温度**：精确解析 PVE `sensors_info`（支持 Intel `Package id 0` 与 AMD `Tdie/Tctl`）。
  - **SSD 固态温度**：直接解析磁盘 SMART Attribute 194/190 与 NVMe 真实物理温度。
  - **系统资源监控**：实时展现 CPU 利用率、RAM 内存占用、RootFS 存储空间及 1 分钟平均负载（LoadAvg）。
- **真实爱快 WAN 外网网速同步**：
  - 针对爱快虚拟机网卡直通无法通过 PVE 统计外网流量的问题，直连爱快路由 Web API (`monitor_lanip total`)。
  - 采用轻量级流式解析，网速数字与走势与爱快后台完全一致，并支持自动平滑降级。
- **全功能 Web 控制台与 AP 配网**：
  - 首次使用或 WiFi 丢失时自动开启 `PVE-Monitor-AP` 热点与 Captive Portal 强制配网门户。
  - 正常联网状态下，局域网电脑/手机浏览器访问小屏幕左下角的 `http://设备IP/` 即可在线修改 WiFi、PVE Token、爱快密码与默认主题，无需重新刷机。
- **丝滑动态波形**：
  - 150ms 连续平滑流水推进（约 7 FPS），自适应动态量程，无锯齿网格，视觉极具科技感。

---

## 🛠️ 硬件接线规格

- **主控芯片**：ESP32-S3 Super Mini (双核 Xtensa LX7 240MHz, 4MB Flash, 2MB PSRAM)
- **显示屏**：1.54 / 2.0 寸 ST7789 TFT LCD（横屏 320×240 分辨率）

| ST7789 屏幕引脚 | ESP32-S3 Super Mini 引脚 | 说明 |
| :--- | :--- | :--- |
| **VCC** | **3.3V / 5V** | 供电正极 |
| **GND** | **GND** | 电源地 |
| **SCL / SCLK** | **GPIO 7** | 硬件 SPI 时钟线 (40MHz) |
| **SDA / MOSI** | **GPIO 5** | 硬件 SPI 数据线 |
| **RES / RST** | **GPIO 1** | 硬件复位引脚 |
| **DC / RS** | **GPIO 6** | 数据/命令选择引脚 |
| **CS** | **GPIO 4** | 片选引脚（无 CS 脚可接地） |
| **BLK / LED** | **GPIO 2** | 背光控制引脚（支持 PWM 调光） |
| **BOOT 按键** | **GPIO 0** | 板载按键，短按单击切换界面主题 |

---

## 🚀 快速上手与烧录

### 1. 编译与上传
本项目基于 [PlatformIO](https://platformio.org/) 构建开发。

```bash
# 1. 克隆代码仓库
git clone https://github.com/Chenjie1002/PVE-Dashboard-ESP32S3.git
cd PVE-Dashboard-ESP32S3

# 2. 编译并烧录至 ESP32-S3 (以 COM3 为例)
platformio run -t upload --upload-port COM3
```

### 2. 首次配网与配置
固件内置 Web 配网与控制台，无需在代码中硬编码任何密码：
1. 首次开机未检测到配置时，设备屏幕将提示热点信息并启动 `PVE-Monitor-AP`；
2. 手机或电脑连接热点后，浏览器访问 `http://192.168.4.1/` 即可进入配置后台；
3. 填入您的 WiFi、PVE 地址与 Token、爱快路由器管理密码后点击保存，设备将自动重启并连接。
4. 后续如需修改参数，在局域网浏览器直接访问屏幕左下角显示的 `http://设备IP/` 即可直接配置！

---

## 📄 开源许可
MIT License.
