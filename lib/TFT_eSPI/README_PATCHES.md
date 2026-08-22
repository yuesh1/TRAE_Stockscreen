# TFT_eSPI 本地修改版（勿直接升级覆盖）

本目录是 bodmer/TFT_eSPI 2.5.43 的修改版，适配
**ESP32-C3 + arduino-esp32 core 3.x**。

原库在 ESP32-C3 上用寄存器级 SPI 直推（`*_spi_cmd = SPI_USR` 忙等待），
与 arduino-esp32 3.x 的 SPIClass 外设管理冲突：满屏填充会挂死，
进而触发 TG1WDT 看门狗复位循环。

## 修改清单

| 文件 | 修改 |
|---|---|
| `User_Setup.h` | 整体替换为项目配置（`include/User_Setup.h` 的副本：ST7789、CS=1/DC=20 等）。**注意**：库编译时 `-include User_Setup.h` 会先命中本目录副本——改项目 `include/` 里的配置后必须重新拷贝一份过来 |
| `Processors/TFT_eSPI_ESP32_C3.h` | `SET_BUS_WRITE_MODE`/`SET_BUS_READ_MODE`/`SPI_BUSY_CHECK` 置空（SPIClass 同步传输无需忙等待）；C3 分支的 `TFT_WRITE_BITS`、`tft_Write_16N` 改为 `spi.transfer()` 逐字节发送（线上低字节先发，与原实现一致） |
| `Processors/TFT_eSPI_ESP32_C3.c` | `pushBlock` 改 `spi.writePattern()`；`pushPixels` 改 `spi.writeBytes()`；`pushSwapBytePixels` 改为缓冲两字节互换后 `spi.writeBytes()` |

配合项目 `include/User_Setup.h` 中的 `SUPPORT_TRANSACTIONS`（已定义）使用，
SPI 才会按 `SPI_FREQUENCY` 27MHz 走 SPISettings 事务，否则走总线默认低速。

## 字节序备忘（改动时对照）

- 原实现 `DAT8TO32 = P[0]<<8 | P[1] | P[2]<<24 | P[3]<<16` 写入小端 W 寄存器
  → FIFO 先发低字节；
- `pushBlock` 线上为 [hi, lo]；`pushPixels` = 内存顺序直发；
  `pushSwapBytePixels` = 每像素两字节互换。

## 升级 TFT_eSPI 时

在 `.pio/libdeps/` 安装新版后，按上表重新打补丁，再把
`include/User_Setup.h` 拷贝覆盖库目录副本，最后整体同步回 `lib/TFT_eSPI/`。
