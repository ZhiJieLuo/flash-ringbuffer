/******************************************************************************
* @file     logbuf.c
* @brief    ���滺��������
* @version  1.0
* @date     2016-12-21
* @author   roger.luo
* 
* Copyright(C) 2016
* All rights reserved.
*
*******************************************************************************/
#include "logbuf.h"
#include "fw_port.h"
#include "fw_debug.h"
#include <string.h>
#include <stddef.h>

/* Private macro -------------------------------------------------------------*/
#define min(a,b) ( (a) < (b) )? (a):(b)   

#define SECTOR_BLEN           fw_port.sector_size()          /*������С ------*/
#define SECTOR_MASK           (SECTOR_BLEN - 1)              /*�������� ------*/
#define SECTOR_BASE(addr)     (addr & (~SECTOR_MASK))        /*�����Ļ���ַ --*/
#define SECTOR_OFFSET(addr)   (addr & SECTOR_MASK)           /*�����ڵ�ƫ�� --*/

#define ITEM_TAG           0xAA55                            /*Ŀ¼��ı�ʶ --*/

/* Private function prototypes -----------------------------------------------*/
static void debug_show_map(logbuf_t *log);

/* Private variables ---------------------------------------------------------*/ 
/*Ŀ¼������ -----------------------------------------------------------------*/
typedef struct
{
    unsigned short tag;                         /*��Ч�������ʶͷ------------*/    
    unsigned int head;                          /*��ǰ������ͷ���� -----------*/
    unsigned int tail;                          /*��ǰ������β���� -----------*/
    unsigned short total_items;                 /*�������ڵ��ܱ������� -------*/
    unsigned short crc;                         /*crc У���� -----------------*/
}dir_item_t;

/*Ŀ¼������Ϣ ---------------------------------------------------------------*/
typedef struct
{
    unsigned int base;                          /*Ŀ¼���Ļ���ַ -------------*/
    unsigned short size;                        /*Ŀ¼���������ܳ��� ---------*/
    unsigned short block_len;                   /*Ŀ¼��� -----------------*/
    unsigned short windex;                      /*��ǰĿ¼����Ŀ¼�������е�д����*/
}dir_info_t;
#if 0
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
#endif
//��ȡĿ¼��Ϣ
void get_dir_info(logbuf_t *log, dir_info_t *info)
{
    info->base = log->base + log->size - log->directory_size; /*Ŀ¼���Ļ���ַ*/
    info->size = log->directory_size;                         /*Ŀ¼������С -*/                
    /*Ŀ¼��Ŀ鳤�� ---------------------------------------------------------*/
    /*���϶���õ�����2���ݵĿ鳤�� ------------------------------------------*/
    //info->block_len = roundup_pow_of_two(sizeof(dir_item_t));
    info->block_len = 16;                                    
    info->windex = log->windex; 
}

/*******************************************************************************
 * @brief		��һ�������ڲ���Ŀ¼��
 * @param[in]   sector_buf - ��ǰ�������ݻ�����
 * @param[out]  offset     - �������������ڵ�ƫ��
 * @return 	    ����ҵ��򷵻�1
 ******************************************************************************/
static int find_dir_item(dir_info_t *info, unsigned char *sector_buf, dir_item_t *item, unsigned int *offset)
{
    int i;    
    unsigned short crc;
    dir_item_t *tmp;
    for (i = 0; i < SECTOR_BLEN; i += info->block_len)
    {
        tmp = (dir_item_t*)&sector_buf[i];
        crc = fw_port.checksum(tmp, offsetof(dir_item_t, crc));
        if (tmp->tag == ITEM_TAG && tmp->crc == crc)
        {             
            *offset = i;                                  /*�õ������ڵ�ƫ�� -*/
            memcpy(item, tmp, sizeof(dir_item_t));
            return 1;
        }
    }    
    return 0;
}

