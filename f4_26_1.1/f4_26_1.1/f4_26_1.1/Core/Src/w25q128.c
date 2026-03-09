#include "w25q128.h"
/* 私有函数声明 */
static void W25Q128_CS_Select(void);
static void W25Q128_CS_Deselect(void);
static uint8_t W25Q128_SendByte(uint8_t byte);

/* 初始化W25Q128 */
void W25Q128_Init(void) {
    W25Q128_CS_Deselect();
    W25Q128_WakeUp();
}

/* 读取设备ID */
uint8_t W25Q128_ReadID(void) {
    uint8_t Temp = 0, Temp1 = 0, Temp2 = 0;
    
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_JEDEC_ID);
    W25Q128_SendByte(0xFF);  /* 制造商ID */
    Temp1 = W25Q128_SendByte(0xFF);  /* 存储器类型 */
    Temp2 = W25Q128_SendByte(0xFF);  /* 容量 */
    W25Q128_CS_Deselect();
    
    if ((Temp1 == 0x40) && (Temp2 == 0x18)) Temp = 1;  /* W25Q128 */
    return Temp;
}

/* 读取状态寄存器1 */
uint8_t W25Q128_ReadStatusReg1(void) {
    uint8_t byte = 0;
    
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_READ_STATUS_REG1);
    byte = W25Q128_SendByte(0xFF);
    W25Q128_CS_Deselect();
    
    return byte;
}

/* 读取状态寄存器2 */
uint8_t W25Q128_ReadStatusReg2(void) {
    uint8_t byte = 0;
    
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_READ_STATUS_REG2);
    byte = W25Q128_SendByte(0xFF);
    W25Q128_CS_Deselect();
    
    return byte;
}

/* 写使能 */
void W25Q128_WriteEnable(void) {
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_WRITE_ENABLE);
    W25Q128_CS_Deselect();
}

/* 写禁止 */
void W25Q128_WriteDisable(void) {
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_WRITE_DISABLE);
    W25Q128_CS_Deselect();
}

/* 等待写操作完成 */
void W25Q128_WaitForWriteEnd(void) {
    uint8_t status;
    
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_READ_STATUS_REG1);
    
    do {
        status = W25Q128_SendByte(0xFF);
    } while ((status & W25Q128_SR_WIP) == SET);  /* 等待WIP位清零 */
    
    W25Q128_CS_Deselect();
}

/* 擦除扇区（4KB） */
void W25Q128_EraseSector(uint32_t SectorAddr) {
    /* 发送扇区擦除命令前需先使能写操作 */
    W25Q128_WriteEnable();
    W25Q128_WaitForWriteEnd();
    
    /* 发送扇区擦除命令 */
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_SECTOR_ERASE);
    W25Q128_SendByte((SectorAddr & 0xFF0000) >> 16);
    W25Q128_SendByte((SectorAddr & 0xFF00) >> 8);
    W25Q128_SendByte(SectorAddr & 0xFF);
    W25Q128_CS_Deselect();
    
    /* 等待擦除完成 */
    W25Q128_WaitForWriteEnd();
}

/* 擦除块（64KB） */
void W25Q128_EraseBlock(uint32_t BlockAddr) {
    W25Q128_WriteEnable();
    W25Q128_WaitForWriteEnd();
    
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_BLOCK_ERASE_64K);
    W25Q128_SendByte((BlockAddr & 0xFF0000) >> 16);
    W25Q128_SendByte((BlockAddr & 0xFF00) >> 8);
    W25Q128_SendByte(BlockAddr & 0xFF);
    W25Q128_CS_Deselect();
    
    W25Q128_WaitForWriteEnd();
}

/* 擦除整个芯片 */
void W25Q128_EraseChip(void) {
    W25Q128_WriteEnable();
    W25Q128_WaitForWriteEnd();
    
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_CHIP_ERASE);
    W25Q128_CS_Deselect();
    
    W25Q128_WaitForWriteEnd();
}

