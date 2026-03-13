/* ---------------- Animation.c ---------------- */
/* ---------------- 菜单元素的线性缩放平移动画 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "math.h"
#include <stdlib.h>
#define PHYSICS_SCALE 10  // 固定点数缩放,将浮点运算转为整数运算

//通过移位,缩小精度避免浮点运算的线性插值算法
AnimationController Controller[5] = {	//需要手动计算delta
    {   // 第一个结构体元素:左侧隐藏元素
		.currentY = 20<<8,
        .deltaY = 0,
		.stepY = 0,
		
        .currentX =-32<<8 ,
        .deltaX = 0,//默认值,待计算
        .stepX = 0,
        
        .currentHeight = 24<<8,
        .deltaHeight = 0,
        .stepHeight = 0,
		
        .currentWidth = 32<<8,
        .deltaWidth = 0,
        .stepWidth = 0
    },
    {   // 第二个结构体元素，可以赋予不同的初值
		.currentY = 20<<8,
        .deltaY = 0,
		.stepY = 0,
		
        .currentX =5<<8 ,
        .deltaX = 0,//默认值,待计算
        .stepX = 0,
        
        .currentHeight = 24<<8,
        .deltaHeight = 0,
        .stepHeight = 0,
		
        .currentWidth = 32<<8,
        .deltaWidth = 0,
        .stepWidth = 0
    },
	
    {   //中间元素
		.currentY =6<<8,
        .deltaY = 0,
		.stepY = 0,
		
        .currentX =40<<8 ,
        .deltaX = 0,//默认值,待计算
        .stepX = 0,
        
        .currentHeight = 36<<8,
        .deltaHeight = 0,
        .stepHeight = 0,
		
        .currentWidth = 48<<8,
        .deltaWidth = 0,
        .stepWidth = 0
    },
    {   // 第四个结构体元素
        .currentY = 20<<8,
        .deltaY = 0,
		.stepY = 0,
		
        .currentX =92<<8 ,
        .deltaX = 0,//默认值,待计算
        .stepX = 0,
        
        .currentHeight = 24<<8,
        .deltaHeight = 0,
        .stepHeight = 0,
		
        .currentWidth = 32<<8,
        .deltaWidth = 0,
        .stepWidth = 0
    },
	{   // 第五个结构体元素
		.currentY = 20<<8,
		.deltaY = 0,
		.stepY = 0,
		
		.currentX =128<<8 ,
		.deltaX = 0,//默认值,待计算
		.stepX = 0,
		
		.currentHeight = 24<<8,
		.deltaHeight = 0,
		.stepHeight = 0,
		
		.currentWidth = 32<<8,
		.deltaWidth = 0,
		.stepWidth = 0
	}
};


void Animation_Start(u8 Direction,AnimationController* ConstController,u8 Frame)
{
	// 1. 定义本地副本（大小与原数组一致）
    AnimationController Controller[5];
    // 2. 深拷贝原始数据（不修改传入的 Controller）
    memcpy(Controller, ConstController, sizeof(Controller));
	// 3.计算每个元素的初末位移差值
	if(Direction)
	{
		for (int i = 1; i <5 ; i++) {
			Controller[i].deltaY = Controller[i-1].currentY-Controller[i].currentY;
			Controller[i].deltaX = Controller[i-1].currentX-Controller[i].currentX;
			Controller[i].deltaHeight = Controller[i-1].currentHeight-Controller[i].currentHeight;
			Controller[i].deltaWidth = Controller[i-1].currentWidth-Controller[i].currentWidth;
		}
	}
	else	//分方向而定
	{
		for (int i = 0; i <4 ; i++) {	//计算位移差和高度差
			
			Controller[i].deltaY = Controller[i+1].currentY-Controller[i].currentY;
			Controller[i].deltaX = Controller[i+1].currentX-Controller[i].currentX;
			Controller[i].deltaHeight = Controller[i+1].currentHeight-Controller[i].currentHeight;
			Controller[i].deltaWidth = Controller[i+1].currentWidth-Controller[i].currentWidth;
		}
	}
	// 4.计算缩放精度8位后的步长
	for (int i = 0; i <5 ; i++) {
		Controller[i].stepY = (Controller[i].deltaY)/Frame;
		Controller[i].stepX = (Controller[i].deltaX)/Frame;
		Controller[i].stepHeight = (Controller[i].deltaHeight)/Frame;
		Controller[i].stepWidth = (Controller[i].deltaWidth)/Frame;
	}
	// 5.开始执行帧数Frame的线性动画帧
	for(int j=1;j<Frame;j++)
	{
		int i=0;	//i从0到4
		OLED_ClearBuf();
		OLED_ShowNum(0,100,FPS,3,16,8,0);
		ShowArrow();
		for(QueueNode *p=pCurrentQueue->head;p != NULL;p=p->next)//
		{	
			
			Controller[i].currentY+=Controller[i].stepY;
			p->block[0]=(Controller[i].currentY)>>8;
			
			Controller[i].currentX+=Controller[i].stepX;
			p->block[1]=(Controller[i].currentX+(Controller[i].stepX ))>>8;
			
			Controller[i].currentHeight+=Controller[i].stepHeight;
			p->block[2]=(Controller[i].currentHeight+(Controller[i].stepHeight ))>>8;
			
			Controller[i].currentWidth+=Controller[i].stepWidth;
			p->block[3]=(Controller[i].currentWidth+(Controller[i].stepWidth ))>>8;
			
			u8 sourceHeight= p->element->id =='B'?36:24;
			u8 sourceWidth = p->element->id =='B'?48:32;
			scale_1bit(p->element->Image,sourceHeight,sourceWidth,ImageTemp,p->block[2],p->block[3]);
			
			//画出单个帧的图像
			OLED_ShowImage(p->block[0], p->block[1],p->block[2], p->block[3],ImageTemp,1);  // 传缓冲区地址
			
			i++;
		}
		
		OLED_Update();	//更新显示
	}
	//6. 最终根据结果纠正剩余偏差
		int i=0;	//i从0到4
		for(QueueNode *p=pCurrentQueue->head;p != NULL;p=p->next)//
		{	
			
			p->block[0]=(ConstController[i].currentY)>>8;
			             
			p->block[1]=(ConstController[i].currentX)>>8;
			             
			p->block[2]=(ConstController[i].currentHeight)>>8;
			             
			p->block[3]=(ConstController[i].currentWidth)>>8;
			
			i++;
		}
}



// 初始化：只调用一次 =
void ElementOffset_Init(ElementOffset *sel, int16_t input) 
{
    // 所有值放大10倍存成整数，STM32才能算
    sel->current_input = sel->target_output = input * PHYSICS_SCALE;

    
    sel->velocity=1;
    
    sel->stiffness = 40;  // 黄金参数：果冻手感
    sel->damping = 6;     // 阻尼系数
    sel->is_moving = 0;   // 初始停止
}

/**
  * @brief  设置新的目标点
  * @brief  将系统的参考点（目标位置）从当前值改变到新值，导致输入偏差从0变为不为0，系统开始响应这个新的参考点。
  * @param  传入结构体指针，设置新的目标参考点，达到目标后触发绑定的action动作
  */