/*******************************************************************************
 * @brief		��ʼ�������Ŀ¼
 * @param[in]   report - ���������Ϣ
 * @return 	    ִ�гɹ����ط�0, ���򷵻�0
 ******************************************************************************/
static int dir_info_init(logbuf_t *log)
{
    int total_sector,i;
    unsigned char *swap_buf;
    unsigned int sector_offset;                    /*��ǰ�����ڵ�ƫ��---------*/
    dir_info_t info;
    dir_item_t item; 
    get_dir_info(rep, &info);   
    
    total_sector = info.size / SECTOR_BLEN;        /*��ȡ�������� ------------*/    
    /*ɨ�����һ�ζϵ�ǰ�����ݵ�ַ -------------------------------------------*/
    swap_buf = (unsigned char *)fw_port.malloc(SECTOR_BLEN);
    if (swap_buf == NULL)return 0;    
    /*���������������ҳ������� -----------------------------------------------*/
    for (i = 0; i < total_sector; i++)               
    {       
        fw_port.read(info.base + i * SECTOR_BLEN, swap_buf, SECTOR_BLEN);
        if (find_dir_item(&info, swap_buf,&item, &sector_offset))
        {
            info.windex = i * SECTOR_BLEN + sector_offset;
            fw_port.free(swap_buf);
            /*�ָ��ϵ�ǰ�Ĳ��� -----------------------------------------------*/
            log->windex = info.windex;
            log->total_items = item.total_items;
            log->head = item.head;
            log->tail = item.tail;
            return 1;
        }
    }
    fw_port.free(swap_buf);
    /*����ʧ���������Ч������ -----------------------------------------------*/    
    log->windex = 0;
    get_dir_info(rep,&info);
    fw_port.erase(info.base);             /*�����������ĵ�һ������ -------*/
    return 0;
	
    
}

/*******************************************************************************
 * @brief		����Ŀ¼��Ϣ
 * @param[in]   report - ���������Ϣ
 * @return 	    ִ�гɹ�����1, 
 ******************************************************************************/
static int save_diretory_info(logbuf_t *log)
{
    unsigned int  last_item_base;                    /*��һ��������Ļ���ַ --*/
    unsigned short crc;
    dir_info_t info;
    dir_item_t item;                                 
    if (log->begin_update)return 1;                  /*��ֹ����Ŀ¼ ----------*/
    get_dir_info(rep, &info);
    
    last_item_base = info.base + info.windex;        /*��һ��Ŀ¼��Ļ���ַ --*/    
    /*ƫ�Ƶ���һ��������� ---------------------------------------------------*/
    info.windex  = (info.windex + info.block_len) % info.size;
    log->windex  = info.windex;    
    if (SECTOR_OFFSET(info.base + info.windex)  == 0)/*����������-------------*/
    {
        FW_DEBUG("Erase sector=>0x%08x",info.base + info.windex);
        fw_port.erase(info.base + info.windex);  /*���������������� ------*/
    }
    /*����Ŀ¼�� -------------------------------------------------------------*/
    item.tag = ITEM_TAG;
    item.head = log->head;
    item.tail = log->tail;
    item.total_items = log->total_items;    
    crc = fw_port.checksum(&item, offsetof(dir_item_t, crc));
    item.crc = crc;
    
    /*д���µ�Ŀ¼------------------------------------------------------------*/
    fw_port.write(info.base + info.windex, &item, sizeof(dir_item_t));        
    /*����ԭ�ȵ�Ŀ¼ ---------------------------------------------------------*/
    memset(&item, 0, sizeof(dir_item_t));
    fw_port.write(last_item_base, &item.tag, sizeof(item.tag)); 
    
    debug_show_map(rep);
    return 1;   
}

/*******************************************************************************
 * @brief		�жϻ����ڵ������Ƿ�ȫΪ0xFF
 * @param[in]   buf  -  ���ݻ�����
 * @param[in]   size -  ���ݳ��� 
 * @return 	    ִ�гɹ�����1, 
 ******************************************************************************/
