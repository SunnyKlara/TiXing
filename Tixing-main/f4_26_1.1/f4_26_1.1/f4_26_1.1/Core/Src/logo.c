/**
  ******************************************************************************
  * @file    logo.c
  * @brief   自定义Logo上传与显示模块实现
  * @note    蓝牙协议:
  *          - LOGO_START:size:crc32  开始传输
  *          - LOGO_DATA:seq:hex      分包数据 (十六进制字符串)
  *          - LOGO_END               传输结束
  *          - GET:LOGO               查询是否有自定义Logo
  *          - LOGO_DELETE            删除自定义Logo
  ******************************************************************************
  */

#include "logo.h"
#include "w25q128.h"
#include "lcd.h"
#include "lcd_init.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 外部引用默认Logo图片数组 (定义在lcd.c通过pic.h)
extern const unsigned char gImage_tou_xiang_154_154[];

// 外部引用UI变量（用于判断当前界面）
extern u8 ui, chu;

// ╔══════════════════════════════════════════════════════════════╗
// ║          外部函数引用                                         ║
// ╚══════════════════════════════════════════════════════════════╝
extern void BLE_SendString(const char* str);
extern void LCD_Writ_Bus(uint8_t *data, uint16_t size);

// ╔══════════════════════════════════════════════════════════════╗
// ║          调试日志函数 - 通过蓝牙发送给APP                      ║
// ╚══════════════════════════════════════════════════════════════╝
static void Logo_SendDebugLog(const char* message) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "DEBUG:%s\n", message);
    BLE_SendString(buffer);
    printf("[LOGO_DEBUG] %s\r\n", message);  // 同时输出到串口
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          LCD调试显示函数 - 在屏幕上显示调试信息                ║
// ╚══════════════════════════════════════════════════════════════╝
#define LCD_DEBUG_MAX_LINES 10
static char lcd_debug_lines[LCD_DEBUG_MAX_LINES][40];
static int lcd_debug_line_count = 0;

static void Logo_ShowDebugOnLCD(const char* message) {
    // 添加新行到缓冲区
    if (lcd_debug_line_count < LCD_DEBUG_MAX_LINES) {
        snprintf(lcd_debug_lines[lcd_debug_line_count], 40, "%s", message);
        lcd_debug_line_count++;
    } else {
        // 缓冲区满，滚动显示（删除第一行，添加到最后）
        for (int i = 0; i < LCD_DEBUG_MAX_LINES - 1; i++) {
            strcpy(lcd_debug_lines[i], lcd_debug_lines[i + 1]);
        }
        snprintf(lcd_debug_lines[LCD_DEBUG_MAX_LINES - 1], 40, "%s", message);
    }
    
    // 只在Logo调试界面(ui=6)时显示
    if (ui != 6) {
        return;
    }
    
    // 清屏
    LCD_Fill(0, 0, 240, 240, BLACK);
    
    // 显示标题
    LCD_ShowString(10, 10, (u8*)"LOGO DEBUG", WHITE, BLACK, 16, 0);
    LCD_ShowString(10, 30, (u8*)"====================", WHITE, BLACK, 12, 0);
    
    // 显示所有调试行
    for (int i = 0; i < lcd_debug_line_count; i++) {
        LCD_ShowString(10, 45 + i * 15, (u8*)lcd_debug_lines[i], GREEN, BLACK, 12, 0);
    }
}

