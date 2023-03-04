#include <stdio.h>
#include <unistd.h>
#include <string.h>

FILE *fs;

char logbuf[1024];

/* format of log strings
    <username>: message
    <username> -><new_username>
    ban:<username>
    kick:<username>
    connect:<username>
    disconnect:<username>
*/

//opes log file and puts cursor at the end
void init_logging(){
    fs = fopen("log.txt","a");
    fseek(fs,0,SEEK_END);
    logbuf[0] = '\0';
    return;
}

void connection_event(char *argument){
    strcat(logbuf,"connect:");
    strcat(logbuf,argument);
    strcat(logbuf,"\n");

    fwrite(logbuf,sizeof(char),strlen(logbuf),fs);
    logbuf[0] = '\0';
    return;
}

void disconnection_event(char *argument){
    strcat(logbuf,"disconnect:");
    strcat(logbuf,argument);
    strcat(logbuf,"\n");

    fwrite(logbuf,sizeof(char),strlen(logbuf),fs);
    logbuf[0] = '\0';
    return; 
}

void nick_event(char *old_nick,char *new_nick){ 
    strcat(logbuf,old_nick);
    strcat(logbuf,"->");
    strcat(logbuf,new_nick);
    strcat(logbuf,"\n");

    fwrite(logbuf,sizeof(char),strlen(logbuf),fs);
    logbuf[0] = '\0';   
    return;
}

void ban_event(char *banned_nick){
    strcat(logbuf,"ban:");
    strcat(logbuf,banned_nick);
    strcat(logbuf,"\n");

    fwrite(logbuf,sizeof(char),strlen(logbuf),fs);
    logbuf[0] = '\0';
    return;
}

void message_event(char *sender,char *message){
    strcat(logbuf,sender);
    strcat(logbuf,":");
    strcat(logbuf,message);
    strcat(logbuf,"\n");

    fwrite(logbuf,sizeof(char),strlen(logbuf),fs);
    logbuf[0] = '\0';
    return;
}

void quit_logging(){
    fclose(fs);
    return;
}