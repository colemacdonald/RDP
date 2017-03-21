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

#define MAX_PAYLOAD_SIZE 1000

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
int 	last_ack;
int 	last_length;
int 	last_window;

//char *	file_data;
int 	file_size;
int 	final_ack_expected		= 0;


int 		type;
int 		ackn;
int 		wsize;

ssize_t 	recsize;
socklen_t 	fromlen;
char 		request[BUFFER_SIZE];

int 		pkt_timeout 		= INIT_PKT_TO;
int 		timer;
////////////////////////////////////////////////////////////////////////////////////////////
//									HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////

int readFileToMemory(char * filename, char * fdata)
{
	if(!fileExists(filename))
		return FALSE;

	FILE * fp = fopen(filename, "r");
	long int bytes_read;

	//determine file length
	fseek(fp, 0L, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	char filebuffer[file_size];

	//read file
	bytes_read = fread(filebuffer, sizeof(char), file_size, fp);

	printf("filebuffer:\n%s\n", filebuffer);
	fclose(fp);
	strcpy(fdata, filebuffer);

	return TRUE;
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
	char seqstr[10];
	sprintf(seqstr, "%d", seqn);
	strcat(header, seqstr);

	// no ackno, synlen = 1, no window
	strcat(header, " 1\r\n\r\n\0");

	strcpy(headerbuffer, header);
}

void generateHeaderDAT(char * headerbuffer, int seqn, int length)
{
	char header[1000] = "CSC361 DAT \0"; //_seq _ackno _length _size\r\n\r\n"

	char seqstr[10];
	sprintf(seqstr, "%d ", seqn);
	strcat(header, seqstr);

	char lenstr[10];
	sprintf(lenstr, "%d\r\n\r\n", length);
	strcat(header, lenstr);


	strcpy(headerbuffer, header);
}

// TODO: Implement
int unique_packet()
{
	return TRUE;
}

int sendPacket(char * data)
{
	printf("Sending:\n%s\n", data);
	int s = sendto(sock, data, strlen(data) + 1, 0, (struct sockaddr*)&sa_r, sizeof sa_r);
	timer = getTimeMS();
	if(s < 0)
	{
		return FALSE;
	}
	return TRUE;
}

// TODO: Implement
int sendSYN()
{
	char header[1000];
	generateHeaderSYN(header);
	//printf("%s\n", header);

	if(!sendPacket(header))
	{
		printf("Could not send SYN, errno: %d\n", errno);
		return FALSE;
	}

	syn_packs_sent += 1;
	return TRUE;
}

// TODO: Implement
int sendDataPacket(int seqn, int length, char * fdata)
{
	char header[2000];

	int l;

	if(length > strlen(&fdata[seqn - seq0]))
		l = strlen(&fdata[seqn - (seq0 + 1)]);
	else
		l = length;

	generateHeaderDAT(header, seqn, l);

	//printf("Sending data...");

	//char payload[length + 1];
	// +1 for syn
	strncat(header, &fdata[seqn - (seq0 + 1)], length);


	//append data to header
	
	if(!sendPacket(header))
	{
		printf("Could not send DAT packet, errno: %d", errno);
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

void generateHeaderFIN(int seqn, char * headerbuffer)
{
	char header[1000] = "CSC361 FIN \0";

	char seqstr[10];
	sprintf(seqstr, "%d", seqn);
	strcat(header, seqstr);

	// finlen = 1
	strcat(header, " 1\r\n\r\n\0");

	strcpy(headerbuffer, header);
}

int sendFIN(int seqn)
{
	char header[1000];
	generateHeaderFIN(seqn, header);

	final_ack_expected = seqn + 1;

	if(!sendPacket(header))
	{
		printf("Could not send FIN, errno = %d\n", errno);
		return FALSE;
	}
	return TRUE;
}

int finishConnection()
{
	return TRUE;
}

int fileTranserComplete(int ack)
{
	if(ack > seq0 + 2)
		return TRUE;
	return FALSE;
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
	f_to_send = "public/AUTHORS.txt";

	if(!fileExists(f_to_send))
	{
		printf("File does not exist.\n");
		return EXIT_FAILURE;
	}

	FILE * fp = fopen(f_to_send, "r");
	long int bytes_read;

	//determine file length
	fseek(fp, 0L, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	char file_data[file_size + 1];
	file_data[file_size] = '\0';
	//read file
	bytes_read = fread(file_data, sizeof(char), file_size, fp);

	printf("file_data:\n%s\n", file_data);
	fclose(fp);

	printf("rdps is running on UDP %s:%s\n", ip_s, port_s);

	int state = states.UNCONNECTED;

	int finished = FALSE;

	while (!finished)
	{
		if(state == states.UNCONNECTED)
		{
			if(!prepareSocket())
			{
				return EXIT_FAILURE;
			}

			sendSYN();
			state = states.LISTENING;
		}
		else if(state == states.LISTENING)
		{
			//TODO: what to do while listening??
			//recvfrom is blocking

			if(timer + pkt_timeout < getTimeMS())
			{
				state = states.TIMEOUT;
				continue;
			}

			fromlen = sizeof(sa_s);

			recsize = recvfrom(sock, (void*) request, sizeof request, 0, (struct sockaddr*)&sa_s, &fromlen);
			if(recsize == -1)
			{
				//printf("Error occured.\n");
				continue;
			}
			state = states.RECEIVED;
		}
		else if(state == states.RECEIVED)
		{
			printf("recvd:\n%s\n", request);

			char * headerinfo[4];

			char tmp[strlen(request) + 1];
			strcpy(tmp, request);

			parse_packet_header(tmp, headerinfo);

			type = typeStrToInt(headerinfo[1]);
			ackn = atoi(headerinfo[2]);
			wsize = atoi(headerinfo[3]);

			if(ackn > last_ack + last_window && ackn != seq0 + 1)
			{
				state = states.RESET;
				printf("rst\n");
				continue;
			}

			last_window = wsize;
			last_ack = ackn;

			state = states.SEND_DATA;

			//TODO: do something else
		}
		else if(state == states.SEND_DATA)
		{
			if(!fileTranserComplete(ackn))
			{
				state = states.LISTENING;
				int sent;
				for(sent = 0; sent < wsize; sent += MAX_PAYLOAD_SIZE)
					sendDataPacket(ackn + sent, MAX_PAYLOAD_SIZE, file_data);
			}
			else
				state = states.FINISH;
		}
		else if(state == states.FINISH)
		{
			if(ackn != final_ack_expected)
			{
				state = states.LISTENING;
				sendFIN(ackn);
			}
			else
				finished = TRUE;
		}
		else if(state == states.RESET)
		{
			close(sock);

			//TODO: reset file information, close and reopen file?

			state = states.UNCONNECTED;
		}
		else if(state == states.TIMEOUT)
		{
			//resend packets
			pkt_timeout *= 2;
			int sent;
			for(sent = 0; sent < last_window; sent += MAX_PAYLOAD_SIZE)
				sendDataPacket(last_ack + sent, MAX_PAYLOAD_SIZE, file_data);
		}
		else
		{
			//TODO: UNKNOWN
		}
	}//end while

	close(sock);

	//TODO: printLogString();

	return EXIT_SUCCESS;
}


