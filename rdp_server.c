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

int 	sock;
char * 	ip_s;
char * 	port_s;
char * 	ip_r;
char * 	port_r;
char * 	f_to_send;
struct 	sockaddr_in sa_s;
struct 	sockaddr_in sa_r;

int 	total_data_bytes_sent 	= 0;
int 	unique_data_bytes_sent 	= 0;
int 	total_data_packs_sent	= 0;
int 	unique_data_packs_sent	= 0;
int 	syn_packs_sent			= 0;
int 	fin_packs_sent			= 0;
int 	rst_packs_sent			= 0;
int 	ack_packs_recv			= 0;
int 	rst_packs_recv			= 0;

int 	start_time				= 0;

int 	seq0;
int 	last_seq;
int 	last_length;



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

	//http://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

	//struct sockaddr_in sa; 
	ssize_t recsize;
	socklen_t fromlen;

	//TODO: Use reciever port and ip, connect instead of bind

	memset(&sa_s, 0, sizeof sa_s);
	sa_s.sin_family = AF_INET;
	sa_s.sin_addr.s_addr = inet_addr(ip_s);//htonl( atoi(ip_s) );
	sa_s.sin_port = htons( atoi( port_s ) ); //convert to int
	fromlen = sizeof(sa_s);
	//end of copy

	if(bind(sock, (struct sockaddr *) &sa_s, sizeof sa_s) != 0)
	{
		printf("socket could not be bound\n");
		close(sock);
		return FALSE;
	}

	// prep sa_r

	memset(&sa_r, 0, sizeof sa_r);
	sa_r.sin_family = AF_INET;
	sa_r.sin_addr.s_addr = inet_addr(ip_r);
	sa_r.sin_port = htons( atoi( port_r ) );

	return TRUE;
}

int generateRandomSequenceNumber()
{
	int num;
	srand(time(NULL));
	num = rand();
	num = num % 1000000;
	return num;
}

void generateHeaderSYN(char * headerbuffer)
{
	char header[1000] = "CSC361 SYN \0"; //_seq _ackno _length _size\r\n\r\n"
	int seqn = generateRandomSequenceNumber();
	seq0 = seqn;
	char seqstr[4];
	sprintf(seqstr, "%d", seqn);
	strcat(header, seqstr);

	// no ackno, synlen = 1, no window
	strcat(header, " 1\r\n\r\n\0");

	strcpy(headerbuffer, header);
}

void generateHeaderDAT(char * headerbuffer, int seqn, int length)
{
	char header[1000] = "CSC361 DAT \0"; //_seq _ackno _length _size\r\n\r\n"

	char seqstr[4];
	sprintf(seqstr, "%d", seqn);
	strcat(header, seqstr);

	// no ackno
	strcat(header, " -1 ");

	char lenstr[10];
	sprintf(lenstr, "%d", length);
	strcat(header, lenstr);

	// only one way, no window
	strcat(header,  "0\r\n\r\n\0");

	strcpy(headerbuffer, header);
}

// TODO: Implement
int unique_packet()
{
	return TRUE;
}

// TODO: Implement
int sendSYN()
{
	char header[1000];
	generateHeaderSYN(header);
	printf("%s\n", header);

	int s = sendto(sock, header, strlen(header) + 1, 0, (struct sockaddr*)&sa_r, sizeof sa_r);
	if(s < 0)
	{
		//TODO: Failure
		printf("Could not send SYN, errno = %d\n", errno);
		return FALSE;
	}

	syn_packs_sent += 1;
	return TRUE;
}

// TODO: Implement
int sendDataPacket(int seqn, int length)
{
	char header[1000];
	generateHeaderDAT(header, seqn, length);

	//append data to header

	strcat(header, "This is the payload data\0");
	int s = sendto(sock, header, strlen(header) + 1, 0, (struct sockaddr*)&sa_r, sizeof sa_r);
	if(s < 0)
	{
		//TODO: Failure
		printf("Could not send SYN, errno = %d\n", errno);
		return FALSE;
	}

	total_data_bytes_sent += length;

	if(unique_packet())
	{
		unique_data_bytes_sent += length;
		unique_data_packs_sent += 1;
	}

	total_data_packs_sent += 1;
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

	ip_s = "192.168.1.100";
	port_s = "8080";
	ip_r = "10.10.1.100";
	port_r = "8080";
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

	printf("rdps is running on UDP %s:%s\n", ip_s, port_s);
	sendSYN();

	while (1)
	{
		ssize_t recsize;
		socklen_t fromlen = sizeof(sa_s);
		char request[BUFFER_SIZE];

		recsize = recvfrom(sock, (void*) request, sizeof request, 0, (struct sockaddr*)&sa_s, &fromlen);
		if(recsize == -1)
		{
			//printf("Error occured.\n");
			continue;
		}

		printf("%s\n", request);


		// TODO: shorten header -> no point in sending seqn or length from recvr
		char * headerinfo[4];
		//ex request: "CSC361 _type _seq _ackno _length _size\r\n\r\n"
		/*if(!parse_packet(request, headerinfo))
		{
			//TODO: Failure
			printf("Could not be properly parsed.");
			continue;
		}*/

		char tmp[strlen(request) + 1];
		strcpy(tmp, request);

		parse_packet_header(tmp, headerinfo);

		int state = typeToState(headerinfo[1]);
		int ackn = atoi(headerinfo[2]);
		int size = atoi(headerinfo[3]);

		switch(state)
		{
			//DAT
			case 1:
				//something wrong
				break;

			//ACK
			case 2:
				//send data packet
				sendDataPacket(ackn, size);//seqn, length);
				break;

			//SYN
			case 3:
			//something wrong
				//send ack
				//sendAckPacket(seqn, length);
				break;

			//FIN
			case 4:
				//something wrong
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


