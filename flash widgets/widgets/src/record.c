/******************************************************************************
* @file     report_manager.c
* @brief    ���������
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

#define DATA_ITEM_TAG           0xAA55                       /*�������ʶ ----*/

/*������ͷ����Ϣ -------------------------------------------------------------*/
#pragma pack(1)                                              /*���ֽڶ��� ----*/
typedef struct{
    unsigned short tag;                                    
    unsigned short crc;                                      /*������crcУ����*/
    unsigned short len;                                      /*��������Ч����-*/
    unsigned short chksum;                                   /*ͷ��У��� ----*/
}item_header_t;
#pragma pack()

/*******************************************************************************
 * @brief		�����������ͷ��
 * @param[in]   rep  - ���λ�������Ϣ
 * @param[out]  hdr   ָ��������ͷָ��
 * @return 	    ��0 - ���ҳɹ�, 0 - ����ʧ��
 ******************************************************************************/
static int find_item_header(logbuf_t *rep, item_header_t *hdr)
{
    unsigned int i; 
    unsigned int search_size;                                 /*���ҳ��� -----*/                           
    unsigned char *swap = NULL;                               /*����������ָ��*/
    item_header_t *tmp;
    
    if (logbuf_peek(rep, hdr, sizeof(item_header_t)) == 0)return 0;
    if (hdr->tag != DATA_ITEM_TAG || 
        hdr->chksum != fw_port.checksum(hdr, offsetof(item_header_t, chksum)))
    {                
        swap = (unsigned char *)fw_port.malloc(4096);
        if (swap == NULL)return 0;
        /*
         * �����������4K���ȵ�����,���û���ҵ�������ͷ��,����Ϊ����ʧ��
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
                logbuf_remove(rep, i);               /*���ҳɹ� ----------*/
                return 1;                  
            }
        }
        fw_port.free(swap);
        logbuf_remove(rep, search_size);             /*�Ƴ���Ч������ ----*/             
    }
    return 1;
}

/*******************************************************************************
 * @brief		��ʼ�����λ�����
 * @param[in]   rep  - �����������Ϣ 
 * @param[in]   base - report_manager_t�����flash����ַ(��������)
 * @param[in]   size - ����report_manager_t����ĳ���(��������������)
 * @return 	    0 - ִ��ʧ��, ��0 - ִ�гɹ�
 ******************************************************************************/
int report_manager_init(report_manager_t *rep, unsigned int base, unsigned int size)
{
    FW_ASSERT(rep != NULL);   
    return logbuf_init(rep, base, size);
}

/*******************************************************************************
 * @brief		д��һ�������������
 * @param[in]   rep     - �����������Ϣ 
 * @param[in]   buf     - ���ݻ�����
 * @param[in]   size    - ���ݳ���
 * @return 	    0 - ִ��ʧ��, ��0 - ʵ��д�볤��
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
        return 0;                                     /*�ռ䲻�� -------------*/
    }    
    report_begin_update(rep);                         /*��ʼд�� -------------*/
    /*����������ͷ����Ϣ -----------------------------------------------------*/
    hdr.tag = DATA_ITEM_TAG;
    hdr.crc = fw_port.checksum(buf, size);           /*������У��� ---------*/
    hdr.len = size;  
    /*ͷ��У��� -------------------------------------------------------------*/
    chksum = fw_port.checksum(&hdr, offsetof(item_header_t, chksum));    
    hdr.chksum = chksum;
    ret = logbuf_put(rep, &hdr, sizeof(item_header_t));     
    if (ret)
    {
        ret = logbuf_put(rep, buf, size);
    }
    rep->total_items++;                                /*������������� ------*/
    report_end_update(rep);                            /*��ɸ��� ------------*/
    FW_DEBUG("Item count->%d\r\n", rep->total_items);
    return ret;    
}

/*******************************************************************************
 * @brief		�ӻ�������Ԥ��ȡһ����������Ƴ��ñ���
 * @param[in]   rep     - �����������Ϣ 
 * @param[out]  buf     - ���ݻ�����
 * @param[in]   size    - ����������
 * @return 	    0 - ִ��ʧ��, ��0 - ʵ�ʶ�ȡ����
 ******************************************************************************/
