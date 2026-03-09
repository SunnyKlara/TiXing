#include "lcd.h"
#include "lcdfont.h"
#include "pic.h"
#include "logo.h"

/**
 * @brief  快速填充指定区域为单一颜色
 * @param  xsta,ysta: 起始坐标
 * @param  xend,yend: 结束坐标（不含该点）
 * @param  color: 填充颜色（16位RGB565格式）
 * @note   采用批量DMA传输，大幅提升填充速度
 */
void LCD_Fill(u16 xsta, u16 ysta, u16 xend, u16 yend, u16 color) {
    // 边界检查
    if (xsta >= LCD_WIDTH || ysta >= LCD_HEIGHT || xend < xsta || yend < ysta) return;

    // 计算填充区域（包含 xend, yend）
    u16 width = xend - xsta + 1;
    u16 height = yend - ysta + 1;
    u32 total_pixels = (u32)width * height;
    if (total_pixels == 0) return;

    // 设置填充区域（LCD_Fill 的 xend, yend 不包含最后一个点）
    LCD_Address_Set(xsta, ysta, xend, yend);

    // 使用静态缓冲区，避免DMA传输时局部变量被销毁的问题
    #define BUFFER_PIXELS 512
    static u8 buffer[BUFFER_PIXELS * 2];  // 改为static，确保DMA传输期间内存有效
    static u16 last_color = 0xFFFF;  // 缓存上次的颜色，避免重复填充
    
    // 只在颜色变化时重新填充缓冲区
    if (color != last_color) {
        for (u16 i = 0; i < BUFFER_PIXELS * 2; i += 2) {
            buffer[i] = color >> 8;    // 高8位
            buffer[i + 1] = color & 0xFF; // 低8位
        }
        last_color = color;
    }

    // 对于小区域（例如单行），使用逐像素写入以确保精度
    if (total_pixels < 16) {
        for (u32 i = 0; i < total_pixels; i++) {
            LCD_WR_DATA(color);
        }
    } else {
        // DMA 批量传输
        u32 remaining = total_pixels;
        while (remaining > 0) {
            u32 send_pixels = (remaining > BUFFER_PIXELS) ? BUFFER_PIXELS : remaining;
            LCD_Writ_Bus(buffer, send_pixels * 2);
            remaining -= send_pixels;
        }
    }
}

/******************************************************************************
      oˉêy?μ?÷￡o?ú???¨?????-μ?
      è??úêy?Y￡ox,y ?-μ?×?±ê
                color μ?μ???é?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_DrawPoint(u16 x,u16 y,u16 color)
{
	LCD_Address_Set(x,y,x,y);//éè??1a±ê???? 
	LCD_WR_DATA(color);
} 


/******************************************************************************
      oˉêy?μ?÷￡o?-??
      è??úêy?Y￡ox1,y1   ?eê?×?±ê
                x2,y2   ???1×?±ê
                color   ??μ???é?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2,u16 color)
{
	u16 t; 
	int xerr=0,yerr=0,delta_x,delta_y,distance;
	int incx,incy,uRow,uCol;
	delta_x=x2-x1; //????×?±ê??á? 
	delta_y=y2-y1;
	uRow=x1;//?-???eμ?×?±ê
	uCol=y1;
	if(delta_x>0)incx=1; //éè??μ￥2?·??ò 
	else if (delta_x==0)incx=0;//′1?±?? 
	else {incx=-1;delta_x=-delta_x;}
	if(delta_y>0)incy=1;
	else if (delta_y==0)incy=0;//?????? 
	else {incy=-1;delta_y=-delta_y;}
	if(delta_x>delta_y)distance=delta_x; //??è??ù±???á?×?±ê?á 
	else distance=delta_y;
	for(t=0;t<distance+1;t++)
	{
		LCD_DrawPoint(uRow,uCol,color);//?-μ?
		xerr+=delta_x;
		yerr+=delta_y;
		if(xerr>distance)
		{
			xerr-=distance;
			uRow+=incx;
		}
		if(yerr>distance)
		{
			yerr-=distance;
			uCol+=incy;
		}
	}
}


/******************************************************************************
      oˉêy?μ?÷￡o?-??D?
      è??úêy?Y￡ox1,y1   ?eê?×?±ê
                x2,y2   ???1×?±ê
                color   ??D?μ???é?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2,u16 color)
{
	LCD_DrawLine(x1,y1,x2,y1,color);
	LCD_DrawLine(x1,y1,x1,y2,color);
	LCD_DrawLine(x1,y2,x2,y2,color);
	LCD_DrawLine(x2,y1,x2,y2,color);
}


/******************************************************************************
      oˉêy?μ?÷￡o?-?2
      è??úêy?Y￡ox0,y0   ?2D?×?±ê
                r       °???
                color   ?2μ???é?
      ·μ???μ￡o  ?T
******************************************************************************/
void Draw_Circle(u16 x0,u16 y0,u8 r,u16 color)
{
	int a,b;
	a=0;b=r;	  
	while(a<=b)
	{
		LCD_DrawPoint(x0-b,y0-a,color);             //3           
		LCD_DrawPoint(x0+b,y0-a,color);             //0           
		LCD_DrawPoint(x0-a,y0+b,color);             //1                
		LCD_DrawPoint(x0-a,y0-b,color);             //2             
		LCD_DrawPoint(x0+b,y0+a,color);             //4               
		LCD_DrawPoint(x0+a,y0-b,color);             //5
		LCD_DrawPoint(x0+a,y0+b,color);             //6 
		LCD_DrawPoint(x0-b,y0+a,color);             //7
		a++;
		if((a*a+b*b)>(r*r))//?D??òa?-μ?μ?ê?·?1y??
		{
			b--;
		}
	}
}

/******************************************************************************
      oˉêy?μ?÷￡o??ê?oo×?′?
      è??úêy?Y￡ox,y??ê?×?±ê
                *s òa??ê?μ?oo×?′?
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o? ?é?? 16 24 32
                mode:  0·?μt?ó?￡ê?  1μt?ó?￡ê?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowChinese(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
	while(*s!=0)
	{
		if(sizey==12) LCD_ShowChinese12x12(x,y,s,fc,bc,sizey,mode);
		else if(sizey==16) LCD_ShowChinese16x16(x,y,s,fc,bc,sizey,mode);
		else if(sizey==24) LCD_ShowChinese24x24(x,y,s,fc,bc,sizey,mode);
		else if(sizey==32) LCD_ShowChinese32x32(x,y,s,fc,bc,sizey,mode);
		else return;
		s+=2;
		x+=sizey;
	}
}

/******************************************************************************
      oˉêy?μ?÷￡o??ê?μ￥??12x12oo×?
      è??úêy?Y￡ox,y??ê?×?±ê
                *s òa??ê?μ?oo×?
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o?
                mode:  0·?μt?ó?￡ê?  1μt?ó?￡ê?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowChinese12x12(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
	u8 i,j,m=0;
	u16 k;
	u16 HZnum;//oo×?êy??
	u16 TypefaceNum;//ò???×?·??ù??×??ú′óD?
	u16 x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	                         
	HZnum=sizeof(tfont12)/sizeof(typFNT_GB12);	//í3??oo×?êy??
	for(k=0;k<HZnum;k++) 
	{
		if((tfont12[k].Index[0]==*(s))&&(tfont12[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(!mode)//·?μt?ó·?ê?
					{
						if(tfont12[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//μt?ó·?ê?
					{
						if(tfont12[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//?-ò???μ?
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}				  	
		continue;  //2é?òμ???ó|μ??ó×??aá￠?′í?3?￡?·à?1?à??oo×????′è??￡′?à′ó°?ì
	}
} 

/******************************************************************************
      oˉêy?μ?÷￡o??ê?μ￥??16x16oo×?
      è??úêy?Y￡ox,y??ê?×?±ê
                *s òa??ê?μ?oo×?
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o?
                mode:  0·?μt?ó?￡ê?  1μt?ó?￡ê?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowChinese16x16(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
	u8 i,j,m=0;
	u16 k;
	u16 HZnum;//oo×?êy??
	u16 TypefaceNum;//ò???×?·??ù??×??ú′óD?
	u16 x0=x;
  TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	HZnum=sizeof(tfont16)/sizeof(typFNT_GB16);	//í3??oo×?êy??
	for(k=0;k<HZnum;k++) 
	{
		if ((tfont16[k].Index[0]==*(s))&&(tfont16[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(!mode)//·?μt?ó·?ê?
					{
						if(tfont16[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//μt?ó·?ê?
					{
						if(tfont16[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//?-ò???μ?
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}				  	
		continue;  //2é?òμ???ó|μ??ó×??aá￠?′í?3?￡?·à?1?à??oo×????′è??￡′?à′ó°?ì
	}
} 


/******************************************************************************
      oˉêy?μ?÷￡o??ê?μ￥??24x24oo×?
      è??úêy?Y￡ox,y??ê?×?±ê
                *s òa??ê?μ?oo×?
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o?
                mode:  0·?μt?ó?￡ê?  1μt?ó?￡ê?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowChinese24x24(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
	u8 i,j,m=0;
	u16 k;
	u16 HZnum;//oo×?êy??
	u16 TypefaceNum;//ò???×?·??ù??×??ú′óD?
	u16 x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	HZnum=sizeof(tfont24)/sizeof(typFNT_GB24);	//í3??oo×?êy??
	for(k=0;k<HZnum;k++) 
	{
		if ((tfont24[k].Index[0]==*(s))&&(tfont24[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(!mode)//·?μt?ó·?ê?
					{
						if(tfont24[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//μt?ó·?ê?
					{
						if(tfont24[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//?-ò???μ?
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}				  	
		continue;  //2é?òμ???ó|μ??ó×??aá￠?′í?3?￡?·à?1?à??oo×????′è??￡′?à′ó°?ì
	}
} 

/******************************************************************************
      oˉêy?μ?÷￡o??ê?μ￥??32x32oo×?
      è??úêy?Y￡ox,y??ê?×?±ê
                *s òa??ê?μ?oo×?
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o?
                mode:  0·?μt?ó?￡ê?  1μt?ó?￡ê?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowChinese32x32(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode)
{
	u8 i,j,m=0;
	u16 k;
	u16 HZnum;//oo×?êy??
	u16 TypefaceNum;//ò???×?·??ù??×??ú′óD?
	u16 x0=x;
	TypefaceNum=(sizey/8+((sizey%8)?1:0))*sizey;
	HZnum=sizeof(tfont32)/sizeof(typFNT_GB32);	//í3??oo×?êy??
	for(k=0;k<HZnum;k++) 
	{
		if ((tfont32[k].Index[0]==*(s))&&(tfont32[k].Index[1]==*(s+1)))
		{ 	
			LCD_Address_Set(x,y,x+sizey-1,y+sizey-1);
			for(i=0;i<TypefaceNum;i++)
			{
				for(j=0;j<8;j++)
				{	
					if(!mode)//·?μt?ó·?ê?
					{
						if(tfont32[k].Msk[i]&(0x01<<j))LCD_WR_DATA(fc);
						else LCD_WR_DATA(bc);
						m++;
						if(m%sizey==0)
						{
							m=0;
							break;
						}
					}
					else//μt?ó·?ê?
					{
						if(tfont32[k].Msk[i]&(0x01<<j))	LCD_DrawPoint(x,y,fc);//?-ò???μ?
						x++;
						if((x-x0)==sizey)
						{
							x=x0;
							y++;
							break;
						}
					}
				}
			}
		}				  	
		continue;  //2é?òμ???ó|μ??ó×??aá￠?′í?3?￡?·à?1?à??oo×????′è??￡′?à′ó°?ì
	}
}


/******************************************************************************
      oˉêy?μ?÷￡o??ê?μ￥??×?·?
      è??úêy?Y￡ox,y??ê?×?±ê
                num òa??ê?μ?×?·?
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o?
                mode:  0·?μt?ó?￡ê?  1μt?ó?￡ê?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowChar(u16 x,u16 y,u8 num,u16 fc,u16 bc,u8 sizey,u8 mode)
{
	u8 temp,sizex,t,m=0;
	u16 i,TypefaceNum;//ò???×?·??ù??×??ú′óD?
	u16 x0=x;
	sizex=sizey/2;
	TypefaceNum=(sizex/8+((sizex%8)?1:0))*sizey;
	num=num-' ';    //μ?μ???ò?oóμ??μ
	LCD_Address_Set(x,y,x+sizex-1,y+sizey-1);  //éè??1a±ê???? 
	for(i=0;i<TypefaceNum;i++)
	{ 
		if(sizey==12)temp=ascii_1206[num][i];		       //μ÷ó?6x12×?ì?
		else if(sizey==16)temp=ascii_1608[num][i];		 //μ÷ó?8x16×?ì?
		else if(sizey==24)temp=ascii_2412[num][i];		 //μ÷ó?12x24×?ì?
		else if(sizey==32)temp=ascii_3216[num][i];		 //μ÷ó?16x32×?ì?
		else return;
		for(t=0;t<8;t++)
		{
			if(!mode)//·?μt?ó?￡ê?
			{
				if(temp&(0x01<<t))LCD_WR_DATA(fc);
				else LCD_WR_DATA(bc);
				m++;
				if(m%sizex==0)
				{
					m=0;
					break;
				}
			}
			else//μt?ó?￡ê?
			{
				if(temp&(0x01<<t))LCD_DrawPoint(x,y,fc);//?-ò???μ?
				x++;
				if((x-x0)==sizex)
				{
					x=x0;
					y++;
					break;
				}
			}
		}
	}   	 	  
}


/******************************************************************************
      oˉêy?μ?÷￡o??ê?×?·?′?
      è??úêy?Y￡ox,y??ê?×?±ê
                *p òa??ê?μ?×?·?′?
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o?
                mode:  0·?μt?ó?￡ê?  1μt?ó?￡ê?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowString(u16 x,u16 y,const u8 *p,u16 fc,u16 bc,u8 sizey,u8 mode)
{         
	while(*p!='\0')
	{       
		LCD_ShowChar(x,y,*p,fc,bc,sizey,mode);
		x+=sizey/2;
		p++;
	}  
}


/******************************************************************************
      oˉêy?μ?÷￡o??ê?êy×?
      è??úêy?Y￡omμ×êy￡?n??êy
      ·μ???μ￡o  ?T
******************************************************************************/
u32 mypow(u8 m,u8 n)
{
	u32 result=1;	 
	while(n--)result*=m;
	return result;
}


