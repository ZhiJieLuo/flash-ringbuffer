/******************************************************************************
* @file     logbuf.c
* @brief    报告缓冲区管理
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

#define SECTOR_BLEN           fw_port.sector_size()          /*扇区大小 ------*/
#define SECTOR_MASK           (SECTOR_BLEN - 1)              /*扇区掩码 ------*/
#define SECTOR_BASE(addr)     (addr & (~SECTOR_MASK))        /*扇区的基地址 --*/
#define SECTOR_OFFSET(addr)   (addr & SECTOR_MASK)           /*扇区内的偏移 --*/

#define ITEM_TAG           0xAA55                            /*目录项的标识 --*/

/* Private function prototypes -----------------------------------------------*/
static void debug_show_map(logbuf_t *log);

/* Private variables ---------------------------------------------------------*/ 
/*目录索引项 -----------------------------------------------------------------*/
typedef struct
{
    unsigned short tag;                         /*有效数据项标识头------------*/    
    unsigned int head;                          /*当前缓冲区头索引 -----------*/
    unsigned int tail;                          /*当前缓冲区尾索引 -----------*/
    unsigned short total_items;                 /*缓冲区内的总报告项数 -------*/
    unsigned short crc;                         /*crc 校验码 -----------------*/
}dir_item_t;

/*目录索引信息 ---------------------------------------------------------------*/
typedef struct
{
    unsigned int base;                          /*目录区的基地址 -------------*/
    unsigned short size;                        /*目录缓冲区的总长度 ---------*/
    unsigned short block_len;                   /*目录项长度 -----------------*/
    unsigned short windex;                      /*当前目录项在目录缓冲区中的写索引*/
}dir_info_t;
#if 0
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
#endif
//获取目录信息
void get_dir_info(logbuf_t *log, dir_info_t *info)
{
    info->base = log->base + log->size - log->directory_size; /*目录区的基地址*/
    info->size = log->directory_size;                         /*目录分区大小 -*/                
    /*目录项的块长度 ---------------------------------------------------------*/
    /*向上对齐得到满足2的幂的块长度 ------------------------------------------*/
    //info->block_len = roundup_pow_of_two(sizeof(dir_item_t));
    info->block_len = 16;                                    
    info->windex = log->windex; 
}

/*******************************************************************************
 * @brief		从一个扇区内查找目录项
 * @param[in]   sector_buf - 当前扇区数据缓冲区
 * @param[out]  offset     - 数据项在扇区内的偏移
 * @return 	    如果找到则返回1
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
            *offset = i;                                  /*得到扇区内的偏移 -*/
            memcpy(item, tmp, sizeof(dir_item_t));
            return 1;
        }
    }    
    return 0;
}

/*******************************************************************************
 * @brief		初始化报告的目录
 * @param[in]   report - 报告管理信息
 * @return 	    执行成功返回非0, 否则返回0
 ******************************************************************************/
static int dir_info_init(logbuf_t *log)
{
    int total_sector,i;
    unsigned char *swap_buf;
    unsigned int sector_offset;                    /*当前扇区内的偏移---------*/
    dir_info_t info;
    dir_item_t item; 
    get_dir_info(rep, &info);   
    
    total_sector = info.size / SECTOR_BLEN;        /*获取总扇区数 ------------*/    
    /*扫描出上一次断电前的数据地址 -------------------------------------------*/
    swap_buf = (unsigned char *)fw_port.malloc(SECTOR_BLEN);
    if (swap_buf == NULL)return 0;    
    /*遍历所有扇区查找出索引项 -----------------------------------------------*/
    for (i = 0; i < total_sector; i++)               
    {       
        fw_port.read(info.base + i * SECTOR_BLEN, swap_buf, SECTOR_BLEN);
        if (find_dir_item(&info, swap_buf,&item, &sector_offset))
        {
            info.windex = i * SECTOR_BLEN + sector_offset;
            fw_port.free(swap_buf);
            /*恢复断电前的参数 -----------------------------------------------*/
            log->windex = info.windex;
            log->total_items = item.total_items;
            log->head = item.head;
            log->tail = item.tail;
            return 1;
        }
    }
    fw_port.free(swap_buf);
    /*查找失败则清空无效数据项 -----------------------------------------------*/    
    log->windex = 0;
    get_dir_info(rep,&info);
    fw_port.erase(info.base);             /*擦除索引区的第一个扇区 -------*/
    return 0;
	
    
}

