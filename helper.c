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

//Simply a helper function for sending data back and forth
void sendBack(struct networkingInfo* info, char* response) {
  if (sendto(info->socket, response, PACKET_SIZE, 0, (struct sockaddr*)&info->clientAddress, info->addressSize) == -1) {
  }
}

//stringify our packet
char* stringify(packet src) {
  char* formatted = calloc(PACKET_SIZE + 1, sizeof(char));
  sprintf(formatted, "%s,%d,%d,%d,%d,\n", src.responseType,
          src.sequenceNumber, src.acknowledge, src.payloadLength, src.winSize);
 memcpy(&formatted[strlen(formatted)], src.information, src.payloadLength);
  return formatted;
}

//parse our string into a packet
packet parse(char* data) {
  packet parsed;
  int i = 0;
  int prev = 0;
  int counter = 0;
  while (counter<5) {
    if (data[i] == ',') {
      if (counter == 0) {
        char formatted[4];
        strncpy(formatted, &data[prev], i - prev);
        formatted[3] = '\0';
        parsed.responseType = formatted;
      } else if (counter == 1) {
        char var[i - prev + 1];
        var[i - prev] = '\0';
        strncpy(var, &data[prev], i - prev);
        sscanf(var, "%d", &parsed.sequenceNumber);
      } else if (counter == 2) {
        char var[i - prev + 1];
        var[i - prev] = '\0';
        strncpy(var, &data[prev], i - prev);
        sscanf(var, "%d", &parsed.acknowledge);
      } else if (counter == 3) {
        char var[i - prev + 1];
        var[i - prev] = '\0';
        strncpy(var, &data[prev], i - prev);
        sscanf(var, "%d", &parsed.payloadLength);
      } else if (counter == 4) {
        char var[i - prev + 1];
        var[i - prev] = '\0';
        strncpy(var, &data[prev], i - prev);
        sscanf(var, "%d", &parsed.winSize);
      }
      i++;
      prev = i;
      counter++;
    } else {
      i++;
    }
  }
  prev++;
  i++;
   memcpy(parsed.information, &data[i], parsed.payloadLength);
  parsed.information[parsed.payloadLength] = '\0';
  return parsed;
}

//Send SYN, using a random sequenceNumber
int SYN(struct networkingInfo* info) {
  srand(time(NULL));
  int sequenceNumber = rand() % 1000;  // dont want the sequenceNumber to be too big
  packet synchronization = {"CSC361", "SYN", sequenceNumber, 0, 0, WINDOW_SIZE, ""};
  char* response = stringify(synchronization);
  sendBack(info, response);
  return sequenceNumber;
}

//Send ACK
void ACK(struct networkingInfo* info, packet pack) {
  packet ack = {"CSC361", "ACK", pack.sequenceNumber, pack.acknowledge, 0, pack.winSize, ""};
  char* response = stringify(ack);
  sendBack(info, response);
}

//Send RST
void RST(struct networkingInfo* info, packet pack) {
  packet reset = {"CSC361", "RST", pack.sequenceNumber, 0, 0, WINDOW_SIZE, ""};
  char* response = stringify(reset);
  sendBack(info, response);
}

//Send FIN
void FIN(struct networkingInfo* info, packet pack) {
  packet finish = {
      "CSC361", "FIN", pack.sequenceNumber, pack.acknowledge, 0, pack.winSize, ""};
  char* response = stringify(finish);
  sendBack(info, response);
}

//Log our recieve or send
void logSend(logger log) {
  time_t logTime;
  char timeFormat[25];
  struct tm* timeInfo;
  struct timeval currentTime;  // used for microtime
  gettimeofday(&currentTime, NULL);
  logTime = currentTime.tv_sec;
  timeInfo = localtime(&logTime);
  // strftime example was found at
  // http://stackoverflow.com/questions/3673226/how-to-print-time-in-format-2009-08-10-181754-811
  strftime(timeFormat, 24, "%H:%M:%S", timeInfo);
  printf("%s.%06li %c %s:%d %s:%d %s %d/%d %d/%d\n", timeFormat,
         currentTime.tv_usec, log.event, log.srcIP, log.srcPort, log.destIP,
         log.destPort, log.packetType, log.sequenceNumber,
         log.acknowledgeNumber, log.payloadLength, log.winSize);
}

//Check if our send has timed out
int checkTimer(struct timeval timeout) {
  struct timeval end;
  struct timeval totalTime;
  gettimeofday(&end, NULL);
  timersub(&end, &timeout, &totalTime);
  if (totalTime.tv_usec > TIMEOUT) {
    return 1;
  } else {
    return 0;
  }
}

int checkFinTimer(struct timeval timeout) {
  struct timeval end;
  struct timeval totalTime;
  gettimeofday(&end, NULL);
  timersub(&end, &timeout, &totalTime);
  if (totalTime.tv_usec > TIMEOUT*10) {
    return 1;
  } else {
    return 0;
  }
}
