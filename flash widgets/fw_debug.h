/******************************************************************************
* @file     fw_debug.h
* @brief    µ÷ÊÔ½Ó¿Ú
* @version  1.0
* @date     2016-12-22
* @author   roger.luo
* 
* Copyright(C) 2016
* All rights reserved.
*
*******************************************************************************/
#ifndef _FW_DEBUG_H_
#define _FW_DEBUG_H_


#define USE_FW_DEBUG

#ifdef  USE_FW_DEBUG
    #include "cmd_log.h"
    #include "tty.h"
    #include "clib.h"
    #define FW_DEBUG(...) 		if (quec_get_log_mode(MOD_FLASH))tty.printf(__VA_ARGS__)
    #define FW_ASSERT(expr) ((expr) ? (void)0 : fw_assert_failed(__FILE__, __LINE__))
    void fw_assert_failed(const char *file, int line);
#else
  #define FW_ASSERT(expr) ((void)0)
  #define FW_DEBUG(...)   ((void)0)
#endif

#endif

