all:
	gcc -o main_client main_client.c
	gcc -o main_server main_server.c

clean:
	rm main_client
	rm main_server
