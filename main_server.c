#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define PORT_NUM 10005
#define USERNAME_SIZE 50
#define MSG_BUFFER_SIZE 256
#define MAX_CLIENTS 16
#define MSG_SIZE 65536
#define IP_SIZE 30
#define START_MSG '\x02'
#define END_MSG '\x03'
#define HEADER_MSG '\x01'

//--------------- COLOR ARRAY T-T ------------------------------
//static array for colors to print
static int color_Print[16] = {52, 1, 166, 214, 3, 2, 22, 6, 117, 26, 12, 54, 99, 219, 205, 126};
int used_Color[16] = { 0 }; //used to determine if a corresponding color was used
//printf("\033[38;5;%dm", i);

//--------------- STRUCTS ;D -----------------------------------

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs; //what

//structure to define client info for each unique client
typedef struct ClientInfo{
	char cli_username[USERNAME_SIZE]; //username
	int clisockfd;
	char cli_ip_iddr[IP_SIZE]; //ip addr
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
Takes an f_msg, which is a string generated by the create_client_message function
Takes empty string username by reference to load into
Takes chat number by reference
Takes message by reference
Lotsa parsing woohoo
*/
void decode_client_message(char* username, int* chat_no, char* ip_addr, int* message_id, int* message_order, char* message, char* f_msg) {
    size_t f_msg_len = strlen(f_msg);
    char chat_str[3];
    char message_id_str[7];
    char message_order_str[7];
    memset(chat_str, 0, 3);
    memset(message_id_str, 0, 7);
    memset(message_order_str, 0, 7);
	memset(username, 0, USERNAME_SIZE);
	memset(ip_addr, 0, IP_SIZE);
	memset(message, 0, MSG_BUFFER_SIZE);
    // INT MAP 1: USERNAME, 2: CHAT_NO, 3: IP_ADDR,  4: message_id, 5: message_order, 6: MESSAGE, 0: SOMETHING WENT HORRIBLY WRONG
    int map = 0;
    // counter map 1: USERNAME, 2: CHAT_NO, 3: IP_ADDR, 4: MESSAGE_ID, 5: MESSAGE_ORDER
    int bracket_counter = 0; 
    for(int i = 0; i < f_msg_len; i++) {
        if (f_msg[i] == START_MSG) 
            map = 6;
        if (f_msg[i] == '[' && bracket_counter < 5) {
            bracket_counter++;
            map++;
        }
        if (map == 1 && f_msg[i] != '[') {
            strcat_char(username, f_msg[i]);
        } else if (map == 2 && f_msg[i] != '[') {
            strcat_char(chat_str, f_msg[i]);
        } else if (map == 3 && f_msg[i] != '[') {
            strcat_char(ip_addr, f_msg[i]);
        } else if (map == 4 && f_msg[i] != '[') {
            strcat_char(message_id_str, f_msg[i]);
        } else if (map == 5 && f_msg[i] != '[') {
            strcat_char(message_order_str, f_msg[i]);
        } else if (map == 6 && f_msg[i] != START_MSG) {
            strcat_char(message, f_msg[i]);
        }
    }

    *chat_no = atoi(chat_str); 
    *message_id = atoi(message_id_str);
    *message_order = atoi(message_order_str);
}

/*
Takes an empty string that will hold the generated server message string
Takes a username parameter, ip address, color number, and message
*/
void create_server_message(char* f_msg, char* username, char* ip_addr, int color_no, char* message) {
    memset(f_msg, 0, MSG_BUFFER_SIZE);
    // format strings to have brackets surrounding them

    char b_username[USERNAME_SIZE + 2 + 1];
    char b_ip_addr[IP_SIZE + 2 + 1];
    char b_color_no[3 + 2 + 1];

    sprintf(b_username, "[%s", username);
    sprintf(b_color_no, "[%d", color_no);
    sprintf(b_ip_addr, "[%s", ip_addr);

    // get string lengths
    size_t usnm_len = strlen(b_username);
    size_t ip_len = strlen(b_ip_addr);
    size_t msg_len = strlen(message);
    size_t chat_no_len = strlen(b_color_no);

    // START HEADER

    f_msg[0] = HEADER_MSG;

    strcat(f_msg, b_username);
    
    strcat(f_msg, b_color_no);

    strcat(f_msg, b_ip_addr);

    // START MESSAGE

    strcat(f_msg, "\x02");

    strcat(f_msg, message);

    strcat(f_msg, "\x03");
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

	do {
		// RECV()
		memset(buffer, 0, MSG_BUFFER_SIZE);
		nrcv = recv(clisockfd, buffer, MSG_BUFFER_SIZE, 0);
		if (nrcv < 0) error("ERROR recv() failed");
	
		//vars for decoding client_message
		char username_d[USERNAME_SIZE];
		int* chat_no_d = (int*)malloc(sizeof(int));
		char ip_addr_d[20 + 2 + 1];
		int* message_id_d = (int*)malloc(sizeof(int));
		int* message_order_d = (int*)malloc(sizeof(int));
		//char message_d[MSG_BUFFER_SIZE];
		char msg_d[MSG_BUFFER_SIZE];

		// DECODE CLIENT MESSAGE
		decode_client_message(username_d, chat_no_d, ip_addr_d, message_id_d, message_order_d, msg_d, buffer);

		int color_no; //preparing to send color for client
		for(int i = 0; i < num_clients; i++) {
			if((strcmp(client_list[i].cli_username, username_d) == 0)){
				color_no = client_list[i].color;
				printf("FOUND COLOR\n");
			}
			printf("client: %s\n", client_list[i].cli_username);
			printf("usnm: %s\n", username_d);
		}

		// CREATE SERVER MESSAGE
		memset(buffer, 0, MSG_BUFFER_SIZE);
		create_server_message(buffer, username_d, ip_addr_d, color_no, msg_d);

		nsen = send(clisockfd, buffer, nrcv, 0);
		if (nsen != nrcv) error("ERROR send() failed");

		send_all(buffer);

		/*char* eom = strchr(buffer, '\0');
		if (eom != NULL) {
			msg_done = 0;
		} else {
			msg_done = 1;
		}*/
		if (strlen(buffer) == 0 /*&& msg_done == 1*/) break;
		if (nrcv < 0) error("ERROR recv() failed thread");
	} while (nrcv > 0);

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
		int client_color;
		int color_count = 0;
		do{ //know y'all haven't seen a do while in a hot second
			color_count++;
			if(color_count >= 16){
				//all colors have been used, must print error message;
				printf("Error: All available colors have been used.\n");
				exit(1);
			}
			else{
				client_color = rand() % 16; //random number between 0 - 15
			}
		} while(used_Color[client_color]); //will loop again if index is used
		used_Color[client_color] = 1; //mark as used
		new_client.color = color_Print[client_color];

		strncpy(new_client.cli_username, username, sizeof(new_client.cli_username));
		new_client.clisockfd = newsockfd; //ip addr
		strcpy(new_client.cli_ip_iddr, inet_ntoa(cli_addr.sin_addr));
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