/******************************************************************************
      oˉêy?μ?÷￡o??ê???êy±?á?
      è??úêy?Y￡ox,y??ê?×?±ê
                num òa??ê???êy±?á?
                len òa??ê?μ???êy
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowIntNum(u16 x,u16 y,u16 num,u8 len,u16 fc,u16 bc,u8 sizey)
{         	
	u8 t,temp;
	u8 enshow=0;
	u8 sizex=sizey/2;
	for(t=0;t<len;t++)
	{
		temp=(num/mypow(10,len-t-1))%10;
		if(enshow==0&&t<(len-1))
		{
			if(temp==0)
			{
				LCD_ShowChar(x+t*sizex,y,' ',fc,bc,sizey,0);
				continue;
			}else enshow=1; 
		 	 
		}
	 	LCD_ShowChar(x+t*sizex,y,temp+48,fc,bc,sizey,0);
	}
} 


/******************************************************************************
      oˉêy?μ?÷￡o??ê?á???D?êy±?á?
      è??úêy?Y￡ox,y??ê?×?±ê
                num òa??ê?D?êy±?á?
                len òa??ê?μ???êy
                fc ×?μ???é?
                bc ×?μ?±3?°é?
                sizey ×?o?
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowFloatNum1(u16 x,u16 y,float num,u8 len,u16 fc,u16 bc,u8 sizey)
{         	
	u8 t,temp,sizex;
	u16 num1;
	sizex=sizey/2;
	num1=num*100;
	for(t=0;t<len;t++)
	{
		temp=(num1/mypow(10,len-t-1))%10;
		if(t==(len-2))
		{
			LCD_ShowChar(x+(len-2)*sizex,y,'.',fc,bc,sizey,0);
			t++;
			len+=1;
		}
	 	LCD_ShowChar(x+t*sizex,y,temp+48,fc,bc,sizey,0);
	}
}


/******************************************************************************
      oˉêy?μ?÷￡o??ê?í???
      è??úêy?Y￡ox,y?eμ?×?±ê
                length í???3¤?è
                width  í????í?è
                pic[]  í???êy×é    
      ·μ???μ￡o  ?T
******************************************************************************/
void LCD_ShowPicture(u16 x,u16 y,u16 length,u16 width,const u8 pic[])
{
	u16 i,j;
	u32 k=0;
	LCD_Address_Set(x,y,x+length-1,y+width-1);
	for(i=0;i<length;i++)
	{
		for(j=0;j<width;j++)
		{
			LCD_WR_DATA8(pic[k*2]);
			LCD_WR_DATA8(pic[k*2+1]);
			k++;
		}
	}			
}
void Fill_Circle(u16 x0, u16 y0, u8 r, u16 color) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    
    // ????”¨Bresenham??—?3??????????è????“?1??‰1é??????……?ˉ???€è??
    while (x <= y) {
        // è???????°??€?a—??￡?1?????……?°′?13?o???μ?????????é?¨?????‰
        LCD_Address_Set(x0 - x, y0 - y, x0 + x, y0 - y);
        for(int i = 0; i <= 2*x; i++) {
            LCD_WR_DATA(color);
        }
        
        // ?ˉ1?§°?????????é?¨????°′?13?o???μ
        LCD_Address_Set(x0 - x, y0 + y, x0 + x, y0 + y);
        for(int i = 0; i <= 2*x; i++) {
            LCD_WR_DATA(color);
        }
        
        // ?ˉ1?§°?????|??€????°′?13?o???μ?????????é?¨?????‰
        LCD_Address_Set(x0 - y, y0 - x, x0 + y, y0 - x);
        for(int i = 0; i <= 2*y; i++) {
            LCD_WR_DATA(color);
        }
        
        // ?ˉ1?§°?????????é?¨????°′?13?o???μ
        LCD_Address_Set(x0 - y, y0 + x, x0 + y, y0 + x);
        for(int i = 0; i <= 2*y; i++) {
            LCD_WR_DATA(color);
        }
        
        // ??′?–°Bresenham??—?3??????°
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/**
 * @brief  在LCD上绘制两端半圆的条形图案（改进版，完整填充）
 * @param  x: 条形图案中心x坐标
 * @param  y: 条形图案中心y坐标
 * @param  width: 条形图案总宽度（包括两端半圆）
 * @param  height: 条形图案高度（不包括两端半圆）
 * @param  color: 16位颜色值
 */
void LCD_DrawRoundedBar(u16 x, u16 y, u16 width, u16 height, u16 color) {
    // 确保宽度大于高度
    if (width <= height) return;

    // 计算半径和矩形部分的坐标
    int16_t radius = height / 2;
    int16_t rect_width = width - height; // 中间矩形宽度
    int16_t rect_x1 = x - rect_width / 2;
    int16_t rect_x2 = x + rect_width / 2;
    int16_t rect_y1 = y - height / 2;
    int16_t rect_y2 = y + height / 2;

    // 1. 绘制中间矩形部分（确保不覆盖半圆）
    if (rect_width > 0) {
        LCD_Fill(rect_x1, rect_y1, rect_x2, rect_y2, color);
    }

    // 2. 绘制左右半圆（优化 Bresenham 算法）
    int16_t centers[2] = {rect_x1, rect_x2}; // 左右圆心 x 坐标
    for (uint8_t i = 0; i < 2; i++) {
        int16_t circle_x = centers[i];
        int16_t circle_y = y;
        int16_t x_circle = 0;
        int16_t y_circle = radius;
        int16_t d = 3 - 2 * radius; // 优化初始决策参数

        while (y_circle >= x_circle) {
            // 绘制上下对称的水平线（确保直径等于 height）
            int16_t line_x1 = circle_x - y_circle;
            int16_t line_x2 = circle_x + y_circle;
            int16_t line_y1 = circle_y - x_circle;
            int16_t line_y2 = circle_y + x_circle;

            // 限制垂直范围，确保半圆直径不超过矩形高度
            if (line_y1 >= rect_y1 && line_y1 <= rect_y2 && line_x1 < LCD_WIDTH && line_x2 >= 0) {
                LCD_Fill(line_x1, line_y1, line_x2 - 1, line_y1, color);
            }
            if (line_y2 >= rect_y1 && line_y2 <= rect_y2 && line_x1 < LCD_WIDTH && line_x2 >= 0) {
                LCD_Fill(line_x1, line_y2, line_x2 - 1, line_y2, color);
            }

            // 绘制对称线（仅当 x != y）
            if (x_circle != y_circle) {
                line_x1 = circle_x - x_circle;
                line_x2 = circle_x + x_circle;
                line_y1 = circle_y - y_circle;
                line_y2 = circle_y + y_circle;
                if (line_y1 >= rect_y1 && line_y1 <= rect_y2 && line_x1 < LCD_WIDTH && line_x2 >= 0) {
                    LCD_Fill(line_x1, line_y1, line_x2 - 1, line_y1, color);
                }
                if (line_y2 >= rect_y1 && line_y2 <= rect_y2 && line_x1 < LCD_WIDTH && line_x2 >= 0) {
                    LCD_Fill(line_x1, line_y2, line_x2 - 1, line_y2, color);
                }
            }

            // 更新 Bresenham 参数（优化计算）
            x_circle++;
            if (d < 0) {
                d += 4 * x_circle + 6;
            } else {
                y_circle--;
                d += 4 * (x_circle - y_circle) + 10;
            }
        }
    }
}

void lcd_zi(u8 num)
{
//	if(num == 1)
//	{
//		LCD_ShowPicture(zi_x,zi_y, zi_width, zi_high, gImage_middle);
//	}
//	else if(num == 2)
//	{
//		LCD_ShowPicture(zi_x,zi_y, zi_width, zi_high, gImage_left);//绿
//	}
//	else if(num == 3)
//	{
//		LCD_ShowPicture(zi_x,zi_y, zi_width, zi_high, gImage_right);//蓝
//	}
//	else if(num == 4)
//	{
//		LCD_ShowPicture(zi_x,zi_y, zi_width, zi_high, gImage_back);//红底
//	}
}

// 定义提取数字各位和位数的函数
void getDigitsAndLength(u16 num, u8 *a, u8 *b, u8 *c, u8 *wei_shu) {
    *a = 0;
    *b = 0;
    *c = 0;
    *wei_shu = 0;
    
    if (num / 100 != 0) {
        *a = num / 100;
        *wei_shu = 3;
        *b = (num % 100) / 10;
        *c = num % 10;
    } else if (num / 10 != 0) {
        *b = num / 10;
        *wei_shu = 2;
        *c = num % 10;
    } else {
        *c = num;
        *wei_shu = 1;
    }
}
// 参数改为：const u8 **pic（指针的指针，接收外部指针的地址）
void dui_yin_width(u8 num, u8 *width, const u8 **pic) {
    if(num == 0) {
        *width = speed_0_width;
        *pic = gImage_speed_0_5153;  // 通过 *pic 修改外部指针的值
    } else if(num == 1) {
        *width = speed_1_width;
        *pic = gImage_speed_1_1553;
    }
    // 其余数字（2~9）的逻辑同上，均用 *pic 赋值
    else if(num == 2) { *width = speed_2_width; *pic = gImage_speed_2_4853; }
    else if(num == 3) { *width = speed_3_width; *pic = gImage_speed_3_4353; }
    else if(num == 4) { *width = speed_4_width; *pic = gImage_speed_4_5153; }
    else if(num == 5) { *width = speed_5_width; *pic = gImage_speed_5_4653; }
    else if(num == 6) { *width = speed_6_width; *pic = gImage_speed_6_4953; }
    else if(num == 7) { *width = speed_7_width; *pic = gImage_speed_7_4653; }
    else if(num == 8) { *width = speed_8_width; *pic = gImage_speed_8_4953; }
    else if(num == 9) { *width = speed_9_width; *pic = gImage_speed_9_4953; }
}
void R_width(u8 num, u8 *width, const u8 **pic) {
    if(num == 0) {
        *width = h_0_width;
        *pic = gImage_h_0_2425;  // 通过 *pic 修改外部指针的值
    } else if(num == 1) {
        *width = h_1_width;
        *pic = gImage_h_1_1125;
    }
    // 其余数字（2~9）的逻辑同上，均用 *pic 赋值
    else if(num == 2) { *width = h_2_width; *pic = gImage_h_2_2225; }
    else if(num == 3) { *width = h_3_width; *pic = gImage_h_3_1925; }
    else if(num == 4) { *width = h_4_width; *pic = gImage_h_4_2325; }
    else if(num == 5) { *width = h_5_width; *pic = gImage_h_5_2125; }
    else if(num == 6) { *width = h_6_width; *pic = gImage_h_6_2325; }
    else if(num == 7) { *width = h_7_width; *pic = gImage_h_7_2125; }
    else if(num == 8) { *width = h_8_width; *pic = gImage_h_8_2125; }
    else if(num == 9) { *width = h_9_width; *pic = gImage_h_9_2225; }
}
void G_width(u8 num, u8 *width, const u8 **pic) {
    if(num == 0) {
        *width = l_0_width;
        *pic = gImage_l_0_2425;  // 通过 *pic 修改外部指针的值
    } else if(num == 1) {
        *width = l_1_width;
        *pic = gImage_l_1_0925;
    }
    // 其余数字（2~9）的逻辑同上，均用 *pic 赋值
    else if(num == 2) { *width = l_2_width; *pic = gImage_l_2_2325; }
    else if(num == 3) { *width = l_3_width; *pic = gImage_l_3_2125; }
    else if(num == 4) { *width = l_4_width; *pic = gImage_l_4_2425; }
    else if(num == 5) { *width = l_5_width; *pic = gImage_l_5_2225; }
    else if(num == 6) { *width = l_6_width; *pic = gImage_l_6_2325; }
    else if(num == 7) { *width = l_7_width; *pic = gImage_l_7_2125; }
    else if(num == 8) { *width = l_8_width; *pic = gImage_l_8_2325; }
    else if(num == 9) { *width = l_9_width; *pic = gImage_l_9_2325; }
}
void B_width(u8 num, u8 *width, const u8 **pic) {
    if(num == 0) {
        *width = b_0_width;
        *pic = gImage_b_0_2425;  // 通过 *pic 修改外部指针的值
    } else if(num == 1) {
        *width = b_1_width;
        *pic = gImage_b_1_0925;
    }
    // 其余数字（2~9）的逻辑同上，均用 *pic 赋值
    else if(num == 2) { *width = b_2_width; *pic = gImage_b_2_2125; }
    else if(num == 3) { *width = b_3_width; *pic = gImage_b_3_1925; }
    else if(num == 4) { *width = b_4_width; *pic = gImage_b_4_2325; }
    else if(num == 5) { *width = b_5_width; *pic = gImage_b_5_2125; }
    else if(num == 6) { *width = b_6_width; *pic = gImage_b_6_2325; }
    else if(num == 7) { *width = b_7_width; *pic = gImage_b_7_2125; }
    else if(num == 8) { *width = b_8_width; *pic = gImage_b_8_2225; }
    else if(num == 9) { *width = b_9_width; *pic = gImage_b_9_2325; }
}
u8 a = 0, b = 0, c = 0, wei_shu = 0,a_old = 0, b_old = 0, c_old = 0;
u8 a_width, b_width, c_width;
extern uint8_t speed_value; // ✅ 引用外部定义的单位状态 (0:km/h, 1:mph)
extern uint8_t wuhuaqi_state; // ✅ 新增：引用外部定义的状态变量，解决编译报错
u8 speed,speed_old;
void LCD_picture(u16 num,u8 key_num) {
    
    const u8 *a_pic = NULL, *b_pic = NULL, *c_pic = NULL;  // 初始化为 NULL
	if(key_num == 0){
		// ✅ 精度修复：使用四舍五入，确保与APP端显示一致
		num = (u16)(3.4f * num + 0.5f);
		getDigitsAndLength(num, &a, &b, &c, &wei_shu);
		speed = 12;
	}
    else if(key_num == 1){
		// ✅ 修复：使用精确的转换系数 0.621371，确保 100 -> 211 mph
		num = (u16)(0.621371f * num * 3.4f + 0.5f);  // +0.5 用于四舍五入
		getDigitsAndLength(num, &a, &b, &c, &wei_shu);
		speed = 11;
	}
    else if(key_num == 2){ // ✅ 新增：绝对速度显示模式（来自蓝牙 SPEED:xxx 命令，传入的是 km/h 绝对值）
        if(speed_value == 1) {
            num = (u16)(num * 0.621371f + 0.5f); // 如果硬件设为 mph，则进行单位转换，+0.5 四舍五入
            speed = 11;
        } else {
            speed = 12;
        }
        getDigitsAndLength(num, &a, &b, &c, &wei_shu);
    }
	if(speed != speed_old)
	{
		LCD_ShowPicture(fengshubiao_x, fengshubiao_y, fengshubiao_width, fengshubiao_high, gImage_fengshubiao_202_43);
		speed_old = speed;
		// ✅ 修复：单位变化时强制重置旧值，确保数字会被重绘
		a_old = 255; b_old = 255; c_old = 255;  // 设为不可能的值，强制触发重绘
	}
	
	// ✅ 核心修复：只有数字变化时才执行清除和绘制，避免高频重复绘制导致显示混乱
	if(a != a_old || b != b_old || c != c_old)
	{
        // 先清除数字区域，防止数字重叠
        // 填充区域限制到 x=155，避免覆盖单位显示区域（speed_kmh_x=158）
        LCD_Fill(15, y_qi, 155, y_qi+speed_num_high, BLACK);
        
        // 绘制新数字
        if(wei_shu == 3) {
            dui_yin_width(a, &a_width, &a_pic);
            dui_yin_width(b, &b_width, &b_pic);
            dui_yin_width(c, &c_width, &c_pic);
            LCD_ShowPicture(x_qi - a_width - b_width - c_width - jianju*3, y_qi, a_width, speed_num_high, a_pic);
            LCD_ShowPicture(x_qi - b_width - c_width - jianju*2, y_qi, b_width, speed_num_high, b_pic);
            LCD_ShowPicture(x_qi - c_width - jianju*1, y_qi, c_width, speed_num_high, c_pic);
        }
        else if(wei_shu == 2) {
            dui_yin_width(b, &b_width, &b_pic);
            dui_yin_width(c, &c_width, &c_pic);
            LCD_ShowPicture(x_qi - b_width - c_width - jianju*2, y_qi, b_width, speed_num_high, b_pic);
            LCD_ShowPicture(x_qi - c_width - jianju*1, y_qi, c_width, speed_num_high, c_pic);
        }
        else {
            dui_yin_width(c, &c_width, &c_pic);
            LCD_ShowPicture(x_qi - c_width - jianju*1, y_qi, c_width, speed_num_high, c_pic);
        }
        
        // 更新旧值记录
        a_old = a; b_old = b; c_old = c;
	}
}

/**
 * @brief  带跳跃动画的速度数字更新（油门模式专用）
 * @param  num: 速度值
 * @param  key_num: 单位模式 (0=km/h, 1=mph, 2=绝对速度)
 * @param  delta: 速度变化量（正=加速，负=减速）
 * @note   数字会先向上跳跃再回到原位，增加动感
 */
void LCD_picture_animated(u16 num, u8 key_num, int8_t delta) {
    // 计算跳跃偏移量：根据变化量大小决定跳跃幅度
    int8_t jump_offset = 0;
    if (delta > 0) {
        // 加速：向上跳跃
        jump_offset = -5;  // 向上偏移5像素
    } else if (delta < 0) {
        // 减速：向下跳跃
        jump_offset = 3;   // 向下偏移3像素
    }
    
    // 如果没有变化，直接调用普通版本
    if (jump_offset == 0) {
        LCD_picture(num, key_num);
        return;
    }
    
    const u8 *a_pic = NULL, *b_pic = NULL, *c_pic = NULL;
    u8 a_anim, b_anim, c_anim, wei_shu_anim;
    u8 a_width_anim, b_width_anim, c_width_anim;
    
    // 计算显示值
    u16 display_num = num;
    if(key_num == 0){
        display_num = (u16)(3.4f * num + 0.5f);
    } else if(key_num == 1){
        display_num = (u16)(0.621371f * num * 3.4f + 0.5f);
    } else if(key_num == 2){
        if(speed_value == 1) {
            display_num = (u16)(num * 0.621371f + 0.5f);
        }
    }
    
    getDigitsAndLength(display_num, &a_anim, &b_anim, &c_anim, &wei_shu_anim);
    
    // 第一帧：跳跃位置
    LCD_Fill(15, y_qi - 10, 155, y_qi + speed_num_high + 10, BLACK);
    
    if(wei_shu_anim == 3) {
        dui_yin_width(a_anim, &a_width_anim, &a_pic);
        dui_yin_width(b_anim, &b_width_anim, &b_pic);
        dui_yin_width(c_anim, &c_width_anim, &c_pic);
        LCD_ShowPicture(x_qi - a_width_anim - b_width_anim - c_width_anim - jianju*3, y_qi + jump_offset, a_width_anim, speed_num_high, a_pic);
        LCD_ShowPicture(x_qi - b_width_anim - c_width_anim - jianju*2, y_qi + jump_offset, b_width_anim, speed_num_high, b_pic);
        LCD_ShowPicture(x_qi - c_width_anim - jianju*1, y_qi + jump_offset, c_width_anim, speed_num_high, c_pic);
    } else if(wei_shu_anim == 2) {
        dui_yin_width(b_anim, &b_width_anim, &b_pic);
        dui_yin_width(c_anim, &c_width_anim, &c_pic);
        LCD_ShowPicture(x_qi - b_width_anim - c_width_anim - jianju*2, y_qi + jump_offset, b_width_anim, speed_num_high, b_pic);
        LCD_ShowPicture(x_qi - c_width_anim - jianju*1, y_qi + jump_offset, c_width_anim, speed_num_high, c_pic);
    } else {
        dui_yin_width(c_anim, &c_width_anim, &c_pic);
        LCD_ShowPicture(x_qi - c_width_anim - jianju*1, y_qi + jump_offset, c_width_anim, speed_num_high, c_pic);
    }
    
    // 短暂延迟（约30ms）
    HAL_Delay(30);
    
    // 第二帧：回到原位
    LCD_picture(num, key_num);
}

void LCD_ui0()
{
	LCD_ShowPicture(0, 0, LCD_WIDTH, LCD_HEIGHT, gImage_beijing_240_240);
	// 🆕 显示自定义Logo（如果有）或默认Logo
	Logo_ShowBoot();  // 居中显示 (43, 43)
}
void LCD_ui1()
{
	LCD_ShowPicture(0, 0, LCD_WIDTH, LCD_HEIGHT, gImage_beijing_240_240);
	LCD_ShowPicture(fengshubiao_x, fengshubiao_y, fengshubiao_width, fengshubiao_high, gImage_fengshubiao_202_43);
}
void LCD_ui2()
{
	LCD_ShowPicture(0, 0, LCD_WIDTH, LCD_HEIGHT, gImage_beijing_240_240);
	LCD_ShowPicture(color_x, color_y, color_width, color_high, gImage_color_183_57);  // 颜色预设界面用原来的大图 (183x57)
	LCD_ShowPicture(color_rize_x, color_rize_y, color_rize_width, color_rize_high, gImage_color_rize_69_28);
}
void LCD_ui3()
{
	LCD_ShowPicture(0, 0, LCD_WIDTH, LCD_HEIGHT, gImage_beijing_240_240);
	LCD_ShowPicture(RGB_b_r_x, RGB_b_r_y, RGB_b_r_width, RGB_b_r_high, gImage_RGB_b_r_4853);
	LCD_ShowPicture(RGB_b_g_x, RGB_b_g_y, RGB_b_g_width, RGB_b_g_high, gImage_RGB_b_g_4853);
	LCD_ShowPicture(RGB_b_b_x, RGB_b_b_y, RGB_b_b_width, RGB_b_b_high, gImage_RGB_b_b_4753);
	LCD_ShowPicture(RGB_left_x, RGB_left_y, RGB_middle_width, RGB_middle_high, gImage_RGB_middle_105_27);
}
void LCD_ui4()
{
	LCD_ShowPicture(0, 0, LCD_WIDTH, LCD_HEIGHT, gImage_beijing_240_240);
	LCD_ShowPicture(brt_x, brt_y, brt_width, brt_high, gImage_brt_6923);
}

// ═══════════════════════════════════════════════════════════════════
// 🌟 UI4 呼吸灯颜色条显示
// ═══════════════════════════════════════════════════════════════════
#define BREATH_BAR_X      30      // 颜色条X坐标
#define BREATH_BAR_Y      95      // 颜色条Y坐标（与数字显示位置接近）
#define BREATH_BAR_WIDTH  180     // 颜色条宽度
#define BREATH_BAR_HEIGHT 30      // 颜色条高度

/**
 * @brief  绘制呼吸灯颜色条
 * @param  brightness: 当前亮度 (0-100)
 * @param  r, g, b: 基础颜色
 */
void LCD_DrawBreathBar(u8 brightness, u8 r, u8 g, u8 b)
{
	// 根据亮度调整颜色
	u8 br = (u8)((u16)r * brightness / 100);
	u8 bg = (u8)((u16)g * brightness / 100);
	u8 bb = (u8)((u16)b * brightness / 100);
	
	// 绘制颜色条
	LCD_DrawSolidBar(BREATH_BAR_X, BREATH_BAR_Y, BREATH_BAR_WIDTH, BREATH_BAR_HEIGHT, br, bg, bb);
}

/**
 * @brief  绘制呼吸灯渐变颜色条（使用当前LED颜色）
 * @param  bright_factor: 当前亮度因子 (0-255)
 */
void LCD_DrawBreathBarGradient(u8 bright_factor)
{
	// 使用当前LED2和LED3的颜色（左右灯带）
	extern int16_t red2_zhong, green2_zhong, blue2_zhong;
	extern int16_t red3_zhong, green3_zhong, blue3_zhong;
	
	// 根据亮度因子调整颜色
	u8 r1 = (u8)((u16)red2_zhong * bright_factor / 255);
	u8 g1 = (u8)((u16)green2_zhong * bright_factor / 255);
	u8 b1 = (u8)((u16)blue2_zhong * bright_factor / 255);
	u8 r2 = (u8)((u16)red3_zhong * bright_factor / 255);
	u8 g2 = (u8)((u16)green3_zhong * bright_factor / 255);
	u8 b2 = (u8)((u16)blue3_zhong * bright_factor / 255);
	
	// 绘制渐变颜色条
	LCD_DrawGradientBar(BREATH_BAR_X, BREATH_BAR_Y, BREATH_BAR_WIDTH, BREATH_BAR_HEIGHT, r1, g1, b1, r2, g2, b2);
}

/**
 * @brief  清除呼吸灯颜色条区域
 */
void LCD_ClearBreathBar(void)
{
	LCD_Fill(BREATH_BAR_X, BREATH_BAR_Y, BREATH_BAR_X + BREATH_BAR_WIDTH, BREATH_BAR_Y + BREATH_BAR_HEIGHT, BLACK);
}

// ============ UI6 音量调节界面 ============
// Voice图标和文字尺寸定义（需要在使用前定义）
#define MENU_ICON_VOICE_WIDTH   73
#define MENU_ICON_VOICE_HEIGHT  58
#define MENU_TEXT_VOICE_WIDTH   90
#define MENU_TEXT_VOICE_HEIGHT  27

// Voice图标和文字数组声明（实际定义在取模数组文件中，后面会include）
extern const unsigned char gImage_voicetubiao[];
extern const unsigned char gImage_voice[];

// 音量界面参数定义
#define VOLUME_ICON_X       84      // Voice图标X坐标 (240-73)/2 = 83.5
#define VOLUME_ICON_Y       30      // Voice图标Y坐标
#define VOLUME_BAR_X        30      // 音量条X坐标
#define VOLUME_BAR_Y        130     // 音量条Y坐标
#define VOLUME_BAR_WIDTH    180     // 音量条总宽度
#define VOLUME_BAR_HEIGHT   20      // 音量条高度
#define VOLUME_NUM_X        140     // 音量数字X坐标（右对齐基准）
#define VOLUME_NUM_Y        160     // 音量数字Y坐标

// 音量界面数字缓存
static u8 ui6_last_volume = 0xFF;

// VOI文字位置（参考brt位置）
#define voi_x  				140
#define voi_y			  	150
#define voi_width  			73
#define voi_high  			20

// VOI图片数组声明（在取模数组文件夹中，通过 #include 引入）
// 注意：gImage_VOI 已通过 #include "../../取模数组/VOI.c" 引入

void LCD_ui6_reset_cache(void)
{
	ui6_last_volume = 0xFF;  // 重置缓存，强制下次刷新
}

/**
 * @brief  音量调节界面初始化 - 简洁大数字风格
 */
void LCD_ui6(void)
{
	// 纯黑背景
	LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK);
	
	// 显示VOI文字（右下角，参考brt位置）
	LCD_ShowPicture(voi_x, voi_y, voi_width, voi_high, gImage_VOI);
	
	// 重置数字缓存，确保首次显示
	LCD_ui6_reset_cache();
}

