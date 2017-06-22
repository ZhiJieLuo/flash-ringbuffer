/******************************************************************************
* @file     report_manager.c
* @brief    报告管理器
* @version  1.0
* @date     2016-12-22
* @author   roger.luo
* 
* Copyright(C) 2016
* All rights reserved.
*
*******************************************************************************/
#include "logbuf.h"
#include "fw_port.h"
#include "fw_debug.h"
#include "report_manager.h"
#include <stddef.h>
#include <string.h>

#define DATA_ITEM_TAG           0xAA55                       /*数据项标识 ----*/

/*数据项头部信息 -------------------------------------------------------------*/
#pragma pack(1)                                              /*单字节对齐 ----*/
typedef struct{
    unsigned short tag;                                    
    unsigned short crc;                                      /*数据项crc校验码*/
    unsigned short len;                                      /*数据项有效长度-*/
    unsigned short chksum;                                   /*头部校验和 ----*/
}item_header_t;
#pragma pack()

/*******************************************************************************
 * @brief		查找数据项的头部
 * @param[in]   rep  - 环形缓冲区信息
 * @param[out]  hdr   指向数据项头指针
 * @return 	    非0 - 查找成功, 0 - 查找失败
 ******************************************************************************/
static int find_item_header(logbuf_t *rep, item_header_t *hdr)
{
    unsigned int i; 
    unsigned int search_size;                                 /*查找长度 -----*/                           
    unsigned char *swap = NULL;                               /*交换缓冲区指针*/
    item_header_t *tmp;
    
    if (logbuf_peek(rep, hdr, sizeof(item_header_t)) == 0)return 0;
    if (hdr->tag != DATA_ITEM_TAG || 
        hdr->chksum != fw_port.checksum(hdr, offsetof(item_header_t, chksum)))
    {                
        swap = (unsigned char *)fw_port.malloc(4096);
        if (swap == NULL)return 0;
        /*
         * 连续搜索最大4K长度的数据,如果没有找到数据项头部,则认为查找失败
         */
        search_size = logbuf_peek(rep, swap, 4096);
        if (search_size < sizeof(item_header_t))
        {
            fw_port.free(swap);
            logbuf_remove(rep, search_size);  
            return 0;
        }
        for (i = 0; i + sizeof(item_header_t) <= search_size; i++)
        {
            tmp = (item_header_t *)&swap[i];            
            if (tmp->tag == DATA_ITEM_TAG && 
                tmp->chksum == fw_port.checksum(tmp, offsetof(item_header_t, chksum)))
            {                       
                *hdr = *tmp; 
                fw_port.free(swap);
                logbuf_remove(rep, i);               /*查找成功 ----------*/
                return 1;                  
            }
        }
        fw_port.free(swap);
        logbuf_remove(rep, search_size);             /*移除无效数据项 ----*/             
    }
    return 1;
}

/*******************************************************************************
 * @brief		初始化环形缓冲区
 * @param[in]   rep  - 报告管理器信息 
 * @param[in]   base - report_manager_t管理的flash基地址(扇区对齐)
 * @param[in]   size - 整个report_manager_t管理的长度(必须是扇区对齐)
 * @return 	    0 - 执行失败, 非0 - 执行成功
 ******************************************************************************/
int report_manager_init(report_manager_t *rep, unsigned int base, unsigned int size)
{
    FW_ASSERT(rep != NULL);   
    return logbuf_init(rep, base, size);
}

/*******************************************************************************
 * @brief		写入一条报告项到缓冲区
 * @param[in]   rep     - 报告管理器信息 
 * @param[in]   buf     - 数据缓冲区
 * @param[in]   size    - 数据长度
 * @return 	    0 - 执行失败, 非0 - 实际写入长度
 ******************************************************************************/
unsigned int report_item_write(report_manager_t *rep, void *buf, unsigned int size)
{
    item_header_t hdr;
    unsigned short chksum;
    int ret;
    FW_ASSERT(buf != NULL);
    if (logbuf_space_left(rep) < sizeof(item_header_t) + size)
    {
        FW_DEBUG("No much more space left for write.\r\n");
        return 0;                                     /*空间不足 -------------*/
    }    
    report_begin_update(rep);                         /*开始写入 -------------*/
    /*生成数据项头部信息 -----------------------------------------------------*/
    hdr.tag = DATA_ITEM_TAG;
    hdr.crc = fw_port.checksum(buf, size);           /*数据项校验和 ---------*/
    hdr.len = size;  
    /*头部校验和 -------------------------------------------------------------*/
    chksum = fw_port.checksum(&hdr, offsetof(item_header_t, chksum));    
    hdr.chksum = chksum;
    ret = logbuf_put(rep, &hdr, sizeof(item_header_t));     
    if (ret)
    {
        ret = logbuf_put(rep, buf, size);
    }
    rep->total_items++;                                /*递增数据项个数 ------*/
    report_end_update(rep);                            /*完成更新 ------------*/
    FW_DEBUG("Item count->%d\r\n", rep->total_items);
    return ret;    
}