unsigned int report_item_peek(report_manager_t *rep, void *buf, unsigned int size)
{
    item_header_t hdr;                                   /*�������ͷ�� ------*/
    int retry = 5;                                       /*���Դ��� ----------*/
    FW_ASSERT(buf != NULL && size > 0); 
    if (logbuf_len(rep) == 0)                        /*�޿��õ����� ------*/
    {
        rep->total_items = 0;                            /*���������� --------*/      
        return 0;
    }
    do{
        if (!find_item_header(rep, &hdr))
        {
            FW_DEBUG("Can not find item header. %d,\r\n",__LINE__);
            return 0; 
        }
        size = size < hdr.len ? size : hdr.len;          /*ʵ�ʶ�ȡ���� ------*/        
        logbuf_remove(rep, sizeof(item_header_t));   /*�Ƴ�ͷ�� ----------*/         
        logbuf_peek(rep, buf, size);                 /*��ȡʵ����Ч���� --*/
        if (hdr.len > size)break;        
        if (hdr.crc != fw_port.checksum(buf, size))     /*У��ʧ�� ----------*/
        {         
            FW_DEBUG("Checking CRC Fail when peek. %d,\r\n",__LINE__);      
            logbuf_remove(rep, hdr.len);             /*�Ƴ���Ч���� ------*/ 
        }
        else 
        {
            logbuf_resume(rep, sizeof(item_header_t));/*�ָ���ǰ�Ƴ���ͷ��*/
            break;
        } 
    }while (--retry); 
    return retry ? size : 0;
}

/*******************************************************************************
 * @brief		�ӻ������ڶ�ȡһ��������,����ȡ�ɹ����Ƴ���������
 * @param[in]   rep     - �����������Ϣ 
 * @param[out]  buf     - ���ݻ�����
 * @param[in]   size    - ����������
 * @return 	    0 - ִ��ʧ��, ��0 - ʵ�ʶ�ȡ����
 * @attention   size ����>= ��ǰ�������ڵ��������,����������ᱻ�ض� 
 ******************************************************************************/
unsigned int report_item_read(report_manager_t *rep, void *buf, unsigned int size)
{
    item_header_t hdr;
    int retry = 10;  
    FW_ASSERT(buf != NULL || size > 0);    
    if (logbuf_len(rep) == 0)                        /*�޿��õ����� ------*/
    {
        rep->total_items = 0;                            /*���������� --------*/
        return 0;                                        
    }    
    report_begin_update(rep);                            /*��ʼ��ȡ ----------*/
    do{
        if (!find_item_header(rep, &hdr))
        {
            FW_DEBUG("Find item header fail. %d,\r\n",__LINE__);
            report_end_update(rep);
            return 0;
        }
        size = size < hdr.len ? size : hdr.len;          /*ʵ�ʶ�ȡ���� ------*/
        logbuf_remove(rep, sizeof(item_header_t));   /*�Ƴ�ͷ�� ----------*/        
        logbuf_peek(rep, buf, size);
        logbuf_remove(rep, hdr.len);        
        if (rep->total_items)rep->total_items--;         /*�������1 ---------*/                                                        
    }while (--retry && hdr.len <= size && hdr.crc != fw_port.checksum(buf, size));     
    report_end_update(rep);                              /*������ȡ ----------*/    
    return retry ? size : 0;
}

/*******************************************************************************
 * @brief		��Դ�������ڵ�ȫ��������ȫ���ƶ���Ŀ�껺������,ͬʱ���Դ������
 * @param[in]   des  - Ŀ�걨�������
 * @

[in]   src  - Դ���������
 * @return 	    0  - ִ��ʧ��, ��0 - ִ�гɹ�
 * @attention   ��Ҫ��֤Ŀ�껺��������Ŀռ�,�����ִ��ʧ��
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
    report_end_update(des);                              /*��������-----------*/                        
    report_end_update(src);     
    return ret;
}

