/******************************************************************************
* @file     nvram.h
* @brief    ����ʧ�����ݴ洢����
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

/*�ǽ���ʧ�����ݴ洢����ṹ -------------------------------------------------*/
typedef struct
{
    unsigned int base;	                  /*��ʼ��ַ(��������)----------------*/
    unsigned int size;                    /*���滺������С(�������������� -)--*/
    unsigned short block_len;             /*������洢�鳤�� -----------------*/
    unsigned short windex;	              /*��д������ַ ---------------------*/
}nvram_t;

int nvram_init(nvram_t *nvram, unsigned int base, unsigned int size, unsigned short max_item_size);
int nvram_write(nvram_t *nvram, void *item, int size);
int nvram_read(nvram_t *nvram,  void *item, int size);


#endif