/*******************************************************************************
 * @brief		从缓冲区内预读取一条报告项但不移除该报告
 * @param[in]   rep     - 报告管理器信息 
 * @param[out]  buf     - 数据缓冲区
 * @param[in]   size    - 缓冲区长度
 * @return 	    0 - 执行失败, 非0 - 实际读取长度
 ******************************************************************************/
unsigned int report_item_peek(report_manager_t *rep, void *buf, unsigned int size)
{
    item_header_t hdr;                                   /*数据项的头部 ------*/
    int retry = 5;                                       /*重试次数 ----------*/
    FW_ASSERT(buf != NULL && size > 0); 
    if (logbuf_len(rep) == 0)                        /*无可用的数据 ------*/
    {
        rep->total_items = 0;                            /*重置总项数 --------*/      
        return 0;
    }
    do{
        if (!find_item_header(rep, &hdr))
        {
            FW_DEBUG("Can not find item header. %d,\r\n",__LINE__);
            return 0; 
        }
        size = size < hdr.len ? size : hdr.len;          /*实际读取长度 ------*/        
        logbuf_remove(rep, sizeof(item_header_t));   /*移除头部 ----------*/         
        logbuf_peek(rep, buf, size);                 /*读取实际有效数据 --*/
        if (hdr.len > size)break;        
        if (hdr.crc != fw_port.checksum(buf, size))     /*校验失败 ----------*/
        {         
            FW_DEBUG("Checking CRC Fail when peek. %d,\r\n",__LINE__);      
            logbuf_remove(rep, hdr.len);             /*移除无效数据 ------*/ 
        }
        else 
        {
            logbuf_resume(rep, sizeof(item_header_t));/*恢复先前移除的头部*/
            break;
        } 
    }while (--retry); 
    return retry ? size : 0;
}

/*******************************************************************************
 * @brief		从缓冲区内读取一个数据项,当读取成功后将移除该数据项
 * @param[in]   rep     - 报告管理器信息 
 * @param[out]  buf     - 数据缓冲区
 * @param[in]   size    - 缓冲区长度
 * @return 	    0 - 执行失败, 非0 - 实际读取长度
 * @attention   size 必须>= 当前缓冲区内的数据项长度,否则数据项会被截断 
 ******************************************************************************/
unsigned int report_item_read(report_manager_t *rep, void *buf, unsigned int size)
{
    item_header_t hdr;
    int retry = 10;  
    FW_ASSERT(buf != NULL || size > 0);    
    if (logbuf_len(rep) == 0)                        /*无可用的数据 ------*/
    {
        rep->total_items = 0;                            /*重置总项数 --------*/
        return 0;                                        
    }    
    report_begin_update(rep);                            /*开始读取 ----------*/
    do{
        if (!find_item_header(rep, &hdr))
        {
            FW_DEBUG("Find item header fail. %d,\r\n",__LINE__);
            report_end_update(rep);
            return 0;
        }
        size = size < hdr.len ? size : hdr.len;          /*实际读取长度 ------*/
        logbuf_remove(rep, sizeof(item_header_t));   /*移除头部 ----------*/        
        logbuf_peek(rep, buf, size);
        logbuf_remove(rep, hdr.len);        
        if (rep->total_items)rep->total_items--;         /*数据项减1 ---------*/                                                        
    }while (--retry && hdr.len <= size && hdr.crc != fw_port.checksum(buf, size));     
    report_end_update(rep);                              /*结束读取 ----------*/    
    return retry ? size : 0;
}

/*******************************************************************************
 * @brief		将源缓冲区内的全部报告项全部移动到目标缓冲区内,同时清空源缓冲区
 * @param[in]   des  - 目标报告管理器
 * @

[in]   src  - 源报告管理器
 * @return 	    0  - 执行失败, 非0 - 执行成功
 * @attention   需要保证目标缓冲区充足的空间,否则会执行失败
 ******************************************************************************/
int report_item_move_all(report_manager_t *des, report_manager_t *src)
{
    int ret;
    FW_ASSERT(des != NULL && src != NULL);    
    report_begin_update(des);                           
    report_begin_update(src);         
	ret =  logbuf_move(des, src, logbuf_len(src));
    if (ret)
    {
        des->total_items += src->total_items;
        src->total_items = 0;        
    }
    report_end_update(des);                              /*结束更新-----------*/                        
    report_end_update(src);     
    return ret;
}

