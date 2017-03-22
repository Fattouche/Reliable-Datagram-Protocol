#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>


#define PACKET_SIZE 1024
#define PAYLOAD_SIZE 900
#define WINDOW_SIZE 16
#define TIMEOUT 20000


typedef struct packet{
	char *protocol;
	char* responseType; //ACK,DAT,FIN,SYN,RST
	int sequenceNumber;
	int acknowledge;
	int payloadLength;
	int winSize;
	char information[PAYLOAD_SIZE+1];
	struct timeval timeout;
}packet;

typedef struct stats {
  int    totalData;
  int    uniqueData;
  int    totalPackets;
  int    uniquePackets;
  int    SYN;
  int    FIN;
  int    RST_RECV;
  int    ACK;
  int    RST_SENT;
  struct timeval timer;
} stats;

typedef struct logger{
  char event;
  char *srcIP;
  int srcPort;
  char *destIP;
  int destPort;
  char packetType[3];
  int sequenceNumber;
  int acknowledgeNumber;
  int payloadLength;
  int winSize;
}logger;

struct networkingInfo{
  socklen_t addressSize;
  int socket;
  int port;
  struct sockaddr_in clientAddress;
  char buffer[PACKET_SIZE];
};

void sendBack(struct networkingInfo *info, char *response);

char* stringify(packet src);

packet parse(char *data);

int SYN(struct networkingInfo *info);

void ACK(struct networkingInfo *info, packet pack);

void RST(struct networkingInfo *info, packet pack);

void FIN(struct networkingInfo *info, packet pack);

void logSend(struct logger log);

int checkTimer(struct timeval timeout);

int checkFinTimer(struct timeval timeout);
