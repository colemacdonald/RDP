all: sws_server.c
	gcc rdp_server.c helper.c -o rdps
	gcc rdp_client.c -o rdpr

clean:
	rm rdps
	rm rdpr