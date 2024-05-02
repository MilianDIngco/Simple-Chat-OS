/*
TO ADD
USERNAME CANNOT INCLUDE [
*/

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
#define START_MSG '\x02'
#define END_MSG '\x03'
#define HEADER_MSG '\x01'

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

void strcat_char(char* destination, char source) {
    int len = strlen(destination);
    destination[len] = source;
    destination[len + 1] = '\0';
}

void print_all(char* message) {
	for(int i = 0; i < strlen(message) + 1; i++) {
		if(message[i] == START_MSG) 
			printf("|x02|");
		if(message[i] == END_MSG)
			printf("|x03|");
		if(message[i] == 0)
			printf("|end|");
		printf("%c", message[i]);
	}

}

/* TAKES 
 * buffer to fill
 * str of characters
 * start index of buffer you want to append from
 * length of characters you want to copy
 */
void fill_buffer(char* buffer, char* str, int start, int length, int str_start) {
	for (int i = start; i < start + length; i++) {
		if (i >= MSG_BUFFER_SIZE) {
			printf("OUTSIDE BOUNDS");
			break;
		}
		buffer[i] = str[i - start + str_start];	
	}
	buffer[start + length] = '\0';
}

/*
final_message - an empty string you will use to send the final string to the server
username - client's username
chat_no - chat number
ip_addr - clients ip
message - string message you want to send
*/
void create_message(char* f_msg, char* username, int chat_no, char* ip_addr, char* message) {
    memset(f_msg, 0, MSG_BUFFER_SIZE);
    // format strings to have brackets surrounding them

    char b_username[USERNAME_SIZE + 2 + 1];
    char b_ip_addr[20 + 2 + 1];
    char b_chat_no[2 + 2 + 1];

    sprintf(b_username, "[%s", username);
    sprintf(b_chat_no, "[%d", chat_no);
    sprintf(b_ip_addr, "[%s", ip_addr);

    // get string lengths
    size_t usnm_len = strlen(b_username);
    size_t ip_len = strlen(b_ip_addr);
    size_t msg_len = strlen(message);
    size_t chat_no_len = strlen(b_chat_no);

    // START HEADER

    f_msg[0] = HEADER_MSG;

    strcat(f_msg, b_username);
    
    strcat(f_msg, b_chat_no);

    strcat(f_msg, b_ip_addr);

    // START MESSAGE

    strcat(f_msg, "\x02");

    strcat(f_msg, message);

    strcat(f_msg, "\x03");
}

// THREAD FOR RECEIVING FROM SERVER
void* recv_thread(void* args) {

	printf("RECV \n");

	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	char buffer[MSG_BUFFER_SIZE + 1];
	char message[MSG_SIZE];
	memset(message, 0, MSG_SIZE);
	int nrcv = 0;
	int total_nrcv = 0;

	do {
		memset(buffer, 0, MSG_BUFFER_SIZE);
        nrcv = recv(sockfd, buffer, MSG_BUFFER_SIZE, 0);
		buffer[MSG_BUFFER_SIZE] = '\0';
		total_nrcv += nrcv;
        if (strlen(buffer) == 0) break;
    	if (nrcv < 0) error("ERROR recv() failed thread");
		// concat buffer to message
		if (total_nrcv < MSG_SIZE) {
			strcat(message, buffer);
		} else {
			message[MSG_SIZE - 1] = '\0';
		}

		// check if message contains end character
		char* eom = strchr(buffer, END_MSG);
		if (eom != NULL) {
			printf("\nSERVER: %s\n", message);
			total_nrcv = 0;
			memset(message, 0, MSG_SIZE);
		}
	} while (nrcv > 0 && !client_leave);
   
	return NULL;
}

// THREAD FOR SENDING TO SERVER
void* send_thread(void* args) {
	
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);
	
	char buffer[MSG_BUFFER_SIZE];
	char message[MSG_SIZE];
	memset(message, 0, MSG_SIZE);
	int nsend = 0;

	while (1) {
		// GET FULL MESSAGE	
		fgets(message, MSG_SIZE, stdin);

		int msg_len = strlen(message);
		int bytes_sent = 0; 
	
		// HANDLE EXIT CASE
		//get rid of \n if empty string
		if (strlen(message) == 1) {
			buffer[0] = '\0';
			send(sockfd, buffer, strlen(buffer), 0);
			break;
		} 

		//get rid of new line
		message[--msg_len] = '\0';

		while (bytes_sent < msg_len + 1) 
		{
			// load buffer
			// if first buffer sent, first character is message start
			memset(buffer, 0, MSG_BUFFER_SIZE);
			if (bytes_sent == 0) 
			{
				buffer[0] = START_MSG;
				if (msg_len <= MSG_BUFFER_SIZE - 3) 
				{
					// fills buffer up to index 254; saves buffer[255] for msg_end character
					fill_buffer(buffer, message, 1, msg_len, 0);
					buffer[msg_len + 1] = END_MSG;	// ADD 1 TO THIS IF CHARACTERS ARE CUT OFF
					buffer[msg_len + 2] = '\0';
					bytes_sent = msg_len + 1;
				} else {
					// fills buffer up to index 255
					fill_buffer(buffer, message, 1, MSG_BUFFER_SIZE - 2, 0); 
					// this is fine because its guaranteed that this buffer will be totally filled since the message length > buffer size
					bytes_sent += (MSG_BUFFER_SIZE - 2);
				}
			} else {
				// if end of message
				if (msg_len - bytes_sent - 1 <= MSG_BUFFER_SIZE - 2) 
				{
					fill_buffer(buffer, message, 0, msg_len - bytes_sent, bytes_sent);
					buffer[msg_len - bytes_sent] = END_MSG;
					buffer[msg_len - bytes_sent + 1] = '\0';
					bytes_sent = msg_len + 1;
				} else {
					// bytes sent here also fill buffer entirely
					fill_buffer(buffer, message, 0, MSG_BUFFER_SIZE - 1, bytes_sent);
					buffer[MSG_BUFFER_SIZE - 1] = '\0';
					bytes_sent += MSG_BUFFER_SIZE - 1;
				}
			}
			nsend = send(sockfd, buffer, strlen(buffer), 0);
			if (nsend < 0) error("ERROR writing to socket");
		}
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
