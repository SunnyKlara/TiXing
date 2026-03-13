/* ---------------- Image.h ---------------- */
#ifndef __IMAGE_H
#define __IMAGE_H
/*----------------图像结点,存有图像数据和下一层队列指针----------------*/
typedef struct LinkNode{
	
	
	
} LinkNode;

typedef struct Link{//头结点为第一个链表图像元素
	
	
} Link;

typedef struct QueueNode {
	
	
} QueueNode;

typedef struct Queue{
	
	
} Queue;
/* ---------------- 分配一级菜单初始参数 ---------------- */



/*--------------------------------对外接口函数--------------------------------*/
void MenuForward(void);		//进入下级菜单
void MenuBack(void);		//返回上级菜单
void scale24x32_to36x48(const uint8_t Image2[3*32], uint8_t ImageNew[5*48]);
//任意图像缩放算法
uint8_t* scale_1bit(const uint8_t *src,
                      uint16_t src_h,uint16_t src_w,
                     uint8_t       *dst,
                     uint16_t dst_h,uint16_t dst_w  );
void Image_Init(void);
void Left_Shift(void);
void Right_Shift(void);
void ShowMenu(u8 Update_Bit);
void ShowText(u8 Bit);
void ShowArrow();

#endif

/*
显示框架:整个显示实现机制以显示一个当前Queue型队列的链表图像结点数据为底层,Queue可以找到它的子队列和父队列
每个队列由其父队列的QNode2结点所存的element->childmenuQueue决定.

*/