/**
 * @brief  更新音量条显示（已废弃，保留空函数兼容）
 * @param  volume: 音量值 (0-100)
 */
void LCD_ui6_volume_bar(u8 volume)
{
	// 不再使用音量条，改用大数字显示
	(void)volume;
}

/**
 * @brief  更新音量数字显示 - 大数字居中显示
 * @param  volume: 音量值 (0-100)
 */
void LCD_ui6_num_update(u8 volume)
{
	if(volume == ui6_last_volume) return;
	
	// 使用和亮度界面一样的大数字显示
	const u8 *a_pic = NULL, *b_pic = NULL, *c_pic = NULL;
	u8 a_w = 0, b_w = 0, c_w = 0;
	u8 digit_a, digit_b, digit_c, digit_count;
	
	// 分解数字
	if(volume >= 100) {
		digit_a = 1;
		digit_b = 0;
		digit_c = 0;
		digit_count = 3;
	} else if(volume >= 10) {
		digit_a = 0;
		digit_b = volume / 10;
		digit_c = volume % 10;
		digit_count = 2;
	} else {
		digit_a = 0;
		digit_b = 0;
		digit_c = volume;
		digit_count = 1;
	}
	
	// 清除数字区域
	LCD_Fill(20, ui4_Y_qi, ui4_x_qi + 5, ui4_Y_qi + speed_num_high, BLACK);
	
	// 显示大数字（复用亮度界面的数字图片）
	if(digit_count == 3) {
		dui_yin_width(digit_a, &a_w, &a_pic);
		dui_yin_width(digit_b, &b_w, &b_pic);
		dui_yin_width(digit_c, &c_w, &c_pic);
		LCD_ShowPicture(ui4_x_qi - a_w - b_w - c_w - ui4_jianju*3, ui4_Y_qi, a_w, speed_num_high, a_pic);
		LCD_ShowPicture(ui4_x_qi - b_w - c_w - ui4_jianju*2, ui4_Y_qi, b_w, speed_num_high, b_pic);
		LCD_ShowPicture(ui4_x_qi - c_w - ui4_jianju*1, ui4_Y_qi, c_w, speed_num_high, c_pic);
	} else if(digit_count == 2) {
		dui_yin_width(digit_b, &b_w, &b_pic);
		dui_yin_width(digit_c, &c_w, &c_pic);
		LCD_ShowPicture(ui4_x_qi - b_w - c_w - ui4_jianju*2, ui4_Y_qi, b_w, speed_num_high, b_pic);
		LCD_ShowPicture(ui4_x_qi - c_w - ui4_jianju*1, ui4_Y_qi, c_w, speed_num_high, c_pic);
	} else {
		dui_yin_width(digit_c, &c_w, &c_pic);
		LCD_ShowPicture(ui4_x_qi - c_w - ui4_jianju*1, ui4_Y_qi, c_w, speed_num_high, c_pic);
	}
	
	ui6_last_volume = volume;
}

