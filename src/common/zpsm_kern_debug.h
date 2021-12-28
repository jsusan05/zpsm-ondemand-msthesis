/**
 * Name: zpsm_kern_debug.h
 * Description: Logs messages in kernel
 * Author: 
 * Copyright:
 */

#ifndef _ZPSM_KERNEL_DEBUG_H
#define _ZPSM_KERNEL_DEBUG_H

/**
* Macro to log messages in kernel-Use the macro and not the function
* @param format list with format specifiers
*/
#define ZPSM_LOG(args...) zpsmlog(args)

/**
* Writes to kernel log buffer
*/
void zpsmlog(char *format, ...);

#endif //_ZPSM_KERNEL_DEBUG_H