/*******************************************************************************
 * @brief		��Դ���滺������ָ�������ı������ƶ���Ŀ�껺������,ͬʱ���Դ��
 *              ���ڵ�������
 * @param[in]   des    - Ŀ�걨�������
 * @param[in]   src    - Դ���������
 * @param[in]   count  - �ƶ��ı�������
 * @return 	    0  - ִ��ʧ��, ��0 - ʵ���ƶ�������
 * @attention   ��Ҫ��֤Ŀ�껺��������Ŀռ�,�����ִ��ʧ��
 ******************************************************************************/
int report_item_move(report_manager_t *des, report_manager_t *src, int count)
{
    item_header_t hdr;                                   /*������ͷ�� --------*/
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
        /*Ŀ�껺�������� -----------------------------------------------------*/
        if (logbuf_space_left(des) < hdr.len)break;
        /*���ݰ��� -----------------------------------------------------------*/
        logbuf_move(des, src, sizeof(item_header_t) + hdr.len);              
        
        if (src->total_items)src->total_items--; 
        des->total_items++;                              
    }
    report_end_update(des);                              /*��������-----------*/                        
    report_end_update(src);   
    FW_DEBUG("src nums:%d, des nums:%d\r\n", src->total_items, des->total_items);     
	return i;
}

/*******************************************************************************
 * @brief		��Դ���滺������ָ�������ı������ƶ���Ŀ�껺������
 * @param[in]   des    - Ŀ�걨�������
 * @param[in]   src    - Դ���������
 * @param[in]   count  - �ƶ��ı�������
 * @return 	    0  - ִ��ʧ��, ��0 - ʵ���ƶ�������
 * @attention   ��Ҫ��֤Ŀ�껺��������Ŀռ�,�����ִ��ʧ��
 ******************************************************************************/
unsigned int report_item_count(report_manager_t *rep)
{
    FW_ASSERT(rep != NULL);
    return logbuf_count(rep);	
}

/*******************************************************************************
 * @brief		��ȡ��ǰ���滺������ʣ����пռ��С
 * @param[in]   report    - ���������
 * @return 	    ���滺�����Ŀ��пռ��С
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
 * @brief       report������������ܳ���
 * @param[in]   report                - ���滺����������Ϣ
 * @return      ��Ч���ݳ���
 ******************************************************************************/
unsigned int report_items_size(report_manager_t *rep)
{
	return logbuf_len(rep);                   
}
/*******************************************************************************
 * @brief		���滺������Ϣ
 * @param[in]   
 * @return 	    0 - ִ��ʧ��, ��0 - ִ�гɹ�
 ******************************************************************************/
int report_item_clear(report_manager_t *rep)
{
    FW_ASSERT(rep != NULL);
    logbuf_clear(rep);    
    return 1;
}

/*******************************************************************************
 * @brief		ɾ��ָ��������������
 * @param[in]   count - ���Ƴ������������
 * @return 	    0 - ִ��ʧ��, ��0 - ִ�гɹ�(������ʵ��ɾ������)
 ******************************************************************************/
int report_item_delete(report_manager_t *rep, int count)
{
    int i;
    item_header_t hdr;
    FW_ASSERT(rep != NULL);
    if (logbuf_len(rep) == 0)                        /*�޿��õ����� ------*/
    {
        rep->total_items = 0;                            /*���������� --------*/
        return 0;                                        
    }
    report_begin_update(rep);                            /*��ʼ��ȡ ----------*/
    for (i = 0; i < count; i++)
    {
        if (!find_item_header(rep, &hdr))
        {
            FW_DEBUG("Find item header fail. %d,\r\n",__LINE__);
            break;
        }            
        logbuf_remove(rep, sizeof(item_header_t) + hdr.len);   
        if (rep->total_items)rep->total_items--;          /*�������1 --------*/                                   
    }  
    report_end_update(rep);                               /*������ȡ ---------*/
    return i;
}