void ElementOffset_SetTarget(ElementOffset *sel, int16_t output,void (*action)) {
    sel->target_output = output * PHYSICS_SCALE;	//乘缩放系数，降低精度避免浮点运算
    sel->is_moving = 1;   // 标记开始运动
	sel->action=action;
}

/**
  * @brief  每一帧调用，计算更新每一帧的相应输出值
  * @param  传入结构体指针。
  */
void ElementOffset_Update(ElementOffset *sel) 
{
   if (!sel->is_moving) return;
   
   // 1. 计算时间步长（10ms = 0.01s）
   const float dt = 0.01f; // 10ms = 0.01秒
   
   // 2. 计算弹簧力（力 = -k * 位移）
   float spring = (sel->target_output - sel->current_input) * sel->stiffness;

    // 3. 计算阻尼力（力 = -d * 速度）
    float damp = sel->velocity * sel->damping;

    
    // 4. 更新速度（速度变化 = 力/质量 * dt）
    // 这里质量设为1（简化计算）
    sel->velocity += (spring - damp) * dt;

    // 5. 更新位置
    sel->current_input += sel->velocity * dt;

    // 6. 判断是否到达目标（误差<0.5像素）
    float distance = fabsf(sel->target_output - sel->current_input);

    
	// 停止运动
    if (distance < 0.5f ) {
        sel->current_input = sel->target_output;
        sel->is_moving = 0;
		sel->action();
    }
}



/*
//函数动态创建控制器,会占用内存
AnimationController* ControllerInit(void)
{
	static AnimationController controllers[4] = {0}; // 初始化为0

    // 在这里对每个控制器做默认初始化
    for (int i = 0; i < 4; i++) {
        controllers[i].currentX = 0;
        controllers[i].deltaX = 0;
        controllers[i].stepX = 1;
        // ... 其他字段 ...
	}
	return 0;
}
*/