/**
 * @brief  显示音量调节状态指示灯
 * @param  state: 0=红点(正常), 1=绿点(激活)
 * @note   参考lcd_ui4_deng实现，位置在VOI文字右侧
 */
void lcd_ui6_deng(u8 state)
{
	if(state == 0)
	{
		// 红点 - 正常状态
		LCD_ShowPicture(voi_x + voi_width, voi_y, h_deng_width, h_deng_high, gImage_h_deng_1221);
	}
	else
	{
		// 绿点 - 激活状态
		LCD_ShowPicture(voi_x + voi_width, voi_y, l_deng_width, l_deng_high, gImage_l_deng_1221);
	}
}

void lcd_wuhuaqi(u8 num,u8 key_num)//num=0为关，1为开，2为油门启动，3为慢慢减速，key_num=0为kmh,1为mph
{
	// ✅ 优化：只有状态变化时才执行LCD_Fill清除，避免高频刷新时的闪烁
	static u8 last_num = 0xFF;  // 初始化为无效值，确保首次会清除
	static u8 last_key_num = 0xFF;
	
	u8 need_clear = (num != last_num || key_num != last_key_num);
	
	if(num == 0 && key_num == 0)
	{
		if(need_clear) LCD_Fill(speed_kmh_x,speed_kmh_y,speed_kmh_x+speed_kmh_width+h_deng_width,speed_kmh_y+h_deng_high,BLACK);
		LCD_ShowPicture(speed_kmh_x,speed_kmh_y,speed_kmh_width,speed_kmh_high,gImage_speed_kmh_6225);
		LCD_ShowPicture(speed_kmh_x+speed_kmh_width,speed_kmh_y,h_deng_width,h_deng_high,gImage_h_deng_1221);
	}
	else if(num == 1 && key_num == 0)
	{
		if(need_clear) LCD_Fill(speed_kmh_x,speed_kmh_y,speed_kmh_x+speed_kmh_width+h_deng_width,speed_kmh_y+h_deng_high,BLACK);
		LCD_ShowPicture(speed_kmh_x,speed_kmh_y,speed_kmh_width,speed_kmh_high,gImage_speed_kmh_6225);
		LCD_ShowPicture(speed_kmh_x+speed_kmh_width,speed_kmh_y,l_deng_width,l_deng_high,gImage_l_deng_1221);
	}
	else if(num == 0 && key_num == 1)
	{
		if(need_clear) LCD_Fill(speed_kmh_x,speed_kmh_y,speed_kmh_x+speed_kmh_width+h_deng_width,speed_kmh_y+h_deng_high,BLACK);
		LCD_ShowPicture(speed_mph_x,speed_mph_y,speed_mph_width,speed_mph_high,gImage_speed_mph_5225);
		LCD_ShowPicture(speed_mph_x+speed_mph_width,speed_mph_y,h_deng_width,h_deng_high,gImage_h_deng_1221);
	}
	else if(num == 1 && key_num == 1)
	{
		if(need_clear) LCD_Fill(speed_kmh_x,speed_kmh_y,speed_kmh_x+speed_kmh_width+h_deng_width,speed_kmh_y+h_deng_high,BLACK);
		LCD_ShowPicture(speed_mph_x,speed_mph_y,speed_mph_width,speed_mph_high,gImage_speed_mph_5225);
		LCD_ShowPicture(speed_mph_x+speed_mph_width,speed_mph_y,l_deng_width,l_deng_high,gImage_l_deng_1221);
	}
	else if((num == 2) && key_num == 1)
	{
		if(need_clear) LCD_Fill(speed_kmh_x,speed_kmh_y,speed_kmh_x+speed_kmh_width+h_deng_width,speed_kmh_y+h_deng_high,BLACK);
		LCD_ShowPicture(speed_mph_x,speed_mph_y,speed_mph_width,speed_mph_high,gImage_speed_mph_5225);
		LCD_ShowPicture(speed_mph_x+speed_mph_width,speed_mph_y,c_deng_width,c_deng_high,gImage_c_deng_1221);
	}
	else if((num == 2) && key_num == 0)
	{
		if(need_clear) LCD_Fill(speed_kmh_x,speed_kmh_y,speed_kmh_x+speed_kmh_width+h_deng_width,speed_kmh_y+h_deng_high,BLACK);
		LCD_ShowPicture(speed_kmh_x,speed_kmh_y,speed_kmh_width,speed_kmh_high,gImage_speed_kmh_6225);
		LCD_ShowPicture(speed_kmh_x+speed_kmh_width,speed_kmh_y,c_deng_width,c_deng_high,gImage_c_deng_1221);
	}
	
	// ✅ 更新状态记录
	last_num = num;
	last_key_num = key_num;
}

