/******************************************************************************
* @file     nvram.c
* @brief    非易失性数据存储管理
* @version  1.0
* @date     2017-03-04
* @author   roger.luo
* 
* Copyright(C) 2017
* All rights reserved.
*
*******************************************************************************/
#include "fw_port.h"
#include "fw_debug.h"
#include "nvram.h"
#include <stddef.h>
#include <string.h>

/* Private macro -------------------------------------------------------------*/
#define min(a,b) ( (a) < (b) )? (a):(b)   

#define SECTOR_BLEN           fw_port.sector_size()          /*扇区大小 ------*/
#define SECTOR_MASK           (SECTOR_BLEN - 1)              /*扇区掩码 ------*/
#define SECTOR_BASE(addr)     (addr & (~SECTOR_MASK))        /*扇区的基地址 --*/
#define SECTOR_OFFSET(addr)   (addr & SECTOR_MASK)           /*扇区内的偏移 --*/

#define ITEM_TAG           0xAA55                            /*目录项的标识 --*/

typedef struct
{
    unsigned short crc;
    unsigned short tag;                                                        
    unsigned short len;                                      /*数据项长度     */    
    unsigned char  data[1];                                  /*数据域开始 ----*/
}nvram_header_t;

/*******************************************************************************
 * @brief		取出最高位1
 * @param[in]   x
 * @return 	    最高位1的位置
 *
 ******************************************************************************/
static int fls(int x)
{
	int position;
	int i;
	if(x)
	{
		for (i = (x >> 1), position = 0; i != 0; ++position)
		    i >>= 1;
	}
	else
	{
	    position = -1;
	} 
	return position + 1;
}

/*******************************************************************************
 * @brief		向上舍入最接近2的幂的值
 * @param[in]   x 
 * @return 	    向上扩展并满足2的幂的值
 ******************************************************************************/
static unsigned int roundup_pow_of_two(unsigned int x)
{
    return 1UL << fls(x - 1);
} 

/*******************************************************************************
 * @brief		
 * @param[in]  
 * @param[out]  
 * @return 	    
 ******************************************************************************/
int nvram_init(nvram_t *nvram, unsigned int base, unsigned int size, 
               unsigned short max_item_size)
{
    int total_sector,i,j;
    nvram_header_t *hdr;
    unsigned short crc;    
    unsigned char *swap_buf;
    if (nvram == NULL)return 0;
    if (SECTOR_OFFSET(base) != 0)return 0;
    if (size & SECTOR_BLEN)return 0;    
    total_sector = size / SECTOR_BLEN;             /*获取总扇区数 ------------*/    
    if (total_sector < 2)return 0;                 /*保证至少两个扇区 --------*/
    if (size < max_item_size * 2)return 0;   
    nvram->base = base;
    nvram->size = size;
    /*分配数据块长度 ---------------------------------------------------------*/
    nvram->block_len = roundup_pow_of_two(max_item_size + offsetof(nvram_header_t, data));
    
    /*扫描出上一次断电前的数据地址 -------------------------------------------*/
    swap_buf = (unsigned char *)fw_port.malloc(SECTOR_BLEN);
    if (swap_buf == NULL)return 0;    
    /*遍历所有扇区查找出索引项 -----------------------------------------------*/
    for (i = 0; i < total_sector; i++)               
    {       
        fw_port.read(nvram->base + i * SECTOR_BLEN, swap_buf, SECTOR_BLEN);
        for (j = 0; j < SECTOR_BLEN; j += nvram->block_len)
        {
            hdr = (nvram_header_t*)&swap_buf[j];
            if (hdr->tag != ITEM_TAG)
                continue;            
            if (hdr->len + offsetof(nvram_header_t, data) > nvram->block_len) 
                continue;              
            crc = fw_port.checksum(hdr->data, hdr->len);
            if (hdr->crc == crc)
            {             
                nvram->windex = i * SECTOR_BLEN + j;
                fw_port.free(swap_buf);
                return 1;
            }
        }
    }
    fw_port.free(swap_buf);
    /*查找失败则清空无效数据项 -----------------------------------------------*/    
    nvram->windex = 0;   
    fw_port.erase(nvram->base);             /*擦除索引区的第一个扇区 ---------*/
    FW_DEBUG("NVRAM->base:%d,size:%d,blocklen:%d,windex:%d\r\n", 
             nvram->base,nvram->size, nvram->block_len, nvram->windex);    
    return 0;    
}

