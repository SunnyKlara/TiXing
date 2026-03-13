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
#include "Image_Icons.h"
//#include "Animation.h"
#include "MenuTree.h"
#include "EventBus.h"
/* ---------------- 菜单选项配置 ---------------- */
MenuOptions menuOptions=
{
	.enableAnimation='1',	//是否允许执行动画,0则跳过
	.AnimationFrame=16,	//菜单动画总帧数
	.SystemClock=72,	//默认72MHZ
	
};

/* ---------------- 创建一级菜单链表结点,根菜单 ---------------- */
LinkNode NodeA={
	.id='A',
	.last=&NodeE,
	.next=&NodeB,
	.Str="时钟",	//这里填写描述
	.Image=Image0,//常量指针指向退化的数组指针	
};
LinkNode NodeB={
	.id='B',
	.last=&NodeA,
	.next=&NodeC,
	.Str="游戏",
	.Image=Image1,
};
LinkNode NodeC={
	.id='C',
	.last=&NodeB,
	.next=&NodeD,
	.Str="表情",
	.Image=Image2,	
};
LinkNode NodeD={
	
	.id='D',
	.last=&NodeC,
	.next=&NodeE,//形成环形链表
	.Str="设置",
	.Image=Image3,	
};
LinkNode NodeE={
	.id='E',
	.last=&NodeD,
	.next=&NodeA,//形成环形链表
	.Str="秒表",
	.Image=Image4,	
};


QueueNode QNode0,QNode1,QNode2,QNode3,QNode4;
Queue RootQueue;//全局结构体,无需申请内存,不会被自动销毁
Queue* pCurrentQueue=&RootQueue;	//当前的队列指针指向根队列(即一级菜单)

/* ---------------- 创建默认一级菜单并刷新显示到屏幕上 ---------------- */
void Image_Init(void)
{
	/* ---------------- 配置环形链表结点初始化---------------- */

	/* ---------------- 配置队列结点初始化---------------- */
	//用于显示左侧未出现元素,用于动画平移效果
	QNode0.sequence = 0;
    QNode0.block[0] = 20; QNode0.block[1] = -32;  QNode0.block[2] = 24; QNode0.block[3] = 32;
    QNode0.element  = &NodeE;
    QNode0.next     = &QNode1;
	//用于显示左侧元素
    QNode1.sequence = 1;
    QNode1.block[0] = 20; QNode1.block[1] = 5;  QNode1.block[2] = 24; QNode1.block[3] = 32;
    QNode1.element  = &NodeA;
    QNode1.next     = &QNode2;
	//用于显示中间元素
    QNode2.sequence = 2;	//中间的框(动作指针选定框)的顺序索引为 1 
    QNode2.block[0] = 6; QNode2.block[1] = 40; QNode2.block[2] = 36; QNode2.block[3] = 48;
    QNode2.element  = &NodeB;
    QNode2.next     = &QNode3;
	//用于显示右侧元素
    QNode3.sequence = 3;
    QNode3.block[0] = 20; QNode3.block[1] = 92; QNode3.block[2] = 24; QNode3.block[3] = 32;
    QNode3.element  = &NodeC;
    QNode3.next     = &QNode4;
	//用于显示右侧隐藏元素
    QNode4.sequence = 4;
    QNode4.block[0] = 20; QNode4.block[1] = 128; QNode4.block[2] = 24; QNode4.block[3] = 32;
    QNode4.element  = &NodeD;
    QNode4.next     = NULL;
	
	//将5个队列结点串起来,方便快速定位队头队尾
	pCurrentQueue->head = &QNode0;
	pCurrentQueue->mid = &QNode2;
    pCurrentQueue->tail = &QNode4;
	pCurrentQueue->Floor = 1;//菜单层级
	
	ShowMenu(1);	//该行用于初始化后显示菜单
	
	
	
}


/* ---------------- 队列结点的元素(LinkNode类型)左移---------------- */
void Left_Shift(void)//逆时针转
{
																														// 动画开关
																														//if (menuOptions.enableAnimation) Animation_Start(1,Controller,menuOptions.AnimationFrame);
	// 元素集体左移
	for(QueueNode *p=pCurrentQueue->head;p != NULL;p=p->next)//判断条件共有五个循环
	{
		 //---------------- 元素平移---------------- 
		p->element = p->element->next;	//队列结点选取的链表图像结点向后平移
	}
	//关闭文案
	ShowText(0);
	//刷新菜单
	ShowMenu(1);

}

/* ---------------- 队列结点的元素(LinkNode类型)右移---------------- */
void Right_Shift(void)//顺时针转
{
																															// 动画开关
																															//if (menuOptions.enableAnimation) Animation_Start(0,Controller,menuOptions.AnimationFrame);
	// 元素集体右移
	for(QueueNode *p=pCurrentQueue->head;p != NULL;p=p->next)//判断条件共有五个循环
	{		
		/* ---------------- 元素平移---------------- */
		p->element = p->element->last;	//队列结点选取的链表图像结点向前平移
	}
	//关闭文案
	ShowText(0);
	//刷新菜单
	ShowMenu(1);	//效果校正
	
}

