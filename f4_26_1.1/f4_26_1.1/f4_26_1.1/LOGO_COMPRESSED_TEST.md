# Logo压缩传输测试指南

## 修改内容

### 1. logo.h
- 修改尺寸：154x154 → 240x240
- 添加解压缓冲区定义：`DECOMPRESS_BUFFER_SIZE 2048`

### 2. logo.c
- 添加RLE解压缩函数 `Logo_DecompressRLE()`
- 添加压缩传输协议支持 `LOGO_START_COMPRESSED`
- 修改 `Logo_ProcessBuffer()` 支持压缩/未压缩双模式
- 修改 `LOGO_END` 处理支持压缩校验

## 测试步骤

### 1. 编译烧录
```bash
# 在Keil MDK中编译
# 烧录到STM32
```

### 2. APP端测试
1. 打开RideWind APP
2. 连接蓝牙设备
3. 进入Logo上传界面（压缩优化版）
4. 选择图片（会自动调整到240x240）
5. 查看压缩信息（应显示30-40%压缩率）
6. 点击"开始上传"

### 3. 观察日志

#### 硬件端日志（串口）
```
[LOGO] COMPRESSED START orig=115200 comp=70000 crc=12345
[LOGO] Ready (compressed mode, 4375 packets)
[LOGO] Compressed: recv=10000/70000 decomp=16000/115200 (14%)
...
[LOGO] Decompression OK: 70000 -> 115200 bytes
[LOGO] CRC: expected=12345, calculated=12345
[LOGO] Upload complete! (compressed=1)
```

#### APP端日志
```
📤 发送: LOGO_START_COMPRESSED:115200:70000:12345
📥 硬件响应: LOGO_ERASING
📥 硬件响应: LOGO_READY
📤 发送包 1/4375
...
📥 硬件响应: LOGO_OK
✅ 上传成功
```

## 预期效果

| 项目 | 未压缩 | 压缩 |
|------|--------|------|
| 数据量 | 115200字节 | ~70000字节 |
| 包数量 | 7200包 | ~4375包 |
| 传输时间 | ~4分钟 | ~2.5分钟 |
| 内存占用 | 800字节 | 2848字节 |

## 故障排查

### 问题1：LOGO_ERROR:SIZE_MISMATCH
- 检查APP是否正确调整到240x240
- 检查硬件端LOGO_WIDTH/HEIGHT定义

### 问题2：LOGO_FAIL:DECOMP_SIZE
- 解压后大小不匹配
- 检查RLE解压缩算法
- 检查APP端压缩算法

### 问题3：LOGO_FAIL:CRC
- CRC校验失败
- 可能是解压错误或传输丢包
- 检查解压后的数据完整性

### 问题4：传输中断
- 检查缓冲区大小是否足够
- 检查Flash写入速度
- 增加调试日志定位问题

## 回退方案

如果压缩传输有问题，可以：
1. APP端使用 `logo_upload_screen.dart`（旧版，未压缩）
2. 硬件端仍支持 `LOGO_START` 命令（向后兼容）
