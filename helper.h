//helper.c

#ifndef HELPER
#define HELPER

typedef struct {
	char * data;
	char * acknowledgement;
	char * sync;
	char * finish;
	char * reset;
} Types;

typedef struct {
	int DAT;
	int ACK;
	int SYN;
	int FIN;
	int RST;
} IntTypes;

typedef struct states {
	int LISTENING;
	int UNCONNECTED;
	int RECEIVED;
	int SEND_DATA;
	int FINISH;
	int RESET;
	int SEND_RESET;
	int TIMEOUT;
	int EMPTY_BUFFER;
} States;

#define TRUE 				1
#define FALSE 				0
#define BUFFER_SIZE 		1024
#define _MAGIC_ 			"CSC361"
#define SOCK_TIMEOUT_s 		0
#define SOCK_TIMEOUT_us		200
#define INIT_PKT_TO		    1


static const Types TYPES = {"DAT", "ACK", "SYN", "FIN", "RST"};
static const IntTypes iTypes = {0, 1, 2, 3, 4};
static const States states = {0, 1, 2, 3, 4, 5, 6, 7, 8};

void getTimeString(char * buffer);

void strToUpper(char * str);

void strTrimInto(char * dst, char * src);

int checkRequestMethod(char * method);

int checkURI(char * filepath);

int fileExists2(char * filepath, char * directory);

int fileExists(char * filepath);

int checkHTTPVersion(char * version);

void parse_request(char * request_string, char ** buffer);

int parse_packet_header(char * recv, char ** buffer);

//int parse_packet_payload(char * recv, char * buffer);

int directoryExists(char * directory);

void printLogString(char * request, char * response, struct sockaddr_in sa, char * file);

int typeStrToInt(char * recv);

int getTimeMS();

int getTimeS();

#endif