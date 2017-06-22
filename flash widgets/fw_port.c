/*******************************************************************************
* @file		fw_port.c
* @brief	flash 部件外部移植接口声明
* @version	1.2
* @date		2017-06-16
* @author	roger.luo
* 
* Copyright(C) 2017
* All rights reserved.
*
*******************************************************************************/
#include "flash.h"
#include "fw_port.h"
#include "Quec_Memory.h"

/*******************************************************************************
 * @brief       (写接口)写入指定长度的数据到指定地址
 * @param[in]   addr  - 物理地址
 * @param[in]   buf   - 数据缓冲区
 * @param[in]   len   - 缓冲区长度
 * @return      
 ******************************************************************************/
static int write(unsigned int addr, void *buf, unsigned int len)
{
    return drv_flash.write(addr, (unsigned char *)buf, len);
}

/*******************************************************************************
 * @brief       (读接口)读取指定长度的数据
 * @param[in]   addr  - 物理地址
 * @param[out]  buf   - 数据缓冲区
 * @param[in]   len   - 缓冲区长度
 * @return      
 ******************************************************************************/
static int read (unsigned int addr, void *buf, unsigned int len)
{
    return drv_flash.read(addr, (unsigned char *)buf, len);
}

/*******************************************************************************
 * @brief       (扇区擦除接口)扇区擦除
 * @param[in]   addr 
 * @return      
 ******************************************************************************/
static int erase(unsigned int addr)
{
	return drv_flash.sector_erase(addr);
    

}

//crc16-CCITT 半查找表
static unsigned short crc_table[16]=
{                                                                                       
	0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7, 
	0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
};     

/*******************************************************************************
 * @brief       crc 16校验接口
 * @param[in]   buf - 数据缓冲区
 * @param[in]   len - 数据长度
 * @return      2字节的校验码
 ******************************************************************************/
static unsigned short crc16(void *buf, unsigned int len)
{
    unsigned char *p = (unsigned char *)buf;
	unsigned short crc = 0xffff;	                                                               
	unsigned char tmp;                                                                    
	while(len--)                                                                           
	{                                                                                      
		tmp = crc >> 12;              /*取出高4位 ---------------------------*/              
		crc <<=4;                      /*取出CRC的低12位) --------------------*/              
		/*CRC的高4位和本字节的前半字节相加后查表计算CRC，然后加上上一次CRC的余数*/            
		crc^=crc_table[tmp^(*p >> 4)];                                                     
		tmp = crc >> 12;                                                                     
		crc <<= 4;                                                                            
		crc^=crc_table[tmp^(*p & 0x0f)];                                                   
		p++;                                                                                
	}                                                                                      
	return crc; 
}

/*******************************************************************************
 * @brief       内存分配
 * @param[in]   nbytes 
 * @return      
 ******************************************************************************/
static void *fw_malloc(unsigned int nbytes)
{
    return Quec_malloc(__FUNCTION__, nbytes);
}

/*******************************************************************************
 * @brief       内存释放
 * @param[in]   addr  - 物理地址
 * @return      
 ******************************************************************************/
static void fw_free(void *p)
{
    Quec_free(__FUNCTION__,p);
}
//获取扇区大小
static unsigned int get_sector_size(void)
{
    return SECTOR_SIZE;
}

/*外部接口定义 ---------------------------------------------------------------*/
const fw_port_t fw_port = 
{
    write,
    read,
    erase,
    get_sector_size,
    crc16,
    fw_malloc,
    fw_free
};


