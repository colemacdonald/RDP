//helper.c

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <dirent.h>
#include <netinet/in.h>
#include <unistd.h> /* for close() for socket */ 
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <stdio.h>

#include "helper.h"

#define TRUE 1
#define FALSE 0
#define BUFFER_SIZE 1024

void getTimeString(char * buffer)
{
	time_t rawtime;
	struct tm * timeinfo;

	time (&rawtime);
	timeinfo = localtime (&rawtime);

	strftime (buffer, 80, "%b %d %T", timeinfo);
}

void strToUpper(char * str)
{
	char * s = str;
	while(*s)
	{
		*s = toupper((unsigned char) *s);
		s++;
	}
}

void strTrimInto(char * dst, char * src)
{
	int len = strcspn(src, "\r\n");

	strncpy(dst, src, len);

	dst[len] = '\0';
}

int checkRequestMethod(char * method)
{
	//printf("meth: %s\n", method);
	strToUpper(method);
	if(strcmp(method, "GET") != 0)
	{
		//printf("bad method: %s\n", method);
		return FALSE;
	}
	return TRUE;
}

int checkURI(char * filepath)
{
	if(filepath[0] != '/')
	{
		//printf("bad uri: %s\n", filepath);
		return FALSE;
	}
	return TRUE;
}

int fileExists2(char * filepath, char * directory)
{
	char tmpFile[BUFFER_SIZE];
	char tmpDir[BUFFER_SIZE];

	realpath(filepath, tmpFile);
	realpath(directory, tmpDir);

	if(strncmp(tmpFile, tmpDir, strlen(tmpDir)) != 0)
	{
		return FALSE;
	}

	//printf("file: %s - dir: %s\n", tmpFile, tmpDir);

	struct stat buf;
	int status = stat(filepath, &buf);

	if(status != 0)
	{
		return FALSE;
	}
	return TRUE;
}

int fileExists(char * filepath)
{
	char tmpFile[BUFFER_SIZE];

	realpath(filepath, tmpFile);

	//printf("file: %s - dir: %s\n", tmpFile, tmpDir);

	struct stat buf;
	int status = stat(filepath, &buf);

	if(status != 0)
	{
		return FALSE;
	}
	return TRUE;
}

int checkHTTPVersion(char * version)
{
	strToUpper(version);

	if(strncmp(version, "HTTP/1.0\r\n\r\n", 12) != 0)
	{
		//printf("bad version: %s\n", version);
		return FALSE;
	}
	return TRUE;
}

void parse_request(char * request_string, char ** buffer)
{
	const char s[2] = " ";

	char * token;

	token = strtok(request_string, s);

	int i = 0;

	while(token != NULL && i < 3)//buffer only has 3 spots
	{
		buffer[i] = token;
		token = strtok(NULL, s);
		i++;
	}
}

int parse_packet(char * recv, char ** buffer)
{
	const char s[2] = " ";
	char * token = strtok(recv, s);

	int i = 0;
	while(token != NULL && i < 6)
	{
		buffer[i] = token;
		printf("%s\n", token);
		token = strtok(NULL, s);
		i++;
	}
	printf("end while\n");
	if(token != NULL || i != 5)
		return FALSE;
	else
		return TRUE;
}

int directoryExists(char * directory)
{
	DIR* dir = opendir(directory);
	if( dir )
	{
		//directory does exist
		return TRUE;
	} 
	else if (ENOENT == errno)
	{
		//directory does not exist
		printf("Directory '%s' does not exist.\n", directory);
		return FALSE;
	}
	else
	{
		//failed for an unknown reason....
		printf("Checking directory failed for unknown reason.\n");
		return FALSE;
	}
}

void printLogString(char * request, char * response, struct sockaddr_in sa, char * file)
{
	//gather time string
	char timestring [80];
	getTimeString(timestring);

	char requestTrimmed[1024];

	strTrimInto(requestTrimmed, request);

	printf("%s %s:%hu %s; %s; %s\n",timestring, inet_ntoa(sa.sin_addr), sa.sin_port, requestTrimmed, response, file);
}

int typeToState(char * recv)
{
	/**
	* recv ex: "CSC361 _type _seq _ackno _length _size\r\n\r\n"
	*/
	if(strcmp(recv, TYPES.data) == 0)
	{
		return 1;
	} else if(strcmp(recv, TYPES.acknowledgement) == 0)
	{
		return 2;
	} else if(strcmp(recv, TYPES.sync) == 0)
	{
		return 3;
	} else if(strcmp(recv, TYPES.finish) == 0)
	{
		return 4;
	} else if (strcmp(recv, TYPES.reset) == 0)
	{
		return 5;
	} else 
	{
		return 0;
	}
}