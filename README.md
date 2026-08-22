# A股行情屏（stockscreen）

在 **TRAE AI 通行证**（FoloToy 出品的 ESP32-C3 徽章设备）上运行的 A 股实时行情显示器：
红涨绿跌、中文名称、交易时段自适应刷新、服务器时间免 NTP 校准、板载电量显示。

<div align="center">

```
┌─────────────────────────────┐
│ ● 19:06:21         100% ←电量│  状态栏：WiFi / 北京时间 / 电量
├─────────────────────────────┤
│ ▎上汽集团                   │
│ ▎ 10.14          +1.05% ←红 │  涨红 / 跌绿 / 平白
│ ▎ +0.11          +1.05%     │
├─────────────────────────────┤
│ ▎宗申动力                   │
│ ▎ 15.30          -0.84% ←绿 │
│ ▎ -0.13          -0.84%     │
├─────────────────────────────┤
│ ▎中国西电                   │
│ ▎ 12.88          +0.23% ←红 │
│ ▎ +0.03          +0.23%     │
├─────────────────────────────┤
│ UPD 08-22 19:06      休市    │  更新时间 / 交易状态
└─────────────────────────────┘
        240 × 320 竖屏
```

</div>

## 硬件

| 项目 | 参数 |
|---|---|
| 设备 | TRAE AI 通行证（FoloToy，[官网](https://ai-passport.folotoy.cn/trae/) / [官方仓库](https://github.com/folotoy/ai-passport)） |
| MCU | ESP32-C3（QFN32，8MB Flash，WiFi/BLE） |
| 屏幕 | 2.0 寸 240×320 ST7789P3（4 线 SPI，需 INVON 反色 + BGR 色序） |
| 电量计 | CW2017（I2C 0x63，读 SOC 百分比，无需 ADC 分压） |
| 开发环境 | VS Code + PlatformIO（espressif32 平台，Arduino 框架） |

屏幕引脚（来自官方仓库 `components/bsp/include/bsp_pins.h`）：

| 功能 | 引脚 | 说明 |
|---|---|---|
| SCLK | GPIO8 | SPI 时钟 |
| MOSI | GPIO9 | SPI 数据 |
| CS | GPIO1 | 片选 |
| DC | GPIO20 | 数据/命令（复用 UART0-RX） |
| RST | 未接 MCU | 硬接 3.3V，走 SWRESET 软件复位 |
| BL | GPIO21 | 背光 |
| I2C SDA / SCL | GPIO10 / GPIO7 | 电量计 CW2017 |

## 功能特性

- **红涨绿跌**，A 股配色习惯；停牌/解析失败显示 `--`
- **股票代码 + 中文名称**，双数据源，断网自动降级
- **免 NTP 时钟**：用 HTTP 响应头 `Date` 字段校准北京时间（详见[时间体系](#时间体系)）
- **交易时段自适应刷新**：交易时段 4s / 休市 60s，按服务器时间判断，无需 RTC
- **WiFi 断线自动重连**，行情失败保留上一帧数据并显示"更新失败"
- **电量百分比显示**（CW2017），<20% 红色预警
- **中文字库按需生成**：只打包自选股名称用到的字（16×16 点阵）

## 快速上手

### 1. 环境

```bash
# VS Code 安装 PlatformIO 插件，打开 stockscreen/ 目录
```

### 2. 配置（只改 `include/config.h`）

```c
static const char* WATCHLIST[] = {"600104", "001696", "601179"};      // 自选股代码
static const char* WATCH_NAMES[] = {"上汽集团", "宗申动力", "中国西电"}; // 一一对应

#define CN_CHARSET "上汽集团宗申动力中国西电交易中断网失败休市"          // 名称用到的字
```

**WiFi 凭据**放在 `include/wifi.local.h`（已被 .gitignore 忽略，不会上传 GitHub）：

```c
// include/wifi.local.h —— 本机专属文件，clone 仓库后自建
#define WIFI_SSID "你的WiFi名"
#define WIFI_PASS "你的WiFi密码"
```

没有该文件时 config.h 使用占位符（`YOUR_WIFI_SSID`），保证仓库公开后不泄露密码。

改完自选股后重新生成字库：

```bash
python3 tools/gen_font.py     # 从 macOS 系统 CJK 字体提取字形 → include/stock_font.h
```

### 3. 编译烧录

```bash
pio run -t upload             # 板子 USB 直连
pio run -t monitor            # 查看串口日志（115200）
```

| 环境 | 用途 |
|---|---|
| `main` | 主固件（默认） |
| `test_screen` | 屏幕诊断固件：色块轮换，用于排查引脚/花屏问题 |

### 4. 日志示例

```
[WiFi] 已连接
[腾讯] HTTP 200 (648ms)
[行情] 成功 3/3 只，耗时 655ms
[时钟] 已用服务器时间校准 1787396781
[电池] SOC 100%
```

## 技术文档

### 数据链路：主源 + 备用源

**主源：东方财富批量接口**（一次请求拉全部自选股）

```
GET https://push2.eastmoney.com/api/qt/ulist.np/get
    ?secids=1.600104,0.001696,1.601179
    &fields=f2,f3,f4,f12,f14,f18,f124&fltt=2&invt=2&ut=fa5fd1943c7b386f172d6893dbfba10b
```

UTF-8 JSON，字段含义：

| 字段 | 含义 |
|---|---|
| f2 | 最新价（`fltt=2` 直接返回 float） |
| f3 | 涨跌幅 % |
| f4 | 涨跌额 |
| f12 | 代码 |
| f14 | 中文名称 |
| f18 | 昨收 |
| f124 | 行情时间戳（Unix epoch） |

**备用源：腾讯行情**（HTTP 明文，GBK 编码）

```
GET http://qt.gtimg.cn/q=sh600104,sz001696,sh601179   （需带 Referer: https://gu.qq.com/）
```

返回 `v_sh600104="1~上汽集团~600104~10.14~...~20260821161453~..."`。GBK 中文无法解析，
所以按 `~` 切段后**只提取 ASCII 数字字段**（价格/涨跌额/涨跌幅/时间戳），名称用
`config.h` 的 `WATCH_NAMES` 映射表兜底。

**熔断策略**：东财失败（TLS 握手失败）后 10 分钟内不再尝试，直接走腾讯源，
避免每轮刷新白等一次握手（见[踩坑记录](#踩坑记录)第 5 条）。

### 时间体系

- 行情时间戳（f124 / 腾讯 14 位 `YYYYMMDDHHMMSS`）→ 统一换算成 UTC epoch
- **时钟校准**：每次 HTTP 200 后解析响应头 `Date: Fri, 22 Aug 2026 08:14:53 GMT` → epoch。
  **不能用行情时间戳当"现在"**——休市时服务器返回的是最近交易日的收盘时刻。
- 显示时 `epoch + 8*3600` 转北京时间，无 NTP/RTC 依赖
- **交易时段判定**：北京时间周一~五 9:15–11:35 / 12:55–15:05 → 4s 刷新，其余 60s
- 本地走秒：`当前时间 = 校准 epoch + (millis() - 校准时刻)/1000`，秒级精度足够

### UI 渲染（240×320）

| 区域 | 内容 |
|---|---|
| 状态栏（26px） | WiFi 状态点（绿=已连）、北京时间、右上角电量（≥50 绿 / 20~49 橙 / <20 红） |
| 股票卡片 | 左色条 + 中文名 + 价格大字（FONT4）+ 涨跌幅/涨跌额；红涨绿跌白平 |
| 底部（20px） | 更新时间 `MM-DD HH:MM` + 状态标签：交易中 / 休市 / 断网 / 失败 |

- 3~4 只自选股用三行卡片布局（60px/只），5 只用紧凑两行布局（50px/只）
- 全屏渲染在每次抓取后，时钟/电量/状态标签每秒局部刷新
- **中文字库**：`tools/gen_font.py` 从 macOS 系统 CJK 字体（STHeiti/PingFang）提取
  `CN_CHARSET` 中每个字的 16×16 点阵 → `include/stock_font.h`；UI 用 UTF-8 匹配字形绘制，
  字库外的字自动跳过

### 电量计（CW2017）

I2C 0x63（与音频 codec ES8311 共用总线，SDA=10/SCL=7），寄存器协议照官方 bsp 移植：

| 寄存器 | 含义 |
|---|---|
| 0x00 | VERSION（上电应答即芯片在位） |
| 0x02 | 14bit 电压，`V(mV) = raw × 312.5 / 1000` |
| 0x04 | SOC 高字节 = 整数百分比 |
| 0x08 | CONFIG，写 0x00 唤醒（退出睡眠/复位态） |

每 5s 采样一次；芯片不在位时返回 -1，UI 自动隐藏电量。驱动见 `src/battery.cpp`。

## 踩坑记录：主要问题与解决方案

### 1. 板卡识别与屏幕引脚

初始只知是"ESP32-C3 + 2.0 寸屏"，各厂 C3 带屏板引脚互不相同（合宙/SUPERMINI/XIAO 全不一样）。
用户透露是 TRAE AI 通行证后，从[官方仓库](https://github.com/folotoy/ai-passport)的
`bsp_pins.h` 拿到权威引脚（CS=1/DC=20 等）。此前按默认引脚烧录出现"背光亮但黑屏"——
**CS/DC 全错**。`test_screen` 诊断固件（色块轮换）保留用于今后排查。

### 2. TG1WDT 看门狗复位循环（最耗时的问题）

现象：主固件启动即循环复位。逐步定位（二分注释法 + addr2line 解析复位 PC）确认毒药在
`tft.init()` 内部。

根因：**TFT_eSPI 2.5.43 在 ESP32-C3 上使用寄存器级 SPI 直推**（`*_spi_cmd = SPI_USR`
忙等待），与 arduino-esp32 3.x 的 SPIClass 外设管理冲突——SPI 外设被 SPIClass 持有，
寄存器直推的忙等待永不结束，满屏填充挂死触发任务看门狗。

解决：给 TFT_eSPI 打补丁（见 [lib/TFT_eSPI/README_PATCHES.md](lib/TFT_eSPI/README_PATCHES.md)），
C3 路径全部改用 SPIClass API：

| 原实现 | 补丁 |
|---|---|
| `SET_BUS_WRITE_MODE` / `SPI_BUSY_CHECK` 忙等待 | 置空（SPIClass 同步传输） |
| `TFT_WRITE_BITS` / `tft_Write_16N` 寄存器写 | `spi.transfer()` 逐字节 |
| `pushBlock` 寄存器填充 | `spi.writePattern()` |
| `pushPixels` / `pushSwapBytePixels` | `spi.writeBytes()` |

字节序通过对原实现的线上字节序分析逐一对齐（`DAT8TO32 = P[0]<<8 | P[1] | P[2]<<24 | P[3]<<16`
写小端 W 寄存器 → FIFO 先发低字节），补丁保持完全一致。同时启用 `SUPPORT_TRANSACTIONS`
让 SPI 按 27MHz SPISettings 走，否则低速满屏填充仍会慢到超时。

### 3. User_Setup.h 配置不生效（CS/DC 仍是 15/0）

PlatformIO 编译库时，`-include User_Setup.h` 会先命中**库目录自带的 User_Setup.h**
（ESP8266 默认引脚 CS=15/DC=0），项目里的配置被遮蔽。解决：把项目配置拷贝覆盖库目录副本。

由于 `pio clean` 会重建 libdeps，**修改版 TFT_eSPI 已整体 vendor 到 `lib/TFT_eSPI/`**，
补丁与配置永不丢失（`lib_deps` 中已移除原版）。

### 4. 串口调试（C3 特有）

arduino-esp32 3.x 在 C3 上 `Serial`（UART0）不可见，日志走 **USB CDC**：用 `log_i()`
+ `-DCORE_DEBUG_LEVEL=3` 构建标志。另外板卡复位需要 DTR/RTS 时序（拉 RTS 复位）。

### 5. 东财 HTTPS 失败：`PADLOCK - Input data should be aligned`

core 3.0.7（IDF 5.1.x）在 ESP32-C3 上的 mbedTLS 硬件 AES 对齐 bug，TLS 握手必败
（free heap 充足，非内存问题）。评估升级 core 到 3.1+（IDF 5.3+）风险收益后**放弃升级**
（最新平台已是 core 4.x/IDF 6.x，对屏幕补丁破坏性大；且腾讯源+名称表已能达到同等效果）。
改为：东财失败即熔断降级腾讯源，中文名走 `WATCH_NAMES` 映射表。
**若将来升级 core，可重新启用东财源拿原生名称。**

### 6. 时间不是北京时间

原实现用行情时间戳当"现在"——休市时服务器返回最近交易日的收盘时刻，时钟自然是错的。
改用 HTTP 响应头 `Date` 字段校准（每次抓取免费对时），彻底解决。

### 7. 涨显示成蓝色

面板红蓝通道反序（RGB565 的 0xF800 被按 BGR 解读成蓝，绿色恰好不受影响）。
启用 `TFT_RGB_ORDER TFT_BGR`（ST7789 通过 MADCTL 颜色顺序位实现，不动字节序，
与 SPI 补丁无冲突）。注意配置在 `include/User_Setup.h` 和
`lib/TFT_eSPI/User_Setup.h` **两处**。

## 目录结构

```
stockscreen/
├── platformio.ini              # 构建配置（main / test_screen 两个环境）
├── include/
│   ├── config.h                # ★ 用户配置：WiFi、自选股、引脚、刷新频率
│   ├── User_Setup.h            # TFT_eSPI 配置（ST7789、BGR、INVON、字体宏）
│   └── stock_font.h            # 中文字库（gen_font.py 生成，勿手改）
├── src/
│   ├── main.cpp                # setup/loop：WiFi、抓取调度、电量采样
│   ├── stocks.h / stocks.cpp   # 行情抓取与解析（东财/腾讯）、HTTP、时间工具
│   ├── ui.h / ui.cpp           # 屏幕渲染：状态栏 + 卡片 + 中文绘制
│   ├── battery.h / battery.cpp # CW2017 电量计驱动
│   └── test_screen.cpp         # 屏幕诊断固件（env:test_screen）
├── lib/
│   └── TFT_eSPI/               # 本地修改版（C3 兼容补丁，见 README_PATCHES.md）
└── tools/
    └── gen_font.py             # 中文字形提取 → include/stock_font.h
```

## 已知限制

- 东财主源在 core 3.0.7（C3）上因 mbedTLS bug 不可用，当前固定走腾讯源（升级 core 可解）
- 字库容量：`CN_CHARSET` 每个字约 32 字节 flash，只放用到的字
- 时钟精度依赖 WiFi 抓取频率，断网时走本地 millis 漂移
- 行情为公开数据接口，未做鉴权；`setInsecure()` 跳过证书校验（个人设备 + 公开数据的取舍，代码内有注释）

## 参考资料

- [folotoy/ai-passport](https://github.com/folotoy/ai-passport) — 官方固件，引脚/电量计参考
- [TRAE AI 通行证官网](https://ai-passport.folotoy.cn/trae/)
- [Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — 显示库（本项目使用本地修改版）
- [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32) — Arduino core 3.x
