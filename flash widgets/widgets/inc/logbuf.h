/******************************************************************************
* @file     logbuf.h
* @brief    日志缓冲区管理接口声明
* @version  1.0
* @date     2016-12-21
* @author   roger.luo
* 
* 环形缓冲区管理区域分布:
********************************************************************************
*                                                         *                    *
*<-----------     环形缓冲区区域    --------------------->*<------目录-------->*
*                                                         *                    *
*<---------------------------------总长度(size)------------------------------->*
********************************************************************************
* Copyright(C) 2016
* All rights reserved.
*
*******************************************************************************/

#ifndef _LOG_BUF_H_
#define _LOG_BUF_H_


/*报告缓冲器管理信息 ---------------------------------------------------------*/
typedef struct{
    unsigned int base;	                  /*开始地址(扇区对齐)----------------*/
    unsigned int size;                    /*报告缓冲区大小(必须是扇区对齐 -)--*/
    unsigned int head;                    /*头指针 ---------------------------*/
    unsigned int tail;                    /*尾指针 ---------------------------*/
    
    unsigned short windex;	              /*目录读写索引地址 -----------------*/
    unsigned short directory_size;        /*目录索引分区的大小 ---------------*/
    unsigned short total_items;           /*已写入的总数据项(注:此值仅供参考)-*/
    unsigned char  begin_update;          /*挂起更新计数,参考report_begin_update-*/
}logbuf_t;

/**
  * @brief  初始化报告缓冲管理信息
  * @param[in] rep - 报告管理信息由应用程序给出,所有的缓冲区都是通过logbuf_t管理
  * @param[in] base- logbuf_t管理的flash基地址(扇区对齐)
  * @param[in] size- 整个logbuf_t管理的长度(必须是扇区对齐 -)
  * @return      0 - 初始化失败 非0 - 初始化成功
  */
unsigned int logbuf_init(logbuf_t *log, unsigned int base, unsigned int size);

/**
  * @brief  将指定长度的报告写入缓冲区内
  * @param[in]  rep  :报告缓冲管理信息   
  * @param[in]  buf :指向报告数据的指针 
  * @param[in]  len :报告长度
  * @retval 如果写入成功则返回当前报告长度(len),当报告缓冲区的容量不
  *         够时则返回0
  */
unsigned int logbuf_put(logbuf_t  *log, void *buf, unsigned int len);

/**
  * @brief      读取指定长度的报告
  * @param[in]  rep  :报告缓冲管理信息   
  * @param[out] buf :指向报告数据的指针    
  * @param[in]  len :缓冲区长度
  * @retval     如果读取成功,则返回指定读取的长度(len)
  * @attention  执行此操作后,原先报告将被移除
  */
unsigned int logbuf_get(logbuf_t *log, void *buf, unsigned int len);

/**
  * @brief      读取指定长度的报告,但不移除该报告
  * @param[in]  rep  :报告缓冲管理信息   
  * @param[out] buf :指向报告数据的指针    
  * @param[in]  len :缓冲区长度
  * @retval     如果读取成功,则返回指定读取的长度(len)
  * @attention  执行此操作并不会将报告将被移除
  */
unsigned int logbuf_peek(logbuf_t *log, void *buf, unsigned int len);


/**
  * @brief      报告搬移(将src中的报告移动到des中,并删除src同的报告)
  * @param[in]  src -  源缓冲区信息 
  * @param[in]  des -  目标缓冲区信息
  * @param[in]  len -  搬移数据长度
  * @retval     0 - 执行失败, 非0 执行失败
  * @attention  必须保证des空闲空间长度大于src有效数据长度,否则执行失败
  */
int logbuf_move(logbuf_t *des, logbuf_t *src, unsigned int len);

/**
  * @brief      移除指定长度的报告
  * @param[in]  rep - 源缓冲区信息 
  * @param[in]  len - 待移除的数据长度
  * @retval     0 - 执行失败, 非0执行成功
  */
unsigned int logbuf_remove(logbuf_t *log, unsigned int len);

/*******************************************************************************
 * @brief       恢复先前的移除指定长度的缓冲区数据
 * @param[in]   report              - 报告缓冲区管理信息
 * @param[in]   len                 - 预恢复的数据长度
 * @return      实际恢复长度
 * @attention   必须保证在logbuf_remove之后才能使用,且恢复的长度不大于先前移
 *              除的长度.
 ******************************************************************************/
unsigned int logbuf_resume(logbuf_t *log, unsigned int len);

/**
  * @brief      获取报告缓冲区的剩余空间长度
  * @param[in]  rep - 源缓冲区信息 
  * @retval     剩余空间长度
  */
unsigned int logbuf_space_left(logbuf_t *log);

/**
  * @brief      获取报告缓冲区有效数据长度
  * @param[in]  rep  :报告缓冲管理信息   
  * @retval     返回当前报告的长度(len)
  */
unsigned int logbuf_len(logbuf_t  *log);


/**
  * @brief      清空报告
  * @param[in]  rep  :报告缓冲管理信息   
  * @retval     none
  */
void logbuf_clear(logbuf_t  *log);

/**
  * @brief      获取报告缓冲区有效报告项数,每次执行logbuf_put
  *             时,报告项数增加1,当执行logbuf_get时,减少1
  * @param[in]  rep  :报告缓冲管理信息   
  * @retval     报告项数
  */
unsigned int logbuf_count(logbuf_t  *log);


/**
  * @brief      开始更新/写入报告(实际是挂起目录更新操作,
  *             如果在短时间内需要多次执行logbuf_put操作,
  *             为了避免频繁更新目录及提高系统效率,可以先
  *             执行report_begin_update操作,当操作执行完所有
  *             的logbuf_put操作之后,调用report_end_update
  *             操作以恢复目录实时更新操作)
  * @param[in]  rep  :报告缓冲管理信息   
  * @retval     none
  */
void report_begin_update(logbuf_t  *log);


/**
  * @brief      结束更新/写入/报告(与report_begin_update配对使用)
  * @param[in]  rep  :报告缓冲管理信息   
  * @retval     none
  */
void report_end_update(logbuf_t  *log);



#endif