/*******************************************************************************
 * @brief		保存目录信息
 * @param[in]   report - 报告管理信息
 * @return 	    执行成功返回1, 
 ******************************************************************************/
static int save_diretory_info(logbuf_t *log)
{
    unsigned int  last_item_base;                    /*上一个数据项的基地址 --*/
    unsigned short crc;
    dir_info_t info;
    dir_item_t item;                                 
    if (log->begin_update)return 1;                  /*禁止更新目录 ----------*/
    get_dir_info(rep, &info);
    
    last_item_base = info.base + info.windex;        /*上一个目录项的基地址 --*/    
    /*偏移到下一块空闲区域 ---------------------------------------------------*/
    info.windex  = (info.windex + info.block_len) % info.size;
    log->windex  = info.windex;    
    if (SECTOR_OFFSET(info.base + info.windex)  == 0)/*进入新扇区-------------*/
    {
        FW_DEBUG("Erase sector=>0x%08x",info.base + info.windex);
        fw_port.erase(info.base + info.windex);  /*擦除新扇区的数据 ------*/
    }
    /*生成目录项 -------------------------------------------------------------*/
    item.tag = ITEM_TAG;
    item.head = log->head;
    item.tail = log->tail;
    item.total_items = log->total_items;    
    crc = fw_port.checksum(&item, offsetof(dir_item_t, crc));
    item.crc = crc;
    
    /*写入新的目录------------------------------------------------------------*/
    fw_port.write(info.base + info.windex, &item, sizeof(dir_item_t));        
    /*销毁原先的目录 ---------------------------------------------------------*/
    memset(&item, 0, sizeof(dir_item_t));
    fw_port.write(last_item_base, &item.tag, sizeof(item.tag)); 
    
    debug_show_map(rep);
    return 1;   
}

/*******************************************************************************
 * @brief		判断缓冲内的数据是否全为0xFF
 * @param[in]   buf  -  数据缓冲区
 * @param[in]   size -  数据长度 
 * @return 	    执行成功返回1, 
 ******************************************************************************/
static int buf_is_valid(unsigned char *buf, unsigned int size)
{
    while (size--)
        if (*buf++ != 0xff)
            return 0;
    return 1;
}

/*******************************************************************************
 * @brief       初始化报告缓冲区信息
 * @param[in]   report              - 报告缓冲区信息
 * @return      0 - 初始化失败 非0 - 初始化成功
 ******************************************************************************/
unsigned int logbuf_init(logbuf_t *log,unsigned int base, unsigned int size)
{    
    unsigned int buf_size;                       /*报告缓冲区大小 ------------*/
    unsigned int windex,rindex;                  /*读写索引指向的地址 --------*/    
	unsigned int  write_base,read_base;          /*读写索引所在扇区的基地址 --*/
	unsigned short write_offset,read_offset;     /*读写索引在当前扇区内的偏移-*/	
    /*当前扇区下空闲区域长度 -------------------------------------------------*/
    unsigned int space_left; 
    unsigned char *swap_buf;                     /*交换缓冲区指针 ------------*/

    if (size < SECTOR_BLEN * 4)                  /*至少需要4个扇区 -----------*/  
    {
        FW_DEBUG("The size of the buffer must be greater than  4 * <SECTOR_BLEN>.\r\n");
        return 0;
    }
    if (base % SECTOR_BLEN)                      /*保证起始地址扇区对齐 ------*/
    {
        FW_DEBUG("The Buffer base address must be sector aligned");
        return 0;
    }         
    
    log->size = size;    
    /*缓冲区的大小小于6个扇区时使用2个扇区作为目录,否则使用4 + 总扇区/16个扇区
      用为目录扇区使用,最大限制为8个扇区 -------------------------------------*/
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
    buf_size = log->size - log->directory_size;    /*有效缓冲区的长度 ---------*/
    if (!dir_info_init(rep))                      /*初始化缓冲区索引信息 -----*/
    {
        /*首次获取目录信息肯定会失败 -----------------------------------------*/
        log->head = log->tail = 0;                 /*重置读写索引 -------------*/ 
        log->windex = 0;                           /*重置目录写索引 -----------*/
        save_diretory_info(rep);                  /*保存索引信息 -------------*/
        FW_DEBUG("Invalid partition\r\n");
        return 1;   
    }
    windex = log->base + log->tail % buf_size;
    rindex = log->base + log->head % buf_size;   
    /*当前读写索引所在扇区的基地址及偏移量 -----------------------------------*/
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
	
	fw_port.read(write_base, swap_buf, SECTOR_BLEN);/*读取当前扇区 -------*/
    /*判断读写索引是否在同一扇区 ---------------------------------------------*/
	if (write_base == read_base && write_offset < read_offset)             
	{
        space_left  = read_offset - write_offset;    /*空闲空间 --------------*/              
	}
	else                                      
	{
		space_left  = SECTOR_BLEN - write_offset;
	}
	/*确保当前扇区下写索引指向的空闲空间全为0xff -----------------------------*/
    if (!buf_is_valid(swap_buf, space_left))
    {
        /*
         *如果空闲空间不全为0xff则擦当前扇区,同时将有效数据写回到当前扇区内
         */      
        fw_port.erase(write_base);         
        memset(&swap_buf[write_offset], 0xFF, space_left);
        fw_port.write(write_base, swap_buf, SECTOR_BLEN);        
    }
    fw_port.free(swap_buf);
	return 1;
}


