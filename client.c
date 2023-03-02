// first stage: client program that sends hello to the server
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <error.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#define MSG_BUF_SIZE 1024

char help_cmd[] = "\\help";
char help_text[] = "help reference";


//returns message length or -1 if msg is too long
int input_msg(char msg[]){
    int input_char;
    int msg_len = 0;
    int letter_printed = 0;

    while ((input_char = getchar())!=EOF && (input_char!='\n')){

        if ((input_char == ' ') && (letter_printed == 0))
            continue; // ignoring whitespaces at the beginning of the word

        letter_printed = 1;

        msg[msg_len] = (char) input_char;
        msg_len++;
        if (msg_len>MSG_BUF_SIZE-2)
            return -1; 
    }
    
    msg[msg_len] = '\0';
    return msg_len;
}

int main(int argc,char *argv[]){
    int sock;
    int msg_len;
    int recv_msg_len;
    unsigned short port_num; 
    struct sockaddr_in addr;
    
    char in_msg_buf[MSG_BUF_SIZE];
    char out_msg_buf[MSG_BUF_SIZE];
    pid_t recv_process_pid;

    if (argc <= 1){
        printf("Not enough arguments\n");
        return 1;
    }
    port_num = (unsigned short) atoi(argv[1]);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(2);
    }

    printf("Type some text and press ENTER to send message to the server\n");
    printf("%s - to get help\n", help_cmd);

    // creating separate process responsible for receiving messages
    if ((recv_process_pid = fork()) == 0)
    {
        while (1)
        {
            recv_msg_len = recv(sock, in_msg_buf, MSG_BUF_SIZE, 0);
            if (recv_msg_len == 0){
                kill(getppid(),SIGKILL);
                printf("SERVER ACCESS FAILED!\n");
                fflush(stdout);
                break;
            }
            
            printf("%s\n",in_msg_buf);
        }
        _exit(0);
    }

    //TODO: check for server disconnection
    //TODO: check message len
    while (1)
    {

        msg_len = input_msg(out_msg_buf);

        if (msg_len == 0){
            printf("EMPTY MESSAGE!\n");
            continue;
        }

        if (msg_len == -1){
            printf("MSG TOO LONG!\n");
            continue;
        }
        
        if (strcmp(out_msg_buf, help_cmd) == 0)
            printf("%s", help_text);

        else
            send(sock, out_msg_buf, msg_len+1, 0); // adding 1 to msg_len in order to include \0
        
    }

    return 0;
}