/*******************************************************************************
 * @brief		将源报告缓冲区内指定个数的报告项移动到目标缓冲区内,同时清除源缓
 *              冲内的数据项
 * @param[in]   des    - 目标报告管理器
 * @param[in]   src    - 源报告管理器
 * @param[in]   count  - 移动的报告项数
 * @return 	    0  - 执行失败, 非0 - 实际移动的项数
 * @attention   需要保证目标缓冲区充足的空间,否则会执行失败
 ******************************************************************************/
int report_item_move(report_manager_t *des, report_manager_t *src, int count)
{
    item_header_t hdr;                                   /*数据项头部 --------*/
    int i;
    
    FW_ASSERT(des != NULL && src != NULL);    
    FW_DEBUG("src nums:%d, des nums:%d\r\n", src->total_items, des->total_items); 
    if (count == 0)return 1;
    if (logbuf_len(src) == 0)return 1;
    
    report_begin_update(des);                           
    report_begin_update(src); 
    
    for (i = 0; i < count; i++)
    {
        if (!find_item_header(src, &hdr))
        {
            FW_DEBUG("Can not find item header. %d,\r\n",__LINE__);                 
            break;
        }    
        /*目标缓冲区不足 -----------------------------------------------------*/
        if (logbuf_space_left(des) < hdr.len)break;
        /*数据搬移 -----------------------------------------------------------*/
        logbuf_move(des, src, sizeof(item_header_t) + hdr.len);              
        
        if (src->total_items)src->total_items--; 
        des->total_items++;                              
    }
    report_end_update(des);                              /*结束更新-----------*/                        
    report_end_update(src);   
    FW_DEBUG("src nums:%d, des nums:%d\r\n", src->total_items, des->total_items);     
	return i;
}

/*******************************************************************************
 * @brief		将源报告缓冲区内指定个数的报告项移动到目标缓冲区内
 * @param[in]   des    - 目标报告管理器
 * @param[in]   src    - 源报告管理器
 * @param[in]   count  - 移动的报告项数
 * @return 	    0  - 执行失败, 非0 - 实际移动的项数
 * @attention   需要保证目标缓冲区充足的空间,否则会执行失败
 ******************************************************************************/
unsigned int report_item_count(report_manager_t *rep)
{
    FW_ASSERT(rep != NULL);
    return logbuf_count(rep);	
}

/*******************************************************************************
 * @brief		获取当前报告缓冲区的剩余空闲空间大小
 * @param[in]   report    - 报告管理器
 * @return 	    报告缓冲区的空闲空间大小
 ******************************************************************************/
unsigned int report_item_space_left(report_manager_t *rep)
{
    FW_ASSERT(rep != NULL);
    if (logbuf_space_left(rep) > sizeof(item_header_t))
        return logbuf_space_left(rep) - sizeof(item_header_t);
    else 
        return 0;
}
/*******************************************************************************
 * @brief       report所有项的数据总长度
 * @param[in]   report                - 报告缓冲区管理信息
 * @return      有效数据长度
 ******************************************************************************/
unsigned int report_items_size(report_manager_t *rep)
{
	return logbuf_len(rep);                   
}
/*******************************************************************************
 * @brief		报告缓冲区信息
 * @param[in]   
 * @return 	    0 - 执行失败, 非0 - 执行成功
 ******************************************************************************/
int report_item_clear(report_manager_t *rep)
{
    FW_ASSERT(rep != NULL);
    logbuf_clear(rep);    
    return 1;
}

/*******************************************************************************
 * @brief		删除指定个数的数据项
 * @param[in]   count - 待移除的数据项个数
 * @return 	    0 - 执行失败, 非0 - 执行成功(并返回实际删除项数)
 ******************************************************************************/
int report_item_delete(report_manager_t *rep, int count)
{
    int i;
    item_header_t hdr;
    FW_ASSERT(rep != NULL);
    if (logbuf_len(rep) == 0)                        /*无可用的数据 ------*/
    {
        rep->total_items = 0;                            /*重置总项数 --------*/
        return 0;                                        
    }
    report_begin_update(rep);                            /*开始读取 ----------*/
    for (i = 0; i < count; i++)
    {
        if (!find_item_header(rep, &hdr))
        {
            FW_DEBUG("Find item header fail. %d,\r\n",__LINE__);
            break;
        }            
        logbuf_remove(rep, sizeof(item_header_t) + hdr.len);   
        if (rep->total_items)rep->total_items--;          /*数据项减1 --------*/                                   
    }  
    report_end_update(rep);                               /*结束读取 ---------*/
    return i;
}