static int buf_is_valid(unsigned char *buf, unsigned int size)
{
    while (size--)
        if (*buf++ != 0xff)
            return 0;
    return 1;
}

/*******************************************************************************
 * @brief       ��ʼ�����滺������Ϣ
 * @param[in]   report              - ���滺������Ϣ
 * @return      0 - ��ʼ��ʧ�� ��0 - ��ʼ���ɹ�
 ******************************************************************************/
unsigned int logbuf_init(logbuf_t *log,unsigned int base, unsigned int size)
{    
    unsigned int buf_size;                       /*���滺������С ------------*/
    unsigned int windex,rindex;                  /*��д����ָ��ĵ�ַ --------*/    
	unsigned int  write_base,read_base;          /*��д�������������Ļ���ַ --*/
	unsigned short write_offset,read_offset;     /*��д�����ڵ�ǰ�����ڵ�ƫ��-*/	
    /*��ǰ�����¿������򳤶� -------------------------------------------------*/
    unsigned int space_left; 
    unsigned char *swap_buf;                     /*����������ָ�� ------------*/

    if (size < SECTOR_BLEN * 4)                  /*������Ҫ4������ -----------*/  
    {
        FW_DEBUG("The size of the buffer must be greater than  4 * <SECTOR_BLEN>.\r\n");
        return 0;
    }
    if (base % SECTOR_BLEN)                      /*��֤��ʼ��ַ�������� ------*/
    {
        FW_DEBUG("The Buffer base address must be sector aligned");
        return 0;
    }         
    
    log->size = size;    
    /*�������Ĵ�СС��6������ʱʹ��2��������ΪĿ¼,����ʹ��4 + ������/16������
      ��ΪĿ¼����ʹ��,�������Ϊ8������ -------------------------------------*/
    log->directory_size = (size < SECTOR_BLEN * 6) ? 2 * SECTOR_BLEN : 4 * SECTOR_BLEN;   
    if (size <= SECTOR_BLEN * 6)
    {
        log->directory_size = 2 * SECTOR_BLEN;
    }
    else 
    {
        if ((4 * SECTOR_BLEN + size / 16) >= (8 * SECTOR_BLEN))
            log->directory_size = 8 * SECTOR_BLEN;
        else 
            log->directory_size = 4 * SECTOR_BLEN + (size / (16 * SECTOR_BLEN)) * SECTOR_BLEN;
    }    
    FW_DEBUG("Directory size=>%d\r\n", log->directory_size);
    log->base = base;
    buf_size = log->size - log->directory_size;    /*��Ч�������ĳ��� ---------*/
    if (!dir_info_init(rep))                      /*��ʼ��������������Ϣ -----*/
    {
        /*�״λ�ȡĿ¼��Ϣ�϶���ʧ�� -----------------------------------------*/
        log->head = log->tail = 0;                 /*���ö�д���� -------------*/ 
        log->windex = 0;                           /*����Ŀ¼д���� -----------*/
        save_diretory_info(rep);                  /*����������Ϣ -------------*/
        FW_DEBUG("Invalid partition\r\n");
        return 1;   
    }
    windex = log->base + log->tail % buf_size;
    rindex = log->base + log->head % buf_size;   
    /*��ǰ��д�������������Ļ���ַ��ƫ���� -----------------------------------*/
	write_base = SECTOR_BASE(windex);      
	read_base  = SECTOR_BASE(rindex);
	write_offset = SECTOR_OFFSET(windex);  
	read_offset  = SECTOR_OFFSET(rindex);   	
    swap_buf = (unsigned char *)fw_port.malloc(SECTOR_BLEN);
    if (swap_buf == NULL)
    {
        FW_DEBUG("malloc fail! %d",__LINE__);        
        return 0;
    }
	
	fw_port.read(write_base, swap_buf, SECTOR_BLEN);/*��ȡ��ǰ���� -------*/
    /*�ж϶�д�����Ƿ���ͬһ���� ---------------------------------------------*/
	if (write_base == read_base && write_offset < read_offset)             
	{
        space_left  = read_offset - write_offset;    /*���пռ� --------------*/              
	}
	else                                      
	{
		space_left  = SECTOR_BLEN - write_offset;
	}
	/*ȷ����ǰ������д����ָ��Ŀ��пռ�ȫΪ0xff -----------------------------*/
    if (!buf_is_valid(swap_buf, space_left))
    {
        /*
         *������пռ䲻ȫΪ0xff�����ǰ����,ͬʱ����Ч����д�ص���ǰ������
         */      
        fw_port.erase(write_base);         
        memset(&swap_buf[write_offset], 0xFF, space_left);
        fw_port.write(write_base, swap_buf, SECTOR_BLEN);        
    }
    fw_port.free(swap_buf);
	return 1;
}


