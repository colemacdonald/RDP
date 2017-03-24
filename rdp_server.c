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
#include <sys/time.h>
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

int 	seq0;
int 	last_ack;
int 	last_length;
int 	last_window;
int 	largest_syn_sent 		= 0;
int 	fin_to					= 0;

//char *	file_data;
int 	file_size;
int 	final_ack_expected		= 0;
int 	rst_seqn				= -2;


int 		type;
int 		ackn;
int 		wsize;

ssize_t 	recsize;
socklen_t 	fromlen;
char 		request[BUFFER_SIZE];

int 		pkt_timeout 		= INIT_PKT_TO;
unsigned long long 		timer;

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
int 	finish_time				= 0;

int 	connected				= FALSE;

////////////////////////////////////////////////////////////////////////////////////////////
//									HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////

void printSummary()
{
	printf("total data bytes sent: %d\n", total_data_bytes_sent);
	printf("unique data bytes sent: %d\n", unique_data_bytes_sent);
	printf("total data packets sent: %d\n", total_data_packs_sent);
	printf("unique data packets sent: %d\n", unique_data_packs_sent);
	printf("SYN packets sent: %d\n", syn_packs_sent);
	printf("FIN packets sent: %d\n", fin_packs_sent);
	printf("RST packets sent: %d\n", rst_packs_sent);
	printf("ACK packets received: %d\n", ack_packs_recv);
	printf("RST packets received: %d\n", rst_packs_recv);
	printf("total time duration (second): %d\n", (finish_time - start_time));
}

void printLogLineSend(int etype, int ptype, int num1, int num2)
{
	//HH:MM:SS.us event_type sip:spt dip:dpt packet_type seqno/ackno length/window
	char timestr[1000];
	getTimeString(timestr);

	char event;
	if(etype == 0)
		event = 's';
	else if(etype == 1)
		event = 'S';
	else if(etype == 2)
		event = 'r';
	else if(etype == 3)
		event = 'R';

	char pts[4];
	if(ptype == iTypes.DAT)
		strcpy(pts, "DAT");
	else if(ptype == iTypes.SYN)
		strcpy(pts, "SYN");
	else if(ptype == iTypes.RST)
		strcpy(pts, "RST");
	else if(ptype == iTypes.FIN)
		strcpy(pts, "FIN");

	printf("%s.%3llu %c %s:%s %s:%s %s %d %d\n", timestr, getTimeUS(), event, ip_s, port_s, ip_r, port_r, pts, num1, num2);
}

void printLogLineRecv(int type, int ptype, int num1, int num2)
{
	//HH:MM:SS.us event_type sip:spt dip:dpt packet_type seqno/ackno length/window
	char timestr[1000];
	getTimeString(timestr);

	char event;
	if(type == 0)
		event = 's';
	else if(type == 1)
		event = 'S';
	else if(type == 2)
		event = 'r';
	else if(type == 3)
		event = 'R';

	char pts[4];
	if(ptype == iTypes.DAT)
		strcpy(pts, "DAT");
	else if(ptype == iTypes.SYN)
		strcpy(pts, "SYN");
	else if(ptype == iTypes.RST)
		strcpy(pts, "RST");
	else if(ptype == iTypes.FIN)
		strcpy(pts, "FIN");
	else if(ptype == iTypes.ACK)
		strcpy(pts, "ACK");

	printf("%s.%.4llu %c %s:%s %s:%s %s %d %d\n", timestr,getTimeUS(), event, ip_r, port_r, ip_s, port_s, pts, num1, num2);
}

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

