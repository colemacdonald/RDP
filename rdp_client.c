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
	sa.sin_addr.s_addr = htonl( atoi(ip_r) );
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

void generateAckHeader(char * headerbuffer, int ackn)
{
	char header[1000] = "CSC361 ACK -1 \0"; //CSC361 _type _seq _ackno _length _size\r\n\r\n"

	char ackstr[4];
	sprintf(ackstr, "%d", ackn);
	strcat(header, ackstr);
	strcat(header, " 1 0\r\n\r\n\0");

	strcpy(headerbuffer, header);
}

int sendAckPacket(int seqn, int length, int window)
{
	char header[1000];
	int ackn = seqn + length;

	generateAckHeader(header, ackn);

	sendto(sock, header, strlen(header), 0, (struct sockaddr*)&sa, sizeof sa);
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
				//timeout
				printf("Error in select (0). Continuing.\n");
				break;
			default:
				//select returned properly
				if(FD_ISSET(sock, &read_fds))
				{
					ssize_t recsize;
					socklen_t fromlen = sizeof(sa);
					char request[BUFFER_SIZE];

					recsize = recvfrom(sock, (void*) request, sizeof request, 0, (struct sockaddr*)&sa, &fromlen);
					if(recsize == -1)
					{
						//printf("Error occured.\n");
						continue;
					}


					printf("%s\n", request);
					char * headerinfo[6];
					//ex request: "CSC361 _type _seq _ackno _length _size\r\n\r\n"
					if(!parse_packet(request, headerinfo))
					{
						//TODO: Failure
						printf("Could not be properly parsed.");
						continue;
					}
					state = typeToState(headerinfo[1]);
					int seqn = atoi(headerinfo[2]);
					int ackn = atoi(headerinfo[3]);
					int length = atoi(headerinfo[4]);
					int size = atoi(headerinfo[5]);

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
					}
				}


				break;
		}//end switch
	}//end while

	close(sock);

	return EXIT_SUCCESS;
}