/*******************************************************************************
 * @brief       report�����Ч���ݳ���
 * @param[in]   report                - ���滺����������Ϣ
 * @return      ��Ч���ݳ���
 ******************************************************************************/
unsigned int logbuf_len(logbuf_t *log)
{
	return log->tail - log->head;                   
}

/*******************************************************************************
 * @brief       �������ڵ�ʣ��������ݳ���
 * @param[in]   report                - ���滺����������Ϣ
 * @return      ʣ����пռ䳤��
 ******************************************************************************/
unsigned int logbuf_space_left(logbuf_t *log)
{
	return (log->size - log->directory_size) - logbuf_len(rep); 
}

/*******************************************************************************
 * @brief       ����������ڵ���������
 * @param[in]   report                - ���滺����������Ϣ
 * @return      ʣ����пռ䳤��
 ******************************************************************************/
void logbuf_clear(logbuf_t *log)
{
    unsigned int buf_size;                           
    if (log->tail == log->head)return; 
    log->head = log->tail;
    log->total_items = 0;                               /*���������� ---------*/
    buf_size = log->size - log->directory_size;         /*��Ч�������ĳ��� ---*/    
    fw_port.erase(log->base + log->tail % buf_size);/*����д������������ -*/    
    save_diretory_info(rep);
}

/*******************************************************************************
 * @brief		��ȡ��������Ч����������
 * @param[in]   report                - ���滺����������Ϣ
 * @return 	    
 * @attention   �����������ǲο�ֵ, �����������׼ȷ��,
 *              ���������������������������ɿ�������֮���ֵ���ܲ�׼.����������
 *              ��Ϊ�պ�,��ֵ����0��ʼ���¼���,�ָ�����.
 ******************************************************************************/
unsigned int logbuf_count(logbuf_t  *log)
{
    if (logbuf_len(rep) == 0)
    {
        log->total_items = 0;
        return 0;
    }
    if (log->total_items == 0)return 1;                  /*�쳣���� ----------*/           
    
    return log->total_items;
}


/*******************************************************************************
 * @brief       ����д����
 * @param[in]   write_addr        - ������д����
 * @param[in]   read_addr         - ������������
 * @param[in]   buf               - ���ݻ�����
 * @param[in]   len               - ���ݳ��� 
 * @return      ��0 - д�ɹ�, 0 - дʧ��
 ******************************************************************************/
