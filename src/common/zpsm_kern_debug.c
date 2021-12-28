/**
 * Name: zpsm_kern_debug.c
 * Description: Logs messages in kernel
 * Author: 
 * Copyright:
 */

#include <linux/kernel.h>
#include "zpsm_kern_debug.h"

/**
* Writes to kernel log buffer
*/
void zpsmlog(char *format, ...)
{
    va_list ap;
    static char buffer[1024] = "";
    static char log_buf[1024];
    char *msg = NULL;

    va_start(ap, format);
	msg = buffer;
    vsprintf(msg, format, ap);
    va_end(ap);

    sprintf(log_buf, "ZPSM LOG::%s", msg);
    printk(KERN_INFO "%s", log_buf);
}