/*******************************************************************************
 * @brief		
 * @param[in]  
 * @param[out]  
 * @return 	    
 ******************************************************************************/
int nvram_write(nvram_t *nvram, void *item, int size)
{
    unsigned int  last_item_base;                    /*上一个数据项的基地址 --*/
    nvram_header_t *hdr;       
    if (size > nvram->block_len - offsetof(nvram_header_t, data))
        return 0;    
    hdr = (nvram_header_t *)fw_port.malloc(nvram->block_len);
    if (hdr == NULL)
        return 0;
    memset(hdr, 0, nvram->block_len);
    last_item_base = nvram->base + nvram->windex;        /*上一个目录项的基地址*/    
    /*偏移到下一块空闲区域 ---------------------------------------------------*/
    nvram->windex  = (nvram->windex + nvram->block_len) % nvram->size;  
    if (SECTOR_OFFSET(nvram->base + nvram->windex)  == 0)/*进入新扇区----------*/
    {
        FW_DEBUG("Erase sector=>0x%08x",nvram->base + nvram->windex);
        fw_port.erase(nvram->base + nvram->windex);  /*擦除新扇区的数据 ----*/
    }        
    hdr->tag = ITEM_TAG;
    hdr->len = size;
    memcpy(hdr->data, item, hdr->len);
    hdr->crc = fw_port.checksum(item, hdr->len);       
    /*写入新的目录------------------------------------------------------------*/
    fw_port.write(nvram->base + nvram->windex, hdr, nvram->block_len);            
    /*销毁原先的目录 ---------------------------------------------------------*/
    memset(hdr, 0, sizeof(nvram_header_t));
    fw_port.write(last_item_base, hdr, sizeof(nvram_header_t)); 
    FW_DEBUG("NVRAM->base:%d,size:%d,blocklen:%d,windex:%d\r\n", 
             nvram->base,nvram->size, nvram->block_len, nvram->windex);
    fw_port.free(hdr);
    return 1;
}

/*******************************************************************************
 * @brief		
 * @param[in]  
 * @param[out]  
 * @return 	    
 ******************************************************************************/ 
int nvram_read(nvram_t *nvram,  void *item, int size)
{
    nvram_header_t *hdr;
    unsigned short crc = 0;
    int ret = 0;
    if (size > nvram->block_len - offsetof(nvram_header_t, data))
        return 0;  
    hdr = (nvram_header_t *)fw_port.malloc(nvram->block_len);
    if (hdr == NULL)
        return 0;  
    fw_port.read(nvram->base + nvram->windex, hdr, nvram->block_len); 
    if (size < hdr->len)                               /*item存储空间与初始化不一致*/
        goto ERR; 
    if (hdr->tag != ITEM_TAG)
        goto ERR;  
    if (hdr->len + offsetof(nvram_header_t, data) > nvram->block_len) 
        goto ERR; 
    crc = fw_port.checksum(hdr->data, hdr->len);    
    if (crc != hdr->crc)                                /*校验失败 -----------*/
        goto ERR;     
    ret = hdr->len;
    memcpy(item, hdr->data, hdr->len);   
ERR:
    fw_port.free(hdr);
    FW_DEBUG("NVRAM->base:%d,size:%d,blocklen:%d,windex:%d\r\n", 
             nvram->base,nvram->size, nvram->block_len, nvram->windex);    
    return ret;
}


