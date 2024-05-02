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
#define MAX_CLIENTS 16

//--------------- COLOR ARRAY T-T ------------------------------
//static array for colors to print
static int color_Print[16] = {7, 8, 52, 1, 166, 214, 3, 2, 22, 6, 117, 26, 12, 99, 219, 205};
int used_Color[16]; //used to determine if a corresponding color was used
//printf("\033[38;5;%dm", i);

//--------------- STRUCTS ;D -----------------------------------

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs; //what

//structure to define client info for each unique client
typedef struct ClientInfo{
	char cli_username[USERNAME_SIZE]; //username
	int clisockfd; //for ip addr
	int chat_room_no; //what chat room they're in
	int color; //color of their text
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

void strcat_char(char* destination, char source) {
    int len = strlen(destination);
    destination[len] = source;
    destination[len + 1] = '\0';
}

/*
Takes f_msg, which is a string sent by the client to decode
Takes empty string username by reference to load into
Takes chat number by reference
Takes message by reference
Lotsa parsing woohoo
*/
void decode_message(char* username, int* chat_no, char* ip_addr, char* message, char* f_msg) {
    size_t f_msg_len = strlen(f_msg);
    char chat_str[3];
    memset(chat_str, 0, 3);
    // INT MAP 1: USERNAME, 2: CHAT_NO, 3: IP_ADDR, 4: MESSAGE, 0: SOMETHING WENT HORRIBLY WRONG
    int map = 0;
    // counter map 1: USERNAME, 2: CHAT_NO, 3: IP_ADDR
    int bracket_counter = 0; 
    for(int i = 0; i < f_msg_len; i++) {
        if (f_msg[i] == START_MSG) 
            map = 4;
        if (f_msg[i] == '[' && bracket_counter < 3) {
            bracket_counter++;
            map++;
        }
        if (map == 1 && f_msg[i] != '[') {
            strcat_char(username, f_msg[i]);
        } else if (map == 2 && f_msg[i] != '[') {
            strcat_char(chat_str, f_msg[i]);
        } else if (map == 3 && f_msg[i] != '[') {
            strcat_char(ip_addr, f_msg[i]);
        } else if (map == 4 && f_msg[i] != START_MSG) {
            strcat_char(message, f_msg[i]);
        }
    }

    *chat_no = atoi(chat_str); 
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
			
			//copy 256 bytes into buffer
			memset(buffer, 0, MSG_BUFFER_SIZE);	
			memcpy(buffer, msg + (n * MSG_BUFFER_SIZE), MSG_BUFFER_SIZE);
			//sends message to clients
			send(client_fd, buffer, ((n == num_sends - 1) ? msg_len % MSG_BUFFER_SIZE : MSG_BUFFER_SIZE), 0);
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
	memset(buffer, 0, MSG_BUFFER_SIZE);
	int nsen, nrcv;
	// msg_done = 0 : a message is still sending
	// msg_done = 1 : message is already done
	int msg_done = 0;

	nrcv = recv(clisockfd, buffer, 256, 0);
	if (nrcv < 0) error("ERROR recv() failed");

	while (nrcv > 0) {
		nsen = send(clisockfd, buffer, nrcv, 0);
		if (nsen != nrcv) error("ERROR send() failed");

		memset(buffer, 0, MSG_BUFFER_SIZE);
		nrcv = recv(clisockfd, buffer, MSG_BUFFER_SIZE, 0);
		/*char* eom = strchr(buffer, '\0');
		if (eom != NULL) {
			msg_done = 0;
		} else {
			msg_done = 1;
		}*/
		if (strlen(buffer) == 0 /*&& msg_done == 1*/) break;
		if (nrcv < 0) error("ERROR recv() failed thread");
	}

	send_all("A user has left");
	close(clisockfd);
	num_clients--;
	//-------------------------------

	return NULL;
}

int main(int argc, char** argv) {
	srand(time(NULL)); //random number generator for colors

	client_list = (struct ClientInfo*)malloc(sizeof(struct ClientInfo) * MAX_CLIENTS);
	
	// CREATE SERVER SOCKET
    	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	
	// SET SOCKET OPTIONS
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	serv_addr.sin_port = htons(PORT_NUM);
	
	// BIND SOCKET TO ADDRESS SPACE
	int status = bind(sockfd, 
			(struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");
	
	// LISTEN FOR CLIENTS
	listen(sockfd, 5);

	while(1) {
		// ACCEPT PENDING CLIENTS
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		//receive username
		char username[USERNAME_SIZE];
		memset(username, 0, USERNAME_SIZE);
		int username_success = recv(newsockfd, username, USERNAME_SIZE, 0);
		if (username_success < 0) error("ERROR failed to get username");

		//make new client
		struct ClientInfo new_client;
		int client_color = rand() % 16;
		do{ //know y'all haven't seen a do while in a hot second
			client_color++; //random number between 0 - 15
		} while(used_Color[client_color]); //will loop again if index is used
		used_Color[client_color] = 1; //mark as used
		new_client.color = client_color;

		strncpy(new_client.cli_username, username, sizeof(new_client.cli_username));
		new_client.clisockfd = newsockfd; //ip addr
		//new_client.chat_room_no = room_no; //which chat room they're in
		//append client to list
		client_list[num_clients++] = new_client;

		if (num_clients > 1) {
			char connected_str[3 + USERNAME_SIZE + 500];
			sprintf(connected_str, "%d: %s has connected: %s\n", num_clients, username, inet_ntoa(cli_addr.sin_addr));
			send_all(connected_str);		
		}

		printf("%d: %s has connected: %s\n", num_clients, username, inet_ntoa(cli_addr.sin_addr));

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
				
		args->clisockfd = newsockfd;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

    return 0;
}
