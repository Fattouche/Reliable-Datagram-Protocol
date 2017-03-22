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
#include "helper.h"

//Used to log the recievers stats at the end
void logStats(stats statistics) {
  struct timeval end;
  struct timeval totalTime;
  gettimeofday(&end, NULL);
  timersub(&end, &statistics.timer, &totalTime);
  printf("\n\ntotal data bytes received: %d\nunique data bytes received: %d\ntotal data packets received: %d\nunique data packets received: %d\nSYN packets received: %d\nFIN packets received: %d\nRST packets received: %d\n""ACK packets sent: %d\nRST packets sent: %d\ntotal time duration: %d.%d\n\n",statistics.totalData, statistics.uniqueData, statistics.totalPackets, statistics.uniquePackets, statistics.SYN, statistics.FIN, statistics.RST_RECV, statistics.ACK, statistics.RST_SENT,(int) totalTime.tv_sec,(int) totalTime.tv_usec);
}

int main(int argc, char *argv[])
{

 //for printing to stdout
  setvbuf (stdout, NULL, _IONBF, 0);
  if (argc != 4) {
    printf("Please input: rdpr receiver_ip receiver_port receiver_file_name");
    exit(-1);
  }

  //Read the args from user
  char *IPAddr = argv[1];
  int portNum = atoi(argv[2]);
  char *fileName = argv[3];
  FILE *file = fopen(fileName, "wb");

  //Create a datagram(UDP) socket
  size_t sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in serverAddress;
  int enable = 1;

  //Allows us to reuse the socket port
  int trySetSockOpt = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (trySetSockOpt < 0) {
     fprintf(stderr, "Failed on SetSockOpt");
     exit(EXIT_FAILURE);
  }

  //clear buffer
  bzero((char *)&serverAddress, sizeof(serverAddress));
  serverAddress.sin_port = htons(portNum);
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = inet_addr(IPAddr);

  //Bind the socket to our ipaddress and given port
  int tryBind = bind(sock, (struct sockaddr *)&serverAddress, sizeof serverAddress);
  if(tryBind!=0){
	  printf("The socket could not be binded with the given server address");
	  close(sock);
	  exit(EXIT_FAILURE);
  }
	//This will be where we recv from the sender
	void* buff = calloc(PACKET_SIZE+1, sizeof(char));

	//Some variables used for keeping track of logs and states
	int prevAck = 0;
	int ackCounter = 0;
	int firstSyn=0;
	int connected = 0;
	int finishing = 0;
	int finSent = 0;
	unsigned long initialAddress = 0;

	logger log;
	stats statistics={0};

	//Define our packets
	packet pack;
	struct networkingInfo *packetInfo = calloc(1, sizeof *packetInfo);
	packetInfo->socket = sock;
    packetInfo->addressSize = sizeof(packetInfo->clientAddress);
	packetInfo->port = portNum;

  while(1) {
    int string = recvfrom(sock, buff, PACKET_SIZE, 0, (struct sockaddr*)&packetInfo->clientAddress, &packetInfo->addressSize); // This socket is blocking
	if(firstSyn==0){
		firstSyn=1;
		gettimeofday(&statistics.timer, NULL);
	}
	//parse through the buffer
	pack = parse(buff);

	//log the parsed packet information
	log.srcIP = inet_ntoa(packetInfo->clientAddress.sin_addr);
	log.srcPort = packetInfo->port;
	log.sequenceNumber = pack.sequenceNumber;
	log.acknowledgeNumber = 0;
	log.payloadLength = pack.payloadLength;
	log.winSize = pack.winSize;
	strcpy(log.packetType, pack.responseType);
	log.destIP=IPAddr;
	log.destPort=portNum;

    if (string == -1) {
      printf("error in recieving bytes");
    }

	//This if/else checks to make sure we are still recieving from the same sender
	 if (initialAddress == 0) {
	  initialAddress = packetInfo->clientAddress.sin_addr.s_addr;
    } else if (initialAddress != packetInfo->clientAddress.sin_addr.s_addr) {
	  log.event='r';
	  log.payloadLength = pack.payloadLength;
	  log.winSize = pack.winSize;
	  strcpy(log.packetType, pack.responseType);
	  logSend(log);
	  log.event = 's';
	  strcpy(log.packetType, "RST");
	  logSend(log);
	  statistics.RST_SENT++;
	  RST(packetInfo, pack); //if we are getting data from new source, reset
	  file = fopen(fileName, "w");
    }


    if (strcmp("NONE",pack.responseType)==0) {
      printf("the packet was corrupted, we need to wait for another request");
    }else{
		//check the packet type
		if(strcmp("SYN",pack.responseType) == 0){
			if(connected==0){
				log.event='r';
				logSend(log);
				log.event = 's';
			}else{
				log.event='R';
				logSend(log);
				log.event = 'S';
			}
			strcpy(log.packetType, "ACK");
			log.acknowledgeNumber=prevAck;
			statistics.SYN++;
			prevAck = pack.sequenceNumber;
			pack.acknowledge = -1;
			connected=1;
			ACK(packetInfo, pack);
		}
		else if(strcmp("ACK",pack.responseType) == 0){
			if(ackCounter==0){
				log.event='r';
				ackCounter++;
			}else{
				log.event='R';
			}
		}
		else if(strcmp("RST",pack.responseType)==0){
			log.event='r';
			statistics.RST_RECV++;
		}
		else if(strcmp("DAT",pack.responseType)==0){
			if(prevAck==pack.sequenceNumber){
				statistics.uniqueData+=pack.payloadLength;
				statistics.uniquePackets++;
				log.event='r';
				logSend(log);
				prevAck = prevAck+pack.payloadLength;
				log.event = 's';
				fwrite(pack.information ,pack.payloadLength, sizeof(char), file);
			}else{
				log.event='R';
				logSend(log);
				log.event='S';
			}
			strcpy(log.packetType, "ACK");
			log.acknowledgeNumber = prevAck;
			statistics.totalData+=pack.payloadLength;
			statistics.totalPackets++;
			pack.acknowledge = prevAck;
			pack.sequenceNumber = prevAck;
			statistics.ACK++;
			ACK(packetInfo,pack);
		}
		else if(strcmp("FIN",pack.responseType)==0){
			strcpy(log.packetType, "FIN");
			log.acknowledgeNumber=pack.sequenceNumber;
			statistics.FIN++;
			log.event='r';
			logSend(log);
			log.event='s';
			logSend(log);
			FIN(packetInfo, pack);
			fd_set watcher;
			struct timeval t;
			t.tv_sec = 0;
			t.tv_usec = TIMEOUT/2;
			//We have recieved a fin packet from sender, enter loop to send a FIN back every 200 milliseconds if no ack recieved.
			while(finSent<200){
				FD_ZERO(&watcher);
				FD_SET(sock, &watcher);
				select(sock + 1, &watcher, NULL, NULL, &t);
				if(FD_ISSET(sock,&watcher)){
					int bytes = recvfrom(sock, buff, PAYLOAD_SIZE + 1, 0, (struct sockaddr*)&packetInfo->clientAddress,&packetInfo->addressSize);
					  if (bytes == -1) {
						printf("error in recieving bytes");
					  }
					  pack = parse(buff);
					  if(strcmp("ACK", pack.responseType) == 0){
						  log.event = 'r';
						  strcpy(log.packetType, "ACK");
						  break;
					  }
				}else{
					log.event='S';
					strcpy(log.packetType, "FIN");
					logSend(log);
					FIN(packetInfo,pack);
					finSent++;
				}
			}
			finishing = 1;
		}
	}
	//Send the individual log
	logSend(log);

	if(strcmp("RST",pack.responseType)==0){
		if(statistics.FIN>0){
			log.event = 'R';
		}else{
			log.event = 'r';
		}
		logStats(statistics);
		finishing = 0;
		file = fopen(fileName, "w");
		continue;
	}
	//if we recieved an ack, and we are finishing it means we can close and logstats
	if(strcmp("ACK",pack.responseType)==0 && finishing==1){
		fclose(file);
		logStats(statistics);
		return 0;
	}
  }

   return 0;
}
