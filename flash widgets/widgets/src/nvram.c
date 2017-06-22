/******************************************************************************
* @file     nvram.c
* @brief    ����ʧ�����ݴ洢����
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

#define SECTOR_BLEN           fw_port.sector_size()          /*������С ------*/
#define SECTOR_MASK           (SECTOR_BLEN - 1)              /*�������� ------*/
#define SECTOR_BASE(addr)     (addr & (~SECTOR_MASK))        /*�����Ļ���ַ --*/
#define SECTOR_OFFSET(addr)   (addr & SECTOR_MASK)           /*�����ڵ�ƫ�� --*/

#define ITEM_TAG           0xAA55                            /*Ŀ¼��ı�ʶ --*/

typedef struct
{
    unsigned short crc;
    unsigned short tag;                                                        
    unsigned short len;                                      /*�������     */    
    unsigned char  data[1];                                  /*������ʼ ----*/
}nvram_header_t;

/*******************************************************************************
 * @brief		ȡ�����λ1
 * @param[in]   x
 * @return 	    ���λ1��λ��
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
 * @brief		����������ӽ�2���ݵ�ֵ
 * @param[in]   x 
 * @return 	    ������չ������2���ݵ�ֵ
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
    total_sector = size / SECTOR_BLEN;             /*��ȡ�������� ------------*/    
    if (total_sector < 2)return 0;                 /*��֤������������ --------*/
    if (size < max_item_size * 2)return 0;   
    nvram->base = base;
    nvram->size = size;
    /*�������ݿ鳤�� ---------------------------------------------------------*/
    nvram->block_len = roundup_pow_of_two(max_item_size + offsetof(nvram_header_t, data));
    
    /*ɨ�����һ�ζϵ�ǰ�����ݵ�ַ -------------------------------------------*/
    swap_buf = (unsigned char *)fw_port.malloc(SECTOR_BLEN);
    if (swap_buf == NULL)return 0;    
    /*���������������ҳ������� -----------------------------------------------*/
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
    /*����ʧ���������Ч������ -----------------------------------------------*/    
    nvram->windex = 0;   
    fw_port.erase(nvram->base);             /*�����������ĵ�һ������ ---------*/
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
    unsigned int  last_item_base;                    /*��һ��������Ļ���ַ --*/
    nvram_header_t *hdr;       
    if (size > nvram->block_len - offsetof(nvram_header_t, data))
        return 0;    
    hdr = (nvram_header_t *)fw_port.malloc(nvram->block_len);
    if (hdr == NULL)
        return 0;
    memset(hdr, 0, nvram->block_len);
    last_item_base = nvram->base + nvram->windex;        /*��һ��Ŀ¼��Ļ���ַ*/    
    /*ƫ�Ƶ���һ��������� ---------------------------------------------------*/
    nvram->windex  = (nvram->windex + nvram->block_len) % nvram->size;  
    if (SECTOR_OFFSET(nvram->base + nvram->windex)  == 0)/*����������----------*/
    {
        FW_DEBUG("Erase sector=>0x%08x",nvram->base + nvram->windex);
        fw_port.erase(nvram->base + nvram->windex);  /*���������������� ----*/
    }        
    hdr->tag = ITEM_TAG;
    hdr->len = size;
    memcpy(hdr->data, item, hdr->len);
    hdr->crc = fw_port.checksum(item, hdr->len);       
    /*д���µ�Ŀ¼------------------------------------------------------------*/
    fw_port.write(nvram->base + nvram->windex, hdr, nvram->block_len);            
    /*����ԭ�ȵ�Ŀ¼ ---------------------------------------------------------*/
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
    if (size < hdr->len)                               /*item�洢�ռ����ʼ����һ��*/
        goto ERR; 
    if (hdr->tag != ITEM_TAG)
        goto ERR;  
    if (hdr->len + offsetof(nvram_header_t, data) > nvram->block_len) 
        goto ERR; 
    crc = fw_port.checksum(hdr->data, hdr->len);    
    if (crc != hdr->crc)                                /*У��ʧ�� -----------*/
        goto ERR;     
    ret = hdr->len;
    memcpy(item, hdr->data, hdr->len);   
ERR:
    fw_port.free(hdr);
    FW_DEBUG("NVRAM->base:%d,size:%d,blocklen:%d,windex:%d\r\n", 
             nvram->base,nvram->size, nvram->block_len, nvram->windex);    
    return ret;
}


