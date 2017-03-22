rdp:
	gcc rdps.c helper.c -o rdps -Wall
	gcc rdpr.c helper.c -o rdpr -Wall 

reciever: 
	./rdpr 10.10.1.100 8081 received.dat

sender:	
	./rdps 192.168.1.100 8080 10.10.1.100 8081 tests/main

clean:
	rm rdpr rdps received.dat