static int logbuf_write_process(unsigned int write_addr,unsigned int read_addr, unsigned char *buf, unsigned int len)
{
	unsigned int  write_base,read_base;          /*��д�������������Ļ���ַ --*/
	unsigned short write_offset,read_offset;     /*��д�����ڵ�ǰ�����ڵ�ƫ��-*/	
	unsigned short sector_remain;                /*����ʣ��ռ� --------------*/
    unsigned char *swap_buf;                     /*����������ָ�� ------------*/	
	while (len)
	{			
		write_base = SECTOR_BASE(write_addr);    /*�����ڵĻ���ַ ------------*/
		read_base  = SECTOR_BASE(read_addr);
		write_offset = SECTOR_OFFSET(write_addr);/*�����ڵ�ƫ�� --------------*/
		read_offset  = SECTOR_OFFSET(read_addr); 
        
		/*
         *��д����λ��ͬһ�����Ҷ�����λ��д�������ұ����������,ÿ��д��������
         *ǰ����Ҫ��������д-�������������.
         */
        if ((write_base == read_base && write_offset < read_offset))             
        {
            FW_DEBUG("wbase = rbase = %08x, woffset:%d,roffset:%d ",write_offset, read_offset);   
            /*ʣ����пռ� ---------------------------------------------------*/
            sector_remain = read_offset - write_offset;
            swap_buf = (unsigned char *)fw_port.malloc(SECTOR_BLEN);     
            if (swap_buf == NULL)
            {
                FW_DEBUG("malloc error line=>%d\r\n",__LINE__);
                return 0;
            } 
            /*д������ݳ��ȱ�ʣ��ռ仹����������������,�ϲ��Ѿ����� ------*/ 
            if (len > sector_remain)               
            {
                FW_DEBUG("Error :%d, len:%d, remain:%d\r\n",__LINE__, len, sector_remain);
                fw_port.free(swap_buf); 
                return 0;
            }
            fw_port.read(write_base, swap_buf, SECTOR_BLEN);
            /*�ж�д����֮��������Ƿ�ȫΪFF ---------------------------------*/
            if (!buf_is_valid(&swap_buf[write_offset], len))
            {                                    
                fw_port.erase(write_base);    /*������ǰ���� -------------*/   
                FW_DEBUG("Earse sector:%08X\r\n",write_base);
            }
            else 
            {
                FW_DEBUG("No need earse:%08X\r\n",write_base);
            }
            /*�޸�֮��д�ص�ǰ���� -------------------------------------------*/
            memset(&swap_buf[write_offset],0xFF,sector_remain);
            memcpy(&swap_buf[write_offset], buf, len);              
            fw_port.write(write_base, swap_buf, SECTOR_BLEN);	
            fw_port.free(swap_buf);             
            return 1;
            
        }
        else if (write_addr == write_base)       /*д�������뵽������ --------*/
        {
            FW_DEBUG("Enter new sector :%08x\r\n",write_addr); 
            sector_remain = SECTOR_BLEN;
            fw_port.erase(write_base);        /*������ǰ���� -------------*/    
        }
        else 
        {
			//��ǰ����ʣ��ռ�
			sector_remain = SECTOR_BLEN - write_offset;	      
        }
        FW_DEBUG("->sector remain:%d\r\n",sector_remain);    		
		//�ж�ʣ��Ŀռ��Ƿ��������������		
		if (len < sector_remain) sector_remain = len;
		//д��������
		fw_port.write(write_addr, buf, sector_remain);
		//дָ��ƫ��
		write_addr += sector_remain;
		buf += sector_remain;
		len -= sector_remain;
	}
	return 1;
}


/*******************************************************************************
 * @brief       д��һ��������
 * @param[in]   report               - ���滺����������Ϣ
 * @param[in]   buf               - ���ݻ�����
 * @param[in]   len               - ���������� 
 * @return      ��0 - д�ɹ�, 0 - дʧ��
 ******************************************************************************/
