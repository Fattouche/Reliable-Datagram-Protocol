# Reliable datagram protocol
Alex Fattouche

### Code Design
To compile: make
To run receiver: make receiver
To run sender: ./rdps 192.168.1.100 8080 10.10.1.100 8081 tests/sent.dat

The code was designed so that the packet header followed the recomended description from the assignment. I use multiple structs to keep track
of logging, stats, networkingInfo and the actual packet. The connection establishment is as follows: the sender sends a SYN, the reciever acks
and the sender starts sending data. The data transfer section is implemented using Go Back N, so the reciever only acknowledges the packets that
it wants. The RDP also uses Clarks solution as described in class, Clarks solution is the idea that we only send data to the reciever when its
window size is large enough, this avoids sending single packets over and over again. The reset packet will be sent when the ipaddress the reciever
is receiving from is different than the sender it synchronized with. In order to ensure the packet still gets sent during a reset, all connections
will be reset and sequence numbers will be re established. When the sender is finished sending data, it will send a FIN to the reciever, the reciever 
will then enter into a loop of sending its own FIN packets back to the sender. Once the sender has recieved the FIN, it will ACK and wait for roughly 200
milliseconds, if it does not receive another FIN during this time, it will assume the reciever got its ACK and it will close down.

The logging for each packet has been setup to follow: 
HH:MM:SS.us event_type sip:spt dip:dpt packet_type seqno/ackno length/window
so that regardless of the sender or reciever, it will log the seqno/ack and length/window,
this is the reason zeros appear in sender or reciever logs.

### Code Structure
From top to bottom of the rdpr.c file the code structure is as follows:

1. Create structs, initialize variables.
2. Wait for SYN, ACK the SYN once received.
3. Wait for DAT, check if sequence number is the one we want, ACK and write to file if it is.
4. If FIN recieved, enter while loop and wait for ACK.
5. If ACK recieved, close file and close down.

From top to bottom of the rdps.c file the code structure is as follows:

1. Create structs, initialize variables.
2. Send SYN's, wait for ACK.
3. When ACK Recieved, send DAT until everything sent.
4. When finished sending data, send FIN.
5. Wait for FIN, send ACK when FIN recieved.
6. Wait for 200ms and close if no FIN recieved(means the reciever has recieved our ACK and is closing).

From top to bottom of the helper.c file the code structure is as follows:

1. sendto function for ease.
2. Stringify struct.
3. Parse string.
4. SYN
5. ACK
6. RST
7. logger
8. timer
9. finTimer

### Performance

We can clearly see that when we activate 10% packet loss in both directions, the transfer speed from a 1mb file goes from 0.8 seconds(without packet loss) 
to around 6-8 seconds with packet loss. This is an expected jump in time because there is an increase in latency when packet loss is activated.

