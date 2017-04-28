/*******************************************************************************
	Copyright (C) 2017 by Andrew Chang <laplacezhang@126.com>
	Licensed under the LGPL v2.1, see the file COPYING in base directory.
	
	File name: 	test_server.c
	
	Description: 	
	    This is the test source for AMCEpoll. If you want to use AMCEpoll itself
	only, please link .a or .so file.
			
	History:
		2017-04-08: File created as "test.c"

	------------------------------------------------------------------------

	    This library is free software; you can redistribute it and/or modify it 
	under the terms of the GNU Lesser General Public License as published by the 
	Free Software Foundation, version 2.1 of the License. 
	    This library is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
	details. 
	    You should have received a copy of the GNU Lesser General Public License 
	along with this library; if not, see <http://www.gnu.org/licenses/>.
		
********************************************************************************/

#include "AMCEpoll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define _CFG_SRV_PORT		8000
#define _LOG(fmt, args...)		printf("[Srv - %04d] "fmt"\n", __LINE__, ##args)


/********/
#define __SIGNAL_OPERATIONS
#ifdef __SIGNAL_OPERATIONS

enum {
	PIPE_READ = 0,
	PIPE_WRITE = 1,
};

static int g_signal_pipe[2] = {-1, -1};

/* ------------------------------------------- */
static void _sigquit_handler(int signum)
{
	if (g_signal_pipe[PIPE_WRITE] > 0) {
		write(g_signal_pipe[PIPE_WRITE], &signum, sizeof(signum));
	}
	return;
}


/* ------------------------------------------- */
static void _callback_signal_read(struct AMCEpollEvent *theEvent, int fd, events_t events, void *arg)
{
	int signum = 0;
	ssize_t callStat = 0;
	if (events & EP_EVENT_READ)
	{
		callStat = AMCFd_Read(fd, &signum, sizeof(signum));
		if (callStat < 0) {
			_LOG("Failed to read data: %s", strerror(errno));
		}
		else if (0 == callStat) {
			//_LOG("Fin %d", signum);
		}
		else if (sizeof(signum) == callStat) {
			_LOG("Got event: %s", strsignal(signum));
			_callback_signal_read(theEvent, fd, events, arg);
		}
		else {
			_LOG("Illegal return status: %d", callStat);
		}
	}
	else {
		_LOG("Unsupported event: 0x%x", events);
	}

	return;
}


/* ------------------------------------------- */
static int _create_signal_handler(struct AMCEpoll *base)
{
	struct AMCEpollEvent *signalEvent = NULL;
	int callStat = 0;
	callStat = pipe(g_signal_pipe);
	if (callStat < 0) {
		_LOG("Failed in pipe(): %s", strerror(errno));
		return -1;
	}

	callStat = AMCFd_MakeCloseOnExec(g_signal_pipe[PIPE_READ]);
	if (callStat < 0) {
		return -1;
	}

	callStat = AMCFd_MakeCloseOnExec(g_signal_pipe[PIPE_WRITE]);
	if (callStat < 0) {
		return -1;
	}

	callStat = AMCFd_MakeNonBlock(g_signal_pipe[PIPE_READ]);
	if (callStat < 0) {
		return -1;
	}

	callStat = AMCFd_MakeNonBlock(g_signal_pipe[PIPE_WRITE]);
	if (callStat < 0) {
		return -1;
	}

	signal(SIGQUIT, _sigquit_handler);

	signalEvent = AMCEpoll_NewFileEvent(g_signal_pipe[PIPE_READ], 
								EP_EVENT_READ | EP_MODE_PERSIST | EP_MODE_EDGE | EP_EVENT_FREE, 
								0, _callback_signal_read, base);

	if (NULL == signalEvent) {
		return -1;
	}

	callStat = AMCEpoll_AddEvent(base, signalEvent);
	if (callStat < 0) {
		AMCEpoll_FreeEvent(signalEvent);
		return -1;
	}

	return 0;
}




#endif



/********/
#define __DEBUG_FUNCTIONS
#ifdef __DEBUG_FUNCTIONS

/* ------------------------------------------- */
static void _general_test()
{
	int tmp;
	for (tmp = 0; tmp < 256; tmp++)
	{
		_LOG("[%03d] %s", tmp, strsignal(tmp));
	}
	return;
}

/* ------------------------------------------- */
static char _char_from_byte(uint8_t byte)
{
	if ((byte >= '!') && (byte <= 0x7F))
	{
		return (char)byte;
	}
	else if ('\n' == byte)
	{
		return '.';
	}
	else if ('\r' == byte)
	{
		return '.';
	}
	else if (' ' == byte)
	{
		return ' ';
	}
	else
	{
		return '.';
	}
}


