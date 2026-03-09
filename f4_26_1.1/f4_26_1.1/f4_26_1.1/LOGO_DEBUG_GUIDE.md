# Logo上传调试指南

## 调试日志说明

已在APP端添加详细的调试日志，所有日志都以 `[LOGO_UPLOAD]` 开头，方便过滤查看。

### 日志级别标识
- 🚀 开始流程
- ✅ 成功/确认
- ❌ 失败/错误
- ⚠️ 警告
- 📤 发送数据
- 📥 接收数据
- ⏳ 等待中
- ⏰ 超时
- 📊 进度统计
- 🔄 循环/重试
- 🔥 Flash操作
- 🎉 完成

## 查看日志的方法

### Android Studio / VS Code
1. 打开终端
2. 运行: `flutter run`
3. 或者查看 Logcat: `adb logcat | grep LOGO_UPLOAD`

### 完整日志示例 (正常流程)

```
[LOGO_UPLOAD] 🚀 开始上传流程
[LOGO_UPLOAD] ✅ 图片处理完成: 47432 字节
[LOGO_UPLOAD] 📦 总包数: 2965, CRC32: 1234567890
[LOGO_UPLOAD] 📤 发送LOGO_START命令...
[LOGO_UPLOAD] 📥 收到响应: LOGO_ERASING
[LOGO_UPLOAD] 🔥 Flash擦除中...
[LOGO_UPLOAD] 📥 擦除后响应: LOGO_READY
[LOGO_UPLOAD] ✅ 硬件已就绪，开始传输...
[LOGO_UPLOAD] 🔄 进入传输循环，总包数: 2965
[LOGO_UPLOAD] 📤 发送包 0/2965
[LOGO_UPLOAD] ⏳ 等待ACK (seq=9)...
[LOGO_UPLOAD] 📥 收到: LOGO_ACK:9
[LOGO_UPLOAD] ✅ ACK确认: 9 (当前seq=9)
[LOGO_UPLOAD] 📤 发送包 100/2965
[LOGO_UPLOAD] 📊 进度: 100/2965 (8%)
[LOGO_UPLOAD] ⏳ 等待ACK (seq=109)...
[LOGO_UPLOAD] 📥 收到: LOGO_ACK:109
[LOGO_UPLOAD] ✅ ACK确认: 109 (当前seq=109)
...
[LOGO_UPLOAD] 📤 发送包 2900/2965
[LOGO_UPLOAD] 📊 进度: 2900/2965 (93%)
[LOGO_UPLOAD] ✅ 传输完成，发送LOGO_END...
[LOGO_UPLOAD] 📥 LOGO_END响应: LOGO_OK
[LOGO_UPLOAD] 🎉 上传成功！用时: 45秒
```

## 常见问题诊断

### 问题1: 进度条卡在0%

**日志特征**:
```
[LOGO_UPLOAD] 🚀 开始上传流程
[LOGO_UPLOAD] ✅ 图片处理完成: 47432 字节
[LOGO_UPLOAD] 📦 总包数: 2965, CRC32: 1234567890
[LOGO_UPLOAD] 📤 发送LOGO_START命令...
[LOGO_UPLOAD] 📥 收到响应: TIMEOUT
[LOGO_UPLOAD] ⏰ 超时，尝试GET:LOGO...
[LOGO_UPLOAD] 📥 GET:LOGO响应: TIMEOUT
[LOGO_UPLOAD] ❌ 上传失败: Exception: 硬件无响应，请重新编译固件
```

**原因**: 硬件端没有响应LOGO_START命令
**解决**:
1. 检查固件是否已烧录最新版本
2. 检查串口是否正常 (在Keil中查看printf输出)
3. 检查`Logo_ParseCommand`是否被调用

---

### 问题2: 卡在包9

**日志特征**:
```
[LOGO_UPLOAD] 📤 发送包 0/2965
[LOGO_UPLOAD] ⏳ 等待ACK (seq=9)...
[LOGO_UPLOAD] 📥 收到: TIMEOUT
[LOGO_UPLOAD] ⏰ 超时！重试次数: 1/5
[LOGO_UPLOAD] ⏳ 等待ACK (seq=9)...
[LOGO_UPLOAD] 📥 收到: TIMEOUT
[LOGO_UPLOAD] ⏰ 超时！重试次数: 2/5
...
```

