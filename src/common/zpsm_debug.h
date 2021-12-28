/**
 * Name: zpsm_debug.h
 * Description: Logs messages in client application in a log file
 * Author: 
 * Copyright:
 */

#ifndef _ZPSM_KERNEL_DEBUG_H
#define _ZPSM_KERNEL_DEBUG_H

/**
* Macro to log messages in application log file-Use the macro and not the function
* @param format list with format specifiers
*/
#define ZPSM_LOG(args...) zpsmlog(args)

/**
* Writes to application log buffer
*/
void zpsmlog(char *format, ...);

/**
* Writes to application log buffer
*/
void writeToLog(char* buffer);

#endif //_ZPSM_KERNEL_DEBUG_H
