/*******************************************************************************
* @file		fw_port.h
* @brief	flash �����ⲿ��ֲ�ӿ�����
* @version	1.2
* @date		2017-06-16
* @author	roger.luo
* 
* Copyright(C) 2017
* All rights reserved.
*
*******************************************************************************/
#ifndef _FW_PORT_H_
#define _FW_PORT_H_

/*���滺��������ӿ����� -----------------------------------------------------*/
typedef struct 
{
	int (*write)(unsigned int addr, void *buf, unsigned int len);   /*flash ��*/
	int (*read) (unsigned int addr, void *buf, unsigned int len);   /*flash д*/
	int (*erase)(unsigned int addr);                                /*��������*/
	unsigned int (*sector_size)(void);                              /*��ȡ������С*/
    unsigned short (*checksum)(void *buf, unsigned int len);        /*У��ӿ�*/
    void *(*malloc)(unsigned int nbytes);                           /*�ڴ����*/
    void (*free)(void *p);                                          /*�ڴ��ͷ�*/    
}fw_port_t;

/*���滺��������ӿڶ��� -----------------------------------------------------*/
extern const fw_port_t fw_port;

#endif