// RGB888 转 RGB565 宏
#define RGB888_TO_RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

/**
 * @brief  绘制圆角胶囊形状的纯色颜色条（左上角坐标）
 *         使用逐行扫描线方式，与渐变条保持一致
 */
void LCD_DrawSolidBar(u16 x, u16 y, u16 width, u16 height, u8 r, u8 g, u8 b)
{
    if(width < 2 || height < 2) return;
    
    u16 color = RGB888_TO_RGB565(r, g, b);
    int16_t radius = height / 2;
    int16_t cy = y + radius;
    int16_t left_cx = x + radius;
    int16_t right_cx = x + width - 1 - radius;
    
    // 使用整数运算代替浮点，提升性能
    int32_t r_sq = (int32_t)radius * radius;
    
    // 预填充行缓冲区（纯色只需填充一次）
    static u8 solid_buf[256];
    static u16 last_solid_color = 0xFFFF;
    
    if(color != last_solid_color) {
        for(int i = 0; i < 128; i++) {
            solid_buf[i * 2] = color >> 8;
            solid_buf[i * 2 + 1] = color & 0xFF;
        }
        last_solid_color = color;
    }
    
    // 逐行绘制
    for(int16_t row = 0; row < height; row++)
    {
        int16_t py = y + row;
        int16_t dy = py - cy;
        int32_t dy_sq = (int32_t)dy * dy;
        
        if(dy_sq >= r_sq) continue;
        
        // 使用整数平方根近似（牛顿迭代法简化版）
        int32_t diff = r_sq - dy_sq;
        int16_t dx_max = 0;
        if(diff > 0) {
            // 快速整数平方根
            int32_t guess = radius;
            guess = (guess + diff / guess) >> 1;
            guess = (guess + diff / guess) >> 1;
            dx_max = (int16_t)guess;
        }
        
        int16_t row_x1 = left_cx - dx_max;
        if(row_x1 < x) row_x1 = x;
        
        int16_t row_x2 = right_cx + dx_max;
        if(row_x2 > x + width - 1) row_x2 = x + width - 1;
        
        if(row_x2 >= row_x1) {
            int16_t line_width = row_x2 - row_x1 + 1;
            LCD_Address_Set(row_x1, py, row_x2, py);
            LCD_Writ_Bus(solid_buf, line_width * 2);
        }
    }
}

/**
 * @brief  绘制圆角胶囊形状的渐变颜色条（优化版，使用整数运算）
 */
void LCD_DrawGradientBar(u16 x, u16 y, u16 width, u16 height, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2)
{
    if(width < 2 || height < 2) return;
    
    int16_t radius = height / 2;
    int16_t cy = y + radius;
    int16_t left_cx = x + radius;
    int16_t right_cx = x + width - 1 - radius;
    
    // 使用整数运算代替浮点
    int32_t r_sq = (int32_t)radius * radius;
    
    // 预计算颜色差值（使用定点数加速，乘以256）
    int32_t dr_256 = ((int32_t)r2 - (int32_t)r1) * 256 / width;
    int32_t dg_256 = ((int32_t)g2 - (int32_t)g1) * 256 / width;
    int32_t db_256 = ((int32_t)b2 - (int32_t)b1) * 256 / width;
    
    // 行缓冲区
    static u8 line_buf[256];
    
    // 逐行绘制
    for(int16_t row = 0; row < height; row++)
    {
        int16_t py = y + row;
        int16_t dy = py - cy;
        int32_t dy_sq = (int32_t)dy * dy;
        
        if(dy_sq >= r_sq) continue;
        
        // 快速整数平方根
        int32_t diff = r_sq - dy_sq;
        int16_t dx_max = 0;
        if(diff > 0) {
            int32_t guess = radius;
            guess = (guess + diff / guess) >> 1;
            guess = (guess + diff / guess) >> 1;
            dx_max = (int16_t)guess;
        }
        
        int16_t row_x1 = left_cx - dx_max;
        if(row_x1 < x) row_x1 = x;
        
        int16_t row_x2 = right_cx + dx_max;
        if(row_x2 > x + width - 1) row_x2 = x + width - 1;
        
        if(row_x2 < row_x1) continue;
        
        int16_t line_width = row_x2 - row_x1 + 1;
        
        // 填充行缓冲区
        int32_t col_start = row_x1 - x;
        for(int16_t i = 0; i < line_width; i++)
        {
            int32_t col = col_start + i;
            u8 cr = r1 + (dr_256 * col) / 256;
            u8 cg = g1 + (dg_256 * col) / 256;
            u8 cb = b1 + (db_256 * col) / 256;
            u16 color = RGB888_TO_RGB565(cr, cg, cb);
            line_buf[i * 2] = color >> 8;
            line_buf[i * 2 + 1] = color & 0xFF;
        }
        
        // 批量写入这一行
        LCD_Address_Set(row_x1, py, row_x2, py);
        LCD_Writ_Bus(line_buf, line_width * 2);
    }
}

/**
 * @brief  绘制实时颜色条（用于流水灯模式）
 * @param  r1,g1,b1: 左侧颜色
 * @param  r2,g2,b2: 右侧颜色
 * @note   直接覆盖绘制，不清屏，避免闪烁
 */
void lcd_pei_se_realtime(u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2)
{
	#define BAR_WIDTH  115
	#define BAR_HEIGHT 14
	#define BAR_X_OFFSET 0
	
	// 直接绘制渐变颜色条，不清屏（覆盖式刷新，无闪烁）
	LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, r1, g1, b1, r2, g2, b2);
}

void lcd_pei_se(u8 num,u8 state)
{
	// 颜色条尺寸：宽115，高14（不遮挡调色盘图标，color_rize_x=150, pei_se_x=25）
	#define BAR_WIDTH  115
	#define BAR_HEIGHT 14
	#define BAR_X_OFFSET 0
	
	// 防重复刷新：记录上次绘制的预设号和状态
	static u8 last_num = 0xFF;
	static u8 last_state = 0xFF;
	
	// 如果是强制刷新标记（0xFE），清除缓存
	if(num == 0xFE) {
		last_num = 0xFF;
		last_state = 0xFF;
		return;
	}
	
	// 如果预设号和状态都没变，跳过绘制
	if(num == last_num && state == last_state) {
		return;
	}
	
	// ✅ 红点现在由 lcd_pei_se_dot() 单独控制，这里不再显示
	// 只负责绘制颜色条
	
	// 切换预设时先清除旧颜色条区域
	if(state != 0)
	{
		LCD_Fill(pei_se_x, pei_se_y, pei_se_x + BAR_WIDTH, pei_se_y + BAR_HEIGHT + 2, BLACK);
	}
	
	// 预设 1-12 使用代码绘制圆角胶囊
	if(num == 1 && state != 0)
	{
		// 赛博霓虹 - 蓝紫→春绿 渐变
		LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 138, 43, 226, 0, 255, 128);
	}
	else if(num == 2 && state != 0)
	{
		// 冰晶青 - 青色纯色
		LCD_DrawSolidBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 0, 234, 255);
	}
	else if(num == 3 && state != 0)
	{
		// 日落熔岩 - 橙红→天青 渐变
		LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 255, 100, 0, 0, 200, 255);
	}
	else if(num == 4 && state != 0)
	{
		// 竞速黄 - 黄色纯色
		LCD_DrawSolidBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 255, 210, 0);
	}
	else if(num == 5 && state != 0)
	{
		// 烈焰红 - 红色纯色
		LCD_DrawSolidBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 255, 0, 0);
	}
	else if(num == 6 && state != 0)
	{
		// 警灯双闪 - 红→警蓝 渐变
		LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 255, 0, 0, 0, 80, 255);
	}
	else if(num == 7 && state != 0)
	{
		// 樱花绯红 - 热粉→玫红 渐变
		LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 255, 105, 180, 255, 0, 80);
	}
	else if(num == 8 && state != 0)
	{
		// 极光幻紫 - 紫→青绿 渐变
		LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 180, 0, 255, 0, 255, 200);
	}
	// 预设 9-12
	else if(num == 9 && state != 0)
	{
		// 暗夜紫晶 - 紫色纯色
		LCD_DrawSolidBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 148, 0, 211);
	}
	else if(num == 10 && state != 0)
	{
		// 薄荷清风 - 薄荷→天蓝 渐变
		LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 0, 255, 180, 100, 200, 255);
	}
	else if(num == 11 && state != 0)
	{
		// 丛林猛兽 - 荧光绿纯色
		LCD_DrawSolidBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 0, 255, 65);
	}
	else if(num == 12 && state != 0)
	{
		// 纯净白 - 暖白纯色
		LCD_DrawSolidBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 225, 225, 225);
	}
	else if(num == 13 && state != 0)
	{
		// 烈焰橙 - 深橙→浅橙
		LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 255, 80, 0, 255, 200, 50);
	}
	else if(num == 14 && state != 0)
	{
		// 霓虹派对 - 青→品红
		LCD_DrawGradientBar(pei_se_x + BAR_X_OFFSET, pei_se_y + 1, BAR_WIDTH, BAR_HEIGHT, 0, 255, 255, 255, 0, 255);
	}
	
	// 更新记录
	last_num = num;
	last_state = state;
}

/**
 * @brief  强制刷新颜色条（清除缓存，确保刷新）
 * @param  num: 预设号 1-12
 * @param  state: 状态
 */
void lcd_pei_se_force(u8 num, u8 state)
{
	// 先清除缓存
	lcd_pei_se(0xFE, 0);  // 0xFE 是清除缓存的特殊标记
	// 再正常绘制
	lcd_pei_se(num, state);
}

/**
 * @brief  刷新流水灯状态指示点
 * @param  flow_state: 流水灯状态 (0=关闭, 1=开启)
 * @note   经测试：gImage_h_deng_1221 是红点，gImage_l_deng_1221 是绿点
 *         流水灯开启时显示绿点，关闭时显示红点
 */
void lcd_pei_se_dot(u8 flow_state)
{
	if(flow_state != 0)
	{
		// 流水灯开启 - 显示绿点
		LCD_ShowPicture(color_rize_x+color_rize_width, color_rize_y+5, l_deng_width, l_deng_high, gImage_l_deng_1221);
	}
	else
	{
		// 流水灯关闭 - 显示红点
		LCD_ShowPicture(color_rize_x+color_rize_width, color_rize_y+5, h_deng_width, h_deng_high, gImage_h_deng_1221);
	}
}

