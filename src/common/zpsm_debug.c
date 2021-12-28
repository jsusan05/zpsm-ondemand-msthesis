/**
 * Name: zpsm_debug.c
 * Description: Logs messages in client application in a log file
 * Author: 
 * Copyright:
 */
//SYSTEM INCLUDES
#include <fcntl.h> 
#include <stdio.h> 
#include <string.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <time.h> 
#include <unistd.h> 
#include <stdarg.h>

//USER INCLUDES
#include "zpsm_debug.h"

#define USER_LOG_PATH 	"../../log/zpsm_client.log"

//GLOBAL VARIABLES
//static FILE* fd = NULL;
static pthread_mutex_t log_mutex;
static int isInit = 0;

static long int get_timestamp() 
{
    struct timeval  tv;
    gettimeofday(&tv, NULL);
	return tv.tv_usec;
}

/*
* Write the buffer into file specified
* @param buffer Buffer
*/
void writeToLog(char* buffer)
{
	if(!isInit)
	{
	    pthread_mutex_init(&log_mutex, NULL);
	    isInit = 1;
	}
	
	pthread_mutex_lock (&log_mutex);

	FILE*	fd = fopen(USER_LOG_PATH, "a+");
	if(fd)	
		fprintf(fd, "%s", buffer);		
	fclose(fd);
	fd = NULL;
	pthread_mutex_unlock (&log_mutex);
}

/**
* Write logs to a file using a format list
*/
void zpsmlog(char *format, ...)
{
    va_list ap;
    static char buffer[256] = "";
    static char log_buf[1024];
    char *msg;

    va_start(ap, format);
	msg = buffer;
    vsprintf(msg, format, ap);
    va_end(ap);

    sprintf(log_buf, "%ld:%s\n", get_timestamp(), msg);
	writeToLog(log_buf);
}


