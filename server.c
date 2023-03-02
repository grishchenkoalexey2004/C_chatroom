/*first stage: server single-threaded program that establishes connection with any client
and sends "yes" message after receiving any data from the client*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <signal.h>

#define MAX_CLIENT_NUM 2
#define MSG_BUF_SIZE 1024

#define MAX_NAME_LEN MSG_BUF_SIZE

typedef struct client_info client_info;

// todo : need dynamicly stored name field
struct client_info
{
	char *name;
	char *contact_arr[MAX_CLIENT_NUM];
	char status;
	char last_msg[MSG_BUF_SIZE]; // last sent message
	int sock_fd;
	int name_set;	  // equals 0 if authentication hasn't been performed yet and we are expecting to receive players name
	int sends_passw;  // equals 1 if server expects to get password from the user with the next message
	int is_connected; // equals 1 if connected
};

char ADMIN_PASSWORD[] = "1234";
static const char ADMIN_STATUS = 'a';
static const char CLIENT_STATUS = 'u';
char announcement_buf[MSG_BUF_SIZE];

// commands
void users_cmd(int caller_id);
void private_cmd(int caller_id);
void privates_cmd(int caller_id);
void nick_cmd(int caller_id);
void quit_cmd(int caller_id);
void ask_password(int client_number);
void ban_cmd(int caller_id);
void kick_cmd(int caller_id);
void shutdown_cmd(int caller_id);

typedef struct cmd_struct
{
	char *cmd;
	void (*fp)(int);
	// firstargument - id of the user, who invoked the command

} cmd_struct;

// TODO: organize these arrays as a structure with fields: cmd_name,function to execute
cmd_struct USER_CMDS[] = {
	{"\\users", &users_cmd},
	{"\\nick", &nick_cmd},
	{"\\admin", &ask_password},
	{"\\quit", &quit_cmd},
	{"\\private", &private_cmd},
	{"\\privates", &privates_cmd},
	{NULL, NULL}};

cmd_struct ADMIN_CMDS[] = {
	{"\\quit", &quit_cmd},
	{"\\users", &users_cmd},
	{"\\ban", &ban_cmd},
	{"\\kick", &kick_cmd},
	{"\\nick", &nick_cmd},
	{"\\shutdown", &shutdown_cmd},
	{"\\private", &private_cmd},
	{"\\privates", &privates_cmd},
	{NULL, NULL}};

int banlist_len = 0;

char **banlist;

int listener; // listener sock

client_info clients[MAX_CLIENT_NUM]; // client db

void kill_server(int sig)
{
	printf("SERVER KILLED\n");
	fflush(stdout);
	close(listener);
	for (int i = 0; i < MAX_CLIENT_NUM; i++)
	{
		if (clients[i].is_connected == 1)
			close(clients[i].sock_fd);
	}

	_exit(0);
}

int is_banned(char *name)
{
	int k = 0;
	while (banlist[k] != NULL)
	{
		if (strcmp(name, banlist[k]) == 0)
			return 1;
		k++;
	}
	return 0;
}

int is_connected(int client_num)
{
	return clients[client_num].is_connected;
}

// returns user_id by his name, or -1 if user wasn't found
int get_user_id(char *username)
{
	for (int i = 0; i < MAX_CLIENT_NUM; i++)
	{
		if (clients[i].is_connected && clients[i].name != NULL && strcmp(clients[i].name, username) == 0)
			return i;
	}
	return -1;
}

// procedures that provide sending interface

void send_to_client(int client_num, char msg[], int put_star)
{
	char msg_copy[MSG_BUF_SIZE];
	strcpy(msg_copy, msg);

	if (put_star)
	{
		memmove(msg_copy + 1, msg_copy, 1023 * sizeof(char));
		msg_copy[0] = '*';
	}

	send(clients[client_num].sock_fd, msg_copy, strlen(msg_copy) + 1, 0);
	return;
}

// sends message (that corresponds to server-client interface) to all clients connected to the server
void send_all(char msg[], int put_star)
{
	for (int i = 0; i < MAX_CLIENT_NUM; i++)
	{
		if (is_connected(i))
			send_to_client(i, msg, put_star);
	}
	return;
}

void send_except(int exception_client, char msg[], int put_star)
{
	for (int client_index = 0; client_index < MAX_CLIENT_NUM; client_index++)
	{
		if (client_index != exception_client && is_connected(client_index))
			send_to_client(client_index, msg, put_star);
	}
	return;
}

// initializes listener socket and binds it to the predefined port
void init_listener_socket(unsigned short port_num)
{
	// AF_INET defines protocol family used for communication
	// SOCK_STREAM param defines socket with pre-established connection
	int opt = 1;

	struct sockaddr_in addr;
	listener = socket(AF_INET, SOCK_STREAM, 0);

	if (listener < 0)
	{
		perror("socket");
		exit(1);
	}

	// binding socket with a port
	// INADDR_ANY will bind socket to current ip address of the laptop
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port_num);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		exit(2);
	}

	listen(listener, 1);
	return;
}

// these procedures compose announcement message and write it to buffer
void create_connection_announcement(int client_num, int is_connection)
{
	announcement_buf[0] = '\0';
	strcat(announcement_buf, "***");
	strcat(announcement_buf, clients[client_num].name);
	if (is_connection)
		strcat(announcement_buf, " CONNECTED");
	else
		strcat(announcement_buf, " DISCONNECTED");
	return;
}

void clear_msg_buf(char msg[])
{
	msg[0] = '\0';
	return;
}

// finds disconnected client profile and returns its index
int find_free_struct()
{
	for (int i = 0; i < MAX_CLIENT_NUM; i++)
	{
		if (clients[i].is_connected == 0)
		{
			clients[i].is_connected = 1;
			return i;
		}
	}

	return -1;
}

void disconnect_client(int client_number, char *disconnection_msg)
{
	int put_star = 0;

	clients[client_number].is_connected = 0;
	clients[client_number].name_set = 0;
	clients[client_number].sends_passw = 0;
	clients[client_number].status = CLIENT_STATUS;

	for (int k = 0; k < MAX_CLIENT_NUM; k++)
	{
		clients[client_number].contact_arr[k] = NULL;
	}

	free(clients[client_number].name);
	close(clients[client_number].sock_fd);
	send_all(disconnection_msg, put_star);
	return;
}

// sends nickname input_prompt
void ask_nickname(int client_number)
{
	int put_star = 0;
	send_to_client(client_number, "ENTER YOUR NICKNAME!", put_star);
	return;
}

void ask_password(int client_number)
{
	int put_star = 0;
	clients[client_number].sends_passw = 1;
	send_to_client(client_number, "ENTER ADMIN PASSWORD!", put_star);
	return;
}

// returns 1 if user has given correct admin password, 0 if not
int verify_password(char msg[])
{
	if (strcmp(msg, ADMIN_PASSWORD) == 0)
		return 1;
	return 0;
}

int is_admin(int client_num)
{
	return (clients[client_num].status == ADMIN_STATUS);
}


void send_auth_success(int client_num)
{
	int put_star = 0;
	send_to_client(client_num, "NOW YOU HAVE ADMIN STATUS!", put_star);
	return;
}

void send_auth_fail(int client_num)
{
	send_to_client(client_num, "ADMIN AUTHENTICATION FAILED!\nPRINT \"\\admin \" AND TRY AGAIN", 0);
	return;
}

// not tested
void add_to_banlist(char *ban_name)
{
	char *name_mem = (char *)malloc((strlen(ban_name) + 1) * sizeof(char));
	strcpy(name_mem, ban_name);
	banlist_len += 1;
	banlist = realloc(banlist, (banlist_len + 1) * sizeof(char *));
	banlist[banlist_len - 1] = name_mem;
	banlist[banlist_len] = NULL;
	return;
}

// user cmds definition
void users_cmd(int caller_id)
{
	char response_buf[MSG_BUF_SIZE];
	response_buf[0] = '\0';

	for (int i = 0; i < MAX_CLIENT_NUM; i++)
	{
		if (is_connected(i))
		{
			strcat(response_buf, clients[i].name);
			strcat(response_buf, "\n");
		}
	}

	send_to_client(caller_id, response_buf, 0);
	return;
}

// writes username to the list of private contacts of user_id
void add_private_contact(int user_id, char *username)
{
	int k = 0;
	char **contacts = clients[user_id].contact_arr;
	while (contacts[k] != NULL)
	{
		if (strcmp(contacts[k], username) == 0)
			return;
		k++;
	}
	clients[user_id].contact_arr[k] = malloc((strlen(username) + 1) * sizeof(char));
	strcpy(clients[user_id].contact_arr[k], username);
	return;
}

void private_cmd(int caller_id){
	int recipient_id;
	char input_copy[MSG_BUF_SIZE];
	strcpy(input_copy, clients[caller_id].last_msg);

	char *cmd = strtok(input_copy, " ");
	char *recepient_username = strtok(NULL, " ");
	char *message = strtok(NULL, " ");

	if (recepient_username == NULL || message == NULL)
	{
		send_to_client(caller_id, "NOT ENOUGH ARGUMENTS FOR THE COMMAND!", 0);
		return;
	}

	recipient_id = get_user_id(recepient_username);
	if (recipient_id == -1)
	{
		send_to_client(caller_id, "USER WITH THIS USERNAME DOENS'T EXIST!", 0);
		return;
	}

	send_to_client(recipient_id, message, 1);
	add_private_contact(caller_id, recepient_username);
	return;
}

void privates_cmd(int caller_id)
{
	int k = 0;
	char **contacts = clients[caller_id].contact_arr;
	char response_buf[MSG_BUF_SIZE];
	response_buf[0] = '\0';

	while (contacts[k] != NULL)
	{
		strcat(response_buf, contacts[k]);
		strcat(response_buf, "\n");
		k++;
	}

	if (k == 0)
	{
		send_to_client(caller_id, "PRIVATE CONTACTS LIST EMPTY!", 0);
		return;
	}

	send_to_client(caller_id, response_buf, 0);
	return;
}

void ban_cmd(int caller_id)
{
	char input_copy[MSG_BUF_SIZE];
	int ban_id;

	strcpy(input_copy, clients[caller_id].last_msg);

	char *cmd = strtok(input_copy, " ");
	char *first_arg = strtok(NULL, " ");  // nickname
	char *second_arg = strtok(NULL, " "); // msg

	if (first_arg == NULL || second_arg == NULL)
	{
		send_to_client(caller_id, "NOT ENOUGH ARGUMENTS FOR THE COMMAND!", 0);
		return;
	}

	get_user_id(first_arg);
	ban_id = get_user_id(first_arg);

	if (ban_id != -1 && clients[ban_id].status == 'a'){
		send_to_client(caller_id,"YOU CANNOT AFFECT ADMIN",0);
		return; // admin cannot in any way affect another admin

	}

	add_to_banlist(clients[ban_id].name);

	announcement_buf[0] = '\0';
	strcat(announcement_buf, "*** USER ");
	strcat(announcement_buf, first_arg);
	strcat(announcement_buf, " HAS BEEN BANNED!");
	printf("USER %s HAS BEEN BANNED\n", first_arg);

	if (ban_id != -1)
	{
		send_to_client(ban_id, second_arg, 1);
		disconnect_client(ban_id, announcement_buf);
	}
	else
		send_all(announcement_buf, 0);
	return;
}

void kick_cmd(int caller_id)
{
	char input_copy[MSG_BUF_SIZE];
	int kick_id;

	strcpy(input_copy, clients[caller_id].last_msg);

	char *cmd = strtok(input_copy, " ");
	char *first_arg = strtok(NULL, " ");
	char *second_arg = strtok(NULL, " ");

	if (first_arg == NULL || second_arg == NULL)
	{
		send_to_client(caller_id, "NOT ENOUGH ARGUMENTS FOR THE COMMAND!", 0);
		return;
	}

	kick_id = get_user_id(first_arg);

	if (kick_id == -1)
	{
		send_to_client(caller_id, "NO SUCH USER!", 0);
		return;
	}

	if (clients[kick_id].status == 'a'){
		send_to_client(caller_id,"YOU CANNOT AFFECT ADMIN",0);
		return; // admins cannot kick other admins
	}

	send_to_client(kick_id, second_arg, 1);
	announcement_buf[0] = '\0';
	strcat(announcement_buf, "*** USER ");
	strcat(announcement_buf, first_arg);
	strcat(announcement_buf, "*** HAS BEEN KICKED!");
	disconnect_client(kick_id, announcement_buf);
	printf("USER %s HAS BEEN KICKED\n", first_arg);
	return;
}

void nick_admin(int caller_id, char *old_username, char *new_username)
{
	int subject_id;

	if (old_username == NULL || new_username == NULL)
	{
		send_to_client(caller_id, "NOT ENOUGH ARGUMENTS FOR THE COMMAND!", 0);
		return;
	}

	subject_id = get_user_id(old_username);
	if (subject_id == -1)
	{
		send_to_client(caller_id, "THERE IS NO USER WITH SUCH NICKNAME!",0);
		return;
	}
	
	if (clients[subject_id].status == 'a'){
		send_to_client(caller_id,"YOU CANNOT AFFECT ADMIN!",0);
		return;
	}

	clients[subject_id].name = realloc(clients[subject_id].name, sizeof(char) * (strlen(new_username) + 1));
	strcpy(clients[subject_id].name, new_username);

	send_to_client(caller_id, "USERNAME HAS BEEN SUCCESSFULLY CHANGED!", 0);
	send_to_client(caller_id, "YOUR NICKNAME HAS BEEN CHANGED!",0);
	return;
}

void nick_user(int caller_id, char *new_username){
	if (new_username == NULL)
	{
		send_to_client(caller_id, "NOT ENOUGH ARGUMENTS FOR THE COMMAND!", 0);
		return;
	}

	clients[caller_id].name = realloc(clients[caller_id].name, sizeof(char) * (strlen(new_username) + 1));
	strcpy(clients[caller_id].name, new_username);
	send_to_client(caller_id, "USERNAME HAS BEEN SUCCESSFULLY CHANGED!", 0);

	return;
}

void nick_cmd(int caller_id)
{
	char client_status = clients[caller_id].status;
	char input_copy[MSG_BUF_SIZE];
	strcpy(input_copy, clients[caller_id].last_msg);

	char *cmd = strtok(input_copy, " ");
	char *first_arg = strtok(NULL, " ");
	char *second_arg = strtok(NULL, " ");

	if (client_status == 'a')
		nick_admin(caller_id, first_arg, second_arg);
	else
		nick_user(caller_id, first_arg);

	return;
}

void shutdown_cmd(int caller_id)
{

	kill_server(SIGKILL);
	return;
}

void quit_cmd(int caller_id)
{
	char input_copy[MSG_BUF_SIZE];
	strcpy(input_copy, clients[caller_id].last_msg);

	char *cmd = strtok(input_copy, " ");
	char *first_arg = strtok(NULL, " ");

	create_connection_announcement(caller_id, 0);

	if (first_arg == NULL)
	{
		disconnect_client(caller_id, announcement_buf);
		return;
	}

	strcat(announcement_buf, " WITH WORDS: ");
	strcat(announcement_buf, "\"");
	strcat(announcement_buf, first_arg);
	strcat(announcement_buf, "\"");
	disconnect_client(caller_id, announcement_buf);
	return;
}

// returns cmd_number or -1
int is_cmd(int user_id)
{
	cmd_struct *arr;
	char input_copy[MSG_BUF_SIZE];
	int k = 0;
	int cmp_flag;
	char *cmd;
	if (clients[user_id].status == 'a')
		arr = ADMIN_CMDS;
	else
		arr = USER_CMDS;

	strcpy(input_copy, clients[user_id].last_msg);
	cmd = strtok(input_copy, " ");

	while (arr[k].cmd != NULL)
	{
		cmp_flag = strcmp(arr[k].cmd, cmd);
		if (cmp_flag == 0)
			return k;
		k++;
	}

	return -1;
}

//
void run_cmd(int caller_id, int cmd_num)
{
	cmd_struct *cmd_array;
	if (clients[caller_id].status == 'a')
		cmd_array = ADMIN_CMDS;
	else
		cmd_array = USER_CMDS;

	(*cmd_array[cmd_num].fp)(caller_id);
	return;
}

// preparing set of sockets for subsequent select call
void refresh_socket_set(fd_set *sockets, int listener_fd)
{

	FD_ZERO(sockets);
	FD_SET(listener_fd, sockets); // adding listener socket to the set of watched sockets

	for (int i = 0; i < MAX_CLIENT_NUM; i++)
	{
		if (is_connected(i))
			FD_SET(clients[i].sock_fd, sockets);
	}
	return;
}

// checking if another client has connected
// returns new client socket fd or 0 if no clients have connected
int check_listener_sock(fd_set *sockets, int listener_fd)
{
	int sock = 0;
	// if another client has connected
	if (FD_ISSET(listener_fd, sockets))
	{
		int free_struct_index = find_free_struct();
		if (free_struct_index == -1)
		{
			printf("SERVER BUSY!\n");
			int extra_sock = accept(listener_fd, NULL, NULL);
			send(extra_sock, "SERVER OVERLOAD ERROR!", strlen("SERVER OVERLOAD ERROR") + 1, 0);
			close(extra_sock);
		}
		else
		{

			sock = accept(listener_fd, NULL, NULL);
			clients[free_struct_index].sock_fd = sock;
			ask_nickname(free_struct_index);
			// ask for name
		}
	}
	return sock;
}

void check_client_socks(fd_set *sockets)
{
	int msg_len;
	int cmd_num;
	for (int client_index = 0; client_index < MAX_CLIENT_NUM; client_index++)
	{
		if (is_connected(client_index) && (FD_ISSET(clients[client_index].sock_fd, sockets)))
		{

			msg_len = recv(clients[client_index].sock_fd, clients[client_index].last_msg, MSG_BUF_SIZE, 0);

			// printf("Received %d bytes\n",msg_len);

			if (msg_len == 0)
			{ // disconnection on the client side
				create_connection_announcement(client_index, 0);
				disconnect_client(client_index, announcement_buf);
				continue;
			}

			if (clients[client_index].name_set == 0)
			{
				if (get_user_id(clients[client_index].last_msg) != -1)
				{
					send_to_client(client_index, "NAME_BUSY!", 0);
					continue;
				}

				if (is_banned(clients[client_index].last_msg))
				{
					send_to_client(client_index, "NAME BANNED!", 0);
					continue;
				}

				clients[client_index].name_set = 1;
				clients[client_index].name = malloc(sizeof(char) * strlen(clients[client_index].last_msg));
				strcpy(clients[client_index].name, clients[client_index].last_msg);

				create_connection_announcement(client_index, 1);

				send_except(client_index, announcement_buf, 0);
				send_to_client(client_index, "AUTHENTICATION SUCCESSFUL!", 0);
				printf("***%s CONNECTED TO THE CHAT!\n", clients[client_index].name);
				continue;
			}

			if (clients[client_index].sends_passw)
			{
				clients[client_index].sends_passw = 0;
				if (verify_password(clients[client_index].last_msg) == 1){
					clients[client_index].status = ADMIN_STATUS;
					send_auth_success(client_index);
				}else{
					send_auth_fail(client_index);
				}
				continue;
			}

			cmd_num = is_cmd(client_index);

			if (cmd_num != -1)
			{
				run_cmd(client_index, cmd_num);
				continue;
			}

			// in case server received normal msg
			// need to add * before messages
			send_all(clients[client_index].last_msg, 1);
		}
	}
	return;
}

int main(int argc, char *argv[])
{

	int max_descr, new_client_sock;
	int change_count;
	unsigned short port_num;
	fd_set sockets;

	if (argc <= 1)
	{
		printf("NOT ENOUGH PARAMS GIVEN\n");
		return 1;
	}
	signal(SIGINT, kill_server);

	for (int i = 0; i < MAX_CLIENT_NUM; i++)
	{
		clients[i].sends_passw = 0;
		clients[i].name_set = 0;
		clients[i].status = CLIENT_STATUS;
		clients[i].is_connected = 0;

		for (int k = 0; k < MAX_CLIENT_NUM; k++)
		{
			clients[i].contact_arr[k] = NULL;
		}
	}

	port_num = (unsigned short)atoi(argv[1]);
	init_listener_socket(port_num);

	max_descr = listener;
	// initializing set of sockets

	banlist = malloc(sizeof(char *));
	banlist[0] = NULL;

	for (;;)
	{

		refresh_socket_set(&sockets, listener);

		change_count = select(max_descr + 1, &sockets, NULL, NULL, NULL);

		if (change_count < 1)
		{
			perror("select");
			exit(1);
		}

		new_client_sock = check_listener_sock(&sockets, listener);
		if ((new_client_sock != 0) && (new_client_sock > max_descr))
			max_descr = new_client_sock;

		check_client_socks(&sockets);
	}

	return 0;
}