/* 页写入（最多256字节） */
void W25Q128_PageWrite(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite) {
    if (NumByteToWrite > W25Q128_PAGE_SIZE) {
        NumByteToWrite = W25Q128_PAGE_SIZE;  /* 限制为一页的大小 */
    }
    
    W25Q128_WriteEnable();
    
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_PAGE_PROGRAM);
    W25Q128_SendByte((WriteAddr & 0xFF0000) >> 16);
    W25Q128_SendByte((WriteAddr & 0xFF00) >> 8);
    W25Q128_SendByte(WriteAddr & 0xFF);
    
    for (uint16_t i = 0; i < NumByteToWrite; i++) {
        W25Q128_SendByte(pBuffer[i]);
    }
    
    W25Q128_CS_Deselect();
    W25Q128_WaitForWriteEnd();
}

/* 缓冲区写入（支持跨页写入） */
void W25Q128_BufferWrite(uint8_t* pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite) {
    uint8_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0;
    
    Addr = WriteAddr % W25Q128_PAGE_SIZE;
    count = W25Q128_PAGE_SIZE - Addr;
    NumOfPage = NumByteToWrite / W25Q128_PAGE_SIZE;
    NumOfSingle = NumByteToWrite % W25Q128_PAGE_SIZE;
    
    /* 若起始地址是页对齐的 */
    if (Addr == 0) {
        /* 若写入字节数不大于一页 */
        if (NumOfPage == 0) {
            W25Q128_PageWrite(pBuffer, WriteAddr, NumByteToWrite);
        } 
        /* 若写入字节数大于一页 */
        else {
            while (NumOfPage--) {
                W25Q128_PageWrite(pBuffer, WriteAddr, W25Q128_PAGE_SIZE);
                WriteAddr += W25Q128_PAGE_SIZE;
                pBuffer += W25Q128_PAGE_SIZE;
            }
            
            if (NumOfSingle != 0) {
                W25Q128_PageWrite(pBuffer, WriteAddr, NumOfSingle);
            }
        }
    } 
    /* 若起始地址不是页对齐的 */
    else {
        /* 若写入字节数不大于剩余页数 */
        if (NumByteToWrite <= count) {
            W25Q128_PageWrite(pBuffer, WriteAddr, NumByteToWrite);
        } 
        /* 若写入字节数大于剩余页数 */
        else {
            /* 🔧 修复：先写入第一部分（填满当前页） */
            W25Q128_PageWrite(pBuffer, WriteAddr, count);
            pBuffer += count;
            WriteAddr += count;
            NumByteToWrite -= count;
            NumOfPage = NumByteToWrite / W25Q128_PAGE_SIZE;
            NumOfSingle = NumByteToWrite % W25Q128_PAGE_SIZE;
            
            while (NumOfPage--) {
                W25Q128_PageWrite(pBuffer, WriteAddr, W25Q128_PAGE_SIZE);
                WriteAddr += W25Q128_PAGE_SIZE;
                pBuffer += W25Q128_PAGE_SIZE;
            }
            
            if (NumOfSingle != 0) {
                W25Q128_PageWrite(pBuffer, WriteAddr, NumOfSingle);
            }
        }
    }
}

/* 缓冲区读取 */
void W25Q128_BufferRead(uint8_t* pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead) {
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_READ_DATA);
    W25Q128_SendByte((ReadAddr & 0xFF0000) >> 16);
    W25Q128_SendByte((ReadAddr & 0xFF00) >> 8);
    W25Q128_SendByte(ReadAddr & 0xFF);
    
    for (uint16_t i = 0; i < NumByteToRead; i++) {
        pBuffer[i] = W25Q128_SendByte(0xFF);
    }
    
    W25Q128_CS_Deselect();
}

/* 进入掉电模式 */
void W25Q128_PowerDown(void) {
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_POWER_DOWN);
    W25Q128_CS_Deselect();
}

/* 唤醒 */
void W25Q128_WakeUp(void) {
    W25Q128_CS_Select();
    W25Q128_SendByte(W25Q128_RELEASE_PD_ID);
    W25Q128_CS_Deselect();
}

/* 私有函数：片选使能 */
static void W25Q128_CS_Select(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
}

/* 私有函数：片选释放 */
static void W25Q128_CS_Deselect(void) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
}

/* 私有函数：SPI发送并接收一个字节 */
static uint8_t W25Q128_SendByte(uint8_t byte) {
    uint8_t rxByte;
    HAL_SPI_TransmitReceive(&hspi1, &byte, &rxByte, 1, 1000);
    return rxByte;
}


