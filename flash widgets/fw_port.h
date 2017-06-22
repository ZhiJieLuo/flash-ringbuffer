/*******************************************************************************
* @file		fw_port.h
* @brief	flash 部件外部移植接口声明
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

/*报告缓冲区管理接口声明 -----------------------------------------------------*/
typedef struct 
{
	int (*write)(unsigned int addr, void *buf, unsigned int len);   /*flash 读*/
	int (*read) (unsigned int addr, void *buf, unsigned int len);   /*flash 写*/
	int (*erase)(unsigned int addr);                                /*扇区擦除*/
	unsigned int (*sector_size)(void);                              /*获取扇区大小*/
    unsigned short (*checksum)(void *buf, unsigned int len);        /*校验接口*/
    void *(*malloc)(unsigned int nbytes);                           /*内存分配*/
    void (*free)(void *p);                                          /*内存释放*/    
}fw_port_t;

/*报告缓冲区管理接口定义 -----------------------------------------------------*/
extern const fw_port_t fw_port;

#endif
