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

#define TRUE 1
#define FALSE 0
#define BUFFER_SIZE 1024
#define _MAGIC_ "CSC361"

static const Types TYPES = {"DAT", "ACK", "SYN", "FIN", "RST"};

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

int parse_packet_payload(char * recv, char ** buffer);

int directoryExists(char * directory);

void printLogString(char * request, char * response, struct sockaddr_in sa, char * file);

int typeToState(char * recv);

#endif