static void Logo_AddDebugLog(const char* message) {
    // 同时发送到蓝牙和LCD
    Logo_SendDebugLog(message);
    Logo_ShowDebugOnLCD(message);
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          接收缓冲区结构体 - 解决Flash写入阻塞问题             ║
// ╚══════════════════════════════════════════════════════════════╝

#define PACKET_BUFFER_SIZE 200  // 🔥 从100增加到200，减少BUSY，提高吞吐量
#define FLASH_WRITE_BATCH_SIZE 16  // 🔥 批量写入：每16包(256字节=1页)写一次Flash，利用W25Q128页写入优化

typedef struct {
    uint32_t seq;           // 包序号
    uint8_t data[16];       // 包数据
    uint8_t len;            // 数据长度
    bool valid;             // 是否有效
} PacketBuffer_t;

typedef struct {
    PacketBuffer_t packets[PACKET_BUFFER_SIZE];  // 包缓冲区
    uint32_t writeIndex;    // 写入索引
    uint32_t readIndex;     // 读取索引
    uint32_t count;         // 当前包数量
    
    uint32_t lastAckSeq;    // 最后ACK的序号
    uint32_t totalPackets;  // 总包数
    
    // 🔥 批量写入缓冲区
    uint8_t flashWriteBuffer[FLASH_WRITE_BATCH_SIZE * 16];  // 批量写入缓冲区
    uint32_t flashWriteCount;  // 当前缓冲区包数
    uint32_t flashWriteBaseSeq;  // 批量写入的起始序号
    uint32_t batchByteCount;    // 🔧 新增：当前批次累计字节数（修复最后一包不足16字节的bug）
} ReceiveWindow_t;

// ╔══════════════════════════════════════════════════════════════╗
// ║          私有变量                                             ║
// ╚══════════════════════════════════════════════════════════════╝

static LogoState_t logo_state = LOGO_STATE_IDLE;
static uint32_t logo_total_size = 0;
static uint32_t logo_received_size = 0;
static uint32_t logo_expected_crc = 0;
static uint32_t logo_current_seq = 0;
static uint8_t  logo_temp_buffer[256];  // 临时缓冲区

static uint32_t logo_flash_written = 0;  // 🔧 新增：已写入Flash的字节数

// 多槽位管理变量
static uint8_t logo_active_slot = 0;     // 当前激活的槽位 (开机显示)
static uint8_t logo_current_slot = 0;    // 当前上传目标槽位

static ReceiveWindow_t g_receiveWindow;  // 接收窗口

// RLE解压缩相关变量
static uint8_t  logo_is_compressed = 0;      // 是否压缩传输
static uint32_t logo_original_size = 0;      // 原始大小（解压后）
static uint32_t logo_compressed_size = 0;    // 压缩大小（传输）
static uint32_t logo_decompressed_total = 0; // 已解压总量
static uint8_t  decompress_buffer[DECOMPRESS_BUFFER_SIZE]; // 解压缓冲区

// 错误显示标志 - 防止UI切换时清除错误信息
static bool logo_error_displayed = false;   // 错误信息是否正在显示

// ╔══════════════════════════════════════════════════════════════╗
// ║          缓冲区操作函数 - 解决Flash写入阻塞问题               ║
// ╚══════════════════════════════════════════════════════════════╝

// 初始化缓冲区
static void Buffer_Init(void)
{
    memset(&g_receiveWindow, 0, sizeof(g_receiveWindow));
    g_receiveWindow.writeIndex = 0;
    g_receiveWindow.readIndex = 0;
    g_receiveWindow.count = 0;
    g_receiveWindow.lastAckSeq = 0;
    g_receiveWindow.flashWriteCount = 0;
    g_receiveWindow.flashWriteBaseSeq = 0;
    g_receiveWindow.batchByteCount = 0;  // 🔧 新增：初始化批次字节计数
}

// 检查缓冲区是否已满
static bool Buffer_IsFull(void)
{
    return g_receiveWindow.count >= PACKET_BUFFER_SIZE;
}

// 检查缓冲区是否为空
static bool Buffer_IsEmpty(void)
{
    return g_receiveWindow.count == 0;
}

// 添加包到缓冲区 (在蓝牙接收中断中调用)
static bool Buffer_Push(uint32_t seq, const uint8_t* data, uint8_t len)
{
    if (Buffer_IsFull()) {
        return false;  // 缓冲区满，丢弃包
    }
    
    PacketBuffer_t* packet = &g_receiveWindow.packets[g_receiveWindow.writeIndex];
    packet->seq = seq;
    packet->len = len;
    memcpy(packet->data, data, len);
    packet->valid = true;
    
    g_receiveWindow.writeIndex = (g_receiveWindow.writeIndex + 1) % PACKET_BUFFER_SIZE;
    g_receiveWindow.count++;
    
    return true;
}

// 从缓冲区取出包 (在主循环中调用)
static bool Buffer_Pop(PacketBuffer_t* outPacket)
{
    if (Buffer_IsEmpty()) {
        return false;
    }
    
    PacketBuffer_t* packet = &g_receiveWindow.packets[g_receiveWindow.readIndex];
    if (!packet->valid) {
        return false;
    }
    
    // 复制数据
    memcpy(outPacket, packet, sizeof(PacketBuffer_t));
    
    // 标记为已处理
    packet->valid = false;
    
    g_receiveWindow.readIndex = (g_receiveWindow.readIndex + 1) % PACKET_BUFFER_SIZE;
    g_receiveWindow.count--;
    
    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          CRC32计算 (标准多项式 0x04C11DB7)                    ║
// ╚══════════════════════════════════════════════════════════════╝

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

static uint32_t CRC32_Calculate(uint8_t* data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

// 从Flash计算CRC32
static uint32_t CRC32_CalculateFlash(uint32_t addr, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buffer[256];
    uint32_t remaining = len;
    uint32_t readAddr = addr;
    
    while (remaining > 0) {
        uint16_t readLen = (remaining > 256) ? 256 : remaining;
        W25Q128_BufferRead(buffer, readAddr, readLen);
        
        for (uint16_t i = 0; i < readLen; i++) {
            crc = (crc >> 8) ^ crc32_table[(crc ^ buffer[i]) & 0xFF];
        }
        
        readAddr += readLen;
        remaining -= readLen;
    }
    
    return crc ^ 0xFFFFFFFF;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          RLE解压缩函数                                        ║
// ╚══════════════════════════════════════════════════════════════╝

/**
 * @brief RLE解压缩（流式处理）
 * @param compressed_data 压缩数据
 * @param compressed_size 压缩数据大小
 * @param output_buffer 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 解压后的实际大小
 * 
 * RLE格式:
 * - 压缩块: 1xxxxxxx vv vv (高位=1, 低7位=重复次数-1, 后跟2字节RGB565)
 * - 原始块: 0xxxxxxx dd dd ... (高位=0, 低7位=像素数-1, 后跟原始数据)
 */
static uint32_t Logo_DecompressRLE(
    const uint8_t* compressed_data, 
    uint32_t compressed_size,
    uint8_t* output_buffer,
    uint32_t output_size
) {
    uint32_t in_pos = 0;
    uint32_t out_pos = 0;
    
    while (in_pos < compressed_size && out_pos < output_size) {
        uint8_t header = compressed_data[in_pos++];
        
        if (header & 0x80) {
            // RLE压缩块: 1xxxxxxx vv vv
            uint8_t count = (header & 0x7F) + 1;  // 1-128像素
            
            if (in_pos + 1 >= compressed_size) break;
            uint8_t byte1 = compressed_data[in_pos++];
            uint8_t byte2 = compressed_data[in_pos++];
            
            // 重复写入count次
            for (uint8_t i = 0; i < count && out_pos + 1 < output_size; i++) {
                output_buffer[out_pos++] = byte1;
                output_buffer[out_pos++] = byte2;
            }
        } else {
            // 原始数据块: 0xxxxxxx dd dd dd ...
            uint8_t count = (header & 0x7F) + 1;  // 1-128像素
            uint16_t bytes = count * 2;
            
            if (in_pos + bytes > compressed_size) break;
            if (out_pos + bytes > output_size) break;
            
            // 直接复制
            memcpy(&output_buffer[out_pos], &compressed_data[in_pos], bytes);
            in_pos += bytes;
            out_pos += bytes;
        }
    }
    
    return out_pos;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          十六进制字符串解码                                   ║
// ╚══════════════════════════════════════════════════════════════╝

static int HexChar2Int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// 十六进制字符串转字节数组（优化版：不使用strlen）
// 返回解码后的字节数，失败返回-1
static int HexDecode(const char* hex, uint8_t* out, int maxLen)
{
    int outLen = 0;
    
    // 🔥 优化：不使用strlen，直接解码直到遇到结束符或非十六进制字符
    while (outLen < maxLen) {
        char c1 = hex[outLen * 2];
        char c2 = hex[outLen * 2 + 1];
        
        // 遇到结束符或非十六进制字符，停止
        if (c1 == '\0' || c2 == '\0' || c1 == '\n' || c1 == '\r') {
            break;
        }
        
        int high = HexChar2Int(c1);
        int low = HexChar2Int(c2);
        
        if (high < 0 || low < 0) {
            break;  // 遇到非十六进制字符，停止
        }
        
        out[outLen] = (high << 4) | low;
        outLen++;
    }
    
    return outLen;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          公共函数实现                                         ║
// ╚══════════════════════════════════════════════════════════════╝

void Logo_Init(void)
{
    logo_state = LOGO_STATE_IDLE;
    logo_received_size = 0;
    logo_current_seq = 0;
    logo_current_slot = 0;
    Buffer_Init();
    
    // 加载配置
    Logo_LoadConfig();
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          多槽位管理函数实现                                   ║
// ╚══════════════════════════════════════════════════════════════╝

/**
 * @brief 获取指定槽位的Flash地址
 * @param slot 槽位编号 (0-2)
 * @return Flash地址
 */
uint32_t Logo_GetSlotAddress(uint8_t slot)
{
    if (slot >= LOGO_MAX_SLOTS) {
        slot = 0;  // 无效槽位默认返回槽位0
    }
    return LOGO_SLOT_ADDR(slot);
}

/**
 * @brief 检查指定槽位是否有有效Logo
 * @param slot 槽位编号 (0-2)
 * @return true=有效, false=无效或空
 */
bool Logo_IsSlotValid(uint8_t slot)
{
    if (slot >= LOGO_MAX_SLOTS) {
        return false;
    }
    
    uint32_t addr = Logo_GetSlotAddress(slot);
    LogoHeader_t header;
    W25Q128_BufferRead((uint8_t*)&header, addr, sizeof(header));
    
    return (header.magic == LOGO_MAGIC && 
            header.width == LOGO_WIDTH && 
            header.height == LOGO_HEIGHT &&
            header.dataSize == LOGO_DATA_SIZE);
}

/**
 * @brief 统计有效槽位数量
 * @return 有效槽位数 (0-3)
 */
uint8_t Logo_CountValidSlots(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < LOGO_MAX_SLOTS; i++) {
        if (Logo_IsSlotValid(i)) {
            count++;
        }
    }
    return count;
}

/**
 * @brief 获取下一个有效槽位 (循环)
 * @param current 当前槽位
 * @return 下一个有效槽位，无有效槽位返回current
 */
uint8_t Logo_NextValidSlot(uint8_t current)
{
    for (uint8_t i = 1; i <= LOGO_MAX_SLOTS; i++) {
        uint8_t next = (current + i) % LOGO_MAX_SLOTS;
        if (Logo_IsSlotValid(next)) {
            return next;
        }
    }
    return current;  // 无其他有效槽位
}

/**
 * @brief 获取上一个有效槽位 (循环)
 * @param current 当前槽位
 * @return 上一个有效槽位，无有效槽位返回current
 */
uint8_t Logo_PrevValidSlot(uint8_t current)
{
    for (uint8_t i = 1; i <= LOGO_MAX_SLOTS; i++) {
        uint8_t prev = (current + LOGO_MAX_SLOTS - i) % LOGO_MAX_SLOTS;
        if (Logo_IsSlotValid(prev)) {
            return prev;
        }
    }
    return current;  // 无其他有效槽位
}

/**
 * @brief 设置激活槽位 (开机显示)
 * @param slot 槽位编号 (0-2)
 */
void Logo_SetActiveSlot(uint8_t slot)
{
    if (slot < LOGO_MAX_SLOTS) {
        logo_active_slot = slot;
    }
}

/**
 * @brief 获取激活槽位
 * @return 激活槽位编号
 */
uint8_t Logo_GetActiveSlot(void)
{
    return logo_active_slot;
}

/**
 * @brief 保存配置到Flash
 */
void Logo_SaveConfig(void)
{
    LogoConfig_t config;
    config.magic = LOGO_CONFIG_MAGIC;
    config.active_slot = logo_active_slot;
    config.reserved = 0;
    config.checksum = 0;  // 简化：暂不计算CRC
    
    // 擦除配置扇区
    W25Q128_EraseSector(LOGO_CONFIG_ADDR);
    
    // 写入配置
    W25Q128_BufferWrite((uint8_t*)&config, LOGO_CONFIG_ADDR, sizeof(config));
    
    printf("[LOGO] Config saved: active_slot=%d\r\n", logo_active_slot);
}

/**
 * @brief 从Flash加载配置
 */
void Logo_LoadConfig(void)
{
    LogoConfig_t config;
    W25Q128_BufferRead((uint8_t*)&config, LOGO_CONFIG_ADDR, sizeof(config));
    
    if (config.magic == LOGO_CONFIG_MAGIC && config.active_slot < LOGO_MAX_SLOTS) {
        logo_active_slot = config.active_slot;
        printf("[LOGO] Config loaded: active_slot=%d\r\n", logo_active_slot);
    } else {
        // 配置无效，使用默认值
        logo_active_slot = 0;
        printf("[LOGO] Config invalid, using default slot 0\r\n");
    }
}

/**
 * @brief 删除指定槽位的Logo
 * @param slot 槽位编号 (0-2)
 * @note 擦除槽位的Flash扇区使其无效
 *       如果删除的是激活槽位，自动切换到下一个有效槽位
 */
void Logo_DeleteSlot(uint8_t slot)
{
    if (slot >= LOGO_MAX_SLOTS) {
        printf("[LOGO] DeleteSlot: invalid slot %d\r\n", slot);
        return;
    }
    
    uint32_t addr = Logo_GetSlotAddress(slot);
    
    // 擦除槽位的Flash扇区（清除magic标志使其无效）
    // 只需擦除第一个扇区即可使header无效
    W25Q128_EraseSector(addr);
    
    printf("[LOGO] Slot %d deleted (addr=0x%08lX)\r\n", slot, (unsigned long)addr);
    
    // 如果删除的是激活槽位，重置为第一个有效槽位
    if (slot == Logo_GetActiveSlot()) {
        for (uint8_t i = 0; i < LOGO_MAX_SLOTS; i++) {
            if (Logo_IsSlotValid(i)) {
                Logo_SetActiveSlot(i);
                Logo_SaveConfig();
                printf("[LOGO] Active slot changed to %d after deletion\r\n", i);
                return;
            }
        }
        // 无有效槽位，重置为0
        Logo_SetActiveSlot(0);
        Logo_SaveConfig();
        printf("[LOGO] No valid slots, active slot reset to 0\r\n");
    }
}

/**
 * @brief 查找第一个空槽位
 * @return 空槽位编号 (0-2), 无空槽位返回 LOGO_MAX_SLOTS
 */
uint8_t Logo_FindEmptySlot(void)
{
    for (uint8_t i = 0; i < LOGO_MAX_SLOTS; i++) {
        if (!Logo_IsSlotValid(i)) {
            printf("[LOGO] Found empty slot: %d\r\n", i);
            return i;
        }
    }
    printf("[LOGO] No empty slot found\r\n");
    return LOGO_MAX_SLOTS;  // 无空槽位
}

/**
 * @brief 获取自动上传目标槽位
 * @return 目标槽位编号 (0-2)
 * @note 优先返回空槽位，所有槽位都满时返回0（自动覆盖最旧的）
 */
uint8_t Logo_GetAutoUploadSlot(void)
{
    // 优先查找空槽位
    for (uint8_t i = 0; i < LOGO_MAX_SLOTS; i++) {
        if (!Logo_IsSlotValid(i)) {
            printf("[LOGO] Auto upload slot: %d (empty)\r\n", i);
            return i;
        }
    }
    // 所有槽位都满，返回Slot 0（自动覆盖最旧的）
    printf("[LOGO] Auto upload slot: 0 (all slots full, overwrite)\r\n");
    return 0;
}

/**
 * @brief 显示指定槽位的Logo
 * @param slot 槽位编号 (0-2)
 * @return true=显示成功, false=槽位无效
 */
bool Logo_ShowSlot(uint8_t slot)
{
    if (slot >= LOGO_MAX_SLOTS || !Logo_IsSlotValid(slot)) {
        return false;
    }
    
    uint32_t addr = Logo_GetSlotAddress(slot);
    
    // 读取头部确认参数
    LogoHeader_t header;
    W25Q128_BufferRead((uint8_t*)&header, addr, sizeof(header));
    
    // 设置LCD显示区域 (居中显示)
    uint16_t x = (240 - header.width) / 2;
    uint16_t y = (240 - header.height) / 2;
    LCD_Address_Set(x, y, x + header.width - 1, y + header.height - 1);
    
    // 分块读取Flash并发送到LCD
    uint8_t buffer[512];
    uint32_t remaining = header.dataSize;
    uint32_t readAddr = addr + LOGO_HEADER_SIZE;
    
    while (remaining > 0) {
        uint16_t readLen = (remaining > 512) ? 512 : remaining;
        W25Q128_BufferRead(buffer, readAddr, readLen);
        LCD_Writ_Bus(buffer, readLen);
        readAddr += readLen;
        remaining -= readLen;
    }
    
    printf("[LOGO] Slot %d displayed (%dx%d)\r\n", slot, header.width, header.height);
    return true;
}

bool Logo_IsValid(void)
{
    LogoHeader_t header;
    W25Q128_BufferRead((uint8_t*)&header, LOGO_FLASH_ADDR, sizeof(header));
    
    return (header.magic == LOGO_MAGIC && 
            header.width == LOGO_WIDTH && 
            header.height == LOGO_HEIGHT &&
            header.dataSize == LOGO_DATA_SIZE);
}

LogoState_t Logo_GetState(void)
{
    return logo_state;
}

uint8_t Logo_GetProgress(void)
{
    if (logo_total_size == 0) return 0;
    return (uint8_t)((logo_received_size * 100) / logo_total_size);
}

// 检查是否有错误信息正在显示
bool Logo_IsErrorDisplayed(void)
{
    return logo_error_displayed;
}

// 清除错误显示标志
void Logo_ClearErrorFlag(void)
{
    logo_error_displayed = false;
}

void Logo_Delete(void)
{
    // 只需擦除第一个扇区，使header无效
    W25Q128_EraseSector(LOGO_FLASH_ADDR);
    printf("[LOGO] Deleted\r\n");
}


// ╔══════════════════════════════════════════════════════════════╗
// ║          蓝牙命令解析                                         ║
// ╚══════════════════════════════════════════════════════════════╝

void Logo_ParseCommand(char* cmd)
{
    char response[64];
    
    // 🔧 优化：只在START命令时发送调试信息
    if (strncmp(cmd, "LOGO_START", 10) == 0) {
        Logo_AddDebugLog("START CMD RCV");
    }
    
    printf("[LOGO] ParseCommand: %s\r\n", cmd);
    
    // ════════════════════════════════════════════════════════════
    // LOGO_START_COMPRESSED:原始大小:压缩大小:CRC32 - 压缩传输
    // ════════════════════════════════════════════════════════════
    if (strncmp(cmd, "LOGO_START_COMPRESSED:", 22) == 0)
    {
        // 清除错误显示标志（开始新的上传）
        logo_error_displayed = false;
        
        // 自动切换到Logo调试界面
        ui = 6;
        chu = 6;
        
        Logo_AddDebugLog("COMPRESSED CMD");
        
        uint32_t orig_size = 0, comp_size = 0, crc = 0;
        
        // 解析参数: LOGO_START_COMPRESSED:115200:70000:12345
        char* p = cmd + 22;
        orig_size = strtoul(p, &p, 10);
        if (*p == ':') {
            comp_size = strtoul(p + 1, &p, 10);
        }
        if (*p == ':') {
            crc = strtoul(p + 1, NULL, 10);
        }
        
        Logo_AddDebugLog("PARSE DONE");
        
        printf("[LOGO] COMPRESSED START orig=%u comp=%u crc=%u\r\n", 
               (unsigned int)orig_size, (unsigned int)comp_size, (unsigned int)crc);
        
        // 校验原始大小
        if (orig_size != LOGO_DATA_SIZE) {
            Logo_AddDebugLog("SIZE MISMATCH");
            sprintf(response, "LOGO_ERROR:SIZE_MISMATCH:%lu\n", (unsigned long)LOGO_DATA_SIZE);
            BLE_SendString(response);
            return;
        }
        
        Logo_AddDebugLog("ERASING FLASH...");
        
        // 开始擦除Flash (240x240需要29个扇区: 115200/4096≈29)
        logo_state = LOGO_STATE_ERASING;
        BLE_SendString("LOGO_ERASING\n");
        
        for (int i = 0; i < 29; i++) {
            W25Q128_EraseSector(LOGO_FLASH_ADDR + i * 4096);
        }
        
        Logo_AddDebugLog("ERASE DONE");
        
        // 初始化压缩接收状态
        logo_is_compressed = 1;
        logo_original_size = orig_size;
        logo_compressed_size = comp_size;
        logo_total_size = comp_size;  // 接收压缩数据
        logo_received_size = 0;
        logo_decompressed_total = 0;
        logo_expected_crc = crc;
        logo_current_seq = 0;
        logo_state = LOGO_STATE_RECEIVING;
        
        // 初始化接收缓冲区
        Buffer_Init();
        g_receiveWindow.totalPackets = (comp_size + 15) / 16;
        g_receiveWindow.lastAckSeq = 0;
        
        Logo_AddDebugLog("READY");
        
        BLE_SendString("LOGO_READY\n");
        printf("[LOGO] Ready (compressed mode, %lu packets)\r\n", 
               (unsigned long)g_receiveWindow.totalPackets);
    }
    
    // ════════════════════════════════════════════════════════════
    // LOGO_START:size:crc32 - 开始传输（未压缩，默认槽位0）
    // LOGO_START:slot:size:crc32 - 开始传输（指定槽位）
    // ════════════════════════════════════════════════════════════
    else if (strncmp(cmd, "LOGO_START:", 11) == 0)
    {
        // 清除错误显示标志（开始新的上传）
        logo_error_displayed = false;
        
        // 自动切换到Logo调试界面
        ui = 6;
        chu = 6;
        
        Logo_AddDebugLog("START CMD RCV");
        
        uint32_t size = 0, crc = 0;
        uint8_t slot = 0;  // 默认槽位0
        
        // 解析参数 - 支持两种格式:
        // LOGO_START:size:crc32 (两参数，自动选择槽位)
        // LOGO_START:slot:size:crc32 (三参数，指定slot)
        char* p = cmd + 11;
        uint32_t first_num = strtoul(p, &p, 10);
        
        if (*p == ':') {
            uint32_t second_num = strtoul(p + 1, &p, 10);
            if (*p == ':') {
                // 三参数格式: slot:size:crc32
                slot = (uint8_t)first_num;
                size = second_num;
                crc = strtoul(p + 1, NULL, 10);
            } else {
                // 两参数格式: size:crc32 (自动选择槽位)
                slot = Logo_GetAutoUploadSlot();
                size = first_num;
                crc = second_num;
            }
        }
        
        // 验证槽位
        if (slot >= LOGO_MAX_SLOTS) {
            Logo_AddDebugLog("INVALID SLOT!");
            sprintf(response, "LOGO_ERROR:INVALID_SLOT:%d\n", slot);
            BLE_SendString(response);
            return;
        }
        
        logo_current_slot = slot;  // 设置当前上传目标槽位
        uint32_t flash_addr = Logo_GetSlotAddress(slot);
        
        char msg[40];
        snprintf(msg, 40, "Slot:%d Size:%u", slot, (unsigned int)size);
        Logo_AddDebugLog(msg);
        snprintf(msg, 40, "CRC:%u", (unsigned int)crc);
        Logo_AddDebugLog(msg);
        
        printf("[LOGO] START slot=%d size=%u crc=%u\r\n", slot, (unsigned int)size, (unsigned int)crc);
        
        // 校验大小
        if (size != LOGO_DATA_SIZE) {
            Logo_AddDebugLog("SIZE MISMATCH!");
            sprintf(response, "LOGO_ERROR:SIZE_MISMATCH:%lu\n", (unsigned long)LOGO_DATA_SIZE);
            BLE_SendString(response);
            return;
        }
        
        Logo_AddDebugLog("ERASING...");
        
        // 开始擦除Flash (240x240需要29个扇区: 115200/4096≈29)
        logo_state = LOGO_STATE_ERASING;
        BLE_SendString("LOGO_ERASING\n");
        
        for (int i = 0; i < 29; i++) {
            W25Q128_EraseSector(flash_addr + i * 4096);
        }
        
        Logo_AddDebugLog("ERASE DONE");
        
        // 初始化接收状态（未压缩）
        logo_is_compressed = 0;
        logo_total_size = size;
        logo_received_size = 0;
        logo_expected_crc = crc;
        logo_current_seq = 0;
        logo_flash_written = 0;  // 🔧 重置已写入Flash的字节数
        logo_state = LOGO_STATE_RECEIVING;
        
        // 初始化接收缓冲区
        Buffer_Init();
        g_receiveWindow.totalPackets = (size + 15) / 16;  // 向上取整
        g_receiveWindow.lastAckSeq = 0;
        
        snprintf(msg, 40, "READY RCV %lu PKT", (unsigned long)g_receiveWindow.totalPackets);
        Logo_AddDebugLog(msg);
        
        // 返回包含槽位信息的响应
        sprintf(response, "LOGO_READY:%d\n", slot);
        BLE_SendString(response);
        printf("[LOGO] Ready to receive %lu packets to slot %d\r\n", 
               (unsigned long)g_receiveWindow.totalPackets, slot);
    }
    
    // ════════════════════════════════════════════════════════════
    // LOGO_DATA:seq:hexdata - 分包数据 
    // 🔥 可靠传输v3：修复缓冲区偏移计算bug
    // 核心思想：APP每发16包等待ACK，硬件批量写入Flash后才发ACK
    // ════════════════════════════════════════════════════════════
    else if (strncmp(cmd, "LOGO_DATA:", 10) == 0)
    {
        if (logo_state != LOGO_STATE_RECEIVING) {
            BLE_SendString("LOGO_ERROR:NOT_READY\n");
            return;
        }
        
        // 解析序号
        char* p = cmd + 10;
        uint32_t seq = strtoul(p, &p, 10);
        
        if (*p != ':') {
            BLE_SendString("LOGO_ERROR:FORMAT\n");
            return;
        }
        
        char* hexData = p + 1;
        
        // 解码十六进制数据到临时缓冲区
        int decodedLen = HexDecode(hexData, logo_temp_buffer, sizeof(logo_temp_buffer));
        if (decodedLen <= 0) {
            printf("[LOGO] HexDecode failed for seq=%lu\r\n", (unsigned long)seq);
            sprintf(response, "LOGO_NAK:%lu\n", (unsigned long)seq);
            BLE_SendString(response);
            return;
        }
        
        // 🔥 关键：检测丢包（序号不连续）
        uint32_t expectedSeq = (logo_current_seq == 0 && logo_received_size == 0) ? 0 : logo_current_seq + 1;
        if (seq != expectedSeq) {
            printf("[LOGO] SEQ MISMATCH: got=%lu, expected=%lu\r\n", 
                   (unsigned long)seq, (unsigned long)expectedSeq);
            
            // 如果收到的是期望序号之前的包，可能是重发，忽略
            if (seq < expectedSeq) {
                printf("[LOGO] Ignoring duplicate packet %lu\r\n", (unsigned long)seq);
                return;
            }
            
            sprintf(response, "LOGO_RESEND:%lu\n", (unsigned long)expectedSeq);
            BLE_SendString(response);
            return;
        }
        
        // 🔧 关键修复：使用累计字节偏移，而不是 flashWriteCount * 16
        // 这样可以正确处理最后一个包不足16字节的情况
        if (g_receiveWindow.flashWriteCount == 0) {
            g_receiveWindow.batchByteCount = 0;  // 新批次开始，重置字节计数
        }
        
        // 复制数据到缓冲区的正确位置
        memcpy(&g_receiveWindow.flashWriteBuffer[g_receiveWindow.batchByteCount], logo_temp_buffer, decodedLen);
        g_receiveWindow.batchByteCount += decodedLen;
        g_receiveWindow.flashWriteCount++;
        
        logo_received_size += decodedLen;
        logo_current_seq = seq;
        
        // 🔥 严格流控：每16包批量写入Flash，然后发ACK
        bool isLastPacket = (seq == g_receiveWindow.totalPackets - 1);
        bool batchComplete = (g_receiveWindow.flashWriteCount >= FLASH_WRITE_BATCH_SIZE);
        
        if (batchComplete || isLastPacket) {
            // 计算写入地址 (使用当前上传槽位)
            uint32_t slotAddr = Logo_GetSlotAddress(logo_current_slot);
            uint32_t writeAddr = slotAddr + LOGO_HEADER_SIZE + logo_flash_written;
            
            // 🔧 关键修复：使用实际累计的字节数，而不是计算值
            uint32_t writeSize = g_receiveWindow.batchByteCount;
            
            // 🔧 调试：打印写入详情
            printf("[LOGO] WRITE: addr=0x%08lX, size=%lu, batch=%lu pkts\r\n",
                   (unsigned long)writeAddr, (unsigned long)writeSize, 
                   (unsigned long)g_receiveWindow.flashWriteCount);
            
            // 写入Flash
            W25Q128_BufferWrite(g_receiveWindow.flashWriteBuffer, writeAddr, writeSize);
            
            // 更新已写入Flash的字节数
            logo_flash_written += writeSize;
            
            // 重置批量缓冲区
            g_receiveWindow.flashWriteCount = 0;
            g_receiveWindow.batchByteCount = 0;
            
            // 🔥 发送ACK（APP等待这个ACK后才发下一批）
            sprintf(response, "LOGO_ACK:%lu\n", (unsigned long)seq);
            BLE_SendString(response);
            
            // 调试：每批次打印进度
            printf("[LOGO] ACK:%lu written=%lu/%lu\r\n", 
                   (unsigned long)seq, 
                   (unsigned long)logo_flash_written, 
                   (unsigned long)logo_total_size);
            
            // LCD更新：每100包更新一次
            if (seq % 100 < FLASH_WRITE_BATCH_SIZE || isLastPacket) {
                LCD_Fill(0, 0, 240, 240, BLACK);
                LCD_ShowString(40, 20, (u8*)"UPLOADING", WHITE, BLACK, 24, 0);
                
                char buf[20];
                sprintf(buf, "PKT:%lu", (unsigned long)seq);
                LCD_ShowString(30, 80, (u8*)buf, CYAN, BLACK, 24, 0);
                
                uint8_t progress = ((seq + 1) * 100) / g_receiveWindow.totalPackets;
                sprintf(buf, "%d%%", progress);
                LCD_ShowString(70, 140, (u8*)buf, GREEN, BLACK, 32, 0);
                
                sprintf(buf, "/%lu", (unsigned long)g_receiveWindow.totalPackets);
                LCD_ShowString(50, 190, (u8*)buf, WHITE, BLACK, 16, 0);
            }
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // LOGO_END - 传输结束 (🔥直接写入方案：不需要等待缓冲区)
    // ════════════════════════════════════════════════════════════
    else if (strcmp(cmd, "LOGO_END") == 0)
    {
        Logo_AddDebugLog("END CMD RCV");
        
        if (logo_state != LOGO_STATE_RECEIVING) {
            printf("[LOGO] ERROR: Not in receiving state for END\r\n");
            BLE_SendString("LOGO_ERROR:NOT_RECEIVING\n");
            return;
        }
        
        // 🔥 直接写入方案：所有数据已在接收时写入Flash，直接校验
        // LCD显示：开始验证
        LCD_Fill(0, 0, 240, 240, BLACK);
        LCD_ShowString(40, 100, (u8*)"VERIFYING...", WHITE, BLACK, 20, 0);
        
        printf("[LOGO] ═══════════════════════════════════\r\n");
        printf("[LOGO] END received, starting verification\r\n");
        printf("[LOGO] Received size: %lu/%lu\r\n", 
               (unsigned long)logo_received_size, 
               (unsigned long)logo_total_size);
        if (logo_is_compressed) {
            printf("  Decompressed size: %lu/%lu\r\n",
                   (unsigned long)logo_decompressed_total,
                   (unsigned long)logo_original_size);
        }
        printf("[LOGO] ═══════════════════════════════════\r\n");
        logo_state = LOGO_STATE_VERIFYING;
        
        // 检查接收大小
        char msg[40];
        snprintf(msg, 40, "RCV:%lu/%lu", (unsigned long)logo_received_size, (unsigned long)logo_total_size);
        Logo_AddDebugLog(msg);
        
        // 🔧 新增：检查实际写入Flash的字节数
        snprintf(msg, 40, "FLASH:%lu", (unsigned long)logo_flash_written);
        Logo_AddDebugLog(msg);
        
        printf("[LOGO] Size check: received=%lu, expected=%lu, flash_written=%lu\r\n",
               (unsigned long)logo_received_size, (unsigned long)logo_total_size,
               (unsigned long)logo_flash_written);
        
        if (logo_received_size != logo_total_size) {
            Logo_AddDebugLog("SIZE FAIL!");
            
            // 设置错误显示标志
            logo_error_displayed = true;
            
            // LCD显示错误
            LCD_Fill(0, 0, 240, 240, BLACK);
            LCD_ShowString(40, 80, (u8*)"SIZE ERROR", RED, BLACK, 24, 0);
            snprintf(msg, 40, "%lu/%lu", (unsigned long)logo_received_size, (unsigned long)logo_total_size);
            LCD_ShowString(40, 120, (u8*)msg, WHITE, BLACK, 16, 0);
            LCD_ShowString(20, 160, (u8*)"Press button to", YELLOW, BLACK, 16, 0);
            LCD_ShowString(20, 180, (u8*)"clear error", YELLOW, BLACK, 16, 0);
            
            printf("[LOGO] ERROR: Size mismatch!\r\n");
            sprintf(response, "LOGO_FAIL:SIZE:%lu/%lu\n", 
                    (unsigned long)logo_received_size, 
                    (unsigned long)logo_total_size);
            BLE_SendString(response);
            logo_state = LOGO_STATE_ERROR;
            return;
        }
        
        // 🔧 新增：检查Flash写入是否完整
        if (logo_flash_written != logo_total_size) {
            snprintf(msg, 40, "FLASH INCOMPLETE!");
            Logo_AddDebugLog(msg);
            printf("[LOGO] ERROR: Flash write incomplete! written=%lu, expected=%lu\r\n",
                   (unsigned long)logo_flash_written, (unsigned long)logo_total_size);
            sprintf(response, "LOGO_FAIL:FLASH_INCOMPLETE:%lu/%lu\n", 
                    (unsigned long)logo_flash_written, 
                    (unsigned long)logo_total_size);
            BLE_SendString(response);
            logo_state = LOGO_STATE_ERROR;
            return;
        }
        
        Logo_AddDebugLog("SIZE OK");
        printf("[LOGO] ✓ Size check passed\r\n");
        
        // 压缩模式：检查解压后的大小
        if (logo_is_compressed) {
            printf("[LOGO] Decompression check: decompressed=%lu, expected=%lu\r\n",
                   (unsigned long)logo_decompressed_total, (unsigned long)logo_original_size);
            
            if (logo_decompressed_total != logo_original_size) {
                Logo_AddDebugLog("DECOMP SIZE ERR!");
                printf("[LOGO] ERROR: Decompressed size mismatch!\r\n");
                sprintf(response, "LOGO_FAIL:DECOMP_SIZE:%lu/%lu\n", 
                        (unsigned long)logo_decompressed_total, 
                        (unsigned long)logo_original_size);
                BLE_SendString(response);
                logo_state = LOGO_STATE_ERROR;
                return;
            }
            Logo_AddDebugLog("DECOMP OK");
            printf("[LOGO] ✓ Decompression size check passed\r\n");
            printf("[LOGO] Compression ratio: %lu -> %lu bytes (%.1f%%)\r\n",
                   (unsigned long)logo_total_size,
                   (unsigned long)logo_decompressed_total,
                   (float)logo_total_size * 100.0f / logo_decompressed_total);
        }
        
        // 计算CRC32校验（对解压后的数据）
        uint32_t dataSize = logo_is_compressed ? logo_decompressed_total : logo_total_size;
        
        Logo_AddDebugLog("CALC CRC...");
        
        // LCD显示：正在计算CRC
        LCD_Fill(0, 0, 240, 240, BLACK);
        LCD_ShowString(40, 100, (u8*)"CALC CRC32...", YELLOW, BLACK, 20, 0);
        
        // 使用当前上传槽位计算地址
        uint32_t slotAddr = Logo_GetSlotAddress(logo_current_slot);
        
        printf("[LOGO] Starting CRC32 calculation...\r\n");
        printf("[LOGO]   Slot: %d, Address: 0x%08lX\r\n", logo_current_slot, (unsigned long)(slotAddr + LOGO_HEADER_SIZE));
        printf("[LOGO]   Size: %lu bytes\r\n", (unsigned long)dataSize);
        
        uint32_t calculatedCRC = CRC32_CalculateFlash(
            slotAddr + LOGO_HEADER_SIZE, 
            dataSize
        );
        
        snprintf(msg, 40, "CRC:0x%08lX", (unsigned long)calculatedCRC);
        Logo_AddDebugLog(msg);
        
        printf("[LOGO] CRC32 verification:\r\n");
        printf("[LOGO]   Expected:   0x%08lX (%lu)\r\n", 
               (unsigned long)logo_expected_crc, (unsigned long)logo_expected_crc);
        printf("[LOGO]   Calculated: 0x%08lX (%lu)\r\n", 
               (unsigned long)calculatedCRC, (unsigned long)calculatedCRC);
        
        if (calculatedCRC != logo_expected_crc) {
            Logo_AddDebugLog("CRC FAIL!");
            
            // 设置错误显示标志
            logo_error_displayed = true;
            
            // 🔧 新增：通过蓝牙发送前32字节数据用于调试
            uint8_t debug_data[32];
            W25Q128_BufferRead(debug_data, slotAddr + LOGO_HEADER_SIZE, 32);
            
            char hex_str[80];
            snprintf(hex_str, 80, "DEBUG:FLASH_DATA:");
            BLE_SendString(hex_str);
            
            // 发送前16字节
            snprintf(hex_str, 80, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
                    debug_data[0], debug_data[1], debug_data[2], debug_data[3],
                    debug_data[4], debug_data[5], debug_data[6], debug_data[7],
                    debug_data[8], debug_data[9], debug_data[10], debug_data[11],
                    debug_data[12], debug_data[13], debug_data[14], debug_data[15]);
            BLE_SendString(hex_str);
            
            // 打印到串口
            printf("[LOGO] Flash data (first 32 bytes):\r\n");
            printf("  %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                   debug_data[0], debug_data[1], debug_data[2], debug_data[3],
                   debug_data[4], debug_data[5], debug_data[6], debug_data[7]);
            printf("  %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                   debug_data[8], debug_data[9], debug_data[10], debug_data[11],
                   debug_data[12], debug_data[13], debug_data[14], debug_data[15]);
            printf("  %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                   debug_data[16], debug_data[17], debug_data[18], debug_data[19],
                   debug_data[20], debug_data[21], debug_data[22], debug_data[23]);
            printf("  %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                   debug_data[24], debug_data[25], debug_data[26], debug_data[27],
                   debug_data[28], debug_data[29], debug_data[30], debug_data[31]);
            
            // LCD显示CRC错误
            LCD_Fill(0, 0, 240, 240, BLACK);
            LCD_ShowString(40, 60, (u8*)"CRC ERROR", RED, BLACK, 24, 0);
            snprintf(msg, 40, "EXP:0x%08lX", (unsigned long)logo_expected_crc);
            LCD_ShowString(20, 100, (u8*)msg, WHITE, BLACK, 16, 0);
            snprintf(msg, 40, "GOT:0x%08lX", (unsigned long)calculatedCRC);
            LCD_ShowString(20, 130, (u8*)msg, WHITE, BLACK, 16, 0);
            
            // 显示前16字节数据用于调试
            LCD_ShowString(20, 170, (u8*)"FIRST 16 BYTES:", CYAN, BLACK, 12, 0);
            for (int i = 0; i < 16; i += 4) {
                snprintf(msg, 40, "%02X %02X %02X %02X", 
                        debug_data[i], debug_data[i+1], debug_data[i+2], debug_data[i+3]);
                LCD_ShowString(20, 185 + i*4, (u8*)msg, GREEN, BLACK, 12, 0);
            }
            
            LCD_ShowString(10, 220, (u8*)"Press button to clear", YELLOW, BLACK, 12, 0);
            
            printf("[LOGO] ERROR: CRC32 mismatch!\r\n");
            printf("[LOGO] ═══════════════════════════════════\r\n");
            sprintf(response, "LOGO_FAIL:CRC:%lu\n", (unsigned long)calculatedCRC);
            BLE_SendString(response);
            logo_state = LOGO_STATE_ERROR;
            return;
        }
        Logo_AddDebugLog("CRC OK");
        printf("[LOGO] ✓ CRC32 check passed\r\n");
        
        // 写入头部信息
        Logo_AddDebugLog("WRITE HDR...");
        printf("[LOGO] Writing header to Flash slot %d...\r\n", logo_current_slot);
        LogoHeader_t header = {
            .magic = LOGO_MAGIC,
            .width = LOGO_WIDTH,
            .height = LOGO_HEIGHT,
            .reserved1 = 0,
            .dataSize = dataSize,
            .checksum = calculatedCRC
        };
        W25Q128_BufferWrite((uint8_t*)&header, slotAddr, sizeof(header));
        
        logo_state = LOGO_STATE_COMPLETE;
        sprintf(response, "LOGO_OK:%d\n", logo_current_slot);
        BLE_SendString(response);
        
        Logo_AddDebugLog("SUCCESS!");
        
        // LCD显示成功
        LCD_Fill(0, 0, 240, 240, BLACK);
        LCD_ShowString(60, 80, (u8*)"SUCCESS!", GREEN, BLACK, 32, 0);
        snprintf(msg, 40, "SLOT %d UPLOADED", logo_current_slot);
        LCD_ShowString(30, 140, (u8*)msg, WHITE, BLACK, 20, 0);
        snprintf(msg, 40, "%lu bytes", (unsigned long)dataSize);
        LCD_ShowString(70, 180, (u8*)msg, CYAN, BLACK, 16, 0);
        
        printf("[LOGO] ═══════════════════════════════════\r\n");
        printf("[LOGO] ✅ Upload complete!\r\n");
        printf("[LOGO]   Slot: %d\r\n", logo_current_slot);
        printf("[LOGO]   Mode: %s\r\n", logo_is_compressed ? "COMPRESSED" : "UNCOMPRESSED");
        printf("[LOGO]   Size: %lu bytes\r\n", (unsigned long)dataSize);
        printf("[LOGO]   CRC32: 0x%08lX\r\n", (unsigned long)calculatedCRC);
        printf("[LOGO] ═══════════════════════════════════\r\n");
    }
    
    // ════════════════════════════════════════════════════════════
    // GET:LOGO - 查询是否有自定义Logo (默认槽位0)
    // ════════════════════════════════════════════════════════════
    else if (strcmp(cmd, "GET:LOGO") == 0)
    {
        if (Logo_IsValid()) {
            BLE_SendString("LOGO:1\n");
        } else {
            BLE_SendString("LOGO:0\n");
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // GET:LOGO_SLOTS - 查询所有槽位状态
    // ════════════════════════════════════════════════════════════
    else if (strcmp(cmd, "GET:LOGO_SLOTS") == 0)
    {
        sprintf(response, "LOGO_SLOTS:%d:%d:%d:%d\n",
                Logo_IsSlotValid(0) ? 1 : 0,
                Logo_IsSlotValid(1) ? 1 : 0,
                Logo_IsSlotValid(2) ? 1 : 0,
                logo_active_slot);
        BLE_SendString(response);
        printf("[LOGO] Slots status: %d %d %d, active=%d\r\n",
               Logo_IsSlotValid(0), Logo_IsSlotValid(1), Logo_IsSlotValid(2), logo_active_slot);
    }
    
    // ════════════════════════════════════════════════════════════
    // SET:LOGO_ACTIVE:slot - 设置激活槽位
    // ════════════════════════════════════════════════════════════
    else if (strncmp(cmd, "SET:LOGO_ACTIVE:", 16) == 0)
    {
        uint8_t slot = (uint8_t)strtoul(cmd + 16, NULL, 10);
        if (slot < LOGO_MAX_SLOTS) {
            Logo_SetActiveSlot(slot);
            Logo_SaveConfig();
            sprintf(response, "LOGO_ACTIVE:%d\n", slot);
            BLE_SendString(response);
            printf("[LOGO] Active slot set to %d\r\n", slot);
        } else {
            BLE_SendString("LOGO_ERROR:INVALID_SLOT\n");
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // LOGO_DELETE - 删除自定义Logo (默认槽位0)
    // ════════════════════════════════════════════════════════════
    else if (strcmp(cmd, "LOGO_DELETE") == 0)
    {
        Logo_Delete();
        BLE_SendString("LOGO_DELETED\n");
    }
    
    // ════════════════════════════════════════════════════════════
    // LOGO_DELETE:slot - 删除指定槽位Logo
    // ════════════════════════════════════════════════════════════
    else if (strncmp(cmd, "LOGO_DELETE:", 12) == 0)
    {
        uint8_t slot = (uint8_t)strtoul(cmd + 12, NULL, 10);
        if (slot < LOGO_MAX_SLOTS) {
            uint32_t addr = Logo_GetSlotAddress(slot);
            W25Q128_EraseSector(addr);
            sprintf(response, "LOGO_DELETED:%d\n", slot);
            BLE_SendString(response);
            printf("[LOGO] Slot %d deleted\r\n", slot);
        } else {
            BLE_SendString("LOGO_ERROR:INVALID_SLOT\n");
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // LOGO_STATUS - 查询传输状态
    // ════════════════════════════════════════════════════════════
    else if (strcmp(cmd, "LOGO_STATUS") == 0)
    {
        sprintf(response, "LOGO_STATE:%d:%d\n", logo_state, Logo_GetProgress());
        BLE_SendString(response);
    }
    
    // ════════════════════════════════════════════════════════════
    // LOGO_QUERY_PROGRESS - 查询进度 (用于断点续传)
    // ════════════════════════════════════════════════════════════
    else if (strcmp(cmd, "LOGO_QUERY_PROGRESS") == 0)
    {
        sprintf(response, "LOGO_PROGRESS:%lu:%lu\n",
                (unsigned long)g_receiveWindow.lastAckSeq,
                (unsigned long)g_receiveWindow.totalPackets);
        BLE_SendString(response);
    }
    
    // ════════════════════════════════════════════════════════════
    // LOGO_TEST - 测试命令：查询Flash中Logo状态
    // ════════════════════════════════════════════════════════════
    else if (strcmp(cmd, "LOGO_TEST") == 0)
    {
        LogoHeader_t header;
        W25Q128_BufferRead((uint8_t*)&header, LOGO_FLASH_ADDR, sizeof(header));
        
        // 发送详细状态信息
        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                "LOGO_TEST_RESULT:\n"
                "Valid:%d\n"
                "Magic:0x%04X\n"
                "Width:%u\n"
                "Height:%u\n"
                "DataSize:%lu\n"
                "CRC32:0x%08lX\n"
                "State:%d\n"
                "RecvSize:%lu\n"
                "ExpCRC:0x%08lX\n",
                Logo_IsValid() ? 1 : 0,
                header.magic,
                header.width,
                header.height,
                (unsigned long)header.dataSize,
                (unsigned long)header.checksum,
                logo_state,
                (unsigned long)logo_received_size,
                (unsigned long)logo_expected_crc);
        BLE_SendString(buffer);
        
        printf("[LOGO] TEST command executed\r\n");
        printf("  Valid: %d\r\n", Logo_IsValid());
        printf("  Magic: 0x%04X (expect 0x%04X)\r\n", header.magic, LOGO_MAGIC);
        printf("  Size: %ux%u\r\n", header.width, header.height);
        printf("  DataSize: %lu (expect %lu)\r\n", 
               (unsigned long)header.dataSize, (unsigned long)LOGO_DATA_SIZE);
        printf("  CRC32: 0x%08lX\r\n", (unsigned long)header.checksum);
    }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          LCD显示函数                                          ║
// ╚══════════════════════════════════════════════════════════════╝

bool Logo_ShowOnLCD(uint16_t x, uint16_t y)
{
    // 优先尝试显示自定义Logo
    if (Logo_IsValid()) {
        // 读取头部确认参数
        LogoHeader_t header;
        W25Q128_BufferRead((uint8_t*)&header, LOGO_FLASH_ADDR, sizeof(header));
        
        // 设置LCD显示区域
        LCD_Address_Set(x, y, x + header.width - 1, y + header.height - 1);
        
        // 分块读取Flash并发送到LCD
        extern void LCD_Writ_Bus(uint8_t *data, uint16_t size);
        
        uint8_t buffer[512];
        uint32_t remaining = header.dataSize;
        uint32_t readAddr = LOGO_FLASH_ADDR + LOGO_HEADER_SIZE;
        
        while (remaining > 0) {
            uint16_t readLen = (remaining > 512) ? 512 : remaining;
            W25Q128_BufferRead(buffer, readAddr, readLen);
            
            // 批量发送到LCD
            LCD_Writ_Bus(buffer, readLen);
            
            readAddr += readLen;
            remaining -= readLen;
        }
        
        printf("[LOGO] Custom logo displayed (%dx%d)\r\n", header.width, header.height);
        return true;
    }
    
    // 没有自定义Logo，显示默认Logo (154x154，居中)
    uint16_t default_x = (240 - DEFAULT_LOGO_WIDTH) / 2;
    uint16_t default_y = (240 - DEFAULT_LOGO_HEIGHT) / 2;
    LCD_ShowPicture(default_x, default_y, DEFAULT_LOGO_WIDTH, DEFAULT_LOGO_HEIGHT, gImage_tou_xiang_154_154);
    printf("[LOGO] Default logo displayed (154x154 centered)\r\n");
    return false;  // 返回false表示显示的是默认Logo
}

void Logo_ShowBoot(void)
{
#if ANIM_ENABLED
    // 播放开机动画
    Logo_PlayAnimation();
    printf("[LOGO] Boot animation displayed\r\n");
#else
    // 开机只显示默认Logo，不显示自定义Logo
    // 自定义Logo在UI6 Logo界面中展示
    uint16_t default_x = (240 - DEFAULT_LOGO_WIDTH) / 2;
    uint16_t default_y = (240 - DEFAULT_LOGO_HEIGHT) / 2;
    LCD_ShowPicture(default_x, default_y, DEFAULT_LOGO_WIDTH, DEFAULT_LOGO_HEIGHT, gImage_tou_xiang_154_154);
    printf("[LOGO] Boot logo displayed (default, centered)\r\n");
#endif
}

// 🆕 Logo界面：显示激活槽位的自定义图片（如果有），否则显示默认
void Logo_ShowCustom(void)
{
    uint8_t active = Logo_GetActiveSlot();
    
    // 尝试显示激活槽位
    if (Logo_IsSlotValid(active)) {
        Logo_ShowSlot(active);
        return;
    }
    
    // 尝试找任意有效槽位
    for (uint8_t i = 0; i < LOGO_MAX_SLOTS; i++) {
        if (Logo_IsSlotValid(i)) {
            Logo_ShowSlot(i);
            return;
        }
    }
    
    // 无有效Logo，显示默认
    uint16_t default_x = (240 - DEFAULT_LOGO_WIDTH) / 2;
    uint16_t default_y = (240 - DEFAULT_LOGO_HEIGHT) / 2;
    LCD_ShowPicture(default_x, default_y, DEFAULT_LOGO_WIDTH, DEFAULT_LOGO_HEIGHT, gImage_tou_xiang_154_154);
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          主循环处理函数 - 从缓冲区写入Flash                   ║
// ╚══════════════════════════════════════════════════════════════╝

/**
 * @brief 处理缓冲区中的包 (在主循环中定期调用)
 * @note  这个函数从缓冲区取出包并写入Flash，不会阻塞蓝牙接收
 *        支持压缩和未压缩两种模式
 *        🔥 优化：批量写入Flash，减少写入次数
 */
void Logo_ProcessBuffer(void)
{
    if (logo_state != LOGO_STATE_RECEIVING) {
        return;
    }
    
    PacketBuffer_t packet;
    int processedCount = 0;
    
    // 🔥 优化：一次处理更多包（最多处理所有缓冲区的包）
    while (!Buffer_IsEmpty() && processedCount < PACKET_BUFFER_SIZE) {
        if (Buffer_Pop(&packet)) {
            if (logo_is_compressed) {
                // ═══════════════════════════════════════════════
                // 压缩模式：批量解压后写入Flash（优化性能）
                // ═══════════════════════════════════════════════
                
                // 如果是新的批次，记录起始序号
                if (g_receiveWindow.flashWriteCount == 0) {
                    g_receiveWindow.flashWriteBaseSeq = packet.seq;
                }
                
                // 添加到批量写入缓冲区（压缩数据）
                memcpy(&g_receiveWindow.flashWriteBuffer[g_receiveWindow.flashWriteCount * 16], 
                       packet.data, 
                       packet.len);
                g_receiveWindow.flashWriteCount++;
                
                logo_received_size += packet.len;
                logo_current_seq = packet.seq;
                
                // 🔥 批量解压：每10包或最后一包时解压并写入Flash
                if (g_receiveWindow.flashWriteCount >= FLASH_WRITE_BATCH_SIZE || 
                    logo_current_seq == g_receiveWindow.totalPackets - 1) {
                    
                    // 批量解压
                    uint32_t batch_size = g_receiveWindow.flashWriteCount * 16;
                    uint32_t decompressed_len = Logo_DecompressRLE(
                        g_receiveWindow.flashWriteBuffer, 
                        batch_size,
                        decompress_buffer, 
                        DECOMPRESS_BUFFER_SIZE
                    );
                    
                    if (decompressed_len > 0) {
                        // 写入Flash（使用解压后的偏移）
                        uint32_t writeAddr = LOGO_FLASH_ADDR + LOGO_HEADER_SIZE + logo_decompressed_total;
                        W25Q128_BufferWrite(decompress_buffer, writeAddr, decompressed_len);
                        logo_decompressed_total += decompressed_len;
                    }
                    
                    // 重置批量写入缓冲区
                    g_receiveWindow.flashWriteCount = 0;
                }
            } else {
                // ═══════════════════════════════════════════════
                // 未压缩模式：批量写入Flash（优化性能）
                // ═══════════════════════════════════════════════
                
                // 如果是新的批次，记录起始序号
                if (g_receiveWindow.flashWriteCount == 0) {
                    g_receiveWindow.flashWriteBaseSeq = packet.seq;
                }
                
                // 添加到批量写入缓冲区
                memcpy(&g_receiveWindow.flashWriteBuffer[g_receiveWindow.flashWriteCount * 16], 
                       packet.data, 
                       packet.len);
                g_receiveWindow.flashWriteCount++;
                
                logo_received_size += packet.len;
                logo_current_seq = packet.seq;
                
                // 🔥 批量写入：每10包或最后一包时写入Flash
                if (g_receiveWindow.flashWriteCount >= FLASH_WRITE_BATCH_SIZE || 
                    logo_current_seq == g_receiveWindow.totalPackets - 1) {
                    
                    uint32_t writeAddr = LOGO_FLASH_ADDR + LOGO_HEADER_SIZE + 
                                        g_receiveWindow.flashWriteBaseSeq * 16;
                    uint32_t writeSize = g_receiveWindow.flashWriteCount * 16;
                    
                    W25Q128_BufferWrite(g_receiveWindow.flashWriteBuffer, writeAddr, writeSize);
                    
                    // 重置批量写入缓冲区
                    g_receiveWindow.flashWriteCount = 0;
                }
            }
            
            processedCount++;
        }
    }
    
    // 每200包打印一次进度（减少串口输出）
    if (processedCount > 0 && logo_current_seq % 200 == 0) {
        if (logo_is_compressed) {
            printf("[LOGO] Processed %d pkts: recv=%lu/%lu decomp=%lu/%lu buf=%lu\r\n", 
                   processedCount,
                   (unsigned long)logo_received_size, 
                   (unsigned long)logo_total_size,
                   (unsigned long)logo_decompressed_total,
                   (unsigned long)logo_original_size,
                   (unsigned long)g_receiveWindow.count);
        } else {
            printf("[LOGO] Processed %d pkts: %lu/%lu buf=%lu\r\n", 
                   processedCount,
                   (unsigned long)logo_received_size, 
                   (unsigned long)logo_total_size,
                   (unsigned long)g_receiveWindow.count);
        }
    }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          开机Logo动画实现                                     ║
// ╚══════════════════════════════════════════════════════════════╝

#if ANIM_ENABLED

// 线条粗细（像素）- 8像素
#define LINE_THICKNESS  8

/**
 * @brief 绘制粗线条（改进版：斜线用圆形笔刷减少毛刺）
 * @param x1, y1 起点
 * @param x2, y2 终点
 * @param thickness 线条粗细
 * @param color 颜色
 */
static void Draw_ThickLine(u16 x1, u16 y1, u16 x2, u16 y2, u8 thickness, u16 color)
{
    int16_t half_t = thickness / 2;
    
    // 判断线条方向
    int16_t dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int16_t dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    
    if (dx == 0) {
        // 垂直线：直接用填充矩形
        u16 min_y = (y1 < y2) ? y1 : y2;
        u16 max_y = (y1 > y2) ? y1 : y2;
        LCD_Fill(x1 - half_t, min_y, x1 + half_t, max_y, color);
    } else if (dy == 0) {
        // 水平线：直接用填充矩形
        u16 min_x = (x1 < x2) ? x1 : x2;
        u16 max_x = (x1 > x2) ? x1 : x2;
        LCD_Fill(min_x, y1 - half_t, max_x, y1 + half_t, color);
    } else {
        // 斜线：用圆形笔刷沿线段绘制（减少毛刺）
        int16_t delta_x = x2 - x1;
        int16_t delta_y = y2 - y1;
        int16_t steps = (dx > dy) ? dx : dy;
        
        // 预计算圆形笔刷半径的平方
        int32_t r_sq = (int32_t)half_t * half_t;
        
        for (int16_t i = 0; i <= steps; i++) {
            int16_t px = x1 + delta_x * i / steps;
            int16_t py = y1 + delta_y * i / steps;
            
            // 用圆形笔刷代替方形，减少锯齿
            for (int16_t oy = -half_t; oy <= half_t; oy++) {
                for (int16_t ox = -half_t; ox <= half_t; ox++) {
                    if ((int32_t)ox * ox + (int32_t)oy * oy <= r_sq) {
                        LCD_DrawPoint(px + ox, py + oy, color);
                    }
                }
            }
        }
    }
}

/**
 * @brief 绘制粗圆环（改进版：填充式绘制，无黑点）
 * @param cx, cy 圆心
 * @param r 半径（圆环中心线的半径）
 * @param thickness 线条粗细
 * @param color 颜色
 * @note 使用距离判断法，遍历所有点，确保无间隙
 */
static void Draw_ThickCircle(u16 cx, u16 cy, u8 r, u8 thickness, u16 color)
{
    int16_t half_t = thickness / 2;
    int16_t r_inner = r - half_t;
    int16_t r_outer = r + half_t;
    
    if (r_inner < 0) r_inner = 0;
    
    // 使用平方避免开方
    int32_t r_inner_sq = (int32_t)r_inner * r_inner;
    int32_t r_outer_sq = (int32_t)r_outer * r_outer;
    
    // 遍历外接正方形的每个点
    for (int16_t dy = -r_outer; dy <= r_outer; dy++) {
        for (int16_t dx = -r_outer; dx <= r_outer; dx++) {
            int32_t dist_sq = (int32_t)dx * dx + (int32_t)dy * dy;
            
            // 在圆环范围内的点都画上
            if (dist_sq >= r_inner_sq && dist_sq <= r_outer_sq) {
                LCD_DrawPoint(cx + dx, cy + dy, color);
            }
        }
    }
}

/**
 * @brief 绘制正方形边框（渐进式，四条边依次绘制，粗线条）
 * @param x0, y0 左上角坐标
 * @param size 边长
 * @param progress 进度 0-100
 * @param color 颜色
 */
static void Animation_DrawSquareProgress(u16 x0, u16 y0, u16 size, u8 progress, u16 color)
{
    int16_t half_t = LINE_THICKNESS / 2;
    
    // 每条边占25%进度
    // 顺序：上边 → 右边 → 下边 → 左边
    
    u16 x1 = x0 + size - 1;
    u16 y1 = y0 + size - 1;
    
    // ═══════════════════════════════════════════════════════════
    // 上边：从左到右 (0-25%)
    // ═══════════════════════════════════════════════════════════
    if (progress >= 0) {
        u8 edge_progress = (progress > 25) ? 100 : (progress * 4);
        u16 end_x = x0 + (size - 1) * edge_progress / 100;
        Draw_ThickLine(x0, y0, end_x, y0, LINE_THICKNESS, color);
        
        // 左上角：上边开始时就填充
        LCD_Fill(x0 - half_t, y0 - half_t, x0 + half_t, y0 + half_t, color);
    }
    
    // ═══════════════════════════════════════════════════════════
    // 右边：从上到下 (25-50%)
    // ═══════════════════════════════════════════════════════════
    if (progress > 25) {
        u8 edge_progress = (progress > 50) ? 100 : ((progress - 25) * 4);
        u16 end_y = y0 + (size - 1) * edge_progress / 100;
        Draw_ThickLine(x1, y0, x1, end_y, LINE_THICKNESS, color);
        
        // 右上角：右边开始时填充
        LCD_Fill(x1 - half_t, y0 - half_t, x1 + half_t, y0 + half_t, color);
    }
    
    // ═══════════════════════════════════════════════════════════
    // 下边：从右到左 (50-75%)
    // ═══════════════════════════════════════════════════════════
    if (progress > 50) {
        u8 edge_progress = (progress > 75) ? 100 : ((progress - 50) * 4);
        u16 start_x = x1 - (size - 1) * edge_progress / 100;
        Draw_ThickLine(x1, y1, start_x, y1, LINE_THICKNESS, color);
        
        // 右下角：下边开始时填充
        LCD_Fill(x1 - half_t, y1 - half_t, x1 + half_t, y1 + half_t, color);
    }
    
    // ═══════════════════════════════════════════════════════════
    // 左边：从下到上 (75-100%)
    // ═══════════════════════════════════════════════════════════
    if (progress > 75) {
        u8 edge_progress = (progress >= 100) ? 100 : ((progress - 75) * 4);
        u16 start_y = y1 - (size - 1) * edge_progress / 100;
        Draw_ThickLine(x0, y1, x0, start_y, LINE_THICKNESS, color);
        
        // 左下角：左边开始时填充
        LCD_Fill(x0 - half_t, y1 - half_t, x0 + half_t, y1 + half_t, color);
    }
}

/**
 * @brief 绘制对角线（从中心向两端延伸）- 左下到右上
 * @param x0, y0 正方形左上角
 * @param size 正方形边长
 * @param progress 进度 0-100
 * @param color 颜色
 */
static void Animation_DrawDiagonalProgress(u16 x0, u16 y0, u16 size, u8 progress, u16 color)
{
    // 中心点
    u16 cx = x0 + size / 2;
    u16 cy = y0 + size / 2;
    
    // 对角线两端：左下角 和 右上角
    u16 corner1_x = x0;              // 左下角
    u16 corner1_y = y0 + size - 1;
    u16 corner2_x = x0 + size - 1;   // 右上角
    u16 corner2_y = y0;
    
    // 从中心向左下延伸
    int16_t dx1 = (int16_t)(corner1_x - cx) * progress / 100;
    int16_t dy1 = (int16_t)(corner1_y - cy) * progress / 100;
    
    // 从中心向右上延伸
    int16_t dx2 = (int16_t)(corner2_x - cx) * progress / 100;
    int16_t dy2 = (int16_t)(corner2_y - cy) * progress / 100;
    
    // 绘制两段对角线（粗线条）
    Draw_ThickLine(cx, cy, cx + dx1, cy + dy1, LINE_THICKNESS, color);
    Draw_ThickLine(cx, cy, cx + dx2, cy + dy2, LINE_THICKNESS, color);
}

/**
 * @brief 绘制圆形（逐渐扩大）- 空心圆，粗线条
 * @param cx, cy 圆心
 * @param target_r 目标半径
 * @param progress 进度 0-100
 * @param color 颜色
 */
static void Animation_DrawCircleProgress(u16 cx, u16 cy, u8 target_r, u8 progress, u16 color)
{
    // 直接画最终大小的圆环（不做扩大动画，避免累积成实心）
    // progress只用于控制是否开始画
    if (progress > 0) {
        Draw_ThickCircle(cx, cy, target_r, LINE_THICKNESS, color);
    }
}

/**
 * @brief 播放开机Logo动画
 * @note  动画序列：
 *        1. 正方形四条边依次绘制（约300ms）
 *        2. 对角线从中心向两端延伸 + 内切圆同时出现（约200ms）
 *        3. 保持显示（约500ms）
 *        总时长约1秒，配合双闪和引擎声
 */
void Logo_PlayAnimation(void)
{
    // 计算正方形左上角坐标（居中）
    u16 x0 = ANIM_CENTER_X - ANIM_SQUARE_SIZE / 2;  // 43
    u16 y0 = ANIM_CENTER_Y - ANIM_SQUARE_SIZE / 2;  // 43
    
    printf("[LOGO] Animation start\r\n");
    
    // 清屏
    LCD_Fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, ANIM_BG_COLOR);
    
    // ════════════════════════════════════════════════════════════
    // 阶段1：绘制正方形（约300ms）
    // ════════════════════════════════════════════════════════════
    u32 frames = ANIM_SQUARE_DURATION / ANIM_FRAME_DELAY;
    for (u32 i = 0; i <= frames; i++) {
        u8 progress = i * 100 / frames;
        Animation_DrawSquareProgress(x0, y0, ANIM_SQUARE_SIZE, progress, ANIM_LINE_COLOR);
        HAL_Delay(ANIM_FRAME_DELAY);
    }
    
    // ════════════════════════════════════════════════════════════
    // 阶段2：对角线 + 内切圆同时绘制（约200ms）
    // ════════════════════════════════════════════════════════════
    u32 phase2_frames = ANIM_DIAGONAL_DURATION / ANIM_FRAME_DELAY;
    
    // 内切圆参数：圆心在正方形右下角区域
    u16 circle_cx = x0 + ANIM_SQUARE_SIZE - ANIM_CIRCLE_RADIUS;
    u16 circle_cy = y0 + ANIM_SQUARE_SIZE - ANIM_CIRCLE_RADIUS;
    
    // 先画一次完整的空心圆环
    Draw_ThickCircle(circle_cx, circle_cy, ANIM_CIRCLE_RADIUS, LINE_THICKNESS, ANIM_LINE_COLOR);
    
    // 对角线动画
    for (u32 i = 0; i <= phase2_frames; i++) {
        u8 progress = i * 100 / phase2_frames;
        Animation_DrawDiagonalProgress(x0, y0, ANIM_SQUARE_SIZE, progress, ANIM_LINE_COLOR);
        HAL_Delay(ANIM_FRAME_DELAY);
    }
    
    // ════════════════════════════════════════════════════════════
    // 阶段3：保持显示（配合引擎声）
    // ════════════════════════════════════════════════════════════
    HAL_Delay(ANIM_HOLD_DURATION);
    
    printf("[LOGO] Animation complete\r\n");
}

#else

// 动画禁用时的空实现
void Logo_PlayAnimation(void)
{
    // 直接显示静态Logo
    uint16_t default_x = (240 - DEFAULT_LOGO_WIDTH) / 2;
    uint16_t default_y = (240 - DEFAULT_LOGO_HEIGHT) / 2;
    LCD_ShowPicture(default_x, default_y, DEFAULT_LOGO_WIDTH, DEFAULT_LOGO_HEIGHT, gImage_tou_xiang_154_154);
}

#endif /* ANIM_ENABLED */