/* ------------------------------------------- */
void _print_data(const void *pData, const size_t size)
{
	size_t column, tmp;
	char lineString[64] = "";
	char linechar[24] = "";
	size_t lineLen = 0;
	uint8_t byte;
	const uint8_t *data = pData;

	printf ("---------------------------------------------------------------------------\n");
	printf ("Base: 0x%08lx, length %d(0x%04x)\n", (unsigned long)(data), size, size);
	printf ("----  +0 +1 +2 +3 +4 +5 +6 +7  +8 +9 +A +B +C +D +E +F    01234567 89ABCDEF\n");
//	printf ("---------------------------------------------------------------------------\n");
	
	for (tmp = 0; 
		(tmp + 16) <= size; 
		tmp += 16)
	{
		memset(lineString, 0, sizeof(lineString));
		memset(linechar, 0, sizeof(linechar));
	
		for (column = 0, lineLen = 0;
			column < 16;
			column ++)
		{
			byte = data[tmp + column];
			sprintf(lineString + lineLen, "%02X ", byte & 0xFF);
			
			lineLen += 3;

			if (column < 7)
			{
				linechar[column] = _char_from_byte(byte);
			}
			else if (7 == column)
			{
				linechar[column] = _char_from_byte(byte);
				linechar[column+1] = ' ';
				sprintf(lineString + lineLen, " ");
				lineLen += 1;
			}
			else
			{
				linechar[column+1] = _char_from_byte(byte);
			}
		}

		printf ("%04X: %s   %s\n", tmp, lineString, linechar);
	}

	/* last line */
	if (tmp < size)
	{
		memset(lineString, 0, sizeof(lineString));
		memset(linechar, 0, sizeof(linechar));
	
		for (column = 0, lineLen = 0;
			column < (size - tmp);
			column ++)
		{
			byte = data[tmp + column];
			sprintf(lineString + lineLen, "%02X ", byte & 0xFF);
			lineLen += 3;

			if (column < 7)
			{
				linechar[column] = _char_from_byte(byte);
			}
			else if (7 == column)
			{
				linechar[column] = _char_from_byte(byte);
				linechar[column+1] = ' ';
				sprintf(lineString + lineLen, " ");
				lineLen += 1;
			}
			else
			{
				linechar[column+1] = _char_from_byte(byte);
			}
		}
#if 1
		for (/* null */;
			column < 16;
			column ++)
		{
			sprintf(lineString + lineLen, "   ");
			lineLen += 3;
		
			if (7 == column)
			{
				sprintf(lineString + lineLen, " ");
				lineLen += 1;
			}
		}
#endif
		printf ("%04X: %s   %s\n", tmp, lineString, linechar);
	}
	
	printf ("---------------------------------------------------------------------------\n");
	
	/* ends */
}


#endif

/********/
#define __EVENT_CALLBACKS
#ifdef __EVENT_CALLBACKS

static const char g_fmtHttpHeader[] = ""
		"HTTP/1.0 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Content-Language: en\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Server: AMCEpoll/1.0.0beta\r\n"
		"Date: %a, %d %b %Y %H:%M:%S %Z\r\n"
		"Connection: keep-alive\r\n"
	"";


static const char g_respData[] = ""
		"<!DOCTYPE html>"
		"<html>"
			"<head>"
				"<title>Hello, epoll!</title>"
			"</head>"
			"<body>"
			"</body>"
		"</html>"
		"\r\n\r\n"
	"";


static ssize_t _write_http_data(int fd)
{
	char buff[2048] = "";
	struct tm tm;
	time_t t;

	t = time(NULL);
	gmtime_r(&t, &tm);

	strftime(buff, sizeof(buff), g_fmtHttpHeader, &tm);
	strcat(buff, g_respData);
	return write(fd, buff, strlen(buff));
}


/* ------------------------------------------- */
static void _callback_read(struct AMCEpollEvent *theEvent, int fd, events_t events, void *arg)
{
	struct AMCEpoll *base = (struct AMCEpoll *)arg;

	if (events & EP_EVENT_FREE) {
		_LOG("Close fd %d", fd);
		close(fd);
		fd = -1;
	}
	else if (events & EP_EVENT_ERROR) {
		_LOG("Error on fd %d", fd);
		AMCEpoll_DelAndFreeEvent(base, theEvent);
		fd = -1;
	}
	else if (events & EP_EVENT_READ) {
		char buff[2048];
		ssize_t readLen = 0;

		readLen = AMCFd_Read(fd, buff, sizeof(buff));
		if (0 == readLen) {
			_LOG("EOF on fd %d", fd);
			AMCEpoll_DelAndFreeEvent(base, theEvent);
			fd = -1;
		}
		else if (readLen < 0) {
			_LOG("Failed in reading data: %s", strerror(errno));
			AMCEpoll_DelAndFreeEvent(base, theEvent);
			fd = -1;
		}
		else {
			ssize_t callStat = 0;
			ssize_t writeLen = 0;

			_LOG("Read data:");
			_print_data(buff, readLen);

			callStat = _write_http_data(fd);
			if (callStat > 0) {
				writeLen += callStat;
			}

			_LOG("Written %d bytes", callStat);
			AMCEpoll_DelAndFreeEvent(base, theEvent);
		}
	}

	return;
}