void MenuForward(void)//进入次级菜单(或执行绑定函数命令)目前仅2级菜单,无命令
{
	u8 i;
	//当子链存在时,即当前指向元素存在子菜单链表
	if(pCurrentQueue->mid->element->ChildMenuLink)
	{
		for(i=3;i<12;i++)
		{
			OLED_OffsetUpdate(i,0);	//加速度
		}
	/*
		Queue *ChildQueue = Queue_Init(pCurrentQueue->mid->element->ChildMenuLink,pCurrentQueue->Floor + 1);//创建子队列
		if (ChildQueue)
		{
			ChildQueue->Parent = (struct Queue*)pCurrentQueue;//绑定队列继承关系
			
			pCurrentQueue = ChildQueue;//显示区块指向当前队列
			
			MenuDown(8);	//菜单下移动画
			ShowMenu(1);//更新菜单
			return;
		}
		*/
	}
	//存在动作函数时,执行动作函数
	else if(pCurrentQueue->mid->element->Action)
	{
		pCurrentQueue->mid->element->Action();
	}
	else
	{
		OLED_ClearBuf();
		OLED_ShowString(25,5,"Nothing !",1,16,8);
		OLED_Update();
	}
}


/**
  * @brief  销毁当前队列释放内存,回到父级队列,即回溯父菜单
  * @param  无
  * @retval 无
  */
void MenuBack(void)//返回上级菜单,目前仅2级菜单,无命令
{
	u8 i;
	if (pCurrentQueue->Floor == 1) {return;}//达到菜单顶部,没有更顶层菜单或子函数了,返回
	
	for(i=6;i<13;i++)
	{
		OLED_OffsetUpdate(-i,0);	//非线性显存向下迁移
	}
	

	MenuDown(3);

	ShowMenu(1);

}


/**
  * @brief  在BLOCK位置展示一个正确的菜单
			让图像尺寸能在三个区块正确匹配大小,以ShowImage函数为实现函数.		
  * @param  Update_Bit 控制是否立即刷新,0:不刷新屏幕;1刷新屏幕
  */
void ShowMenu(u8 Update_Bit)
{

	/* ---------------- 不在非显示区块(Queue队列头结点和尾结点)的图像操作---------------- */
	for(QueueNode *p=pCurrentQueue->head->next;p->next != NULL;p=p->next)//
	{		

		//更改图像尺寸大小
		if(p->sequence==2 )
		{
			scale24x32_to36x48(p->element->Image, ImageTemp);  // 将图像缩放,把结果写进 ImageTemp
		
			OLED_ShowImage(p->block[0], p->block[1],
						   p->block[2], p->block[3],
						   ImageTemp,1);                            // 传缓冲区地址
		}
		
		else//图像无需转换
		{
			   OLED_ShowImage(p->block[0], p->block[1],
			   p->block[2], p->block[3],
			   (uint8_t *)p->element->Image,1);                            // 传缓冲区地址	
		}
	#include "Encoder.h"	
	ShowText(0);
	ShowArrow();
	if(Update_Bit) {OLED_Update();}//
	}
}

/**
  * @brief  做一个菜单从上方屏幕外逐渐下移到屏幕中动画效果
  * @param  Step:步长
  * @retval 无
  */
void MenuDown(uint8_t Step)
{

	u8 i;
	u8 Line=0;//累计的行数
	
	ShowMenu(0);//刷新
	
	memcpy(ImageTemp,DRAW_BUF, sizeof(DRAW_BUF));
	
																														/*
																														for(i=64;i>=Step;i-=Step)	//OLED:高64*宽128
																														{	

																															OLED_ClearBufArea(64-i,0,i,128);
																															OLED_OffsetUpdate(-i,0);
																															OLED_ShowImage(0,0,64,128,(const u8 *)ImageTemp,1);//代替menu函数
																															if(Step>2&&i<24)Step--;
																														}
																														*/
	OLED_Update();//最后将Buf数组更新到OLED屏幕上
}



/**
  * @brief  中心图像下方显示描述文案
  * @param  Bit:控制是否反色
  * @retval 无
  */
void ShowText(u8 Bit)
{
	OLED_ShowChinese(QNode2.block[0]+34, QNode2.block[1]+7,(char*)pCurrentQueue->mid->element->Str,16,16,1,Bit); 
}

/**
  * @brief  显示菜单底部的箭头
  */
void ShowArrow()
{
	OLED_ShowImage(56,58,8,8,(uint8_t *)Arrow,1);
}


/**
  * @brief  将缓存数组24*32通过knn算法拉伸到36*48
  * @param  源图像数组,新图像数组
  * @retval 无
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