//rgb = led_num;选择名字还是三个rgb
void lcd_rgb_num(u8 rgb,u8 num)
{
	const u8 *a_pic = NULL, *b_pic = NULL, *c_pic = NULL;  // 初始化为 NULL
	getDigitsAndLength(num, &a, &b, &c, &wei_shu);//获取位数
	if(rgb == 4)
	{
		LCD_Fill(RGB_b_r_x-10,RGB_b_r_y,RGB_b_g_x-1,RGB_b_r_y+RGB_b_r_high,BLACK);
		if(wei_shu == 3) {
			R_width(a, &a_width, &a_pic);
			R_width(b, &b_width, &b_pic);
			R_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_r_x-b_width/2-a_width, num_r_y, a_width, rgb_high, a_pic);
			LCD_ShowPicture(num_r_x-b_width/2, num_r_y, b_width, rgb_high, b_pic);
			LCD_ShowPicture(num_r_x+b_width/2, num_r_y, c_width, rgb_high, c_pic);
		}
		else if(wei_shu == 2) {
			R_width(b, &b_width, &b_pic);
			R_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_r_x-b_width, num_r_y, b_width, rgb_high, b_pic);
			LCD_ShowPicture(num_r_x, num_r_y, c_width, rgb_high, c_pic);
		}
		else {
			R_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_r_x-c_width/2, num_r_y, c_width, rgb_high, c_pic);
		}
	}
	else if(rgb == 5)
	{
		LCD_Fill(RGB_b_g_x-10,RGB_b_g_y,RGB_b_b_x-1,RGB_b_g_y+RGB_b_g_high,BLACK);
		if(wei_shu == 3) {
			G_width(a, &a_width, &a_pic);
			G_width(b, &b_width, &b_pic);
			G_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_g_x-b_width/2-a_width, num_g_y, a_width, rgb_high, a_pic);
			LCD_ShowPicture(num_g_x-b_width/2, num_g_y, b_width, rgb_high, b_pic);
			LCD_ShowPicture(num_g_x+b_width/2, num_g_y, c_width, rgb_high, c_pic);
		}
		else if(wei_shu == 2) {
			G_width(b, &b_width, &b_pic);
			G_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_g_x-b_width, num_g_y, b_width, rgb_high, b_pic);
			LCD_ShowPicture(num_g_x, num_g_y, c_width, rgb_high, c_pic);
		}
		else {
			G_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_g_x-c_width/2, num_g_y, c_width, rgb_high, c_pic);
		}
	}
	else if(rgb == 6)
	{
		LCD_Fill(RGB_b_b_x-10,RGB_b_b_y,RGB_b_b_x+RGB_b_b_width+10,RGB_b_b_y+RGB_b_b_high,BLACK);
		if(wei_shu == 3) {
			B_width(a, &a_width, &a_pic);
			B_width(b, &b_width, &b_pic);
			B_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_b_x-b_width/2-a_width, num_b_y, a_width, rgb_high, a_pic);
			LCD_ShowPicture(num_b_x-b_width/2, num_b_y, b_width, rgb_high, b_pic);
			LCD_ShowPicture(num_b_x+b_width/2, num_b_y, c_width, rgb_high, c_pic);
		}
		else if(wei_shu == 2) {
			B_width(b, &b_width, &b_pic);
			B_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_b_x-b_width, num_b_y, b_width, rgb_high, b_pic);
			LCD_ShowPicture(num_b_x, num_b_y, c_width, rgb_high, c_pic);
		}
		else {
			B_width(c, &c_width, &c_pic);
			LCD_ShowPicture(num_b_x-c_width/2, num_b_y, c_width, rgb_high, c_pic);
		}
	}
}
void lcd_rgb_update(u8 num)
{
	if(num == 4)
	{
		LCD_Fill(RGB_b_r_x-15,RGB_b_r_y,RGB_b_g_x-1,RGB_b_r_y+RGB_b_r_high,BLACK);
		LCD_ShowPicture(RGB_b_r_x, RGB_b_r_y, RGB_b_r_width, RGB_b_r_high, gImage_RGB_b_r_4853);
	}
	else if(num == 5)
	{
		LCD_Fill(RGB_b_r_x+RGB_b_r_width,RGB_b_g_y,RGB_b_b_x-1,RGB_b_g_y+RGB_b_g_high,BLACK);
		LCD_ShowPicture(RGB_b_g_x, RGB_b_g_y, RGB_b_g_width, RGB_b_g_high, gImage_RGB_b_g_4853);
	}
	else if(num == 6)
	{
		LCD_Fill(RGB_b_g_x+RGB_b_g_width,RGB_b_b_y,RGB_b_b_x+RGB_b_b_width+10,RGB_b_b_y+RGB_b_b_high,BLACK);
		LCD_ShowPicture(RGB_b_b_x, RGB_b_b_y, RGB_b_b_width, RGB_b_b_high, gImage_RGB_b_b_4753);
	}
}
void lcd_rgb_cai_update(u8 num)
{
	if(num == 4)
	{
		LCD_Fill(RGB_b_r_x-15,RGB_b_r_y,RGB_b_g_x-1,RGB_b_r_y+RGB_b_r_high,BLACK);
		LCD_ShowPicture(RGB_b_r_x, RGB_b_r_y, RGB_b_r_width, RGB_b_r_high, gImage_RGB_h_r_4853);
	}
	else if(num == 5)
	{
		LCD_Fill(RGB_b_r_x+RGB_b_r_width,RGB_b_g_y,RGB_b_b_x-1,RGB_b_g_y+RGB_b_g_high,BLACK);
		LCD_ShowPicture(RGB_b_g_x, RGB_b_g_y, RGB_b_g_width, RGB_b_g_high, gImage_RGB_l_g_4853);
	}
	else if(num == 6)
	{
		LCD_Fill(RGB_b_g_x+RGB_b_g_width,RGB_b_b_y,RGB_b_b_x+RGB_lan_b_width+10,RGB_b_b_y+RGB_b_b_high,BLACK);
		LCD_ShowPicture(RGB_b_b_x, RGB_b_b_y, RGB_lan_b_width, RGB_lan_b_high, gImage_RGB_lan_b_4653);
	}
}
void lcd_name_update(u8 num)
{
	if(num == 0)
	{
		LCD_Fill(RGB_left_x,RGB_left_y,RGB_left_x+RGB_middle_width+h_deng_width,RGB_left_y+RGB_right_high,BLACK);
		LCD_ShowPicture(RGB_left_x, RGB_left_y, RGB_middle_width, RGB_middle_high, gImage_RGB_middle_105_27);
	}
	else if(num == 1)
	{
		LCD_Fill(RGB_left_x,RGB_left_y,RGB_left_x+RGB_middle_width+h_deng_width,RGB_left_y+RGB_right_high,BLACK);
		LCD_ShowPicture(RGB_left_x, RGB_left_y, RGB_left_width, RGB_left_high, gImage_RGB_left_5527);
	}
	else if(num == 2)
	{
		LCD_Fill(RGB_left_x,RGB_left_y,RGB_left_x+RGB_middle_width+h_deng_width,RGB_left_y+RGB_right_high,BLACK);
		LCD_ShowPicture(RGB_left_x, RGB_left_y, RGB_right_width, RGB_right_high, gImage_RGB_right_8033);
	}
	else if(num == 3)
	{
		LCD_Fill(RGB_left_x,RGB_left_y,RGB_left_x+RGB_middle_width+h_deng_width,RGB_left_y+RGB_right_high,BLACK);
		LCD_ShowPicture(RGB_left_x, RGB_left_y, RGB_back_width, RGB_back_high, gImage_RGB_back_7727);
	}
}
void lcd_deng(u8 key_num,u8 name)
{
	if(key_num == 0)
	{
		if(name == 0)
		{
			LCD_ShowPicture(RGB_left_x+RGB_middle_width,RGB_left_y+5,l_deng_width,l_deng_high,gImage_l_deng_1221);
		}
		else if(name == 1)
		{
			LCD_ShowPicture(RGB_left_x+RGB_left_width,RGB_left_y+5,l_deng_width,l_deng_high,gImage_l_deng_1221);
		}
		else if(name == 2)
		{
			LCD_ShowPicture(RGB_left_x+RGB_right_width,RGB_left_y+5,l_deng_width,l_deng_high,gImage_l_deng_1221);
		}
		else if(name == 3)
		{
			LCD_ShowPicture(RGB_left_x+RGB_back_width,RGB_left_y+5,l_deng_width,l_deng_high,gImage_l_deng_1221);
		}
	}
	else if(key_num != 0)
	{
		if(name == 0)
		{
			LCD_ShowPicture(RGB_left_x+RGB_middle_width,RGB_left_y+5,h_deng_width,h_deng_high,gImage_h_deng_1221);
		}
		else if(name == 1)
		{
			LCD_ShowPicture(RGB_left_x+RGB_left_width,RGB_left_y+5,h_deng_width,h_deng_high,gImage_h_deng_1221);
		}
		else if(name == 2)
		{
			LCD_ShowPicture(RGB_left_x+RGB_right_width,RGB_left_y+5,h_deng_width,h_deng_high,gImage_h_deng_1221);
		}
		else if(name == 3)
		{
			LCD_ShowPicture(RGB_left_x+RGB_back_width,RGB_left_y+5,h_deng_width,h_deng_high,gImage_h_deng_1221);
		}
	}
}
void LCD_ui4_num_update(u16 num)
{
	const u8 *a_pic = NULL, *b_pic = NULL, *c_pic = NULL;  // 初始化为 NULL
	getDigitsAndLength(num, &a, &b, &c, &wei_shu);
	if(a != a_old || b != b_old || c != c_old)
	{
		LCD_Fill(20,ui4_Y_qi,ui4_x_qi+5,ui4_Y_qi+speed_num_high,BLACK);
	}
	if(wei_shu == 3) {
        dui_yin_width(a, &a_width, &a_pic);
        dui_yin_width(b, &b_width, &b_pic);
        dui_yin_width(c, &c_width, &c_pic);
        LCD_ShowPicture(ui4_x_qi - a_width - b_width - c_width - ui4_jianju*3, ui4_Y_qi, a_width, speed_num_high, a_pic);
        LCD_ShowPicture(ui4_x_qi - b_width - c_width - ui4_jianju*2, ui4_Y_qi, b_width, speed_num_high, b_pic);
        LCD_ShowPicture(ui4_x_qi - c_width - jianju*1, ui4_Y_qi, c_width, speed_num_high, c_pic);
    }
    else if(wei_shu == 2) {
        dui_yin_width(b, &b_width, &b_pic);
        dui_yin_width(c, &c_width, &c_pic);
        LCD_ShowPicture(ui4_x_qi - b_width - c_width - ui4_jianju*2, ui4_Y_qi, b_width, speed_num_high, b_pic);
        LCD_ShowPicture(ui4_x_qi - c_width - ui4_jianju*1, ui4_Y_qi, c_width, speed_num_high, c_pic);
    }
    else {
        dui_yin_width(c, &c_width, &c_pic);
        LCD_ShowPicture(ui4_x_qi - c_width - ui4_jianju*1, ui4_Y_qi, c_width, speed_num_high, c_pic);
    }
	a_old = a;b_old = b;c_old = c;
}
void lcd_ui4_deng(u8 key_num)
{
	if(key_num == 0)
	{
		LCD_ShowPicture(brt_x+brt_width,brt_y,h_deng_width,h_deng_high,gImage_h_deng_1221);
	}
	else if(key_num != 0)
	{
		LCD_ShowPicture(brt_x+brt_width,brt_y,l_deng_width,l_deng_high,gImage_l_deng_1221);
	}
}

// ✅ UI5蓝牙调试界面已删除
// ✅ UI6电机滚轮界面已删除

// ════════════════════════════════════════════════════════════════════════════
// ✅ 菜单界面重构：新增菜单相关函数
// ════════════════════════════════════════════════════════════════════════════
// ✅ 菜单界面重构 V2：全屏滑动式菜单
// ════════════════════════════════════════════════════════════════════════════

// 菜单页面名称（英文）
static const char* menu_names[5] = {"Speed", "Color", "RGB", "Bright", "Logo"};

// 页面指示点参数
#define MENU_DOT_Y          205     // 指示点Y坐标
#define MENU_DOT_RADIUS     3       // 指示点半径
#define MENU_DOT_SPACING    15      // 指示点间距
#define MENU_DOT_ACTIVE     0xFFFF  // 当前页指示点颜色（白色）
#define MENU_DOT_INACTIVE   0x4208  // 非当前页指示点颜色（深灰色）

// 图标参数
#define MENU_ICON_CENTER_Y  90      // 图标中心Y坐标
#define MENU_TEXT_Y         155     // 文字Y坐标

// ============ 图片取模方式配置 ============
// 设置为 1 使用图片取模方式，设置为 0 使用代码绘制方式
#define USE_IMAGE_ICONS     1

