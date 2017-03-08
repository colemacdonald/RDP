//rdp_server.c

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

////////////////////////////////////////////////////////////////////////////////////////////
//										CONSTANTS
////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////
//									GLOBAL VARIABLES
////////////////////////////////////////////////////////////////////////////////////////////

int sock;
char * ip_s;
char * port_s;
char * ip_r;
char * port_r;
char * f_to_send;
struct sockaddr_in sa;


////////////////////////////////////////////////////////////////////////////////////////////
//									HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////


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
	sa.sin_port = htons( atoi( port_s ) ); //convert to int
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

int generateRandomSequenceNumber()
{
	int num;
	srand(time(NULL));
	num = rand();
	num = num % 1000;
	return num;
}

int establishConnection()
{
	char header[1000] = "CSC361 SYN \0"; //_seq _ackno _length _size\r\n\r\n"
	int seq = generateRandomSequenceNumber();
	char seqstr[4] = itoa(seq);
	strcat(header, seqstr);
	strcat(header, " -1 1 0\0");
	printf("%s\n", header);
	return TRUE;
}

int finishConnection()
{
	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////
//										MAIN
////////////////////////////////////////////////////////////////////////////////////////////

int main( int argc, char ** argv )
{
	/*if( argc != 6)
	{
		printf("Incorrect number of arguments. Run as follows:\n ./sws <port> <directory>\n");
		return EXIT_FAILURE;
	}*/

	/*ip_s = argv[1];
	port_s = argv[2];
	ip_r = argv[3];
	port_r = argv[4];
	f_to_send = argv[5];*/

	port_s = "8080";
	f_to_send = "public/index.html";

	if(!fileExists(f_to_send))
	{
		printf("does not exist\n");
		return EXIT_FAILURE;
	}

	if(!prepareSocket())
	{
		return EXIT_FAILURE;
	}

	//prep fdset
	int select_result;
	fd_set read_fds;

	printf("rdp is running on UDP port %s\n", port_s);
	establishConnection();

	while (1)
	{
		//ensure stdin does not unneccesarily trigger select
		fflush(STDIN_FILENO);

		//prepare the fd_set
		FD_ZERO( &read_fds );

		//time out counter -> use select timeout?
		//FD_SET( STDIN_FILENO, &read_fds );
		FD_SET( sock, &read_fds );

		//replace (i think) final NULL with timeout value;
		select_result = select( sock + 1, &read_fds, NULL, NULL, NULL /*timeout*/);

		switch( select_result )
		{
			case -1:
				//error
				printf("Error in select (-1). Continuing.\n");
				break;
			case 0:

				printf("Error in select (0). Continuing.\n");
				break;
			default:
				//select returned properly
				break;
		}//end switch
	}//end while

	close(sock);

	return EXIT_SUCCESS;
}