/*******************************************************************************
 * @brief       report项的有效数据长度
 * @param[in]   report                - 报告缓冲区管理信息
 * @return      有效数据长度
 ******************************************************************************/
unsigned int logbuf_len(logbuf_t *log)
{
	return log->tail - log->head;                   
}

/*******************************************************************************
 * @brief       缓冲区内的剩余空闲数据长度
 * @param[in]   report                - 报告缓冲区管理信息
 * @return      剩余空闲空间长度
 ******************************************************************************/
unsigned int logbuf_space_left(logbuf_t *log)
{
	return (log->size - log->directory_size) - logbuf_len(rep); 
}

/*******************************************************************************
 * @brief       清除缓冲区内的所有数据
 * @param[in]   report                - 报告缓冲区管理信息
 * @return      剩余空闲空间长度
 ******************************************************************************/
void logbuf_clear(logbuf_t *log)
{
    unsigned int buf_size;                           
    if (log->tail == log->head)return; 
    log->head = log->tail;
    log->total_items = 0;                               /*重置总项数 ---------*/
    buf_size = log->size - log->directory_size;         /*有效缓冲区的长度 ---*/    
    fw_port.erase(log->base + log->tail % buf_size);/*擦除写索引所在扇区 -*/    
    save_diretory_info(rep);
}

/*******************************************************************************
 * @brief		获取缓冲内有效的数据项数
 * @param[in]   report                - 报告缓冲区管理信息
 * @return 	    
 * @attention   数据项数仅是参考值, 正常情况下是准确的,
 *              当缓冲区个别数据由于其它不可抗力被坏之后该值可能不准.而当缓冲区
 *              再为空后,此值将从0开始重新计数,恢复正常.
 ******************************************************************************/
unsigned int logbuf_count(logbuf_t  *log)
{
    if (logbuf_len(rep) == 0)
    {
        log->total_items = 0;
        return 0;
    }
    if (log->total_items == 0)return 1;                  /*异常处理 ----------*/           
    
    return log->total_items;
}


/*******************************************************************************
 * @brief       报告写处理
 * @param[in]   write_addr        - 缓冲区写索引
 * @param[in]   read_addr         - 缓冲区读索引
 * @param[in]   buf               - 数据缓冲区
 * @param[in]   len               - 数据长度 
 * @return      非0 - 写成功, 0 - 写失败
 ******************************************************************************/