// ============ Speed 菜单页图片尺寸 ============
// 风速图标尺寸
#define MENU_ICON_FENGSU_WIDTH   68
#define MENU_ICON_FENGSU_HEIGHT  58
// "Speed" 文字图片尺寸
#define MENU_TEXT_SPEED_WIDTH    99
#define MENU_TEXT_SPEED_HEIGHT   33

// ============ Color 菜单页图片尺寸 ============
// 调色盘图标尺寸
#define MENU_ICON_TIAOSEPAN_WIDTH   74
#define MENU_ICON_TIAOSEPAN_HEIGHT  74
// "Color" 文字图片尺寸 (88x31) - 修正拼写错误后的新图片
#define MENU_TEXT_COLOR_WIDTH    88
#define MENU_TEXT_COLOR_HEIGHT   31

// ============ RGB 菜单页图片尺寸 ============
// RGB图标尺寸
#define MENU_ICON_RGB_WIDTH   65
#define MENU_ICON_RGB_HEIGHT  68
// "RGB" 文字图片尺寸
#define MENU_TEXT_RGB_WIDTH    72
#define MENU_TEXT_RGB_HEIGHT   27

// ============ Bright 菜单页图片尺寸 ============
// 亮度图标尺寸
#define MENU_ICON_BRIGHT_WIDTH   72
#define MENU_ICON_BRIGHT_HEIGHT  72
// "Bright" 文字图片尺寸
#define MENU_TEXT_BRIGHT_WIDTH    103
#define MENU_TEXT_BRIGHT_HEIGHT   33

// ============ Logo 菜单页图片尺寸 ============
// Logo图标尺寸
#define MENU_ICON_LOGO_WIDTH   68
#define MENU_ICON_LOGO_HEIGHT  68
// "Logo" 文字图片尺寸
#define MENU_TEXT_LOGO_WIDTH    79
#define MENU_TEXT_LOGO_HEIGHT   33

// ============ 包含图片数组 ============
// 风速图标 (68x58 像素, RGB565)
#include "../../取模数组/fengsu_68_58.c"
// 调色盘图标 (74x74 像素, RGB565)
#include "../../取模数组/tiaosepan_74_74.c"
// Color 文字 (103x27 像素, RGB565) - 使用原来的文件
#include "../../取模数组/corlor_103_27.c"
// RGB图标 (65x68 像素, RGB565)
#include "../../取模数组/rgbtubiao.c"
// RGB 文字 (72x27 像素, RGB565)
#include "../../取模数组/rgb.c"
// 亮度图标 (72x72 像素, RGB565)
#include "../../取模数组/brighttubiao.c"
// Bright 文字 (103x33 像素, RGB565)
#include "../../取模数组/bright.c"
// Logo图标 (68x68 像素, RGB565)
#include "../../取模数组/logotubiao.c"
// Logo 文字 (79x33 像素, RGB565)
#include "../../取模数组/logo.c"
// Voice图标 (73x58 像素, RGB565)
#include "../../取模数组/voicetubiao.c"
// Voice 文字 (90x27 像素, RGB565)
#include "../../取模数组/voice.c"
// VOI 文字 (73x20 像素, RGB565) - 用于音量界面
#include "../../取模数组/VOI.c"
// Speed 文字数组已在 pic.h 中定义 (gImage_speed_99_33)

#if USE_IMAGE_ICONS
// 菜单图标尺寸（根据实际取模图片调整）
#define MENU_ICON_WIDTH     80
#define MENU_ICON_HEIGHT    80
// 指示点图片尺寸
#define MENU_DOT_IMG_SIZE   6
#endif

/**
 * @brief  菜单界面初始化（清屏）
 */
void LCD_Menu_Init(void)
{
    LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, MENU_BG_COLOR);
}

/**
 * @brief  绘制小圆点（用于指示点）
 * @param  x,y: 圆心坐标
 * @param  r: 半径
 * @param  color: 颜色
 */
static void Draw_Small_Dot(u16 x, u16 y, u8 r, u16 color)
{
#if USE_IMAGE_ICONS
    // 使用图片方式绘制指示点
    // 根据颜色选择对应图片
    // if(color == MENU_DOT_ACTIVE) {
    //     LCD_ShowPicture(x - MENU_DOT_IMG_SIZE/2, y - MENU_DOT_IMG_SIZE/2, 
    //                     MENU_DOT_IMG_SIZE, MENU_DOT_IMG_SIZE, gImage_dot_active_6x6);
    // } else {
    //     LCD_ShowPicture(x - MENU_DOT_IMG_SIZE/2, y - MENU_DOT_IMG_SIZE/2,
    //                     MENU_DOT_IMG_SIZE, MENU_DOT_IMG_SIZE, gImage_dot_inactive_6x6);
    // }
    // 临时：图片未准备好时使用填充圆
    Fill_Circle(x, y, r, color);
#else
    // 使用 Fill_Circle 绘制实心圆点（效果更好）
    Fill_Circle(x, y, r, color);
#endif
}

/**
 * @brief  绘制页面指示点
 * @param  current: 当前选中的页面 (1-4)
 * @param  total: 总页面数 (4)
 */
static void Draw_Menu_Dots(uint8_t current, uint8_t total)
{
    // 计算指示点起始X坐标（居中）
    int16_t start_x = (LCD_WIDTH - (total - 1) * MENU_DOT_SPACING) / 2;
    
    for(uint8_t i = 0; i < total; i++) {
        int16_t dot_x = start_x + i * MENU_DOT_SPACING;
        uint16_t color = (i == current - 1) ? MENU_DOT_ACTIVE : MENU_DOT_INACTIVE;
        Draw_Small_Dot(dot_x, MENU_DOT_Y, MENU_DOT_RADIUS, color);
    }
}

/**
 * @brief  绘制粗线条（通过绘制多条平行线实现）
 * @param  x1,y1: 起点坐标
 * @param  x2,y2: 终点坐标
 * @param  thickness: 线条粗细
 * @param  color: 颜色
 */
static void LCD_DrawThickLine(u16 x1, u16 y1, u16 x2, u16 y2, u8 thickness, u16 color)
{
    int8_t offset = thickness / 2;
    for(int8_t i = -offset; i <= offset; i++) {
        // 水平线：上下偏移
        if(abs(x2 - x1) >= abs(y2 - y1)) {
            LCD_DrawLine(x1, y1 + i, x2, y2 + i, color);
        }
        // 垂直线：左右偏移
        else {
            LCD_DrawLine(x1 + i, y1, x2 + i, y2, color);
        }
    }
}

/**
 * @brief  绘制圆弧（四分之一圆）
 * @param  cx,cy: 圆心坐标
 * @param  r: 半径
 * @param  quadrant: 象限 (1=右上, 2=左上, 3=左下, 4=右下)
 * @param  thickness: 线条粗细
 * @param  color: 颜色
 */