unsigned int logbuf_put(logbuf_t *log, void *buf, unsigned int len)
{ 
    unsigned char *ptr = (unsigned char *)buf;
    unsigned int left;                             
    unsigned int buf_size;                           /*�������ܴ�С ----------*/              
	if (logbuf_space_left(rep) < len)return 0;   /*�ռ䲻�� --------------*/
    buf_size = log->size - log->directory_size;
    
	len = min(len , buf_size + log->head - log->tail);/*�õ�ʵ��д������ݳ���*/
	/*д������������ĩβ�Ŀ��г��� -------------------------------------------*/
    left = min(len, buf_size - log->tail % buf_size);    
   	if (logbuf_write_process(log->base + log->tail % buf_size, 
   	                      log->base + log->head % buf_size, ptr, left) == 0)
   	{
   	    return 0;
   	}    
   	/*��ʣ�������д�뻺������ͷ ---------------------------------------------*/
	if (logbuf_write_process(log->base, log->base + log->head % buf_size,
	                      ptr + left, len - left) == 0)
	{
	    return 0;
	}
	log->tail += len;
    
	save_diretory_info(rep);                                /*���滺������Ϣ -*/    
	return len;   	
}

/*******************************************************************************
 * @brief       ��ȡָ�����ȵ�������,�����Ƴ�����
 * @param[in]   report              - ���滺����������Ϣ
 * @param[out]  buf              - ���ݻ�����
 * @param[in]   len              - ���������� 
 * @return      ʵ�ʶ�ȡ����
 ******************************************************************************/
unsigned int logbuf_peek(logbuf_t *log, void *buf, unsigned int len)
{
    unsigned char *ptr = (unsigned char *)buf;
	unsigned int left;
    unsigned int buf_size;   
    buf_size = log->size - log->directory_size;
	len = min(len , log->tail - log->head); 
	/*��ȡ��ָ����������β������Ч���� ---------------------------------------*/
	left = min(len, buf_size - log->head % buf_size);
	fw_port.read(log->base + log->head % buf_size,ptr, left);
	fw_port.read(log->base, ptr + left, len - left);
    return len;
}

/*******************************************************************************
 * @brief       �ӻ������ڶ�ȡָ�����ȵ�����,��ȡ�ɹ����Ƴ�������
 * @param[in]   report               - ���滺����������Ϣ
 * @param[out]  buf               - ���ݻ�����
 * @param[in]   len               - ����������
 * @return      ʵ�ʶ�ȡ����
 ******************************************************************************/
unsigned int logbuf_get(logbuf_t *log, void *buf, unsigned int len)
{
	len = logbuf_peek(rep, buf, len);	
	log->head += len; 
    if (len)
    {        
        save_diretory_info(rep);                          /*����Ŀ¼��Ϣ -----*/        
    }
	return len;
}

/*******************************************************************************
 * @brief       �Ƴ�ָ�����ȵĻ���������
 * @param[in]   report              - ���滺����������Ϣ
 * @param[in]   len              - Ԥ�Ƴ������ݳ���
 * @return      ʵ���Ƴ�����
 ******************************************************************************/
unsigned int logbuf_remove(logbuf_t *log, unsigned int len)
{
    len = min(len , log->tail - log->head); 
	log->head += len;    
	return len;
}

/*******************************************************************************
 * @brief       �ָ���ǰ���Ƴ�ָ�����ȵĻ���������
 * @param[in]   report              - ���滺����������Ϣ
 * @param[in]   len                 - Ԥ�ָ������ݳ���
 * @return      ʵ�ʻָ�����
 * @attention   ���뱣֤��logbuf_remove֮�����ʹ��,�һָ��ĳ��Ȳ�������ǰ��
 *              ���ĳ���.
 ******************************************************************************/
unsigned int logbuf_resume(logbuf_t *log, unsigned int len)
{
    len = min(logbuf_space_left(rep), len);
    log->head -= len;     
	return len;    
}

/*******************************************************************************
 * @brief      ��ʼ����/д�뱨��(ʵ���ǹ���Ŀ¼���²���,
 *             ����ڶ�ʱ������Ҫ���ִ��logbuf_put����,
 *             Ϊ�˱���Ƶ������Ŀ¼�����ϵͳЧ��,������
 *             ִ��report_begin_update����,������ִ��������
 *             ��logbuf_put����֮��,����report_end_update
 *             �����Իָ�Ŀ¼ʵʱ���²���)
 *
 * @param[in]   report              - ���滺����������Ϣ
 * @return      none
 ******************************************************************************/
