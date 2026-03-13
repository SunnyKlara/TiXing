/* ---------------- MenuTree.h ---------------- */
#ifndef __MenuTree_H
#define __MenuTree_H

/* ---------------- 结构创建函数 ---------------- */
Link* Link_Init(uint8_t  ID,const uint8_t *img,const char *str,void (*action)(void));
BitAction Link_PushBack(Link *lk,uint8_t  id,const uint8_t *img,const char *str,void (*action)(void));
Queue* Queue_Init(const Link* lk,const u8 floor);//const
/* ---------------- 销毁函数 ---------------- */
void Queue_Destroy(Queue *queue);
#endif