static int logbuf_write_process(unsigned int write_addr,unsigned int read_addr, unsigned char *buf, unsigned int len)
{
	unsigned int  write_base,read_base;          /*读写索引所在扇区的基地址 --*/
	unsigned short write_offset,read_offset;     /*读写索引在当前扇区内的偏移-*/	
	unsigned short sector_remain;                /*扇区剩余空间 --------------*/
    unsigned char *swap_buf;                     /*交换缓冲区指针 ------------*/	
	while (len)
	{			
		write_base = SECTOR_BASE(write_addr);    /*扇区内的基地址 ------------*/
		read_base  = SECTOR_BASE(read_addr);
		write_offset = SECTOR_OFFSET(write_addr);/*扇区内的偏移 --------------*/
		read_offset  = SECTOR_OFFSET(read_addr); 
        
		/*
         *读写索引位于同一扇区且读索引位于写索引的右边这种情况下,每次写入新数据
         *前都需要擦除界于写-读索引间的数据.
         */
        if ((write_base == read_base && write_offset < read_offset))             
        {
            FW_DEBUG("wbase = rbase = %08x, woffset:%d,roffset:%d ",write_offset, read_offset);   
            /*剩余空闲空间 ---------------------------------------------------*/
            sector_remain = read_offset - write_offset;
            swap_buf = (unsigned char *)fw_port.malloc(SECTOR_BLEN);     
            if (swap_buf == NULL)
            {
                FW_DEBUG("malloc error line=>%d\r\n",__LINE__);
                return 0;
            } 
            /*写入的数据长度比剩余空间还长这种情况不会出现,上层已经处理 ------*/ 
            if (len > sector_remain)               
            {
                FW_DEBUG("Error :%d, len:%d, remain:%d\r\n",__LINE__, len, sector_remain);
                fw_port.free(swap_buf); 
                return 0;
            }
            fw_port.read(write_base, swap_buf, SECTOR_BLEN);
            /*判断写索引之后的数据是否全为FF ---------------------------------*/
            if (!buf_is_valid(&swap_buf[write_offset], len))
            {                                    
                fw_port.erase(write_base);    /*擦除当前扇区 -------------*/   
                FW_DEBUG("Earse sector:%08X\r\n",write_base);
            }
            else 
            {
                FW_DEBUG("No need earse:%08X\r\n",write_base);
            }
            /*修改之后写回当前扇区 -------------------------------------------*/
            memset(&swap_buf[write_offset],0xFF,sector_remain);
            memcpy(&swap_buf[write_offset], buf, len);              
            fw_port.write(write_base, swap_buf, SECTOR_BLEN);	
            fw_port.free(swap_buf);             
            return 1;
            
        }
        else if (write_addr == write_base)       /*写索引进入到新扇区 --------*/
        {
            FW_DEBUG("Enter new sector :%08x\r\n",write_addr); 
            sector_remain = SECTOR_BLEN;
            fw_port.erase(write_base);        /*擦除当前扇区 -------------*/    
        }
        else 
        {
			//当前扇区剩余空间
			sector_remain = SECTOR_BLEN - write_offset;	      
        }
        FW_DEBUG("->sector remain:%d\r\n",sector_remain);    		
		//判断剩余的空间是否可以容纳新数据		
		if (len < sector_remain) sector_remain = len;
		//写入新数据
		fw_port.write(write_addr, buf, sector_remain);
		//写指针偏移
		write_addr += sector_remain;
		buf += sector_remain;
		len -= sector_remain;
	}
	return 1;
}


/*******************************************************************************
 * @brief       写入一个数据项
 * @param[in]   report               - 报告缓冲区管理信息
 * @param[in]   buf               - 数据缓冲区
 * @param[in]   len               - 缓冲区长度 
 * @return      非0 - 写成功, 0 - 写失败
 ******************************************************************************/
unsigned int logbuf_put(logbuf_t *log, void *buf, unsigned int len)
{ 
    unsigned char *ptr = (unsigned char *)buf;
    unsigned int left;                             
    unsigned int buf_size;                           /*缓冲区总大小 ----------*/              
	if (logbuf_space_left(rep) < len)return 0;   /*空间不足 --------------*/
    buf_size = log->size - log->directory_size;
    
	len = min(len , buf_size + log->head - log->tail);/*得到实际写入的数据长度*/
	/*写索引至缓冲区末尾的空闲长度 -------------------------------------------*/
    left = min(len, buf_size - log->tail % buf_size);    
   	if (logbuf_write_process(log->base + log->tail % buf_size, 
   	                      log->base + log->head % buf_size, ptr, left) == 0)
   	{
   	    return 0;
   	}    
   	/*将剩余的数据写入缓冲区开头 ---------------------------------------------*/
	if (logbuf_write_process(log->base, log->base + log->head % buf_size,
	                      ptr + left, len - left) == 0)
	{
	    return 0;
	}
	log->tail += len;
    
	save_diretory_info(rep);                                /*保存缓冲区信息 -*/    
	return len;   	
}

/*******************************************************************************
 * @brief       读取指定长度的数据项,但不移除数据
 * @param[in]   report              - 报告缓冲区管理信息
 * @param[out]  buf              - 数据缓冲区
 * @param[in]   len              - 缓冲区长度 
 * @return      实际读取长度
 ******************************************************************************/
