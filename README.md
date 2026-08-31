# A股 + 加密货币行情屏（stockscreen）

在 **TRAE AI 通行证**（FoloToy 出品的 ESP32-C3 徽章设备）上运行的行情显示器：
A 股自选 + 加密货币市值前五双页显示（UP/DOWN 键翻页）、红涨绿跌、中文名称、
交易时段自适应刷新、服务器时间免 NTP 校准、夜间深睡眠省电、BLE/串口双通道配网、
BLE 行情推送、板载电量显示。

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
| 三键 ADC | GPIO0 | UP / DOWN / OK 共用 ADC 分压 |
| I2C SDA / SCL | GPIO10 / GPIO7 | 电量计 CW2017 |

## 功能特性

- **红涨绿跌**，A 股配色习惯；停牌/解析失败显示 `--`
- **股票代码 + 中文名称**，双数据源，断网自动降级
- **加密货币页**：动态跟踪市值前五大币（CoinGecko 主源 / OKX 备用），美元价 +
  24h 涨跌幅 + 市值缩写，自动跳过稳定币；UP/DOWN 键在 A股页 ⇄ 币市页之间切换，
  OK 键立即刷新当前页（详见[加密货币页](#加密货币页)）
- **免 NTP 时钟**：用 HTTP 响应头 `Date` 字段校准北京时间（详见[时间体系](#时间体系)）
- **交易时段自适应刷新**：交易时段 4s / 休市 60s，按服务器时间判断，无需 RTC；
  币市页独立节奏（默认 45s，全天有效）
- **WiFi 断线自动重连**，行情失败保留上一帧数据并显示"更新失败"
- **USB 串口 + BLE 双通道配网**：串口（115200）或手机 BLE 写入
  `wifi <SSID> <密码>` 即配即用，凭据存入 NVS 断电不丢（详见[配网](#配网)）
- **BLE 行情推送**：手机订阅通知特征后，每轮刷新自动收到全部股票与币种的
  最新价和涨跌幅（详见 [BLE 配网与推送](#ble-配网与推送)）
- **夜间深睡眠省电**：默认北京时间 23:30~07:00 息屏后进入深睡眠（微安级功耗），
  定时自动唤醒，按任意实体键随时唤醒（详见[深睡眠](#夜间深睡眠)）
- **电量百分比显示**（CW2017），<20% 红色预警
- **20 秒自动息屏**：关闭背光但继续更新行情，按任意实体键立即显示缓存的最新数据
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

### 配网

**直接烧录社区固件的用户**（无需改代码重编译）：

1. USB 连接设备，打开串口监视器 115200
   （PlatformIO 自带，或任意终端：`screen /dev/cu.usbmodem* 115200`）
2. 输入并回车：

```
wifi 你的WiFi名 你的WiFi密码
```

3. 设备自动保存（NVS，断电不丢）并重连；连不上 WiFi 时屏幕也会显示配网提示

常用命令：

| 命令 | 作用 |
|---|---|
| `wifi <SSID> <密码>` | 保存凭据并立即重连 |
| `wifi-status` | 查看当前凭据来源（不显示密码） |
| `wifi-clear` | 清除 NVS 凭据，回退编译期默认值 |
| `help` | 打印帮助 |

凭据优先级：**NVS（串口/BLE 配置）> include/wifi.local.h（编译期）**。
自己编译的用户可以继续用 `wifi.local.h`，也可以烧录后直接用串口或 BLE 配置。

### BLE 配网与推送

没有电脑也能配网：手机安装通用 BLE 调试工具（iOS/Android 的 **nRF Connect** 或
**LightBlue**），连接名为 `StockScreen` 的设备（名称可在 `config.h` 改）：

| 特征 | UUID | 用法 |
|---|---|---|
| 命令（写） | `FFF1` | 以 UTF-8 文本写入命令，与串口命令完全一致：`wifi <SSID> <密码>`、`wifi-status`、`wifi-clear`、`help` |
| 推送（通知） | `FFF2` | 订阅后接收命令回应；每轮行情刷新自动推送一份文本（每行 `代码 名称 价格 涨跌幅` / `币种 $价格 24h涨跌幅`） |

- 服务 UUID `FFF0`；通知按协商 MTU 自动分片，nRF Connect 默认会请求大 MTU
- 手机保持连接时设备不会进入夜间深睡眠
- 不需要 BLE 时把 `config.h` 的 `BLE_ENABLE` 改 0，可省约 50KB 内存

### 加密货币页

按 **UP 或 DOWN 键**在 A股页与币市页之间切换（底部圆点指示当前页），
**OK 键**立即刷新当前页。币市页每行显示：币种符号、美元价（小数位自适应）、
24h 涨跌幅（红涨绿跌，与 A股页一致）、市值缩写（`$2.31T`）。

数据链路与 A股页同样的"主源 + 备用 + 熔断"结构：

- **主源 CoinGecko** `/api/v3/coins/markets`：按市值降序动态返回，排名变化自动跟随；
  默认跳过稳定币/包装币（`config.h` 的 `CRYPTO_EXCLUDE`）
- **备用 OKX** `/api/v5/market/ticker`：固定列表（`CRYPTO_FALLBACK_LIST`）逐个查询，
  无市值数据（该列显示 `-`）；主源失败后熔断 10 分钟直接走备用
- 两源都是 HTTPS。**大陆网络下这两个域名可能无法直连**，`config.h` 里
  `CRYPTO_CG_HOST` / `CRYPTO_OKX_HOST` 可改为可用的镜像域名（OKX 可试 `aws.okx.com`）
- 刷新间隔 `CRYPTO_REFRESH_MS` 默认 45s（CoinGecko 免费接口有频率限制，勿低于 30s）

### 夜间深睡眠

默认北京时间 **23:30~07:00**（`config.h` 的 `DEEP_SLEEP_START_MIN/END_MIN`，可跨零点）：
息屏 20 秒后自动进入深睡眠，功耗从几十 mA 降到微安级；RTC 定时器睡到窗口结束自动
唤醒并恢复运行。窗口内**按任意实体键**随时唤醒（三键分压电路会把 GPIO0 拉到低电平，
外部上拉常供电，深睡期间有效）。

注意事项：

- 深睡唤醒 = 重新启动，需要重连 WiFi 并抓一轮数据（约 5~10 秒）后画面才有内容
- 手机 BLE 保持连接时不进入深睡（避免推送中断）；服务器时间未校准成功时也不进入
  （无法判断当前时刻）
- RTC 时钟有 ±5% 漂移，提前醒来会自动补睡到窗口结束
- 不需要该功能把 `DEEP_SLEEP_ENABLE` 改 0
- 官方仓库注明按键唤醒电路"尚无板级验证"，本项目按分压电路原理推断可行；
  若实测按键无法唤醒，等定时唤醒或拔插 USB 即可，欢迎反馈

### 三键操作

三键共用 GPIO0 ADC 分压，电压窗口来自官方仓库 `bsp_pins.h`
（UP 0~150mV / DOWN 150~447mV / OK 447~1900mV，松开约 3300mV）：

| 按键 | 亮屏时 | 息屏时 |
|---|---|---|
| UP / DOWN | 切换 A股页 ⇄ 币市页 | 唤醒屏幕（不翻页） |
| OK | 立即刷新当前页 | 唤醒屏幕（不刷新） |

### 自定义自选股

自选股清单目前是**编译期配置**，下载社区固件的用户暂不能直接修改（WiFi 可通过
串口配网，但股票清单与中文字库要重新编译）。想换监控的股票：

1. clone 仓库，用 VS Code + PlatformIO 打开
2. 只改 `include/config.h`：

```c
static const char* WATCHLIST[] = {"600519", "002594"};      // 自选股代码（6xx→上海，0xx/3xx→深圳）
static const char* WATCH_NAMES[] = {"贵州茅台", "比亚迪"};   // 与代码一一对应

#define CN_CHARSET "交易中断网失败休市配请用串口贵州茅台比亚迪"  // 名称用到的字 + UI 提示字
```

3. 重新生成字库并烧录：

```bash
python3 tools/gen_font.py     # 生成 include/stock_font.h
pio run -t upload             # 板子 USB 直连
```

> 提示：将来如需"下载即自定义自选股"，可在 USB 串口增加 `watch` 命令 +
> 全量 GB2312 字库 + 腾讯源 GBK 名称解码。当前为保持固件精简，字库只打包用到的字。

### 3. 编译烧录

```bash
pio run -t upload             # 板子 USB 直连
pio run -t monitor            # 查看串口日志（115200）
```

> **分区表说明**：为容纳 BLE 协议栈，项目改用官方 `partitions.csv`
> （factory 3MB，与 folotoy/ai-passport 一致，不触碰 0x700000 的官方 recovery 区）。
> 从旧版固件（默认 4MB 双 OTA 分区）升级烧录后，NVS 里保存的 WiFi 凭据可能丢失，
> 重新用串口或 BLE 配一次网即可。

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

### 8. 换官方分区表后无限复位（RTC_SW_SYS_RST 循环）

现象：烧录后串口只反复打印 ROM 启动头（`rst:0x3 ... entry 0x403cc710`），
约 30ms 一轮，没有任何应用日志。二级 bootloader 的日志走 UART0（GPIO21，
被背光占用），所以在 USB 口完全看不到失败原因。

根因：`board_build.flash_size = 8MB` **并不会改镜像头里的 flash 尺寸字节**
（`esp32-c3-devkitm-1` 板定义的 `upload.flash_size=4MB` 才是生效值，镜像头
第 4 字节为 `0x2?` = 4MB）。bootloader 按 4MB 校验分区表，官方分区表里
0x700000 起的 recovery 区"越界"→ 分区表整表被拒 → 软复位循环。

解法：`platformio.ini` 同时设置三项（缺一不可）：

```ini
board_build.flash_size = 8MB
board_upload.flash_size = 8MB
board_upload.maximum_size = 8388608
```

验证：`xxd -l 4 .pio/build/main/bootloader.bin` 第 4 字节应为 `0x3?`（3 = 8MB）。

## 目录结构

```
stockscreen/
├── platformio.ini              # 构建配置（main / test_screen 两个环境）
├── include/
│   ├── config.h                # ★ 用户配置：WiFi、自选股、引脚、刷新频率
│   ├── User_Setup.h            # TFT_eSPI 配置（ST7789、BGR、INVON、字体宏）
│   └── stock_font.h            # 中文字库（gen_font.py 生成，勿手改）
├── partitions.csv              # 官方分区布局（factory 3MB，避开 recovery 区）
├── src/
│   ├── main.cpp                # setup/loop：WiFi、抓取调度、翻页、深睡、电量采样
│   ├── stocks.h / stocks.cpp   # A股行情抓取与解析（东财/腾讯）、时间工具
│   ├── crypto.h / crypto.cpp   # 加密货币行情（CoinGecko/OKX，市值前五动态跟踪）
│   ├── net_http.h / net_http.cpp # HTTP GET / 响应头解析 / chunked 解码 / 日历
│   ├── ui.h / ui.cpp           # 屏幕渲染：状态栏 + 双页卡片 + 中文绘制
│   ├── buttons.h / buttons.cpp # 三键 ADC 区分（UP/DOWN/OK）+ 去抖
│   ├── blecfg.h / blecfg.cpp   # BLE 配网 + 行情推送（NimBLE，FFF0/FFF1/FFF2）
│   ├── powersave.h / powersave.cpp # 夜间深睡眠（定时 + 按键唤醒）
│   ├── battery.h / battery.cpp # CW2017 电量计驱动
│   ├── wificonfig.h / wificonfig.cpp # 配网命令处理（串口 + BLE 共用）+ NVS 存储
│   └── test_screen.cpp         # 屏幕诊断固件（env:test_screen）
├── lib/
│   └── TFT_eSPI/               # 本地修改版（C3 兼容补丁，见 README_PATCHES.md）
└── tools/
    └── gen_font.py             # 中文字形提取 → include/stock_font.h
```

## 已知限制

- 东财主源在 core 3.0.7（C3）上因 mbedTLS bug 不可用，当前固定走腾讯源（升级 core 可解）
- 自选股与中文名称是编译期配置：下载社区固件的用户需按[自定义自选股](#自定义自选股)自行编译修改
- 字库容量：`CN_CHARSET` 每个字约 32 字节 flash，只放用到的字
- 时钟精度依赖 WiFi 抓取频率，断网时走本地 millis 漂移
- 行情为公开数据接口，未做鉴权；`setInsecure()` 跳过证书校验（个人设备 + 公开数据的取舍，代码内有注释）

## 参考资料

- [folotoy/ai-passport](https://github.com/folotoy/ai-passport) — 官方固件，引脚/电量计参考
- [TRAE AI 通行证官网](https://ai-passport.folotoy.cn/trae/)
- [Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — 显示库（本项目使用本地修改版）
- [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32) — Arduino core 3.x
