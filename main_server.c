#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT_NUM 10004
#define USERNAME_SIZE 50
#define MSG_BUFFER_SIZE 256

//--------------- STRUCTS ;D -----------------------------------

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs; //what

//structure to define client info for each unique client
typedef struct ClientInfo{
	char cli_username[USERNAME_SIZE];
	int clisockfd;
	int chat_room_no;
} ClientInfo;

//-------------- GLOBAL VARIABLES :3> --------------------------
int num_clients = 0;
ClientInfo* client_list;

//-------------- FUNCTIONS >:3 ---------------------------------
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

void send_all(const char *msg)
{
	char buffer[MSG_BUFFER_SIZE];
	for(int i = 0; i < num_clients; i++) {
		int msg_len = strlen(msg);
		// calculates number of times will need to send() for buffer size
		int num_sends = msg_len / MSG_BUFFER_SIZE + ((msg_len == MSG_BUFFER_SIZE) ? 0 : 1);
		for(int n = 0; n < num_sends; n++) {
			// gets client fd
			int client_fd = client_list[i].clisockfd;
			printf("%d\n", client_fd);
		}
	}
}

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[MSG_BUFFER_SIZE];
	int nsen, nrcv;

	nrcv = recv(clisockfd, buffer, 256, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		nsen = send(clisockfd, buffer, nrcv, 0);
		if (nsen != nrcv) error("ERROR send() failed");

		nrcv = recv(clisockfd, buffer, 256, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	}

	send_all("GAYS");

	close(clisockfd);
	//-------------------------------

	return NULL;
}

int main(int argc, char** argv) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int username_sock; //for  
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5);

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		//receive username
		char username[USERNAME_SIZE];
		recv(username_sock, username, sizeof(username), 0);
		
		//make new client
		struct ClientInfo new_client;
		strncpy(new_client.cli_username, username, sizeof(new_client.cli_username));
		new_client.clisockfd = newsockfd; //ip addr
		//new_client.chat_room_no = room_no; //which chat room they're in
		//keep track of numbr of clients
		num_clients++;

		printf("%s has connected: %s\n", username, inet_ntoa(cli_addr.sin_addr));

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

    return 0;
}
