#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>

#define PORT_NUM 10004
#define USERNAME_SIZE 50
#define MSG_BUFFER_SIZE 256
#define MSG_SIZE 65536

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_leave = 0;

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

// THREAD FOR RECEIVING FROM SERVER
void* recv_thread(void* args) {
	
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[MSG_BUFFER_SIZE];
	char message[MSG_SIZE];
	memset(message, 0, MSG_SIZE);
	int nrcv = 0;
	int total_nrcv = 0;

	nrcv = recv(sockfd, buffer, MSG_BUFFER_SIZE, 0);
	if (nrcv < 0) error("ERROR recv() failed");
	while (nrcv > 0 && !client_leave) {
		memset(buffer, 0, MSG_BUFFER_SIZE);
                nrcv = recv(sockfd, buffer, MSG_BUFFER_SIZE, 0);
		total_nrcv += nrcv;
                if (strlen(buffer) == 0) break;
    	        if (nrcv < 0) error("ERROR recv() failed thread");
		// concat buffer to message
		if (total_nrcv < MSG_SIZE) {
			strcat(message, buffer);
		} else {
			printf("Message Too Long: ");
			message[MSG_SIZE - 1] = '\0';
		}

		// check if message contains null terminator
		char* eom = strchr(buffer, '\0');
		if (eom != NULL) {
			printf("SERVER: %s\n", message);
			total_nrcv = 0;
			memset(message, 0, MSG_SIZE);
		}
	}
   
	return NULL;
}

// THREAD FOR SENDING TO SERVER
void* send_thread(void* args) {
	
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);
	
	char buffer[MSG_BUFFER_SIZE];
	int nsend = 0;

	while (1) {
		memset(buffer, 0, MSG_BUFFER_SIZE);
		fgets(buffer, MSG_BUFFER_SIZE, stdin);

		//get rid of \n if empty string
		if (strlen(buffer) == 1) buffer[0] = '\0';

		nsend = send(sockfd, buffer, strlen(buffer), 0);
		if (nsend < 0) error("ERROR writing to socket");
		if (nsend == 0) break;
	}	

        close(sockfd);
	client_leave = 1;

	return NULL;
}

int main(int argc, char *argv[])
{
	if (argc < 2) error("Please speicify hostname");

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct hostent* server = gethostbyname(argv[1]);
	if (server == NULL) error("ERROR, no such host");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((char*)server->h_addr, 
			(char*)&serv_addr.sin_addr.s_addr,
			server->h_length);
	serv_addr.sin_port = htons(PORT_NUM);

	int status = connect(sockfd, 
			(struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	char buffer[MSG_BUFFER_SIZE];
	int n;
		
	//ask for username
	char username[USERNAME_SIZE]; 
	memset(username, 0, USERNAME_SIZE);
	printf("Please enter a username: ");
	scanf("%s", username); //read user name
	//add check is less than username size

	send(sockfd, username, strlen(username), 0);

	//clear input buffer
	int c;
	while ((c = getchar()) != '\n' && c != EOF);

	// create threads and run them
	ThreadArgs* send_args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
	ThreadArgs* recv_args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
	if (send_args == NULL || recv_args == NULL) error("ERROR creating thread arguments");

	send_args->clisockfd = sockfd;
	recv_args->clisockfd = sockfd;

	pthread_t send_tid, recv_tid;
	pthread_create(&recv_tid, NULL, recv_thread, (void*)recv_args);
	pthread_create(&send_tid, NULL, send_thread, (void*)send_args);

	pthread_join(send_tid, NULL);
	pthread_join(recv_tid, NULL);

	return 0;
}
