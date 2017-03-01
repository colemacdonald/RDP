all: sws_server.c
	gcc sws_server.c -o sws

clean:
	rm sws