void generateHeaderRST(int seqn, char * headerbuffer)
{
	char header[1000] = "CSC361 RST \0";
	char seqstr[10];
	sprintf(seqstr, "%d", seqn);
	strcat(header, seqstr);

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

int sendPacket(char * data)
{
	//printf("Sending:\n%s\n", data);
	int s = sendto(sock, data, strlen(data) + 1, 0, (struct sockaddr*)&sa_r, sizeof sa_r);
	timer = getTimeMS();
	if(s < 0)
	{
		return FALSE;
	}
	return TRUE;
}

int sendSYN()
{
	int type;
	if(seq0 == 0)
		type = 0;
	else
		type = 1;

	char header[1000];
	generateHeaderSYN(header);
	//printf("%s\n", header);

	if(!sendPacket(header))
	{
		//printf("Could not send SYN, errno: %d\n", errno);
		return FALSE;
	}

	printLogLineSend(type, iTypes.SYN, seq0, 1);

	syn_packs_sent += 1;
	return TRUE;
}

int sendRST(int seqn)
{
	char header[1000];
	generateHeaderRST(seqn, header);

	int type;

	if(rst_seqn == -2)
		type = 1;
	else
		type = 0;

	if(!sendPacket(header))
	{
		return FALSE;
	}

	printLogLineSend(type, iTypes.RST, seqn, 1);

	rst_packs_sent++;
	return TRUE;
}

int sendDataPacket(int seqn, int length, char * fdata)
{
	char header[2000];

	int l;

	if(seqn - seq0 > strlen(fdata))
		return FALSE;

	if(length > strlen(&fdata[seqn - seq0]))
		l = strlen(&fdata[seqn - (seq0 + 1)]);
	else
		l = length;

	generateHeaderDAT(header, seqn, l);

	//printf("Sending data...");

	//char payload[length + 1];
	// +1 for syn
	//printf("here");
	strncat(header, &fdata[seqn - (seq0 + 1)], l);
	strcat(header, "\0");


	//append data to header
	
	if(!sendPacket(header))
	{
		printf("Could not send DAT packet, errno: %d", errno);
		return FALSE;
	}

	if(seqn > largest_syn_sent)
	{
		unique_data_bytes_sent += length;
		unique_data_packs_sent += 1;
		largest_syn_sent = seqn;
		printLogLineSend(0, iTypes.DAT, seqn, length);
	}
	else
		printLogLineSend(1, iTypes.DAT, seqn, length);

	total_data_bytes_sent += length;
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

	if(final_ack_expected == seqn + 1)
		printLogLineSend(1, iTypes.FIN, seqn, 1);
	else
		printLogLineSend(0, iTypes.FIN, seqn, 1);

	final_ack_expected = seqn + 1;
	fin_packs_sent++;

	if(!sendPacket(header))
	{
		printf("Could not send FIN, errno = %d\n", errno);
		return FALSE;
	}
	return TRUE;
}

int fileTranserComplete(int ack)
{
	if(ack > seq0 + file_size)
		return TRUE;
	return FALSE;
}

int sockReady4Recv()
{
	recsize = recvfrom(sock, (void*) request, sizeof request, 0, (struct sockaddr*)&sa_s, &fromlen);
	if(recsize != -1)
	{
		//printf("Error occured.\n");
		return TRUE;
	}
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////
//										MAIN
////////////////////////////////////////////////////////////////////////////////////////////

int main( int argc, char ** argv )
{
	if( argc != 6)
	{
		ip_s = "192.168.1.100";
		port_s = "8080";
		ip_r = "10.10.1.100";
		port_r = "8080";
		f_to_send = "public/big.txt";

		//TODO: Uncomment
		//printf("Incorrect number of arguments. Run as follows:\n ./sws <port> <directory>\n");
		//return EXIT_FAILURE;
	} else {
		ip_s = argv[1];
		port_s = argv[2];
		ip_r = argv[3];
		port_r = argv[4];
		f_to_send = argv[5];
	}

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

	//printf("file_data:\n%s\n", file_data);
	fclose(fp);

	printf("rdps is running on UDP %s:%s\n", ip_s, port_s);

	int state = states.UNCONNECTED;

	int finished = FALSE;

	start_time = getTimeS();

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
			//printf("recvd:\n%s\n", request);

			pkt_timeout = INIT_PKT_TO;
			connected = TRUE;

			char * headerinfo[4];

			char tmp[strlen(request) + 1];
			strcpy(tmp, request);

			parse_packet_header(tmp, headerinfo);

			type = typeStrToInt(headerinfo[1]);

			ackn = atoi(headerinfo[2]);
			wsize = atoi(headerinfo[3]);

			if(type == iTypes.ACK)
			{
				ack_packs_recv++;
				if(ackn > last_ack)
					printLogLineRecv(2, iTypes.ACK, ackn, wsize);
				else
					printLogLineRecv(3, iTypes.ACK, ackn, wsize);

				/*if(sockReady4Recv())
				{
					continue;
				}	*/
			}
			else if(type == iTypes.RST)
			{
				rst_packs_recv++;
				if(ackn > last_ack)
					printLogLineRecv(2, iTypes.RST, ackn, wsize);
				else
					printLogLineRecv(3, iTypes.RST, ackn, wsize);
			}

			if(ackn > last_ack + last_window + MAX_PAYLOAD_SIZE && ackn != seq0 + 1)
			{
				state = states.RESET;
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
				{
					if((sent / MAX_PAYLOAD_SIZE) % 4 == 0)
					{
						if(sockReady4Recv())
						{
							state = states.RECEIVED;
							sent = wsize;
						}
						else
							sendDataPacket(ackn + sent, MAX_PAYLOAD_SIZE, file_data);
					}
					else
						sendDataPacket(ackn + sent, MAX_PAYLOAD_SIZE, file_data);
				}
				//state = states.LISTENING;
			}
			else
				state = states.FINISH;
		}
		else if(state == states.FINISH)
		{
			if(fin_to > 3)
			{
				finished = TRUE;
				continue;
			}
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
			sendRST(last_ack);
			state = states.UNCONNECTED;
		}
		else if(state == states.TIMEOUT)
		{
			//resend packets
			pkt_timeout = 2 * pkt_timeout;
			if(sockReady4Recv())
			{
				state = states.RECEIVED;
				continue;
			}
			else if(!connected)
			{
				sendSYN();
			}
			else if(fileTranserComplete(last_ack))
			{
				state = states.FINISH;
				fin_to++;
				continue;
			}
			else
			{
				int sent;
				for(sent = 0; sent < wsize; sent += MAX_PAYLOAD_SIZE)
				{
					if(sockReady4Recv())
					{
						state = states.RECEIVED;
						sent = wsize;
						continue;
					}
					else
						sendDataPacket(last_ack + sent, MAX_PAYLOAD_SIZE, file_data);
				}
			}
			state = states.LISTENING;
		}
		else
		{
			//TODO: UNKNOWN
		}
	}//end while
	
	finish_time = getTimeS();
	printSummary();
	close(sock);

	return EXIT_SUCCESS;
}


