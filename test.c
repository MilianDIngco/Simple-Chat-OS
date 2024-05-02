#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_NUM 10004
#define USERNAME_SIZE 50
#define MSG_BUFFER_SIZE 256
#define MSG_SIZE 65536
#define START_MSG '\x02'
#define END_MSG '\x03'
#define HEADER_MSG '\x01'

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

int main(int argc, char** argv) {

    char f_msg[MSG_BUFFER_SIZE];
    char* username = "DiyasBF";
    int chat_no = 11;
    char* ip_addr = "127.0.0.1";
    char* message = "Test message";

    create_message(f_msg, username, chat_no, ip_addr, message);

    print_all(f_msg);

    char username_d[USERNAME_SIZE];
    int* chat_no_d = (int*)malloc(sizeof(int));
    char ip_addr_d[20 + 2 + 1];
    char message_d[MSG_SIZE];

    decode_message(username_d, chat_no_d, ip_addr_d, message_d, f_msg);
    printf("\n");
    
    printf("Username: %s, chat_no: %d, IP: %s, Message: %s\n", username_d, *chat_no_d, ip_addr_d, message_d);

    free(chat_no_d);

    return 0;
}