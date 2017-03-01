/*
sws_server.c

implements a simple web server using the UDP

Basis of file is from udp_server.c as shown in lab 2

*/
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

////////////////////////////////////////////////////////////////////////////////////////////
//										CONSTANTS
////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	char * data;
	char * acknowledgement;
	char * sync;
	char * finish;
	char * reset;
} Types;

#define TRUE 1
#define FALSE 0
#define BUFFER_SIZE 1024
#define _MAGIC_ "CSC361"


static const Types TYPES = {"DAT", "ACK", "SYN", "FIN", "RST"};



////////////////////////////////////////////////////////////////////////////////////////////
//									GLOBAL VARIABLES
////////////////////////////////////////////////////////////////////////////////////////////

int sock;
char * port;
char * directory;
struct sockaddr_in sa;

////////////////////////////////////////////////////////////////////////////////////////////
//									HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////

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

int fileExists(char * filepath, char * directory)
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

int prepareSocket()
{
	//copied from udp_server.c
	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == -1)
	{
		close(sock);
		printf("socket could not be created - please try again\n");
		return FALSE;
	}

	//printf("sock = %d\n", sock);

	//http://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

	//struct sockaddr_in sa; 
	ssize_t recsize;
	socklen_t fromlen;

	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons( atoi( port ) ); //convert to int
	fromlen = sizeof(sa);
	//end of copy

	if(bind(sock, (struct sockaddr *) &sa, sizeof sa) != 0)
	{
		printf("socket could not be bound\n");
		close(sock);
		return FALSE;
	}
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////
//										MAIN
////////////////////////////////////////////////////////////////////////////////////////////


int main( int argc, char ** argv )
{
	if( argc != 3)
	{
		printf("Incorrect number of arguments. Run as follows:\n ./sws <port> <directory>\n");
		return EXIT_FAILURE;
	}

	port = argv[1];
	directory = argv[2];

	//avoid double / (//) in filepath
	if((char)directory[strlen(directory) - 1] == '/')
	{
		directory[strlen(directory) - 1] = '\0';
	} else 
	{
		strcat(directory, "\0");
	}

	if(!directoryExists(directory))
	{
		return EXIT_FAILURE;
	}

	if(!prepareSocket())
	{
		return EXIT_FAILURE;
	}

	//prep fdset
	int select_result;
	fd_set read_fds;

	printf("sws is running on UDP port %s and serving %s\n", port, directory);
	printf("press 'q' to quit ...\n");

	while (1)
	{
		//ensure stdin does not unneccesarily trigger select
		fflush(STDIN_FILENO);

		//prepare the fd_set
		FD_ZERO( &read_fds );
		FD_SET( STDIN_FILENO, &read_fds );
		FD_SET( sock, &read_fds );

		select_result = select( sock + 1, &read_fds, NULL, NULL, NULL );

		switch( select_result )
		{
			case -1:
				//error
				printf("Error in select (-1). Continuing.\n");
				break;
			case 0:
				//timeout -> should never happen as timeout is null
				printf("Error in select (0). Continuing.\n");
				break;
			default:
				//select returned properly
				if(FD_ISSET(STDIN_FILENO, &read_fds))
				{
					char readbuffer[BUFFER_SIZE];
					read(STDIN_FILENO, readbuffer, BUFFER_SIZE);
					//fflush(STDIN_FILENO);
					if(strncmp(readbuffer, "q", 1) == 0) //what was entered STARTS WITH q TODO: change to only q
					{
						printf("Goodbye!\n");
						close(sock);
						return EXIT_SUCCESS;
					}
					else
					{
						printf("Unrecognized command.\n");
					}
					fflush(STDIN_FILENO);
				} else if(FD_ISSET(sock, &read_fds))
				{
					fflush(STDIN_FILENO);
					ssize_t recsize;
					socklen_t fromlen = sizeof(sa);
					char request[BUFFER_SIZE];

					recsize = recvfrom(sock, (void*) request, sizeof request, 0, (struct sockaddr*)&sa, &fromlen);
					if(recsize == -1)
					{
						//printf("Error occured.\n");
						continue;
					}

					//[0] == request method, [1] == request file, [2] == connection type
					char * arrRequest[3];

					char tmp[strlen(request) + 1];
					strcpy(tmp, request);
					parse_request(tmp, arrRequest);

					char response[BUFFER_SIZE];
					strcpy(response, "HTTP/1.0 ");

					FILE * fp;
					long int file_size;
					long int bytes_read;
					char dir[strlen(directory) + BUFFER_SIZE];

					int boolFileExists = FALSE;

					if(!checkRequestMethod(arrRequest[0]) || !checkURI(arrRequest[1]) || !checkHTTPVersion(arrRequest[2]))
					{
						strcat(response, "400 Bad Request");
					}
					else
					{
						//concat requested file onto served directory
						strcpy(dir, directory);
						if(strcmp(arrRequest[1], "/") == 0)
						{
							strcat(dir, "/index.html");
						}
						else
						{
							strcat(dir, arrRequest[1]);
						}
						
						if(!fileExists(dir, directory))
						{
							strcat(response, "404 Not Found");
						}
						else
						{
							//printf("dir: %s\n", dir);
							strcat(response, "200 OK");
							fp = fopen(dir, "r");
							boolFileExists = TRUE;
						}
					}

					if(boolFileExists)
					{
						printLogString(request, response, sa, dir);
					}
					else
					{
						printLogString(request, response, sa, "");
					}

					strcat(response, "\r\n\r\n");

					sendto(sock, response, strlen(response), 0, (struct sockaddr*)&sa, sizeof sa);

					if(boolFileExists)
					{
						//determine file length
						fseek(fp, 0L, SEEK_END);
						file_size = ftell(fp);
						fseek(fp, 0L, SEEK_SET);

						//read file
						char filebuffer[file_size];
						bytes_read = fread(filebuffer, sizeof(char), file_size, fp);
						fclose(fp);

						if(bytes_read <= BUFFER_SIZE)
						{
							sendto(sock, filebuffer, file_size, 0, (struct sockaddr*)&sa, sizeof sa);
						}
						else
						{
							//sending large file
							int index = 0;
							while(index < bytes_read - BUFFER_SIZE)
							{
								sendto(sock, &filebuffer[index], BUFFER_SIZE, 0, (struct sockaddr*)&sa, sizeof sa);
								index += BUFFER_SIZE;
							}
							//send remainder of file
							sendto(sock, &filebuffer[index], strlen(&filebuffer[index]), 0, (struct sockaddr*)&sa, sizeof sa);
						}
					}
				}
				break;
		}//end switch
	}//end while

	close(sock);
	return EXIT_SUCCESS;
}