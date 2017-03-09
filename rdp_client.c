//rdp_client.c

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
#include "helper.c"

////////////////////////////////////////////////////////////////////////////////////////////
//										CONSTANTS
////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////
//									GLOBAL VARIABLES
////////////////////////////////////////////////////////////////////////////////////////////

int 	sock;
char * 	ip_r;
char * 	port_r;
char * 	file_save_name;
struct 	sockaddr_in sa;

int 	bytes_recv 			= 0;
int 	unique_bytes_recv 	= 0;
int 	packs_recv 			= 0;
int 	unique_packs_recv 	= 0;
int 	syn_packs_recv 		= 0;	
int 	fin_packs_recv 		= 0;
int 	rst_packs_recv 		= 0;
int 	ack_packs_sent 		= 0;
int 	rst_packs_sent 		= 0;
int 	duration 			= 0;

int 	state				= 0;
int 	last_ack			= 0;

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
	sa.sin_addr.s_addr = inet_addr(ip_r);  //htonl( INADDR_ANY );
	sa.sin_port = htons( atoi( port_r ) ); //convert to int
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

int establishConnection()
{
	return TRUE;
}

int finishConnection()
{
	return TRUE;
}

void generateAckHeader(char * headerbuffer, int ackn, int window)
{
	char header[1000] = "CSC361 ACK \0"; //CSC361 _type _ackno _size\r\n\r\n"

	char ackstr[10];
	char winstr[10];
	sprintf(ackstr, "%d", ackn);
	sprintf(winstr, " %d", window);
	strcat(header, ackstr);
	strcat(header, winstr);
	strcat(header, "\r\n\r\n\0");

	strcpy(headerbuffer, header);
}

int sendAckPacket(int seqn, int length, int window)
{
	char header[1000];
	int ackn = seqn + length;

	generateAckHeader(header, ackn, window);

	printf("Sending:\n%s\n", header);

	if(sendto(sock, header, strlen(header) + 1, 0, (struct sockaddr*)&sa, sizeof sa) < 0)
	{
		printf("Could not send ack, errno: %d", errno);
	}
	ack_packs_sent += 1;
	last_ack = ackn;

	return 1;
}

int getWindowSize()
{
	//TODO: what is size of header? Have to take it into account
	return 1024;
}
////////////////////////////////////////////////////////////////////////////////////////////
//										MAIN
////////////////////////////////////////////////////////////////////////////////////////////

int main (int argc, char ** argv)
{
	/*if(argc != 4)
	{
		printf("Incorrect number of arguments, run as follows:\n./rdpr ")
		return EXIT_FAILUE;
	}*/

	ip_r 			= "10.10.1.100";	//argv[1];
	port_r 			= "8080"; 			//argv[2];
	file_save_name 	= "save.txt";		//argv[3];

	if(!prepareSocket())
	{
		return EXIT_FAILURE;
	}

	//prep fdset
	int select_result;
	fd_set read_fds;

	printf("rdpc is running on UDP port %s\n", port_r);

	while (1)
	{	
		ssize_t recsize;
		socklen_t fromlen = sizeof(sa);
		char request[BUFFER_SIZE];

		recsize = recvfrom(sock, (void*) request, sizeof request, 0, (struct sockaddr*)&sa, &fromlen);
		if(recsize < 0)
		{
			printf("Error occured.\n");
			continue;
		}

		printf("%s\n", request);

		char * headerinfo[4];

		char tmp[strlen(request) + 1];
		strcpy(tmp, request);

		parse_packet_header(tmp, headerinfo);

		state = typeToState(headerinfo[1]);

		printf("state = %d\n", state);
		int seqn = atoi(headerinfo[2]);
		int length = atoi(headerinfo[3]);

		int window = getWindowSize();

		switch(state)
		{
			//DAT
			case 1:
				//read in
				//ack
				sendAckPacket(seqn, length, window);
				break;
			//ACK
			case 2:
				//something wrong
				break;
			//SYN
			case 3:
				//send ack
				sendAckPacket(seqn, length, window);
				break;
			//FIN
			case 4:
				//ack
				sendAckPacket(seqn, length, 0);
				break;
			//RST
			case 5:
				//toss all data
				//clear buffers
				//empty file
				//ack
				break;
			//unknown state
			default:
				break;
		}//end switch
	}//end while

	close(sock);

	return EXIT_SUCCESS;
}