static void LCD_DrawQuarterArc(u16 cx, u16 cy, u8 r, u8 quadrant, u8 thickness, u16 color)
{
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    
    while(x <= y) {
        // 根据象限绘制点（带粗细）
        for(int8_t t = 0; t < thickness; t++) {
            int8_t rt = r - thickness/2 + t;  // 调整半径实现粗细
            int xt = x * rt / r;
            int yt = y * rt / r;
            
            switch(quadrant) {
                case 1:  // 右上象限
                    LCD_DrawPoint(cx + xt, cy - yt, color);
                    LCD_DrawPoint(cx + yt, cy - xt, color);
                    break;
                case 2:  // 左上象限
                    LCD_DrawPoint(cx - xt, cy - yt, color);
                    LCD_DrawPoint(cx - yt, cy - xt, color);
                    break;
                case 3:  // 左下象限
                    LCD_DrawPoint(cx - xt, cy + yt, color);
                    LCD_DrawPoint(cx - yt, cy + xt, color);
                    break;
                case 4:  // 右下象限
                    LCD_DrawPoint(cx + xt, cy + yt, color);
                    LCD_DrawPoint(cx + yt, cy + xt, color);
                    break;
            }
        }
        
        if(d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

/**
 * @brief  绘制带圆弧弯钩的风线
 * @param  x_start: 水平线起点X
 * @param  y: 水平线Y坐标
 * @param  line_length: 水平线长度
 * @param  hook_radius: 弯钩半径
 * @param  hook_up: 1=向上弯, 0=向下弯
 * @param  thickness: 线条粗细
 * @param  color: 颜色
 */
static void Draw_Wind_Line(u16 x_start, u16 y, u16 line_length, u8 hook_radius, u8 hook_up, u8 thickness, u16 color)
{
    // 1. 绘制水平直线部分
    u16 x_end = x_start + line_length;
    for(int8_t i = -thickness/2; i <= thickness/2; i++) {
        LCD_DrawLine(x_start, y + i, x_end, y + i, color);
    }
    
    // 2. 绘制圆弧弯钩
    // 弯钩圆心在水平线末端
    u16 arc_cx = x_end;
    u16 arc_cy;
    
    if(hook_up) {
        // 向上弯：圆心在线的上方
        arc_cy = y - hook_radius;
        // 绘制右下象限的圆弧（从3点钟方向到6点钟方向）
        for(u8 t = 0; t < thickness; t++) {
            u8 rt = hook_radius - thickness/2 + t;
            int x = 0;
            int yy = rt;
            int d = 3 - 2 * rt;
            while(x <= yy) {
                // 右下象限
                LCD_DrawPoint(arc_cx + yy, arc_cy + x, color);
                LCD_DrawPoint(arc_cx + x, arc_cy + yy, color);
                if(d < 0) {
                    d = d + 4 * x + 6;
                } else {
                    d = d + 4 * (x - yy) + 10;
                    yy--;
                }
                x++;
            }
        }
    } else {
        // 向下弯：圆心在线的下方
        arc_cy = y + hook_radius;
        // 绘制右上象限的圆弧（从3点钟方向到12点钟方向）
        for(u8 t = 0; t < thickness; t++) {
            u8 rt = hook_radius - thickness/2 + t;
            int x = 0;
            int yy = rt;
            int d = 3 - 2 * rt;
            while(x <= yy) {
                // 右上象限
                LCD_DrawPoint(arc_cx + yy, arc_cy - x, color);
                LCD_DrawPoint(arc_cx + x, arc_cy - yy, color);
                if(d < 0) {
                    d = d + 4 * x + 6;
                } else {
                    d = d + 4 * (x - yy) + 10;
                    yy--;
                }
                x++;
            }
        }
    }
}

/**
 * @brief  绘制 Speed 图标（风/气流图标）
 * @note   三条带圆弧弯钩的风线，复刻设计图
 */
static void Draw_Icon_Speed(void)
{
    uint16_t cx = LCD_WIDTH / 2;  // 120
    uint16_t cy = MENU_ICON_CENTER_Y;  // 90
    uint16_t color = WHITE;
    uint8_t thickness = 4;  // 线条粗细
    uint8_t hook_r = 10;    // 弯钩半径（稍小一点）
    
    // 三条风线的参数（更紧凑的布局）
    // 第一条（最上面，最短，向上弯）
    u16 line1_x = cx - 15;
    u16 line1_y = cy - 22;
    u16 line1_len = 35;
    
    // 第二条（中间，中等长度，向上弯）
    u16 line2_x = cx - 30;
    u16 line2_y = cy;
    u16 line2_len = 55;
    
    // 第三条（最下面，最长，向下弯）
    u16 line3_x = cx - 25;
    u16 line3_y = cy + 22;
    u16 line3_len = 55;
    
    // 绘制三条风线
    Draw_Wind_Line(line1_x, line1_y, line1_len, hook_r, 1, thickness, color);  // 向上弯
    Draw_Wind_Line(line2_x, line2_y, line2_len, hook_r, 1, thickness, color);  // 向上弯
    Draw_Wind_Line(line3_x, line3_y, line3_len, hook_r, 0, thickness, color);  // 向下弯
}

/**
 * @brief  绘制 Color 图标（调色板图标）
 * @note   简化版：一个圆形调色板轮廓 + 几个小圆点
 */
static void Draw_Icon_Color(void)
{
    uint16_t cx = LCD_WIDTH / 2;
    uint16_t cy = MENU_ICON_CENTER_Y;
    uint16_t color = WHITE;
    
    // 绘制调色板外轮廓（大圆）
    Draw_Circle(cx, cy, 35, color);
    Draw_Circle(cx, cy, 34, color);
    
    // 绘制调色板上的颜色点（用实心小圆表示）
    Fill_Circle(cx - 15, cy - 15, 6, MENU_BG_COLOR);  // 左上
    Fill_Circle(cx + 10, cy - 18, 5, MENU_BG_COLOR);  // 右上
    Fill_Circle(cx + 18, cy, 5, MENU_BG_COLOR);       // 右中
    Fill_Circle(cx - 5, cy + 15, 6, MENU_BG_COLOR);   // 下中
    
    // 红点装饰
    Fill_Circle(cx + 25, cy + 10, 8, 0xF800);  // 红色点
}

/**
 * @brief  绘制 RGB 图标（RGB滑块图标）
 * @note   简化版：一个矩形框 + 滑块 + 红点
 */
static void Draw_Icon_RGB(void)
{
    uint16_t cx = LCD_WIDTH / 2;
    uint16_t cy = MENU_ICON_CENTER_Y;
    uint16_t color = WHITE;
    
    // 绘制顶部矩形框（显示屏）
    LCD_DrawRectangle(cx - 30, cy - 35, cx + 30, cy - 15, color);
    LCD_Fill(cx - 28, cy - 33, cx + 28, cy - 17, color);
    
    // 绘制下方的滑块支架
    LCD_DrawLine(cx - 15, cy - 15, cx - 15, cy + 10, color);
    LCD_DrawLine(cx - 15, cy + 10, cx - 25, cy + 25, color);
    LCD_DrawLine(cx - 15, cy + 10, cx + 5, cy + 10, color);
    LCD_DrawLine(cx + 5, cy + 10, cx + 5, cy + 30, color);
    
    // 红点装饰
    Fill_Circle(cx + 15, cy - 5, 10, 0xF800);  // 红色点
}

/**
 * @brief  绘制 Bright 图标（太阳/亮度图标）
 * @note   简化版：中心圆 + 8条射线
 */
static void Draw_Icon_Bright(void)
{
    uint16_t cx = LCD_WIDTH / 2;
    uint16_t cy = MENU_ICON_CENTER_Y;
    uint16_t color = WHITE;
    
    // 绘制中心圆（太阳本体）
    Fill_Circle(cx, cy, 18, color);
    
    // 绘制8条射线
    int16_t ray_inner = 25;  // 射线起点距离中心
    int16_t ray_outer = 38;  // 射线终点距离中心
    
    // 上
    LCD_Fill(cx - 3, cy - ray_outer, cx + 3, cy - ray_inner, color);
    // 下
    LCD_Fill(cx - 3, cy + ray_inner, cx + 3, cy + ray_outer, color);
    // 左
    LCD_Fill(cx - ray_outer, cy - 3, cx - ray_inner, cy + 3, color);
    // 右
    LCD_Fill(cx + ray_inner, cy - 3, cx + ray_outer, cy + 3, color);
    
    // 斜向射线（简化为小矩形）
    // 左上
    LCD_Fill(cx - 28, cy - 28, cx - 20, cy - 20, color);
    // 右上
    LCD_Fill(cx + 20, cy - 28, cx + 28, cy - 20, color);
    // 左下
    LCD_Fill(cx - 28, cy + 20, cx - 20, cy + 28, color);
    // 右下
    LCD_Fill(cx + 20, cy + 20, cx + 28, cy + 28, color);
}

/**
 * @brief  绘制当前菜单页面
 * @param  page: 页面编号 (1-4)
 *         1=Speed, 2=Color, 3=RGB, 4=Bright
 */
void Draw_Menu_Page(uint8_t page)
{
    // 清屏
    LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, MENU_BG_COLOR);
    
#if USE_IMAGE_ICONS
    // ============ 图片取模方式 ============
    switch(page) {
        case 1:  // Speed 页面 - 使用图片
        {
            // 计算风速图标居中位置
            uint16_t icon_x = (LCD_WIDTH - MENU_ICON_FENGSU_WIDTH) / 2;
            uint16_t icon_y = MENU_ICON_CENTER_Y - MENU_ICON_FENGSU_HEIGHT / 2;
            // 显示风速图标
            LCD_ShowPicture(icon_x, icon_y, MENU_ICON_FENGSU_WIDTH, MENU_ICON_FENGSU_HEIGHT, gImage_fengsu_68_58);
            
            // 计算 "Speed" 文字居中位置
            uint16_t text_x = (LCD_WIDTH - MENU_TEXT_SPEED_WIDTH) / 2;
            uint16_t text_y = MENU_TEXT_Y;
            // 显示 "Speed" 文字图片
            LCD_ShowPicture(text_x, text_y, MENU_TEXT_SPEED_WIDTH, MENU_TEXT_SPEED_HEIGHT, gImage_speed_99_33);
        }
            break;
        case 2:  // Color 页面 - 使用图片
        {
            // 计算调色盘图标居中位置
            uint16_t icon_x = (LCD_WIDTH - MENU_ICON_TIAOSEPAN_WIDTH) / 2;
            uint16_t icon_y = MENU_ICON_CENTER_Y - MENU_ICON_TIAOSEPAN_HEIGHT / 2;
            // 显示调色盘图标
            LCD_ShowPicture(icon_x, icon_y, MENU_ICON_TIAOSEPAN_WIDTH, MENU_ICON_TIAOSEPAN_HEIGHT, gImage_tiaosepan_74_74);
            
            // 计算 "Color" 文字居中位置
            uint16_t text_x = (LCD_WIDTH - MENU_TEXT_COLOR_WIDTH) / 2;
            uint16_t text_y = MENU_TEXT_Y;
            // 显示 "Color" 文字图片（修正拼写：corlor -> color）
            LCD_ShowPicture(text_x, text_y, MENU_TEXT_COLOR_WIDTH, MENU_TEXT_COLOR_HEIGHT, gImage_new_color);
        }
            break;
        case 3:  // RGB 页面 - 使用图片
        {
            // 计算RGB图标居中位置
            uint16_t icon_x = (LCD_WIDTH - MENU_ICON_RGB_WIDTH) / 2;
            uint16_t icon_y = MENU_ICON_CENTER_Y - MENU_ICON_RGB_HEIGHT / 2;
            // 显示RGB图标
            LCD_ShowPicture(icon_x, icon_y, MENU_ICON_RGB_WIDTH, MENU_ICON_RGB_HEIGHT, gImage_rgbtubiao);
            
            // 计算 "RGB" 文字居中位置
            uint16_t text_x = (LCD_WIDTH - MENU_TEXT_RGB_WIDTH) / 2;
            uint16_t text_y = MENU_TEXT_Y;
            // 显示 "RGB" 文字图片
            LCD_ShowPicture(text_x, text_y, MENU_TEXT_RGB_WIDTH, MENU_TEXT_RGB_HEIGHT, gImage_rgb);
        }
            break;
        case 4:  // Bright 页面 - 使用图片
        {
            // 计算亮度图标居中位置
            uint16_t icon_x = (LCD_WIDTH - MENU_ICON_BRIGHT_WIDTH) / 2;
            uint16_t icon_y = MENU_ICON_CENTER_Y - MENU_ICON_BRIGHT_HEIGHT / 2;
            // 显示亮度图标
            LCD_ShowPicture(icon_x, icon_y, MENU_ICON_BRIGHT_WIDTH, MENU_ICON_BRIGHT_HEIGHT, gImage_brighttubiao);
            
            // 计算 "Bright" 文字居中位置
            uint16_t text_x = (LCD_WIDTH - MENU_TEXT_BRIGHT_WIDTH) / 2;
            uint16_t text_y = MENU_TEXT_Y;
            // 显示 "Bright" 文字图片
            LCD_ShowPicture(text_x, text_y, MENU_TEXT_BRIGHT_WIDTH, MENU_TEXT_BRIGHT_HEIGHT, gImage_bright);
        }
            break;
        case 5:  // Logo 页面 - 使用图片
        {
            // 计算Logo图标居中位置
            uint16_t icon_x = (LCD_WIDTH - MENU_ICON_LOGO_WIDTH) / 2;
            uint16_t icon_y = MENU_ICON_CENTER_Y - MENU_ICON_LOGO_HEIGHT / 2;
            // 显示Logo图标
            LCD_ShowPicture(icon_x, icon_y, MENU_ICON_LOGO_WIDTH, MENU_ICON_LOGO_HEIGHT, gImage_logotubiao);
            
            // 计算 "Logo" 文字居中位置
            uint16_t text_x = (LCD_WIDTH - MENU_TEXT_LOGO_WIDTH) / 2;
            uint16_t text_y = MENU_TEXT_Y;
            // 显示 "Logo" 文字图片
            LCD_ShowPicture(text_x, text_y, MENU_TEXT_LOGO_WIDTH, MENU_TEXT_LOGO_HEIGHT, gImage_logo);
        }
            break;
        case 6:  // Voice 页面 - 使用图片
        {
            // 计算Voice图标居中位置
            uint16_t icon_x = (LCD_WIDTH - MENU_ICON_VOICE_WIDTH) / 2;
            uint16_t icon_y = MENU_ICON_CENTER_Y - MENU_ICON_VOICE_HEIGHT / 2;
            // 显示Voice图标
            LCD_ShowPicture(icon_x, icon_y, MENU_ICON_VOICE_WIDTH, MENU_ICON_VOICE_HEIGHT, gImage_voicetubiao);
            
            // 计算 "Voice" 文字居中位置
            uint16_t text_x = (LCD_WIDTH - MENU_TEXT_VOICE_WIDTH) / 2;
            uint16_t text_y = MENU_TEXT_Y;
            // 显示 "Voice" 文字图片
            LCD_ShowPicture(text_x, text_y, MENU_TEXT_VOICE_WIDTH, MENU_TEXT_VOICE_HEIGHT, gImage_voice);
        }
            break;
        default:
            return;
    }
#else
    // ============ 代码绘制方式 ============
    // 绘制对应图标
    switch(page) {
        case 1:
            Draw_Icon_Speed();
            break;
        case 2:
            Draw_Icon_Color();
            break;
        case 3:
            Draw_Icon_RGB();
            break;
        case 4:
            Draw_Icon_Bright();
            break;
        case 5:
            // Logo 页面暂无代码绘制方式，使用文字代替
            LCD_ShowString(100, MENU_ICON_CENTER_Y - 16, (const u8*)"LOGO", WHITE, MENU_BG_COLOR, 32, 0);
            break;
        case 6:
            // Voice 页面暂无代码绘制方式，使用文字代替
            LCD_ShowString(96, MENU_ICON_CENTER_Y - 16, (const u8*)"VOICE", WHITE, MENU_BG_COLOR, 32, 0);
            break;
        default:
            return;
    }
    
    // 绘制页面名称（居中显示，使用32号字体）
    const char* name = menu_names[page - 1];
    uint8_t name_len = strlen(name);
    uint16_t text_x = (LCD_WIDTH - name_len * 16) / 2;  // 32号字体，每字符宽16像素
    LCD_ShowString(text_x, MENU_TEXT_Y, (const u8*)name, WHITE, MENU_BG_COLOR, 32, 0);
#endif
    
    // 绘制页面指示点
    Draw_Menu_Dots(page, 6);
}

/**
 * @brief  绘制菜单项（兼容旧接口，现在调用 Draw_Menu_Page）
 */
void Draw_Menu_Items(void)
{
    extern uint8_t menu_selected;
    Draw_Menu_Page(menu_selected);
}

/**
 * @brief  绘制/清除菜单项选中边框（兼容旧接口，新版本不需要边框）
 * @param  item: 菜单项编号 (1-4)
 * @param  show: 1=绘制边框, 0=清除边框
 * @note   新版本菜单是全屏显示，不需要边框，此函数保留但不执行操作
 */
void Draw_Menu_Border(uint8_t item, uint8_t show)
{
    // 新版本菜单不需要边框，切换页面时直接重绘整个页面
    (void)item;
    (void)show;
}

