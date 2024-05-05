#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#define PORT_NUM 10004
#define USERNAME_SIZE 50
#define MSG_BUFFER_SIZE 256
#define MSG_SIZE 65536
#define IP_SIZE 30
#define START_MSG '\x02'
#define END_MSG '\x03'
#define HEADER_MSG '\x01'
#define MAX_MSG_Q 160
#define MAX_CHAT_ROOMS 999

typedef struct Message {
    char message[MSG_BUFFER_SIZE];
    char username[USERNAME_SIZE];
    int chat_no;
    char ip_addr[IP_SIZE];
    int message_id;
    struct Message* next_msg;
} Message;

typedef struct ClientInfo{
	char cli_username[USERNAME_SIZE]; //username
	int clisockfd; //for ip addr
	int chat_room_no; //what chat room they're in
	int color; //color of their text
} ClientInfo;

// message head pointer arrays
Message** msg_q;
int num_msg = 0;

int chat_no[MAX_CHAT_ROOMS];

void strcat_char(char* destination, char source) {
    int len = strlen(destination);
    destination[len] = source;
    destination[len + 1] = '\0';
}

void print_all(char* message) {
	for(int i = 0; i < strlen(message) + 1; i++) {
        if(message[i] == HEADER_MSG) 
            printf("|x01|");
		if(message[i] == START_MSG) 
			printf("|x02|");
		if(message[i] == END_MSG)
			printf("|x03|");
		if(message[i] == 0)
			printf("|end|");
		printf("%c", message[i]);
	}

}

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
void create_client_message(char* f_msg, char* username, int chat_no, char* ip_addr, int message_id, int message_order, char* message) {
    memset(f_msg, 0, MSG_BUFFER_SIZE);
    // format strings to have brackets surrounding them

    char b_username[USERNAME_SIZE + 1 + 1];
    char b_ip_addr[IP_SIZE];
    char b_chat_no[2 + 1 + 1];
    char b_message_id[7 + 1 + 1];
    char b_message_order[7 + 1 + 1];

    sprintf(b_username, "[%s", username);
    sprintf(b_chat_no, "[%d", chat_no);
    sprintf(b_ip_addr, "[%s", ip_addr);
    sprintf(b_message_id, "[%d", message_id);
    sprintf(b_message_order, "[%d", message_order);

    // get string lengths
    // size_t usnm_len = strlen(b_username);
    // size_t ip_len = strlen(b_ip_addr);
    // size_t msg_len = strlen(message);
    // size_t chat_no_len = strlen(b_chat_no);
    // size_t message_id_len = strlen(b_message_id);

    // START HEADER

    f_msg[0] = HEADER_MSG;

    strcat(f_msg, b_username);
    
    strcat(f_msg, b_chat_no);

    strcat(f_msg, b_ip_addr);

    strcat(f_msg, b_message_id);

    strcat(f_msg, b_message_order);

    // START MESSAGE

    strcat(f_msg, "\x02");

    strcat(f_msg, message);

    strcat(f_msg, "\x03");
}

/*
Takes f_msg, which is a string sent by the client to decode
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

void decode_server_message(char* username, char* ip_addr, int* color_no, char* message, char* f_msg) {
    size_t f_msg_len = strlen(f_msg);
    char color_str[4];
    memset(color_str, 0, 4);
    // INT MAP 1: USERNAME, 2: COLOR_NO, 3: IP_ADDR, 4: MESSAGE, 0: SOMETHING WENT HORRIBLY WRONG
    int map = 0;
    // counter map 1: USERNAME, 2: COLOR_NO, 3: IP_ADDR
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
            strcat_char(color_str, f_msg[i]);
        } else if (map == 3 && f_msg[i] != '[') {
            strcat_char(ip_addr, f_msg[i]);
        } else if (map == 4 && f_msg[i] != START_MSG) {
            strcat_char(message, f_msg[i]);
        }
    }

    *color_no = atoi(color_str); 
}

void add_message_queue(char* username, int* chat_no, char* ip_addr, int* message_id, char* message, int* message_order) {
    // creates message node
    struct Message* tempMsg = (Message*)malloc(sizeof(Message));
    strcpy(tempMsg->message, message);
    strcpy(tempMsg->username, username);
    tempMsg->chat_no = *chat_no;
    strcpy(tempMsg->ip_addr, ip_addr);
    tempMsg->message_id = *message_id;
    tempMsg->next_msg = NULL;

    // search through message list to see
    if (msg_q == NULL) {
        msg_q = (Message**)malloc(sizeof(Message*) * MAX_MSG_Q);
        msg_q[0] = tempMsg;
        num_msg = 1;
    } else {
        int found = -1;
        for(int i = 0; i < num_msg; i++) {
            if((msg_q[i]->message_id == tempMsg->message_id) && (msg_q[i]->chat_no == tempMsg->chat_no) && (strcmp(msg_q[i]->username, tempMsg->username) == 0)) {
                found = i;
                break;
            }
        }

        if(found >= 0) {
            Message* temp = msg_q[found];
            while(temp->next_msg != NULL) {
                temp = temp->next_msg;
            }
            temp->next_msg = tempMsg;
        } else {
            // linear search thru array to find empty spot
            for(int i = 0; i < MAX_MSG_Q; i++) {
                if(msg_q[i] == NULL) {
                    msg_q[i] = tempMsg;
                    break;
                }
            }
            num_msg++;
        }

    }
}

/*
void* thread_main(void* args)
{

	//-------------------------------
	// Now, we receive/send messages
	char buffer[MSG_BUFFER_SIZE];
	memset(buffer, 0, MSG_BUFFER_SIZE);
	int nsen, nrcv;

    do {
        // recv some message

        // decode client side message

        // add to message queue

        // send message at head of queue

    } while(nrcv > 0);

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
		}
		if (strlen(buffer) == 0 /*&& msg_done == 1) break;
		if (nrcv < 0) error("ERROR recv() failed thread");
	}

	send_all("A user has left");
	close(clisockfd);
	num_clients--;
	//-------------------------------

	return NULL;
}*/

