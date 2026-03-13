/* ---------------- Image.h ---------------- */
#ifndef __IMAGE_H
#define __IMAGE_H
/*----------------图像结点,存有图像数据和下一层队列指针----------------*/
typedef struct LinkNode{
    uint8_t id;
    const uint8_t *Image;	//指针指向存储的图标数据,数组类型
	const char *Str;
    struct LinkNode *next, *last;
	const struct Link *ChildMenuLink;
	void (*Action)(void);
} LinkNode;

typedef struct Link{//头结点为第一个链表图像元素
    LinkNode *head;
    LinkNode *tail;
	const uint8_t Floor;//显示这是第几层的菜单图像链表
    uint8_t size;
} Link;

typedef struct QueueNode {
    int sequence;
    int8_t block[5];//起始行,起始列,Height,Width
    LinkNode *element;
    struct QueueNode *next;
} QueueNode;

typedef struct Queue{
	uint8_t Floor;
    QueueNode *head;//每个head为Qnode0
	QueueNode *mid;//中间队列结点,第一层为QNode2
    QueueNode *tail;
	struct Queue *Parent;//上一层的父队列
	//struct Queue *Child;//下一层的子队列,暂时没用上
} Queue;
/* ---------------- 分配一级菜单初始参数 ---------------- */
extern LinkNode NodeA,NodeB,NodeC,NodeD,NodeE;	//一级菜单(根菜单)链表元素结点
extern QueueNode QNode0,QNode1,QNode2,QNode3,QNode4;	//一级菜单(根队列)队列结点
extern Queue RootQueue;			//根队列
extern Queue* pCurrentQueue;	//队列指针,指向当前所在队列

/*----------------菜单选项定义----------------*/
typedef struct MenuOptions{
	volatile uint8_t enableAnimation;//动画控制位
	volatile uint8_t AnimationFrame;
	volatile uint8_t SystemClock;	//系统主频,8~72(单位MHZ)
} MenuOptions;

/* ---------------- 全局变量 ---------------- */
extern MenuOptions menuOptions;

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
void MenuDown(uint8_t Step);
void ShowText(u8 Bit);
void ShowArrow();

#endif

/*
显示框架:整个显示实现机制以显示一个当前Queue型队列的链表图像结点数据为底层,Queue可以找到它的子队列和父队列
每个队列由其父队列的QNode2结点所存的element->childmenuQueue决定.

*/