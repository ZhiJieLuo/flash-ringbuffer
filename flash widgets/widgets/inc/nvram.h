/******************************************************************************
* @file     nvram.h
* @brief    非易失性数据存储管理
* @version  1.0
* @date     2017-03-04
* @author   roger.luo
* 
* Copyright(C) 2017
* All rights reserved.
*
*******************************************************************************/
#ifndef _NVRAM_H_
#define _NVRAM_H_

/*非交接失性数据存储管理结构 -------------------------------------------------*/
typedef struct
{
    unsigned int base;	                  /*开始地址(扇区对齐)----------------*/
    unsigned int size;                    /*报告缓冲区大小(必须是扇区对齐 -)--*/
    unsigned short block_len;             /*数据项存储块长度 -----------------*/
    unsigned short windex;	              /*读写索引地址 ---------------------*/
}nvram_t;

int nvram_init(nvram_t *nvram, unsigned int base, unsigned int size, unsigned short max_item_size);
int nvram_write(nvram_t *nvram, void *item, int size);
int nvram_read(nvram_t *nvram,  void *item, int size);


#endif

