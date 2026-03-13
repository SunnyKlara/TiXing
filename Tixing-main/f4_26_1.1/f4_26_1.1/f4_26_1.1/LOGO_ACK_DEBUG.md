# Logo上传ACK调试指南

## 问题描述
进度卡在10%，APP不断重传相同的数据包，说明硬件没有发送ACK或APP没有收到ACK。

## 已添加的调试日志

### 1. ACK发送确认
每50包打印一次ACK发送确认：
```
[LOGO] ACK sent: seq=49 buf=10/50
```
- `seq`: 当前ACK的序号
- `buf`: 当前缓冲区包数/总容量

### 2. 缓冲区满警告
当缓冲区满时打印：
```
[LOGO] WARN: Buffer full at seq=100 (count=50)
```

### 3. 处理进度
每100包打印一次处理进度：
```
[LOGO] Processed 10 pkts: recv=1600/62547 decomp=3200/115200 buf=5
```
- `Processed X pkts`: 本次循环处理了多少包
- `recv`: 已接收压缩数据/总压缩数据
- `decomp`: 已解压数据/总原始数据
- `buf`: 当前缓冲区剩余包数

## 测试步骤

### 步骤1: 重新编译固件
1. 打开Keil MDK
2. 按F7编译
3. 按F8下载到硬件

### 步骤2: 上传Logo并观察日志
1. 打开APP的Logo上传调试界面
2. 选择图片开始上传
3. **重点观察以下信息**：

#### 正常情况应该看到：
```
[00:00:01.000] 🔧 硬件: BLE_len=46
[00:00:01.020] [LOGO] ACK sent: seq=9 buf=10/50
[00:00:01.040] 🔧 硬件: BLE_len=46
[00:00:01.060] 🔧 硬件: BLE_len=46
...
[00:00:01.200] [LOGO] ACK sent: seq=19 buf=10/50
[00:00:02.000] [LOGO] Processed 10 pkts: recv=160/62547 decomp=320/115200 buf=0
```

#### 异常情况1: ACK没有发送
如果看到大量`BLE_len=46`但没有`ACK sent`，说明：
- ACK发送逻辑有问题
- 或者`BLE_SendString()`函数失败

#### 异常情况2: 缓冲区满
如果看到：
```
[LOGO] WARN: Buffer full at seq=50 (count=50)
```
说明：
- `Logo_ProcessBuffer()`处理太慢
- Flash写入速度跟不上蓝牙接收速度
- 需要增大`PACKET_BUFFER_SIZE`或优化Flash写入

#### 异常情况3: 处理停滞
如果看到：
```
[LOGO] Processed 0 pkts: ...
```
说明：
- `Logo_ProcessBuffer()`没有被调用
- 或者缓冲区为空但数据还在传输

## 可能的问题和解决方案

### 问题1: ACK被printf阻塞
**症状**: 看到`BLE_len=46`重复出现，但没有ACK
**原因**: printf输出太多，阻塞了蓝牙发送
**解决**: 减少printf输出频率（已优化为每50包一次）

### 问题2: 缓冲区处理太慢
**症状**: 频繁出现`Buffer full`
**原因**: Flash写入速度慢，缓冲区来不及清空
**解决**: 
- 增大`PACKET_BUFFER_SIZE`（当前50）
- 优化`Logo_ProcessBuffer()`批量处理

### 问题3: BLE_SendString失败
**症状**: 看到`ACK sent`但APP没收到
**原因**: 蓝牙发送缓冲区满或发送失败
**解决**: 检查`BLE_SendString()`实现，确保发送成功

### 问题4: APP端过滤问题
**症状**: 硬件发送了ACK，但APP没有处理
**原因**: APP端可能过滤掉了ACK消息
**解决**: 检查APP端`_setupResponseListener()`

## 下一步调试

根据上传时的日志，我们可以判断：

1. **如果看到ACK sent但进度卡住** → APP端接收问题
2. **如果看不到ACK sent** → 硬件端发送问题
3. **如果看到Buffer full** → 处理速度问题
4. **如果Processed=0** → 主循环调用问题

请上传一次Logo，把完整的日志发给我，我会根据日志进一步分析。