void report_begin_update(logbuf_t  *log)
{
    log->begin_update++;
}

/*******************************************************************************
 * @brief      ��������/д��/����(��report_begin_update���ʹ��)
 *
 * @param[in]   report              - ���滺����������Ϣ
 * @return      none
 ******************************************************************************/  
void report_end_update(logbuf_t  *log)
{
    if (log->begin_update)
    {
        log->begin_update--;        
    }
    if (log->begin_update == 0)
    {
        save_diretory_info(rep);                          /*���滺������Ϣ ---*/
    }        
}


/*******************************************************************************
 * @brief       �������(��src�еı����ƶ���des��,��ɾ��srcͬ�ı���)
 * @param[in]   src -  Դ��������Ϣ
 * @param[in]   des -  Ŀ�껺������Ϣ
 * @return      ��0   ִ�гɹ�  0  ִ��ʧ��
  * @attention  ���뱣֤des���пռ䳤�ȴ���src��Ч���ݳ���,����ִ��ʧ��
 ******************************************************************************/
int logbuf_move(logbuf_t *des, logbuf_t *src, unsigned int len)
{
    unsigned short size;
    unsigned char swap[256];                              /*���������� -------*/
    len = min(logbuf_len(src), len);                  /*ʵ�ʰ��Ƴ��� -----*/
    if (len == 0)return 1;                                /*Դ������Ϊ�� -----*/
    
    /*�ж�Ŀ�껺�����Ŀ��пռ��Ƿ��ܹ�����Դ�������ڵ�ȫ������ ---------------*/
    if (logbuf_space_left(des) < len)return 0;    

    report_begin_update(src);                             /*����Ŀ¼���²��� -*/
    report_begin_update(des);  
   
    do{
        /*��ȡԴ������������ -------------------------------------------------*/
        size = logbuf_get(src, swap, len > sizeof(swap) ? sizeof(swap) : len);   
        len -= size;
    }while (size && logbuf_put(des, swap, size) && len );/*д�뵽Ŀ�껺������*/
    report_end_update(src);                                  /*�ָ�Ŀ¼���²��� -*/
    report_end_update(des);
    return 1;
}




/*
 * ��ʾreportӳ���(��Ч����ʹ��#��ʾ,���пռ�ʹ��-��ʾ)
 */
void debug_show_map(logbuf_t *log)
{
#ifdef  USE_FW_DEBUG    
    int i;
    unsigned int buf_size;
    int head, tail;
    buf_size = log->size - log->directory_size; 
    if (buf_size == 0)return;
    head = ((int)((log->head % buf_size * 50.0) / buf_size + 0.5)) % 50;
    tail = ((int)((log->tail % buf_size * 50.0) / buf_size + 0.5))% 50;
    FW_DEBUG("base:%X,size:%X,head:%X,tail:%X,windex:%X,bsize:%d,dsize:%d,items:%d\r\n",
                 log->base,log->size,log->head,log->tail,log->windex,buf_size,
                 log->directory_size,log->total_items);
    FW_DEBUG("\r\nSketch map =>[");
    if (head <= tail)
    {
        for (i = 0; i < head; i++)
        {
            FW_DEBUG("-");
        }
        for (;i < tail;i++)
        {
            FW_DEBUG("#");
        }
        for (;i < 50;i++)
        {
            FW_DEBUG("-");
        }
    }
    else
    {
        for (i = 0; i < tail; i++)
        {
            FW_DEBUG("#");
        }
        for (;i < head;i++)
        {
            FW_DEBUG("-");
        }
        for (;i < 50;i++)
        {
            FW_DEBUG("#");
        }        
    }
    
    FW_DEBUG("]\r\n");
#endif    
}

