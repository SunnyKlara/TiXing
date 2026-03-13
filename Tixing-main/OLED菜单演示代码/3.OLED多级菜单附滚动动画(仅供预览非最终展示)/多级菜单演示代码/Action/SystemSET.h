/* ---------------- SystemSET.h ---------------- */
#ifndef __SYSTEMSET_H
#define __SYSTEMSET_H

void SystemSET_Enter();
void SystemSET_KeyAction(uint8_t Event);
void SystemSET_ShowCurPage(void);
typedef struct SystemElement{
    const uint8_t Inx;		//若为255,代表此元素为末尾选项
	const char *Str;
	uint8_t *Value;
	const uint8_t *Icon;
	const struct SystemElement *ChildPage;
	void (*Action)();
} SystemElement;

/* ---------------- 选项指针指示器 ---------------- */
typedef struct PointerContorller	
{
	const SystemElement *CurrentPage;	//指向的当前页,ui的显示永远显示此指针指向页面
	uint8_t BaseOffset;		//页面的基本偏移
	uint8_t PointerOffset;	//指针的基本偏移,总偏移=页面基本偏移+指针基本偏移,利用总偏移充当SystemElement数组的索引
	uint8_t ReversePos;		//用于指示在第几个索引需要反色显示
	struct PointerContorller *ChildContorller;	//动态创建子控制器,用于记缓存进入子页前的当前页信息指针数据,在离开子页时恢复初态
} PointerContorller;
/* ---------------- 成员函数 ---------------- */
void Display_Reverse();
void SystemSET_SysClockConfig_Max();
void SystemSET_SysClockConfig_Medium();
void SystemSET_SysClockConfig_Low();
void SystemSET_SysClockConfig_HSI();
void Display_Note();

#endif