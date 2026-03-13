/* ---------------- CreateMenu.c ---------------- */
/* ---------------- 使用MenuTree.h的方法来创建元素链表,菜单队列等实例并绑定动作等 ---------------- */
/* ---------------- 为一级菜单配置子链或动作函数逻辑 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include "MenuTree.h"
#include "CreateMenu_ICONS.h"
#include "Action.h"
#include "OLED.h"
/**
  * @brief  二级菜单初始化,动态处理链表
			创建的链表结点和其控制块会存储在ram中,须扩大堆内存heap size
  * @retval 无
  */
void Menu_Init(void)
{
	/* ---------------- 创建新链表并推入链表结点---------------- */
	
	Link* LinkB=Link_Init('A',ImageB_0,"飞机大战",0);
	//Link_PushBack(LinkB,'B',ImageB_1,"像素鸟",FlappyBird_Enter);
	//Link_PushBack(LinkB,'C',ImageB_2,"贪吃蛇",SNAKE_Enter);
	//Link_PushBack(LinkB,'D',ImageB_3,"恐龙",DinoWar_Enter);
	//Link_PushBack(LinkB,'E',ImageB_4,"CUBE",MPU6050_Enter);
	
	/*
	//创建子链
	Link* LinkA=Link_Init('A',ImageA_0,"High",0);
	Link_PushBack(LinkA,'B',ImageA_1,"Item",0);
	Link_PushBack(LinkA,'C',ImageA_2,"Jake",0);
	Link_PushBack(LinkA,'D',ImageB_3,"Keil",0);
	*/
	
	/*
	Link* LinkC=Link_Init('A',ImageC_0,"inx1",0);
	Link_PushBack(LinkC,'B',ImageC_1,"inx2",0);
	Link_PushBack(LinkC,'C',ImageC_2,"inx3",0);
	Link_PushBack(LinkC,'D',ImageB_3,"inx4",0);
	*/
	
	Link* LinkD=Link_Init('A',ImageD_0,"时间",0);
	//Link_PushBack(LinkD,'B',ImageD_1,"刷新",OLED_Refresh);
	//Link_PushBack(LinkD,'C',ImageD_2,"动画",FPSSET_Enter);
	//Link_PushBack(LinkD,'D',ImageD_3,"系统",SystemSET_Enter);
	
	/*
	Link* LinkE=Link_Init('A',ImageE_0,"inx1",0);
	Link_PushBack(LinkE,'B',ImageE_1,"inx2",0);
	Link_PushBack(LinkE,'C',ImageE_2,"inx3",0);
	Link_PushBack(LinkE,'D',ImageB_3,"inx4",0);
	*/
	
	/* ---------------- 将二级菜单链表绑定一级菜单所属LinkNode或把节点元素绑定要执行的动作 ---------------- */
	NodeB.ChildMenuLink=(struct Link*)LinkB;
	
	//NodeA.ChildMenuLink=(struct Link*)LinkA;
	//NodeA.Action=CLOCK_Enter;
	
	
	//NodeC.ChildMenuLink=(struct Link*)LinkC;
	//NodeC.Action=Emoji_Enter;
	
	NodeD.ChildMenuLink=(struct Link*)LinkD;
	
	//NodeE.ChildMenuLink=(struct Link*)LinkE;
	//NodeE.Action=Stopwatch_Enter;
	
	
}