/* ------------------------------------------- */
static void _callback_accept(struct AMCEpollEvent *theEvent, int fd, events_t events, void *arg)
{
	struct AMCEpoll *base = (struct AMCEpoll *)arg;

	if (events & EP_EVENT_FREE) {
		_LOG("Close fd %d", fd);
		close(fd);
		fd = -1;
	}
	else if (events & EP_EVENT_ERROR) {
		_LOG("Error on fd %d", fd);
		AMCEpoll_DelAndFreeEvent(base, theEvent);
		fd = -1;
	}
	else if (events & EP_EVENT_READ) {
		BOOL isOK =TRUE;
		int newFd = -1;
		int callStat = 0;
		struct sockaddr_in sockAddr;
		socklen_t addrLen = sizeof(sockAddr);

		newFd = accept(fd, (struct sockaddr *)(&sockAddr), &addrLen);
		if (newFd < 0) {
			_LOG("Failed to accept: %s", strerror(errno));
			isOK = FALSE;
		}
		else if (fd > FD_SETSIZE) {
			_LOG("fd > FD_SETSIZE");
			isOK = FALSE;
		}

		if (isOK) {
			callStat = AMCFd_MakeCloseOnExec(newFd);
			if (callStat < 0) {
				isOK = FALSE;
			}
		}

		if (isOK) {
			callStat = AMCFd_MakeNonBlock(newFd);
			if (callStat < 0) {
				isOK = FALSE;
			}
		}

		if (isOK) {
			struct AMCEpollEvent *newEvent= AMCEpoll_NewFileEvent(newFd, 
										EP_EVENT_READ | EP_EVENT_ERROR | EP_EVENT_FREE | EP_MODE_PERSIST, 
										0, _callback_read, base);
			if (NULL == newEvent) {
				_LOG("Failed to create a new event: %s", strerror(errno));
			}

			callStat = AMCEpoll_AddEvent(base, newEvent);
			if (callStat < 0) {
				_LOG("Failed to add read event");
				AMCEpoll_FreeEvent(newEvent);
				isOK = FALSE;
			}
		}

		if (FALSE == isOK) {
			if (newFd > 0) {
				close(newFd);
				newFd = -1;
			}
		}
	}
	return;
}

#endif

/********/
#define __SERVER_PREPARATION
#ifdef __SERVER_PREPARATION

/* ------------------------------------------- */
int _create_local_server(struct AMCEpoll *base)
{
	int retCode = -1;
	int callStat = 0;
	struct sockaddr_in address;
	struct AMCEpollEvent *acceptEvent = NULL;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		goto ERROR;
	}

	callStat = AMCFd_MakeNonBlock(fd);
	if (callStat < 0) {
		goto ERROR;
	}

	callStat = AMCFd_MakeCloseOnExec(fd);
	if (callStat < 0) {
		goto ERROR;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(_CFG_SRV_PORT);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	callStat = bind(fd, (struct sockaddr *)&address, sizeof(struct sockaddr_in));
	if (callStat < 0) {
		int err = errno;
		_LOG("Failed to bind for fd %d: %s", fd, strerror(err));
		errno = err;
		goto ERROR;
	}
	else {
		_LOG("Server binded at Port: %d", ntohs(address.sin_port));
	}

	callStat = listen(fd, 65565);
	if (callStat < 0) {
		int err = errno;
		_LOG("Failed to listen for fd %d: %s", fd, strerror(err));
		errno = err;
		goto ERROR;
	}

	acceptEvent = AMCEpoll_NewFileEvent(fd, 
					EP_MODE_PERSIST | EP_MODE_EDGE | EP_EVENT_READ | EP_EVENT_ERROR | EP_EVENT_FREE | EP_EVENT_TIMEOUT, 
					0, _callback_accept, base);
	if (NULL == acceptEvent) {
		_LOG("Failed to create event: %s", strerror(errno));
		goto ERROR;
	}

	callStat = AMCEpoll_AddEvent(base, acceptEvent);
	if (callStat < 0) {
		_LOG("Failed to add event: %s", strerror(errno));
		goto ERROR;
	}

	return 0;
ERROR:
	if (acceptEvent) {
		AMCEpoll_FreeEvent(acceptEvent);
		acceptEvent = NULL;
	}
	if (fd > 0) {
		close(fd);
		fd = -1;
	}
	return retCode;
}


#endif


/********/
#define __MAIN_LOOP
#ifdef __MAIN_LOOP

/* ------------------------------------------- */
int main(int argc, char* argv[])
{
	int callStat = 0;
	struct AMCEpoll *base = NULL;
	_LOG("Hello, AMCEpoll!");

	_general_test();
	
	base = AMCEpoll_New(1024);
	if (NULL == base) {
		goto END;
	}

	callStat = _create_signal_handler(base);
	if (callStat < 0) {
		goto END;
	}

	callStat = _create_local_server(base);
	if (callStat < 0) {
		goto END;
	}

	callStat = AMCEpoll_Dispatch(base);
	if (callStat < 0) {
		goto END;
	}
END:
	if (base) {
		AMCEpoll_Free(base);
		base = NULL;
	}
	_LOG("Goodbye, AMCEpoll!");
	return 0;
}

#endif