unsigned int logbuf_peek(logbuf_t *log, void *buf, unsigned int len)
{
    unsigned char *ptr = (unsigned char *)buf;
	unsigned int left;
    unsigned int buf_size;   
    buf_size = log->size - log->directory_size;
	len = min(len , log->tail - log->head); 
	/*获取读指针至缓冲区尾部的有效长度 ---------------------------------------*/
	left = min(len, buf_size - log->head % buf_size);
	fw_port.read(log->base + log->head % buf_size,ptr, left);
	fw_port.read(log->base, ptr + left, len - left);
    return len;
}

/*******************************************************************************
 * @brief       从缓冲区内读取指定长度的数据,读取成功后移除该数据
 * @param[in]   report               - 报告缓冲区管理信息
 * @param[out]  buf               - 数据缓冲区
 * @param[in]   len               - 缓冲区长度
 * @return      实际读取长度
 ******************************************************************************/
unsigned int logbuf_get(logbuf_t *log, void *buf, unsigned int len)
{
	len = logbuf_peek(rep, buf, len);	
	log->head += len; 
    if (len)
    {        
        save_diretory_info(rep);                          /*保存目录信息 -----*/        
    }
	return len;
}

/*******************************************************************************
 * @brief       移除指定长度的缓冲区数据
 * @param[in]   report              - 报告缓冲区管理信息
 * @param[in]   len              - 预移除的数据长度
 * @return      实际移除长度
 ******************************************************************************/
unsigned int logbuf_remove(logbuf_t *log, unsigned int len)
{
    len = min(len , log->tail - log->head); 
	log->head += len;    
	return len;
}

/*******************************************************************************
 * @brief       恢复先前的移除指定长度的缓冲区数据
 * @param[in]   report              - 报告缓冲区管理信息
 * @param[in]   len                 - 预恢复的数据长度
 * @return      实际恢复长度
 * @attention   必须保证在logbuf_remove之后才能使用,且恢复的长度不大于先前移
 *              除的长度.
 ******************************************************************************/
unsigned int logbuf_resume(logbuf_t *log, unsigned int len)
{
    len = min(logbuf_space_left(rep), len);
    log->head -= len;     
	return len;    
}

/*******************************************************************************
 * @brief      开始更新/写入报告(实际是挂起目录更新操作,
 *             如果在短时间内需要多次执行logbuf_put操作,
 *             为了避免频繁更新目录及提高系统效率,可以先
 *             执行report_begin_update操作,当操作执行完所有
 *             的logbuf_put操作之后,调用report_end_update
 *             操作以恢复目录实时更新操作)
 *
 * @param[in]   report              - 报告缓冲区管理信息
 * @return      none
 ******************************************************************************/
void report_begin_update(logbuf_t  *log)
{
    log->begin_update++;
}

/*******************************************************************************
 * @brief      结束更新/写入/报告(与report_begin_update配对使用)
 *
 * @param[in]   report              - 报告缓冲区管理信息
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
        save_diretory_info(rep);                          /*保存缓冲区信息 ---*/
    }        
}


/*******************************************************************************
 * @brief       报告搬移(将src中的报告移动到des中,并删除src同的报告)
 * @param[in]   src -  源缓冲区信息
 * @param[in]   des -  目标缓冲区信息
 * @return      非0   执行成功  0  执行失败
  * @attention  必须保证des空闲空间长度大于src有效数据长度,否则执行失败
 ******************************************************************************/
int logbuf_move(logbuf_t *des, logbuf_t *src, unsigned int len)
{
    unsigned short size;
    unsigned char swap[256];                              /*交换缓冲区 -------*/
    len = min(logbuf_len(src), len);                  /*实际搬移长度 -----*/
    if (len == 0)return 1;                                /*源缓冲区为空 -----*/
    
    /*判断目标缓冲区的空闲空间是否能够容纳源缓冲区内的全部数据 ---------------*/
    if (logbuf_space_left(des) < len)return 0;    

    report_begin_update(src);                             /*挂起目录更新操作 -*/
    report_begin_update(des);  
   
    do{
        /*读取源缓冲区的数据 -------------------------------------------------*/
        size = logbuf_get(src, swap, len > sizeof(swap) ? sizeof(swap) : len);   
        len -= size;
    }while (size && logbuf_put(des, swap, size) && len );/*写入到目标缓冲区内*/
    report_end_update(src);                                  /*恢复目录更新操作 -*/
    report_end_update(des);
    return 1;
}




/*
 * 显示report映射表(有效数据使用#表示,空闲空间使用-表示)
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

