#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include "helper.h"

int currentWindowSize = WINDOW_SIZE;
int sequenceNumber = 0;
int initSequenceNumber = 0;
int finSent = 0;
int fileSize;
int maxAck;
int dataSent = 0;
struct timeval packetTimeout;
logger senderLog;
stats statistics={0};
FILE* file;

//This is the data sender function
void DAT(struct networkingInfo* info, packet pack) {
	//If we have read the entire file we can send a FIN and close
  if (pack.sequenceNumber - initSequenceNumber == fileSize) {
	if(finSent==0){
		senderLog.event='s';
		finSent=1;
	}else{
		senderLog.event='S';
	}
	strcpy(senderLog.packetType, "FIN");
	senderLog.sequenceNumber = pack.sequenceNumber;
	senderLog.payloadLength=0;
	senderLog.acknowledgeNumber=0;
	logSend(senderLog);
	statistics.FIN++;
    FIN(info, pack);
    return;
  }
  //We only read the file based off what the reciever has acked back(in this case we use the recievers sequence number)
  int packetsSend = pack.sequenceNumber - initSequenceNumber;
  
  packet data;
  
  //While we can still send packets, go to the file location and put into struct.
  while (currentWindowSize > 0) {
    fseek(file, packetsSend, SEEK_SET);
    size_t reader = fread(data.information, sizeof(char), PAYLOAD_SIZE, file);
    if (reader == 0) {  // read everything
	  gettimeofday(&packetTimeout, NULL);
	  dataSent=packetsSend;
      return;
    } else {
      data.protocol = "CSC361";
      data.responseType = "DAT";
      data.sequenceNumber = packetsSend + initSequenceNumber;
      data.payloadLength = (int)reader;
      data.winSize = currentWindowSize;
      data.information[reader] = '\0';
	  
	  //Call to stringify the struct
      char* response = stringify(data);
	  //Send the stringified response to the receiver
      sendBack(info, response);
      currentWindowSize--;
      packetsSend += (int)reader;
	  //Check if we have already sent this data
	  if(packetsSend>dataSent){
		  senderLog.event='s';
		  statistics.uniqueData+=data.payloadLength;
		  statistics.uniquePackets++;
	  }else{
		  senderLog.event='S';
	  }
	  senderLog.sequenceNumber = data.sequenceNumber;
	  senderLog.winSize = currentWindowSize;
	  senderLog.payloadLength = data.payloadLength;
	  senderLog.acknowledgeNumber=0;
	  strcpy(senderLog.packetType, "DAT");
	  //Log our send
	  logSend(senderLog);
	  statistics.totalData+=data.payloadLength;
	  statistics.totalPackets++;
    }
  }
  if(packetsSend>dataSent)
	dataSent = packetsSend;
  gettimeofday(&packetTimeout, NULL);
}

//log the stats for sender at end of connection
void logStats(stats statistics) {
  struct timeval end;
  struct timeval totalTime;
  gettimeofday(&end, NULL);
  timersub(&end, &statistics.timer, &totalTime);
  printf("\n\ntotal data bytes sent: %d\nunique data bytes sent: %d\ntotal data packets sent: %d\nunique data packets sent: %d\nSYN packets sent: %d\nFIN packets sent: %d\nRST packets sent: %d\n""ACK packets received: %d\nRST packets received: %d\ntotal time duration: %d.%d\n\n",statistics.totalData, statistics.uniqueData, statistics.totalPackets, statistics.uniquePackets, statistics.SYN, statistics.FIN, statistics.RST_RECV, statistics.ACK, statistics.RST_SENT,(int) totalTime.tv_sec,(int) totalTime.tv_usec);
}