int main(int argc, char** argv) {

    int n;
    struct ifreq ifr;
    char array[] = "eth0";
 
    n = socket(AF_INET, SOCK_DGRAM, 0);
    //Type of address to retrieve - IPv4 IP address
    ifr.ifr_addr.sa_family = AF_INET;
    //Copy the interface name in the ifreq structure
    strncpy(ifr.ifr_name , array , IFNAMSIZ - 1);
    ioctl(n, SIOCGIFADDR, &ifr);
    close(n);
    //display result
    printf("IP Address is %s - %s\n" , array , inet_ntoa(( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr) );


//     char f_msg[MSG_BUFFER_SIZE];
//     char* username = "DiyasBF";
//     int chat_no = 11;
//     char* ip_addr = "127.0.0.1";
//     int message_id = 1234;
//     int message_order = 0;
//     char* message = "omg diya urso hot and funny and i love you sm";

//     create_client_message(f_msg, username, chat_no, ip_addr, message_id, message_order, message);

//     // printf("CLIENT MESSAGE\n");
//     // print_all(f_msg);

//     char username_d[USERNAME_SIZE];
//     int* chat_no_d = (int*)malloc(sizeof(int));
//     char ip_addr_d[20 + 2 + 1];
//     int* message_id_d = (int*)malloc(sizeof(int));
//     int* message_order_d = (int*)malloc(sizeof(int));
//     char message_d[MSG_SIZE];

//    decode_client_message(username_d, chat_no_d, ip_addr_d, message_id_d, message_order_d, message_d, f_msg);
//    printf("\n");
    
// //    printf("MessageID: %d, Username: %s, chat_no: %d, IP: %s, Message_id: %d, Message_order: %d, Message: %s\n", *message_id_d, username_d, *chat_no_d, ip_addr_d, *message_id_d, *message_order_d, message_d);

//     // add to msg q
//     add_message_queue(username_d, chat_no_d, ip_addr_d, message_id_d, message_d, message_order_d);

//     username = "DiyasBF";
//     chat_no = 11;
//     ip_addr = "127.0.0.1";
//     message_id = 1234;
//     message_order = 1;
//     message = "omggg girlfriend soo pretty";

//     create_client_message(f_msg, username, chat_no, ip_addr, message_id, message_order, message);

//     char username_d_1[USERNAME_SIZE];
//     int* chat_no_d_1 = (int*)malloc(sizeof(int));
//     char ip_addr_d_1[20 + 2 + 1];
//     int* message_id_d_1 = (int*)malloc(sizeof(int));
//     int* message_order_d_1 = (int*)malloc(sizeof(int));
//     char message_d_1[MSG_SIZE];

//     decode_client_message(username_d_1, chat_no_d_1, ip_addr_d_1, message_id_d_1, message_order_d_1, message_d_1, f_msg);

//     add_message_queue(username_d_1, chat_no_d_1, ip_addr_d_1, message_id_d_1, message_d_1, message_order_d_1);

//     for(int i = 0; i < num_msg; i++) {
//         Message* temp = msg_q[i];
//         while(temp != NULL) {
//             printf("Message Q: %s\n", temp->message);
//             temp = temp->next_msg;
//         }
//         printf("\nNEXTMSG\n");
//     }

    // printf("\nSERVER MESSAGE\n");

    // char* username_s = "Diya!!";
    // char* ip_addr_s = "127.0.0.2";
    // int color_no_s = 16;
    // char* message_s = "hello wow milian ur so cool and hot and silly fr";
    // create_server_message(f_msg, username_s, ip_addr_s,color_no_s, message_s);

    // print_all(f_msg);
    // printf("\n");

    // char username_d_s[USERNAME_SIZE];
    // char ip_addr_d_s[20+2+1];
    // char message_d_s[MSG_SIZE];
    // int* color_no_d_s = (int*)malloc(sizeof(int));
    // decode_server_message(username_d_s, ip_addr_d_s, color_no_d_s, message_d_s, f_msg);
    // printf("Username: %s, color_no: %d, IP: %s, Message: %s\n", username_d_s, *color_no_d_s, ip_addr_d_s, message_d_s);
    // free(color_no_d_s);
    // free(chat_no_d);
    // free(message_id_d);
    // free(message_order_d);

    // for(int i = 0; i < num_msg; i++) {
    //     Message* temp = msg_q[i];
    //     while(temp != NULL) {
    //         Message* temp2 = temp;

    //         temp = temp2->next_msg;
    //     }
    // }
    // free(msg_q);



    return 0;
}