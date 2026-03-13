/* ---------------- Image.c ---------------- */
/* ---------------- 菜单的交互逻辑处理层,包括一级菜单的初次配置,不包括子链或动作逻辑配置 ---------------- */
#include "stdio.h"
#include "stm32f10x.h" 
#include <math.h>
#include "OLED.h"
#include <stdint.h>
#include "Image.h"
#include <string.h>
#include "delay.h"
#include "Encoder.h"	
#include "Image_Icons.h"
#include "EventBus.h"

/* ---------------- 创建一级菜单链表结点,根菜单 ---------------- */
LinkNode NodeA={
	
};
LinkNode NodeB={

};
LinkNode NodeC={

};
LinkNode NodeD={
	
};
LinkNode NodeE={

};


QueueNode QNode0,QNode1,QNode2,QNode3,QNode4;	//对应五个显示区域(其中两个隐藏)
Queue RootQueue;	//一级菜单
Queue* pCurrentQueue=&RootQueue;	//当前的队列指针指向根队列(即一级菜单)



/* ---------------- 创建默认一级菜单并刷新显示到屏幕上 ---------------- */
void Image_Init(void)
{


}


/* ---------------- 队列结点的元素(LinkNode类型)左移---------------- */
void Left_Shift(void)//逆时针转
{
																														

}

/* ---------------- 队列结点的元素(LinkNode类型)右移---------------- */
void Right_Shift(void)//顺时针转
{
																															

	
}

void MenuForward(void)//进入次级菜单(或执行绑定函数命令)
{

}


/**
  * @brief  销毁当前队列释放内存,回到父级队列,即回溯父菜单
  * @param  无
  * @retval 无
  */
void MenuBack(void)//返回上级菜单,目前仅2级菜单,无命令
{
	/*
	u8 i;
	if (pCurrentQueue->Floor == 1) {return;}//达到菜单顶部,没有更顶层菜单或子函数了,返回
	
	for(i=6;i<13;i++)
	{
		OLED_OffsetUpdate(-i,0);	//非线性显存向下迁移
	}
	*/
}


/**
  * @brief  在BLOCK位置展示一个正确的菜单
			让图像尺寸能在三个区块正确匹配大小,以ShowImage函数为实现函数.		
  * @param  Update_Bit 控制是否立即刷新,0:不刷新屏幕;1刷新屏幕
  */
void ShowMenu(u8 Update_Bit)
{


}




/**
  * @brief  将缓存数组24*32通过最近邻插值算法拉伸到36*48
  * @param  源图像数组,新图像数组
  */
void scale24x32_to36x48(const uint8_t Image2[3*32], uint8_t ImageNew[5*48]) {
    // 目标图像初始化（全暗），尺寸改为5页*48列（共5*48字节，对应36行48列）
    for (int i = 0; i < 5*48; i++) ImageNew[i] = 0x00;
    
    // 遍历目标图像的每一页（0-4，共5页）和每一列（0-47，共48列）
    for (int page_new = 0; page_new < 5; page_new++) {
        for (int col_new = 0; col_new < 48; col_new++) {
            // 计算源图像的列索引（最近邻插值）；宽度缩放不变（源32列->目标48列）
            int col_old = (col_new * 32 + 24) / 48;  // +24实现四舍五入，映射新列到旧列
            
            // 跳过越界列（源图像仅32列）
            if (col_old >= 32) continue;
            
            // 处理当前col_new值目标图像的每个位（0-7）
            for (int bit = 0; bit < 8; bit++) {
                // 计算目标像素的全局行号（page_new*8 + bit）
                int row_global = page_new * 8 + bit;
                
                // 计算源图像的行号（最近邻插值）；高度缩放调整（源24行->目标36行）
                int row_old = (row_global * 24 + 18) / 36;  // +18实现四舍五入（36/2=18）
                
                // 计算源图像的页号和页内行
                int page_old = row_old / 8;
                int bit_old = row_old % 8;
                
                // 跳过越界页（源图像仅3页）
                if (page_old >= 3) continue;
                
                // 获取新图当前bit映射的源像素值
                uint8_t src_byte = Image2[page_old * 32 + col_old];
                uint8_t src_bit = (src_byte >> bit_old) & 0x01;
                
                // 设置目标像素值
                if (src_bit) {
                    ImageNew[page_new * 48 + col_new] |= (1 << bit);
                }
            }
        }
    }
}

/**
  * @brief  将任意尺寸缓存数组通过最近邻插值算法拉伸为任意尺寸,除法改移位提快算力
  * @param  源图像数组,源高,源宽,目标图像数组,目标高,目标宽
  * @retval 缓冲区dst指针
  */
uint8_t* scale_1bit(const uint8_t *src,
                      uint16_t src_h,uint16_t src_w,
                     uint8_t       *dst,
                     uint16_t dst_h,uint16_t dst_w  )
{
    /* 2. 常量 */
    const uint16_t src_pages = ((src_h - 1)>>3) + 1;   // 源页数,向高位进位
    const uint16_t dst_pages = ((dst_h - 1)>>3) + 1;   // 目标页数,向高位进位

    /* 3. 清空目标图（全 0x00） */
    for (uint16_t i = 0; i < (uint16_t)dst_pages * dst_w; ++i)
        dst[i] = 0x00;

    /* 4. 主循环：目标页、目标列、目标 bit */
    for (uint16_t page_new = 0; page_new < dst_pages; ++page_new) {
        for (uint16_t col_new = 0; col_new < dst_w; ++col_new) {
            /* 4.1 源列 */
            uint16_t col_old = ((uint16_t)col_new * src_w * 2 + src_w) / (dst_w * 2); // 四舍五入
            if (col_old >= src_w) continue;
            for (uint8_t bit = 0; bit < 8; ++bit) {
                /* 4.2 全局行号 */
                uint16_t row_new = page_new * 8 + bit;

                /* 4.3 源行号 */
                uint16_t row_old = ((uint16_t)row_new * src_h * 2 + src_h) / (dst_h * 2); // 四舍五入
                if (row_old >= src_h) continue;

                /* 4.4 源页与位 */
                uint16_t page_old = row_old >> 3;
                uint8_t  bit_old  = row_old & 0x07;

                /* 4.5 读源像素 */
                uint8_t src_byte = src[page_old * src_w + col_old];
                uint8_t pixel    = (src_byte >> bit_old) & 0x01;

                /* 4.6 写目标像素 */
                if (pixel)
                    dst[page_new * dst_w + col_new] |= (1u << bit);
            }
        }
    }
	return dst;
}

	/*

	*/