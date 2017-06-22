/******************************************************************************
* @file     logbuf.h
* @brief    ��־����������ӿ�����
* @version  1.0
* @date     2016-12-21
* @author   roger.luo
* 
* ���λ�������������ֲ�:
********************************************************************************
*                                                         *                    *
*<-----------     ���λ���������    --------------------->*<------Ŀ¼-------->*
*                                                         *                    *
*<---------------------------------�ܳ���(size)------------------------------->*
********************************************************************************
* Copyright(C) 2016
* All rights reserved.
*
*******************************************************************************/

#ifndef _LOG_BUF_H_
#define _LOG_BUF_H_


/*���滺����������Ϣ ---------------------------------------------------------*/
typedef struct{
    unsigned int base;	                  /*��ʼ��ַ(��������)----------------*/
    unsigned int size;                    /*���滺������С(�������������� -)--*/
    unsigned int head;                    /*ͷָ�� ---------------------------*/
    unsigned int tail;                    /*βָ�� ---------------------------*/
    
    unsigned short windex;	              /*Ŀ¼��д������ַ -----------------*/
    unsigned short directory_size;        /*Ŀ¼���������Ĵ�С ---------------*/
    unsigned short total_items;           /*��д�����������(ע:��ֵ�����ο�)-*/
    unsigned char  begin_update;          /*������¼���,�ο�report_begin_update-*/
}logbuf_t;

/**
  * @brief  ��ʼ�����滺�������Ϣ
  * @param[in] rep - ���������Ϣ��Ӧ�ó������,���еĻ���������ͨ��logbuf_t����
  * @param[in] base- logbuf_t�����flash����ַ(��������)
  * @param[in] size- ����logbuf_t����ĳ���(�������������� -)
  * @return      0 - ��ʼ��ʧ�� ��0 - ��ʼ���ɹ�
  */
unsigned int logbuf_init(logbuf_t *log, unsigned int base, unsigned int size);

/**
  * @brief  ��ָ�����ȵı���д�뻺������
  * @param[in]  rep  :���滺�������Ϣ   
  * @param[in]  buf :ָ�򱨸����ݵ�ָ�� 
  * @param[in]  len :���泤��
  * @retval ���д��ɹ��򷵻ص�ǰ���泤��(len),�����滺������������
  *         ��ʱ�򷵻�0
  */
unsigned int logbuf_put(logbuf_t  *log, void *buf, unsigned int len);

/**
  * @brief      ��ȡָ�����ȵı���
  * @param[in]  rep  :���滺�������Ϣ   
  * @param[out] buf :ָ�򱨸����ݵ�ָ��    
  * @param[in]  len :����������
  * @retval     �����ȡ�ɹ�,�򷵻�ָ����ȡ�ĳ���(len)
  * @attention  ִ�д˲�����,ԭ�ȱ��潫���Ƴ�
  */
unsigned int logbuf_get(logbuf_t *log, void *buf, unsigned int len);

/**
  * @brief      ��ȡָ�����ȵı���,�����Ƴ��ñ���
  * @param[in]  rep  :���滺�������Ϣ   
  * @param[out] buf :ָ�򱨸����ݵ�ָ��    
  * @param[in]  len :����������
  * @retval     �����ȡ�ɹ�,�򷵻�ָ����ȡ�ĳ���(len)
  * @attention  ִ�д˲��������Ὣ���潫���Ƴ�
  */
unsigned int logbuf_peek(logbuf_t *log, void *buf, unsigned int len);


/**
  * @brief      �������(��src�еı����ƶ���des��,��ɾ��srcͬ�ı���)
  * @param[in]  src -  Դ��������Ϣ 
  * @param[in]  des -  Ŀ�껺������Ϣ
  * @param[in]  len -  �������ݳ���
  * @retval     0 - ִ��ʧ��, ��0 ִ��ʧ��
  * @attention  ���뱣֤des���пռ䳤�ȴ���src��Ч���ݳ���,����ִ��ʧ��
  */
int logbuf_move(logbuf_t *des, logbuf_t *src, unsigned int len);

/**
  * @brief      �Ƴ�ָ�����ȵı���
  * @param[in]  rep - Դ��������Ϣ 
  * @param[in]  len - ���Ƴ������ݳ���
  * @retval     0 - ִ��ʧ��, ��0ִ�гɹ�
  */
unsigned int logbuf_remove(logbuf_t *log, unsigned int len);

/*******************************************************************************
 * @brief       �ָ���ǰ���Ƴ�ָ�����ȵĻ���������
 * @param[in]   report              - ���滺����������Ϣ
 * @param[in]   len                 - Ԥ�ָ������ݳ���
 * @return      ʵ�ʻָ�����
 * @attention   ���뱣֤��logbuf_remove֮�����ʹ��,�һָ��ĳ��Ȳ�������ǰ��
 *              ���ĳ���.
 ******************************************************************************/
unsigned int logbuf_resume(logbuf_t *log, unsigned int len);

/**
  * @brief      ��ȡ���滺������ʣ��ռ䳤��
  * @param[in]  rep - Դ��������Ϣ 
  * @retval     ʣ��ռ䳤��
  */
unsigned int logbuf_space_left(logbuf_t *log);

/**
  * @brief      ��ȡ���滺������Ч���ݳ���
  * @param[in]  rep  :���滺�������Ϣ   
  * @retval     ���ص�ǰ����ĳ���(len)
  */
unsigned int logbuf_len(logbuf_t  *log);


/**
  * @brief      ��ձ���
  * @param[in]  rep  :���滺�������Ϣ   
  * @retval     none
  */
void logbuf_clear(logbuf_t  *log);

/**
  * @brief      ��ȡ���滺������Ч��������,ÿ��ִ��logbuf_put
  *             ʱ,������������1,��ִ��logbuf_getʱ,����1
  * @param[in]  rep  :���滺�������Ϣ   
  * @retval     ��������
  */
unsigned int logbuf_count(logbuf_t  *log);


/**
  * @brief      ��ʼ����/д�뱨��(ʵ���ǹ���Ŀ¼���²���,
  *             ����ڶ�ʱ������Ҫ���ִ��logbuf_put����,
  *             Ϊ�˱���Ƶ������Ŀ¼�����ϵͳЧ��,������
  *             ִ��report_begin_update����,������ִ��������
  *             ��logbuf_put����֮��,����report_end_update
  *             �����Իָ�Ŀ¼ʵʱ���²���)
  * @param[in]  rep  :���滺�������Ϣ   
  * @retval     none
  */
void report_begin_update(logbuf_t  *log);


/**
  * @brief      ��������/д��/����(��report_begin_update���ʹ��)
  * @param[in]  rep  :���滺�������Ϣ   
  * @retval     none
  */
void report_end_update(logbuf_t  *log);



#endif
