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

#define RECV_BUFFER_SIZE 	10240
#define MIN_WINDOW_SIZE		512

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

int 	type				= 0;
int 	last_ack			= 0;


int 		buffer_used		= 0;
char 		filebuffer[RECV_BUFFER_SIZE];

 
int 		seqn;
int 		length;
int 		window;

ssize_t 	recsize;
socklen_t 	fromlen;
char 		request[BUFFER_SIZE];

FILE * 		fp;

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

	//http://forums.codeguru.com/showthread.php?353217-example-of-SO_RCVTIMEO-using-setsockopt()
	struct timeval tv;
	tv.tv_sec = SOCK_TIMEOUT_s;
	tv.tv_usec = SOCK_TIMEOUT_us;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

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

int getWindowSize()
{
	//TODO: what is size of header? Have to take it into account
	int l = RECV_BUFFER_SIZE - strlen(filebuffer);
	if(l < MIN_WINDOW_SIZE)
		l = MIN_WINDOW_SIZE;
	return l;
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

int sendAckPacket(int ackn)
{
	char header[1000];

	window = getWindowSize();

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

int emptyBufferToFile()
{
	fputs(filebuffer, fp);
	buffer_used = 0;
	filebuffer[0] = '\0';

	return TRUE;
}

int parse_packet_payload(char * recv, char * buffer, int length)
{
	//will be /r/n/r/n hence the +4
	int pos = strcspn(recv, "\r\n") + 4;
	int len = strlen(recv) - pos;

	if(length != len)
		return FALSE;

	if(len > RECV_BUFFER_SIZE - buffer_used)
		return FALSE;

	char dst[len + 1];
	strncpy(dst, &recv[pos], len);
	//dst now contains payload
	dst[len] = '\0';

	strcat(buffer, dst);

	buffer_used += len;

	return TRUE;
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

	fp = fopen(file_save_name, "w");
	if(!fp)
	{
		printf("File with specified name could not be created. Given: %s\n", file_save_name);
		return EXIT_FAILURE;
	}

	printf("rdpc is running on UDP port %s\n", port_r);

	int state = states.UNCONNECTED;

	int listening = TRUE;
	while (listening)
	{	
		if(state == states.UNCONNECTED)
		{
			if(!prepareSocket())
			{
				return EXIT_FAILURE;
			}
			state = states.LISTENING;
		}
		else if(state == states.LISTENING)
		{
			//TODO: while 'listening' empty buffer, right now recvfrom is blocking

			fromlen = sizeof(sa);

			recsize = recvfrom(sock, (void*) request, sizeof request, 0, (struct sockaddr*)&sa, &fromlen);
			if(recsize < 0)
			{
				//printf("Error occured.\n");
				emptyBufferToFile();
				continue;
			}
			state = states.RECEIVED;
		}
		else if(state == states.EMPTY_BUFFER)
		{
			if(buffer_used > 0)
			{
				emptyBufferToFile();
			}

			state = states.LISTENING;
		}
		else if(state == states.RECEIVED)
		{
			printf("recvd:\n%s\n", request);

			char * headerinfo[4];

			char tmp[strlen(request) + 1];
			strcpy(tmp, request);

			parse_packet_header(tmp, headerinfo);

			type = typeStrToInt(headerinfo[1]);

			//printf("state = %d\n", state);
			seqn = atoi(headerinfo[2]);
			length = atoi(headerinfo[3]);

			window = getWindowSize();

			if(type == iTypes.DAT)
			{
				//read in
				if(length < RECV_BUFFER_SIZE - buffer_used)
				{
					if(parse_packet_payload(request, filebuffer, length))
						sendAckPacket(seqn + length);
					//else
					//TODO: resend ack
				}

				//TODO: Deal with data
			}
			else if(type == iTypes.SYN)
			{
				//send ack
				sendAckPacket(seqn + length);
			}
			else if(type == iTypes.FIN)
			{
				sendAckPacket(seqn + length);
				emptyBufferToFile();
				listening = FALSE;
			}
			else
			{
				//TODO: UNKNOWN
			}

			if(window < MIN_WINDOW_SIZE)
				state = states.EMPTY_BUFFER;
			else
				state = states.LISTENING;
		}
		else if(state == states.FINISH)
		{
			//might be uneeded
		}
		else if(state == states.RESET)
		{
			close(sock);
			//TODO: clear buffer and empty file
			state = states.UNCONNECTED;
		}
		else if(state == states.TIMEOUT)
		{
			//TODO: resend ack
			sendAckPacket(last_ack);
		}
		else
		{
			//TODO: UNKNOWN
		}// end if else...
	}//end while

	close(sock);
	fclose(fp);

	return EXIT_SUCCESS;
}