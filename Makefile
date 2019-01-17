
PORT=54461
CFLAGS= -DPORT=\$(PORT) -g -Wall -std=gnu99 
#CFLAGS = -g -Wall -std=gnu99

all: hcq_server 
hcq_server: hcq_server.o hcq.o
	gcc ${CFLAGS} -o $@ $^
	
%.o: %.c socket.h hcq.h
	gcc ${CFLAGS} -c $<
	
clean: 
	rm *.o hcq_server 