**原因**: 硬件端没有发送ACK
**解决**:
1. 检查硬件端是否调用了`Logo_ProcessBuffer()`
2. 检查缓冲区是否正常工作
3. 查看硬件端串口输出，确认是否收到包0-9

---

### 问题3: 频繁出现LOGO_BUSY

**日志特征**:
```
[LOGO_UPLOAD] 📤 发送包 100/2965
[LOGO_UPLOAD] ⏳ 等待ACK (seq=109)...
[LOGO_UPLOAD] 📥 收到: LOGO_BUSY:109
[LOGO_UPLOAD] ⚠️ 硬件忙，等待200ms...
[LOGO_UPLOAD] ⏳ 等待ACK (seq=109)...
[LOGO_UPLOAD] 📥 收到: LOGO_ACK:109
```

**原因**: 硬件缓冲区满，处理速度跟不上
**解决**:
1. 增大`PACKET_BUFFER_SIZE` (在logo.c中)
2. 增加`Logo_ProcessBuffer`中的批量处理数量
3. 优化Flash写入速度 (使用DMA)

---

### 问题4: ACK序号不匹配

**日志特征**:
```
[LOGO_UPLOAD] ⏳ 等待ACK (seq=109)...
[LOGO_UPLOAD] 📥 收到: LOGO_ACK:99
[LOGO_UPLOAD] ⚠️ ACK序号不匹配，从100继续
```

**原因**: 硬件端丢包或ACK发送错误
**解决**:
1. 检查硬件端ACK逻辑
2. 确认`(seq + 1) % 10 == 0`的判断是否正确
3. 检查是否有包被重复处理

---

### 问题5: CRC校验失败

**日志特征**:
```
[LOGO_UPLOAD] ✅ 传输完成，发送LOGO_END...
[LOGO_UPLOAD] 📥 LOGO_END响应: LOGO_FAIL:CRC:987654321
[LOGO_UPLOAD] ❌ CRC校验失败: LOGO_FAIL:CRC:987654321
```

**原因**: 数据传输过程中有错误
**解决**:
1. 检查是否有包丢失
2. 检查Flash写入是否正确
3. 对比APP端和硬件端的CRC32算法

## 截图要求

当出现问题时，请提供以下截图:

### 1. APP界面截图
- 显示进度条、状态文本、调试信息框

### 2. 日志截图
- 完整的日志输出 (从开始到失败)
- 使用命令: `adb logcat | grep LOGO_UPLOAD > logo_debug.txt`

### 3. 硬件端串口输出
- 在Keil中查看printf输出
- 或使用串口助手查看USART2输出

## 硬件端调试

### 添加调试输出
在`logo.c`中已有的printf输出:
```c
printf("[LOGO] START size=%lu crc=%lu\r\n", size, crc);
printf("[LOGO] Ready to receive %lu packets\r\n", totalPackets);
printf("[LOGO] Processed: %lu/%lu (%d%%)\r\n", received, total, progress);
printf("[LOGO] Upload complete!\r\n");
```

### 查看串口输出
1. 连接ST-Link
2. 打开串口助手 (115200波特率)
3. 连接到USART2 (PA2-TX, PA3-RX)
4. 观察输出

## 性能指标

### 正常情况
- 传输时间: 40-60秒
- 丢包率: <5%
- 重传次数: <10次
- LOGO_BUSY出现: 0-5次

### 异常情况
- 传输时间: >120秒 → 信号差或硬件处理慢
- 丢包率: >15% → 蓝牙连接不稳定
- 重传次数: >50次 → 协议同步问题
- LOGO_BUSY出现: >20次 → 缓冲区太小

## 快速诊断流程

1. **检查连接**: 运行"测试通信"按钮，确认硬件响应
2. **查看日志**: 找到第一个错误/超时的位置
3. **对照上面的常见问题**: 找到对应的解决方案
4. **提供截图**: 如果无法解决，提供完整日志和截图

## 联系信息

如果问题无法解决，请提供:
1. APP日志 (完整的LOGO_UPLOAD日志)
2. 硬件串口输出
3. APP界面截图
4. 固件版本和编译时间
