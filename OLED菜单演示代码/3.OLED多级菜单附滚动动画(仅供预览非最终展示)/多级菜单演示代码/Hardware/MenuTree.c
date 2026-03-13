/* ---------------- MenuTree.c ---------------- */
/* ---------------- 菜单数据结构的定义和结构操作 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include "OLED.h"
#include "Delay.h"
#include <stdlib.h>//申请内存

/* ---------------- 创建链表并初始化头结点参数 ---------------- */
Link* Link_Init(uint8_t  ID,
               const uint8_t *img,
               const char *str,void (*action)(void))	//函数执行完毕后，NewLink 被销毁,因此不能返回指针形式
{
	// 1. 申请堆内存
	Link *NewLink = (Link *)malloc(sizeof(Link));
	
	LinkNode *HeadNode = (LinkNode *)malloc(sizeof(LinkNode));
	
    if (HeadNode == NULL)  return NewLink;   // 申请失败处理
	
	 // 2. 初始化头节点
    HeadNode->id   = ID;
    HeadNode->Image= img;
    HeadNode->Str  = str;
    HeadNode->next = NULL;
    HeadNode->last = NULL;
	HeadNode->Action=action;

	// 3. 配置连接关系
	NewLink->head=NewLink->tail=HeadNode;

	return NewLink;
}	


/* ---------------- 链表添加图像元素 ---------------- */
BitAction Link_PushBack(Link *lk,
                   uint8_t  id,
                   const uint8_t *img,
                   const char *str,void (*action)(void))
{
    if (lk == NULL) return 0;

    /* 1. 新建节点 */
    LinkNode *NewNode = (LinkNode *)malloc(sizeof(LinkNode));
    if (NewNode == NULL) return 0;

    /* 2. 填数据 */
    NewNode->id   = id;
    NewNode->Image= img;
    NewNode->Str  = str;
    NewNode->next = lk->head;//形成环形链表
    NewNode->last = lk->tail;
	NewNode->Action=action;

    /* 3. 挂到链表尾部 */
    lk->tail->next = NewNode;//把链表旧尾LinkNode结点的下一节点指向新节点
    lk->tail       = NewNode;//更新链表尾结点
	lk->head->last = NewNode;//头节点的 last 指向新尾节点（保持环形）
    return 1;
}


/**
  * @brief  创建新队列,为每个队列结点绑定图像链表结点
  * @param  无
  * @retval 无
  */
/* ---------------- 创建队列并初始化 ---------------- */
Queue* Queue_Init(const Link* lk,const u8 floor)//“承诺函数内部不会修改 lk 指向的内容”
{
	Queue *NewQueue = (Queue *)malloc(sizeof(Queue));

	// 1. 为5个队列节点申请堆内存
	QueueNode *NewQNode0 = (QueueNode *)malloc(sizeof(QueueNode));//屏幕隐藏左侧
    QueueNode *NewQNode1 = (QueueNode *)malloc(sizeof(QueueNode));//屏幕左侧
	QueueNode *NewQNode2 = (QueueNode *)malloc(sizeof(QueueNode));//屏幕中
	QueueNode *NewQNode3 = (QueueNode *)malloc(sizeof(QueueNode));
	QueueNode *NewQNode4 = (QueueNode *)malloc(sizeof(QueueNode));//屏幕隐藏右
	
	if(NewQNode4 == NULL)
	{//内存分配失败处理
		OLED_ClearBuf();
		OLED_ShowString(25,5,"Heap Error",1,16,8);
		OLED_Update();
		return 0;
	}
	
	
	NewQueue->head = NewQNode0;
	NewQueue->mid  = NewQNode2;
    NewQueue->tail = NewQNode4;
	NewQueue->Floor = floor;//菜单层级
    //NewQueue->size = 5;
	
	
	// 2. 配置队列结点初始参数
    NewQNode0->sequence = 0;
    NewQNode0->block[0] = 20; NewQNode0->block[1] = -32;  NewQNode0->block[2] = 24; NewQNode0->block[3] = 32;
    NewQNode0->next     = NewQNode1;
	
	NewQNode1->sequence = 1;
    NewQNode1->block[0] = 20; NewQNode1->block[1] = 5;  NewQNode1->block[2] = 24; NewQNode1->block[3] = 32;
    NewQNode1->next     = NewQNode2;

    NewQNode2->sequence = 2;
    NewQNode2->block[0] = 6; NewQNode2->block[1] = 40; NewQNode2->block[2] = 36; NewQNode2->block[3] = 48;
    NewQNode2->next     = NewQNode3;

    NewQNode3->sequence = 3;
    NewQNode3->next     = NewQNode4;
    NewQNode3->block[0] = 20; NewQNode3->block[1] = 92; NewQNode3->block[2] = 24; NewQNode3->block[3] = 32;

    NewQNode4->sequence = 4;
    NewQNode4->block[0] = 20; NewQNode4->block[1] = 128; NewQNode4->block[2] = 24; NewQNode4->block[3] = 32;
    NewQNode4->next     = NULL;
	
	uint8_t i=0,num=0;//初始值
	
	// 3. 为每个队列结点绑定最初的element(来自于lk)
	//循环五次,取参数lk链表的图像结点元素遍历填入QNodex
	for(QueueNode *p1=NewQueue->head;p1 != NULL;p1=p1->next) //NewQueue->head第一个队列结点,为左侧隐藏结点,
															//	其存储元素应为链表的最后结点图像!
	{
		num=0;//每次循环后num重置,这行不能忽略
		LinkNode *p2=lk->tail;//默认左侧隐藏结点存放链表的最后元素
		//该循环实现QNodex一一映射链表第x个元素,再填入队列结点的element中;
		for(p2=lk->tail;num<i;p2=p2->next)//第一个队列结点对应环形链表末尾结点
		{
			num++;
		}
		p1->element=p2;
		
		i++;//用于和num比较,决定取链表几号元素
	}
	
	return NewQueue;
}

/* 释放整条队列（不释放 element，因为 element 指向外部 Link 链表） */
void Queue_Destroy(Queue *queue)
{
    if (queue == NULL) return;

    QueueNode *current = queue->head;
    QueueNode *next;

    /* 逐个释放 QueueNode */
    for (uint8_t i = 0; i < 5; i++) {
        next = current->next;
        free(current);          /* 只释放节点本身 */
        current = next;
    }

    /* 最后释放队列控制块 */
    free(queue);
}




/*
在非malloc申请堆内存的变量中:
| 写法             | 放在哪里            | 生命周期    | 读写权限 | 作用域       |
| -------------- | --------------- | ------- 		  | ---- 	| --------- |
| `static`       | RAM（.bss/.data） | 整个程序运行期 | 可读可写 | 仅本文件/函数   |
| `static const` | Flash（RO-data）  | 整个程序运行期 | 只读   	| 仅本文件/函数   |
| `const`        | Flash（RO-data）  | 整个程序运行期 | 只读   	| 全局（外部可引用)|



*/