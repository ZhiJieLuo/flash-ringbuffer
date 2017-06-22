/*******************************************************************************
* @file		fw_port.c
* @brief	flash �����ⲿ��ֲ�ӿ�����
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
 * @brief       (д�ӿ�)д��ָ�����ȵ����ݵ�ָ����ַ
 * @param[in]   addr  - �����ַ
 * @param[in]   buf   - ���ݻ�����
 * @param[in]   len   - ����������
 * @return      
 ******************************************************************************/
static int write(unsigned int addr, void *buf, unsigned int len)
{
    return drv_flash.write(addr, (unsigned char *)buf, len);
}

/*******************************************************************************
 * @brief       (���ӿ�)��ȡָ�����ȵ�����
 * @param[in]   addr  - �����ַ
 * @param[out]  buf   - ���ݻ�����
 * @param[in]   len   - ����������
 * @return      
 ******************************************************************************/
static int read (unsigned int addr, void *buf, unsigned int len)
{
    return drv_flash.read(addr, (unsigned char *)buf, len);
}

/*******************************************************************************
 * @brief       (���������ӿ�)��������
 * @param[in]   addr 
 * @return      
 ******************************************************************************/
static int erase(unsigned int addr)
{
	return drv_flash.sector_erase(addr);
    

}

//crc16-CCITT ����ұ�
static unsigned short crc_table[16]=
{                                                                                       
	0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7, 
	0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
};     

/*******************************************************************************
 * @brief       crc 16У��ӿ�
 * @param[in]   buf - ���ݻ�����
 * @param[in]   len - ���ݳ���
 * @return      2�ֽڵ�У����
 ******************************************************************************/
static unsigned short crc16(void *buf, unsigned int len)
{
    unsigned char *p = (unsigned char *)buf;
	unsigned short crc = 0xffff;	                                                               
	unsigned char tmp;                                                                    
	while(len--)                                                                           
	{                                                                                      
		tmp = crc >> 12;              /*ȡ����4λ ---------------------------*/              
		crc <<=4;                      /*ȡ��CRC�ĵ�12λ) --------------------*/              
		/*CRC�ĸ�4λ�ͱ��ֽڵ�ǰ���ֽ���Ӻ������CRC��Ȼ�������һ��CRC������*/            
		crc^=crc_table[tmp^(*p >> 4)];                                                     
		tmp = crc >> 12;                                                                     
		crc <<= 4;                                                                            
		crc^=crc_table[tmp^(*p & 0x0f)];                                                   
		p++;                                                                                
	}                                                                                      
	return crc; 
}

/*******************************************************************************
 * @brief       �ڴ����
 * @param[in]   nbytes 
 * @return      
 ******************************************************************************/
static void *fw_malloc(unsigned int nbytes)
{
    return Quec_malloc(__FUNCTION__, nbytes);
}

/*******************************************************************************
 * @brief       �ڴ��ͷ�
 * @param[in]   addr  - �����ַ
 * @return      
 ******************************************************************************/
static void fw_free(void *p)
{
    Quec_free(__FUNCTION__,p);
}
//��ȡ������С
static unsigned int get_sector_size(void)
{
    return SECTOR_SIZE;
}

/*�ⲿ�ӿڶ��� ---------------------------------------------------------------*/
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