int main(int argc, char* argv[]) {
  setvbuf(stdout, NULL, _IONBF, 0);
  if (argc != 6) {
    printf(
        "Please input: rdps sender_ip sender_port receiver_ip receiver_port "
        "sender_file_name");
    exit(-1);
  }
  // Setup
  char* senderIP = argv[1];
  int senderPort = atoi(argv[2]);
  char* hostIP = argv[3];
  int hostPort = atoi(argv[4]);
  char* fileName = argv[5];
  file = fopen(fileName, "rb");
  
  if (!file) {
    printf("error reading file %s\n", fileName);
    exit(1);
  }
  //get the filesize for FIN
  fseek(file, 0L, SEEK_END);
  fileSize = ftell(file);

  //Socket setup
  size_t sock = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in hostAddress;
  struct sockaddr_in senderAddress;
  
  int enable = 1;

  int trySetSockOpt = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (trySetSockOpt < 0) {
    fprintf(stderr, "Failed on SetSockOpt");
    exit(EXIT_FAILURE);
  }

  // clear buffer
  bzero((char*)&senderAddress, sizeof(senderAddress));
  senderAddress.sin_port = htons(senderPort);
  senderAddress.sin_family = AF_INET;
  senderAddress.sin_addr.s_addr = inet_addr(senderIP);

  hostAddress.sin_family = AF_INET;
  hostAddress.sin_port = htons(hostPort);
  hostAddress.sin_addr.s_addr = inet_addr(hostIP);

  // Bind the socket to our ipaddress and given port
  int tryBind =
      bind(sock, (struct sockaddr*)&senderAddress, sizeof(senderAddress));
  if (tryBind != 0) {
    printf("The socket could not be binded with the given server address");
    close(sock);
    exit(EXIT_FAILURE);
  }

  char* buff = calloc(PAYLOAD_SIZE + 1, sizeof(char));
  fd_set watcher;

  int connected = 0;
  int closing = 0;

  struct networkingInfo* hostInfo = calloc(1, sizeof *hostInfo);
  hostInfo->socket = sock;
  hostInfo->addressSize = sizeof(hostInfo->clientAddress);
  hostInfo->port = hostPort;
  hostInfo->clientAddress = hostAddress;

  //This wont change.
  senderLog.srcIP = senderIP;
  senderLog.srcPort = senderPort;
  senderLog.destIP=hostIP;
  senderLog.destPort=hostPort;

  int synSent = 0;
  int finRecvd = 0;
  
  //start the global timer for stats
  struct timeval timeout;
  gettimeofday(&statistics.timer, NULL);
  
  while (1) {
    packet pack;
    FD_ZERO(&watcher);
    FD_SET(sock, &watcher);
    fd_set dup = watcher;
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT;
    if (select(sock + 1, &dup, NULL, NULL, &timeout) < 0) {
      perror("select");
      return -1;
    }

	senderLog.acknowledgeNumber = 0;
	senderLog.winSize = currentWindowSize;

	//If we havent connected yet, keep sending SYNs untill we connect.
    if (connected == 0) {
      sequenceNumber = SYN(hostInfo);
      initSequenceNumber = sequenceNumber;
	  maxAck = sequenceNumber;
	  senderLog.sequenceNumber = sequenceNumber;
	  if(synSent==0){
		synSent=1;
		senderLog.event='s';
	  }else{
		senderLog.event='S';
	  }
	  statistics.SYN++;
	  strcpy(senderLog.packetType, "SYN");
	  logSend(senderLog);
      if (FD_ISSET(sock, &dup)) {
        int bytes = recvfrom(sock, buff, PAYLOAD_SIZE + 1, 0,
                             (struct sockaddr*)&hostInfo->clientAddress,
                             &hostInfo->addressSize);
        if (bytes == -1) {
          printf("error in recieving bytes");
        }
        pack = parse(buff);

		if(maxAck<pack.sequenceNumber){
			senderLog.event = 'R';
		}else{
			senderLog.event = 'r';
		}
		senderLog.acknowledgeNumber=pack.acknowledge;
		senderLog.sequenceNumber=pack.sequenceNumber;
		senderLog.winSize = currentWindowSize;
		strcpy(senderLog.packetType, pack.responseType);
		logSend(senderLog);
		
        if (strcmp("ACK", pack.responseType) == 0 && pack.acknowledge == -1) {
          connected = 1;
		  //Now that the reciever has acked us, we can start sending data.
          DAT(hostInfo, pack);
        }
        pack.responseType = "NONE";
      }
    } else {
		//we have connected, wait untill we recieve a packet
      while (strcmp("NONE", pack.responseType) == 0) {
        FD_ZERO(&watcher);
        FD_SET(sock, &watcher);
        fd_set dup = watcher;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT;
        if (select(sock + 1, &dup, NULL, NULL, &timeout) < 0) {
          perror("select");
          return -1;
        }
        if (FD_ISSET(sock, &dup)) {
          int bytes = recvfrom(sock, buff, PAYLOAD_SIZE + 1, 0, (struct sockaddr*)&hostInfo->clientAddress,&hostInfo->addressSize);
          if (bytes == -1) {
            printf("error in recieving bytes");
          }
		  
		  //parse the packet
          pack = parse(buff);
          if (pack.acknowledge == -1) {
            pack.responseType = "NONE";
          }
		  
		  
        } else if (checkTimer(packetTimeout) == 1) {
          if (closing == 1) {
			if (checkTimer(packetTimeout) == 1){
				 //we havent recieved another FIN, this means the reciever has recieved our ACK and is closed now
				fclose(file);
				logStats(statistics);
				exit(0);
			}
          }
		  //we timed out, reset the window size and send again
		  currentWindowSize=WINDOW_SIZE;
		  pack.sequenceNumber=maxAck;
          DAT(hostInfo, pack);
        }
      }
      if (strcmp("ACK", pack.responseType) == 0) {
		statistics.ACK++;
        if (pack.acknowledge != -1) {
			//keep track of the ACK, this follows clarks solution where we only advertise the window size when we have enough space. The difference is that the sender keeps track.
          if (currentWindowSize < WINDOW_SIZE) 
			  currentWindowSize++;
          if (maxAck < pack.sequenceNumber) {
			  senderLog.event = 'r';
			  maxAck = pack.sequenceNumber;
          }else{
			  senderLog.event = 'R';
		  }
		  strcpy(senderLog.packetType, "ACK");
		  senderLog.acknowledgeNumber = pack.acknowledge;
		  senderLog.winSize = pack.winSize;
		  logSend(senderLog);
          if (currentWindowSize > WINDOW_SIZE-currentWindowSize/2) {
            if (maxAck > pack.sequenceNumber) pack.sequenceNumber = maxAck;
            DAT(hostInfo, pack);
          }
        }
		//if we received a RST flag, we reset the state machine.
      } else if (strcmp("RST", pack.responseType) == 0) {
		strcpy(senderLog.packetType, "RST");
		senderLog.event = 'r';
		logSend(senderLog);
		statistics.RST_RECV++;
		connected=0;
		closing=0;
		continue;
		//if we received a FIN we ACK back and wait for timeout.
      } else if (strcmp("FIN", pack.responseType) == 0) {
		if(finRecvd==0){
			senderLog.event = 'r';
			finRecvd=1;
			logSend(senderLog);
			senderLog.event='s';
		}else{
			senderLog.event = 'R';
			logSend(senderLog);
			senderLog.event='S';
		}
		statistics.FIN++;
		strcpy(senderLog.packetType, "ACK");
		senderLog.sequenceNumber = pack.sequenceNumber;
		senderLog.payloadLength=0;
		senderLog.acknowledgeNumber=0;
		logSend(senderLog);
        gettimeofday(&packetTimeout, NULL);
        closing = 1;
        ACK(hostInfo, pack);
      }
    }
    pack.responseType = "NONE";  // reset the packet;
  }
  return 0;
}
