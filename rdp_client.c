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
char * 	ip_s;
char 	port_s[10];
char * 	file_save_name;
struct 	sockaddr_in sa;

int 	type				= 0;
int 	last_ack_sent		= 0;
int 	seq_expecting		= 0;
	
int 		buffer_used		= 0;
char 		filebuffer[RECV_BUFFER_SIZE];
 
int 		seqn 			= 0;
int 		length;
int 		window;

ssize_t 	recsize;
socklen_t 	fromlen;
char 		request[BUFFER_SIZE];

FILE * 		fp;

int 		timer 			= 0;
int 		pkt_timeout 	= INIT_PKT_TO;

int 	bytes_recv 			= 0;
int 	unique_bytes_recv 	= 0;
int 	packs_recv 			= 0;
int 	unique_packs_recv 	= 0;
int 	syn_packs_recv 		= 0;	
int 	fin_packs_recv 		= 0;
int 	rst_packs_recv 		= 0;
int 	ack_packs_sent 		= 0;
int 	rst_packs_sent 		= 0;
int 	start_time			= 0;
int 	finish_time 		= 0;
////////////////////////////////////////////////////////////////////////////////////////////
//									HELPER FUNCTIONS
////////////////////////////////////////////////////////////////////////////////////////////
void printSummary()
{
	finish_time = getTimeS();

	//printf("start: %d --- finish: %d\n", start_time, finish_time);

	printf("total data bytes received: %d\n", bytes_recv);
	printf("unique data bytes received: %d\n", unique_bytes_recv);
	printf("total data packets received: %d\n", packs_recv);
	printf("unique data packets received: %d\n", unique_packs_recv);
	printf("SYN packets received: %d\n", syn_packs_recv);
	printf("FIN packets received: %d\n", fin_packs_recv);
	printf("RST packets received: %d\n", rst_packs_recv);
	printf("ACK packets sent: %d\n", ack_packs_sent);
	printf("RST packets sent: %d\n", rst_packs_sent);
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
	else if(ptype == iTypes.ACK)
		strcpy(pts, "ACK");

	printf("%s %c %s:%s %s:%s %s %d %d\n", timestr, event, ip_s, port_s, ip_r, port_r, pts, num1, num2);
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

	printf("%s %c %s:%s %s:%s %s %d %d\n", timestr, event, ip_r, port_r, ip_s, port_s, pts, num1, num2);
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

int getWindowSize()
{
	int l = RECV_BUFFER_SIZE - strlen(filebuffer);
	if(l < MIN_WINDOW_SIZE)
		l = 0;
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

	if(window < MIN_WINDOW_SIZE)
		window = 0;

	generateAckHeader(header, ackn, window);

	//printf("Sending:\n%s\n", header);

	if(sendto(sock, header, strlen(header) + 1, 0, (struct sockaddr*)&sa, sizeof sa) < 0)
	{
		printf("Could not send ack, errno: %d", errno);
		return FALSE;
	}

	if(ackn > last_ack_sent)
		printLogLineSend(0, iTypes.ACK, ackn, window);
	else
		printLogLineSend(1, iTypes.ACK, ackn, window);

	ack_packs_sent++;
	last_ack_sent = ackn;
	seq_expecting = ackn;

	return TRUE;
}

int emptyBufferToFile()
{
	if(strlen(filebuffer) > 0)
	{
		fputs(filebuffer, fp);
		buffer_used = 0;
		filebuffer[0] = '\0';
	}

	return TRUE;
}

int parse_packet_payload(char * recv, char * buffer, int length)
{
	//will be /r/n/r/n hence the +4
	int pos = strcspn(recv, "\r\n") + 4;
	int len = strlen(recv) - pos;

	bytes_recv += len;
	packs_recv++;

	if(seqn == seq_expecting)
	{
		unique_bytes_recv += len;
		unique_packs_recv++;
	}
	if(length != len)
	{
		//printf("client, line 192: length does not match. Header: %d, Payload: %d\n", length, len);
		return FALSE;
	}
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

int resendLastAck()
{
	return sendAckPacket(last_ack_sent);
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
	//printf("%d", getTimeMS());

	int state = states.UNCONNECTED;

	int listening = TRUE;

	start_time = getTimeS();
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
			if(getWindowSize() > MIN_WINDOW_SIZE && seqn != 0)
			{
				printf("here");
				sendAckPacket(seq_expecting);
			}

			if(timer + pkt_timeout > getTimeMS())
			{
				state = states.TIMEOUT;
				continue;
			}

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
			//printf("recvd:\n%s\n", request);

			pkt_timeout = INIT_PKT_TO;

			char tmp[strlen(request) + 1];
			strcpy(tmp, request);

			char * headerinfo[4];
			parse_packet_header(tmp, headerinfo);

			type = typeStrToInt(headerinfo[1]);

			seqn = atoi(headerinfo[2]);
			length = atoi(headerinfo[3]);

			window = getWindowSize();

			if(type == iTypes.DAT)
			{
				if(seqn < seq_expecting)
					printLogLineRecv(3, iTypes.DAT, seqn, length);
				else
					printLogLineRecv(2, iTypes.DAT, seqn, length);

				if(seqn == seq_expecting)
				{
					if(length < RECV_BUFFER_SIZE - buffer_used)
					{
						if(parse_packet_payload(request, filebuffer, length))
						{
							//sendAckPacket(seqn + length);
							seq_expecting = seqn + length;
							//printf("seq_exp: %d\nwin: %d\n", seq_expecting, window);
							if(window < MIN_WINDOW_SIZE)
								sendAckPacket(seq_expecting);
							//timer = 
						}
						else
						{
							//printf("resending 1\n");
							resendLastAck();
						}
					}
					else
						sendAckPacket(seq_expecting);
				}
				else
					sendAckPacket(seq_expecting);
			}
			else if(type == iTypes.SYN)
			{
				//send ack
				ip_s = inet_ntoa(sa.sin_addr);
				sprintf (port_s, "%u", sa.sin_port);

				syn_packs_recv++;
				if(seqn < seq_expecting)
					printLogLineRecv(3, iTypes.SYN, seqn, length);
				else
					printLogLineRecv(2, iTypes.SYN, seqn, length);

				sendAckPacket(seqn + 1);
			}
			else if(type == iTypes.FIN)
			{
				fin_packs_recv++;
				if(seqn < seq_expecting)
					printLogLineRecv(3, iTypes.SYN, seqn, length);
				else
					printLogLineRecv(2, iTypes.SYN, seqn, length);

				sendAckPacket(seqn + 1);
				emptyBufferToFile();
				listening = FALSE;
			}
			else if(type == iTypes.RST)
			{
				if(seqn < seq_expecting)
					printLogLineRecv(3, iTypes.SYN, seqn, length);
				else
					printLogLineRecv(2, iTypes.SYN, seqn, length);

				rst_packs_recv++;
				state = states.RESET;
			}
			else
			{
				//TODO: UNKNOWN
			}

			state = states.LISTENING;
		}
		else if(state == states.FINISH)
		{
			//might be uneeded
		}
		else if(state == states.RESET)
		{
			close(sock);
			fclose(fp);
			fp = fopen(file_save_name, "w");

			memset(filebuffer, '\0', RECV_BUFFER_SIZE);

			state = states.UNCONNECTED;
		}
		else if(state == states.TIMEOUT)
		{
			//TODO: resend ack
			//printf("in TO\n");
			resendLastAck();
		}
		else
		{
			//TODO: UNKNOWN
		}// end if else...
	}//end while

	finish_time = getTimeS();
	printSummary();

	close(sock);
	fclose(fp);

	return EXIT_SUCCESS;
}
