
const char * usage =
"                                                               \n"
"IRCServer:                                                   \n"
"                                                               \n"
"Simple server program used to communicate multiple users       \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   IRCServer <port>                                          \n"
"                                                               \n"
"Where 1024 < port < 65536.                                     \n"
"                                                               \n"
"In another window type:                                        \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where talk-server      \n"
"is running. <port> is the port number you used when you run    \n"
"daytime-server.                                                \n"
"                                                               \n";

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "IRCServer.h"

int QueueLength = 5;

int
IRCServer::open_server_socket(int port) {

	// Set the IP address and port for this server
	struct sockaddr_in serverIPAddress; 
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);
  
	// Allocate a socket
	int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
	if ( masterSocket < 0) {
		perror("socket");
		exit( -1 );
	}

	// Set socket options to reuse port. Otherwise we will
	// have to wait about 2 minutes before reusing the sae port number
	int optval = 1; 
	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
			     (char *) &optval, sizeof( int ) );
	
	// Bind the socket to the IP address and port
	int error = bind( masterSocket,
			  (struct sockaddr *)&serverIPAddress,
			  sizeof(serverIPAddress) );
	if ( error ) {
		perror("bind");
		exit( -1 );
	}
	
	// Put socket in listening mode and set the 
	// size of the queue of unprocessed connections
	error = listen( masterSocket, QueueLength);
	if ( error ) {
		perror("listen");
		exit( -1 );
	}

	return masterSocket;
}

void
IRCServer::runServer(int port)
{
	int masterSocket = open_server_socket(port);

	initialize();
	
	while ( 1 ) {
		
		// Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket,
					  (struct sockaddr *)&clientIPAddress,
					  (socklen_t*)&alen);
		
		if ( slaveSocket < 0 ) {
			perror( "accept" );
			exit( -1 );
		}
		
		// Process request.
		processRequest( slaveSocket );		
	}
}

int
main( int argc, char ** argv )
{
	// Print usage if not enough arguments
	if ( argc < 2 ) {
		fprintf( stderr, "%s", usage );
		exit( -1 );
	}
	
	// Get the port from the arguments
	int port = atoi( argv[1] );

	IRCServer ircServer;

	// It will never return
	ircServer.runServer(port);
	
}

//
// Commands:
//   Commands are started y the client.
//
//   Request: ADD-USER <USER> <PASSWD>\r\n
//   Answer: OK\r\n or DENIED\r\n
//
//   REQUEST: GET-ALL-USERS <USER> <PASSWD>\r\n
//   Answer: USER1\r\n
//            USER2\r\n
//            ...
//            \r\n
//
//   REQUEST: CREATE-ROOM <USER> <PASSWD> <ROOM>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: LIST-ROOMS <USER> <PASSWD>\r\n
//   Answer: room1\r\n
//           room2\r\n
//           ...
//           \r\n
//
//   Request: ENTER-ROOM <USER> <PASSWD> <ROOM>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: LEAVE-ROOM <USER> <PASSWD>\r\n
//   Answer: OK\n or DENIED\r\n
//
//   Request: SEND-MESSAGE <USER> <PASSWD> <MESSAGE> <ROOM>\n
//   Answer: OK\n or DENIED\n
//
//   Request: GET-MESSAGES <USER> <PASSWD> <LAST-MESSAGE-NUM> <ROOM>\r\n
//   Answer: MSGNUM1 USER1 MESSAGE1\r\n
//           MSGNUM2 USER2 MESSAGE2\r\n
//           MSGNUM3 USER2 MESSAGE2\r\n
//           ...\r\n
//           \r\n
//
//    REQUEST: GET-USERS-IN-ROOM <USER> <PASSWD> <ROOM>\r\n
//    Answer: USER1\r\n
//            USER2\r\n
//            ...
//            \r\n
//

void
IRCServer::processRequest( int fd )
{
	// Buffer used to store the comand received from the client
	const int MaxCommandLine = 1024;
	char commandLine[ MaxCommandLine + 1 ];
	int commandLineLength = 0;
	int n;
	
	// Currently character read
	unsigned char prevChar = 0;
	unsigned char newChar = 0;
	
	//
	// The client should send COMMAND-LINE\n
	// Read the name of the client character by character until a
	// \n is found.
	//

	// Read character by character until a \n is found or the command string is full.
	while ( commandLineLength < MaxCommandLine &&
		read( fd, &newChar, 1) > 0 ) {

		if (newChar == '\n' && prevChar == '\r') {
			break;
		}
		
		commandLine[ commandLineLength ] = newChar;
		commandLineLength++;

		prevChar = newChar;
	}
	
	// Add null character at the end of the string
	// Eliminate last \r
	commandLineLength--;
        commandLine[ commandLineLength ] = 0;
	char * fullCommand = strdup(commandLine);
	char ** commandParts = (char **) malloc(4 * sizeof(char *));
	bzero(commandParts, 4 * sizeof(char *));
	int parts = 0;
	char * start = fullCommand;
	while(*fullCommand != '\0' && parts < 3) {
		if (*fullCommand == ' ') {
			commandParts[parts] = start;
			*fullCommand = '\0';
			parts++;
			start = fullCommand + 1;
		}
		fullCommand++;
	}
	commandParts[parts] = start;
	
	const char * command = commandParts[0];
	const char * user = commandParts[1];
	const char * password = commandParts[2];
	const char * args = commandParts[3];

	/*
	int i;
	for(i = 0; i < 4; i++) {
		printf("%s, ", commandParts[i]);
	}
	puts("");

	printf("RECEIVED: %s\n", commandLine);

	printf("The commandLine has the following format:\n");
	printf("COMMAND <user> <password> <arguments>. See below.\n");
	printf("You need to separate the commandLine into those components\n");
	printf("For now, command, user, and password are hardwired.\n");

	const char * command = "ADD-USER";
	const char * user = "peter";
	const char * password = "spider";
	const char * args = "";
	*/

	printf("command=%s\n", command);
	printf("user=%s\n", user);
	printf( "password=%s\n", password );
	printf("args=%s\n", args);

	if (!strcmp(command, "") || user == NULL || password == NULL) {
		if (!strcmp(command, "")) {
			const char * msg =  "ERROR (Command missing)\r\n";
			write(fd, msg, strlen(msg));
		} else {
			const char * msg =  "ERROR (User missing)\r\n";
			write(fd, msg, strlen(msg));
		}
	} else {
		// check password
		if (!strcmp(command, "ADD-USER")) {
			addUser(fd, user, password, args);
		} else if (checkPassword(fd, user, password)) {
		/*
			if (!strcmp(command, "ADD-USER")) {
				addUser(fd, user, password, args);
			}
				*/
			if (!strcmp(command, "CREATE-ROOM")) { // added
				createRoom(fd, user, password, args);
			}
			else if (!strcmp(command, "ENTER-ROOM")) {
				enterRoom(fd, user, password, args);
			} else if (!strcmp(command, "LIST-ROOMS")) {
				listRooms(fd, user, password, args);
			}
			else if (!strcmp(command, "LEAVE-ROOM")) {
				leaveRoom(fd, user, password, args);
			}
			else if (!strcmp(command, "SEND-MESSAGE")) {
				sendMessage(fd, user, password, args);
			}
			else if (!strcmp(command, "GET-MESSAGES")) {
				getMessages(fd, user, password, args);
			}
			else if (!strcmp(command, "GET-USERS-IN-ROOM")) {
				getUsersInRoom(fd, user, password, args);
			}
			else if (!strcmp(command, "GET-ALL-USERS")) {
				getAllUsers(fd, user, password, args);
			}
			else if (!strcmp(command, "GET-ONLINE-USERS")) {
				getOnlineUsers(fd, user, password, args);
			}
			else if (!strcmp(command, "GET-ONLINE-USERS-ROOMS")) {
				getOnlineUsersRooms(fd, user, password, args);
			}
			else {
				const char * msg =  "UNKNOWN COMMAND\r\n";
				write(fd, msg, strlen(msg));
			}

			// Send OK answer
			//const char * msg =  "OK\n";
			//write(fd, msg, strlen(msg));
		} else {
			const char * msg = "ERROR (Wrong password)\r\n";
			write(fd, msg, strlen(msg));
		}
	}

	close(fd);	
}

void
IRCServer::initialize()
{
	// Open password file
	FILE * pass_fd;
	pass_fd = fopen(PASSWORD_FILE, "a");
	fclose(pass_fd);
	pass_fd = fopen(PASSWORD_FILE, "r");

	// Initialize users in room
	users = (UserList *) malloc(sizeof(UserList));
	users->head = NULL;

	char * pass = (char *) malloc(100);
	char * name = (char *) malloc(100);
	UserEntry * node;
	while(fscanf(pass_fd, "%s %s\n", name, pass) != EOF) {
		node = (UserEntry *) malloc(sizeof(UserEntry));
		node->name = strdup(name);
		node->pass = strdup(pass);

		node->next = users->head;
		users->head = node;
	}

	// Initialize the rooms list
	rooms = (RoomList *) malloc(sizeof(RoomList));
	rooms->head - NULL;

	// Initalize message list
}

bool
IRCServer::checkPassword(int fd, const char * user, const char * password) {
	// Here check the password
	UserEntry * node = users->head;
	while (node != NULL) {
		if (!strcmp(node->name, user)) {
			if (!strcmp(node->pass, password)) {
				return true;
			} else {
				return false;
			}
		}
		node = node->next;
	}
	return false;
}

void IRCServer::createRoom(int fd, const char * user, const char * password, const char * args) {
	// make sure a room name was provided
	if (args == NULL) {
		const char * msg = "ERROR (Not enough args)\r\n";
		write(fd, msg, strlen(msg));
	} else {
		char * roomName = (char *) malloc(MAX_ROOMNAME);
		char * roomPass = (char *) malloc(MAX_PASSWORD);
		int n;
		n = sscanf(args, "%s %s", roomName, roomPass);

		// check that the room doesn't alreay exist
		RoomEntry * tmp = rooms->head;
		while (tmp != NULL) {
			if (!strcmp(tmp->name, roomName)) {
				const char * msg = "ERROR (Room already exists)\r\n";
				write(fd, msg, strlen(msg));
				return;
			}
			tmp = tmp->next;
		}

		RoomEntry * node = (RoomEntry *) malloc(sizeof(RoomEntry));
		node->name = strdup(roomName);
		node->users = (UserList *) malloc(sizeof(UserList));
		node->messages = (Message *) malloc(100 * sizeof(Message));
		node->msgNum = -1;
		node->next = rooms->head;
		rooms->head = node;	
		if (n == 2) {
			node->pass = roomPass;
		} else {
			node->pass = NULL;
			free(roomPass);
			roomPass = NULL;
		}
		
		const char * msg = "OK\r\n";
		write(fd, msg, strlen(msg));
	}
}

void
IRCServer::addUser(int fd, const char * user, const char * password, const char * args)
{
	// check if the username is already being used
	UserEntry * node = users->head;
	while (node != NULL) {
		if (!strcmp(node->name, user)) {
			const char * msg = "ERROR (User Exists)\r\n";
			write(fd, msg, strlen(msg));
			return;
		}
		node = node->next;
	}

	// add user to passwords file
	FILE * pass_fd;
	pass_fd = fopen(PASSWORD_FILE, "a");
	fprintf(pass_fd, "%s %s\n", user, password);
	fclose(pass_fd);

	// add user to users list
	node = (UserEntry *) malloc(sizeof(UserEntry));
	node->name = strdup(user);
	node->pass = strdup(password);
	node->next = users->head;
	users->head = node;

	const char * msg = "OK\r\n";
	write(fd, msg, strlen(msg));
	
	/*
	// Here add a new user. For now always return OK.

	const char * msg =  "OK\r\n";
	write(fd, msg, strlen(msg));
	*/

	return;		
}

void
IRCServer::enterRoom(int fd, const char * user, const char * password, const char * args)
{
	if (args == NULL) {
		const char * msg = "ERROR (Not enough Args)\r\n";
		write(fd, msg, strlen(msg));
		return;
	}
	char * roomName = (char *) malloc(MAX_ROOMNAME);
	char * roomPass = (char *) malloc(MAX_PASSWORD);

	int n;
	n = sscanf(args, "%s %s", roomName, roomPass);
	// check that the room exists
	RoomEntry * roomNode = rooms->head;
	bool foundRoom = false;
	while (roomNode != NULL) {
		if (!strcmp(roomNode->name, roomName)) {
			foundRoom = true;
			break;
		}
		roomNode = roomNode->next;
	}
	if (!foundRoom) {
		const char * msg = "ERROR (No room)\r\n";
		write(fd, msg, strlen(msg));
		return;
	}
	
	// check if user is already in room
	UserEntry * userNode = roomNode->users->head;
	while (userNode != NULL) {
		if (!strcmp(userNode->name, user)) {
			//const char * msg = "ERROR (already in room)\r\n";
			const char * msg = "OK\r\n";
			write(fd, msg, strlen(msg));
			return;
		}
		userNode = userNode->next;
	}

	// check that the passwords match if one is required
	if (roomNode->pass != NULL && strcmp(roomNode->pass, roomPass)) {
		const char * msg = "ERROR (Wrong password)\r\n";
		write(fd, msg, strlen(msg));
		return;	
	}

	// add user to room
	UserEntry * node = (UserEntry *) malloc(sizeof(UserEntry));
	node->name = strdup(user);
	node->pass = strdup(password);
	node->next = roomNode->users->head;
	roomNode->users->head = node;

	const char * msg = "OK\r\n";
	write(fd, msg, strlen(msg));
}

void
IRCServer::listRooms(int fd, const char * user, const char * password, const char * args) {
	
	FILE * fp = fdopen(fd, "w");

	RoomEntry * node = rooms->head;
	while(node != NULL) {
		if (node->pass != NULL) {
			node = node->next;
			continue;
		}
		fprintf(fp, "%s\r\n", node->name);
		node = node->next;
	}
	fprintf(fp, "\r\n");
	fflush(fp);
}

void
IRCServer::leaveRoom(int fd, const char * user, const char * password, const char * args)
{
	RoomEntry * roomNode = rooms->head;
	UserEntry * userNode;
	UserEntry * lastUser;
	bool found = false;
	while (roomNode != NULL) {
		// remove user from the room provided in args or all rooms if args is NULL
		if (args == NULL || !strcmp(args, roomNode->name)) {
			userNode = roomNode->users->head;
			lastUser = NULL;
			while(userNode != NULL) {
				if (!strcmp(user, userNode->name)) {
					// remove node from list
					if (lastUser == NULL) {
						roomNode->users->head = userNode->next;
					} else {
						lastUser->next = userNode->next;
					}
					// free the usernode
					free(userNode->name);
					userNode->name = NULL;
					free(userNode->pass);
					userNode->pass = NULL;
					free(userNode);
					userNode = NULL;

					found = true;
					break;
				}
				lastUser = userNode;
				userNode  = userNode->next;
			}
		}
		roomNode = roomNode->next;
	}
	if (!found) {
		const char * msg = "ERROR (No user in room)\r\n";
		write(fd, msg, strlen(msg));
	} else {
		const char * msg= "OK\r\n";
		write(fd, msg, strlen(msg));
	}
}

void
IRCServer::sendMessage(int fd, const char * user, const char * password, const char * args)
{
	// parse args
	char * data = strdup(args);
	char * room = data;
	char * message;
	while(*data != '\0' && *data != ' ') {
		data++;
	}
	/*if (*data == '\0') {
		const char * msg = "DENIED\r\n";
		write(fd, msg, strlen(msg));
	} else {*/
		*data = '\0';
		message = data + 1;
	//}

	// find room in rooms list
	RoomEntry * roomNode = rooms->head;
	while (roomNode != NULL) {
		if (!strcmp(roomNode->name, room)) {
			break;
		}
		roomNode = roomNode->next;
	}
	if (roomNode == NULL) {
		const char * msg = "DENIED\r\n";
		write(fd, msg, strlen(msg));
		return;
	}
	
	// check that ther user is in the room
	UserEntry * userNode = roomNode->users->head;
	bool foundUser = false;
	while (userNode != NULL) {
		if (!strcmp(userNode->name, user)) {
			foundUser = true;
			break;	
		}
		userNode = userNode->next;
	}
	if (!foundUser) {
		const char * msg = "ERROR (user not in room)\r\n";
		write(fd, msg, strlen(msg));
		return;
	}

	// add message to messagelist
	roomNode->msgNum++;
	roomNode->messages[(roomNode->msgNum) % 100].msg = strdup(message);
	roomNode->messages[(roomNode->msgNum) % 100].user = strdup(user);
	const char * msg = "OK\r\n";
	write(fd, msg, strlen(msg));
}

void
IRCServer::getMessages(int fd, const char * user, const char * password, const char * args)
{	
	int lastMsg;
	char * roomName = (char *) malloc(100);
	if (sscanf(args, "%d %s", &lastMsg, roomName) != 2) {
		const char * msg = "DENIED\r\n";
		write(fd, msg, strlen(msg));
		return;
	}

	//check that  the lastMsg number is valid
	if (lastMsg < 0) {
		const char * msg = "DENIED\r\n";
		write(fd, msg, strlen(msg));
		return;
	}
	
	FILE * fp = fdopen(fd, "w");
	RoomEntry * roomNode = rooms->head;
	while (roomNode != NULL) {
		if (!strcmp(roomNode->name, roomName)) {

			// check that ther user is in the room
			UserEntry * userNode = roomNode->users->head;
			bool foundUser = false;
			while (userNode != NULL) {
				if (!strcmp(userNode->name, user)) {
					foundUser = true;
					break;	
				}
				userNode = userNode->next;
			}
			if (!foundUser) {
				const char * msg = "ERROR (User not in room)\r\n";
				write(fd, msg, strlen(msg));
				return;
			}

			// check that lastMsg is greater than the number of messages in the room
			if (lastMsg > roomNode->msgNum) {
				const char * msg = "NO-NEW-MESSAGES\r\n";
				write(fd, msg, strlen(msg));
				return;
			}
			// message and msgNum share the same hundreds place
			if (roomNode->msgNum / 100 == lastMsg / 100) {
				int i;
				for (i = lastMsg % 100; i <= roomNode->msgNum % 100; i++) {
					fprintf(fp, "%d %s %s\r\n",lastMsg / 100 * 100 + i, roomNode->messages[i].user, roomNode->messages[i].msg);
				}
			} else { // message and msgnum do not share the same hundreds place
				int i;
				if (roomNode->msgNum % 100 > lastMsg % 100) { // if more than 100 messages behind load the last 100
					for (i = roomNode->msgNum % 100; i < 100; i++) {
						fprintf(fp, "%s %s\r\n", roomNode->messages[i].user, roomNode->messages[i].msg);
					}
				} else { // send the messages from the last message to the most current
					for (i = lastMsg % 100; i < 100; i++) {
						fprintf(fp, "%s %s\r\n", roomNode->messages[i].user, roomNode->messages[i].msg);
					}
				}
				for (i = 0; i <= roomNode->msgNum % 100; i++) {
					fprintf(fp, "%s %s\r\n", roomNode->messages[i].user, roomNode->messages[i].msg);
				}
			}
			break;
		}
		roomNode = roomNode->next;
	}
	if (roomNode == NULL) {
		const char * msg = "DENIED\r\n";
		write(fd, msg, strlen(msg));
		return;
	}
	fprintf(fp, "\r\n");
	fflush(fp);
}

void
IRCServer::getUsersInRoom(int fd, const char * user, const char * password, const char * args)
{
	// find room in rooms list
	RoomEntry * roomNode = rooms->head;
	while (roomNode != NULL) {
		if (!strcmp(roomNode->name, args)) {
			break;
		}
		roomNode = roomNode->next;
	}
	if (roomNode == NULL) {
		const char * msg = "DENIED\r\n";
		write(fd, msg, strlen(msg));
		return;
	}

	// read users into array and sort it
	int maxArray = 10;
	char ** userArray = (char **) malloc(sizeof(char *) * maxArray);
	int n = 0;
	UserEntry * userNode = roomNode->users->head;
	while (userNode != NULL) {
		if (n == maxArray) {
			maxArray *= 2;
			userArray = (char **) realloc(userArray, sizeof(char *) * maxArray);
		}
		userArray[n] = userNode->name;
		n++;
		userNode = userNode->next;
	}
	
	int i;
	int j;
	char * tmp;
	for (i = 0; i < n - 1; i++) {
		for (j = 0; j < n - i - 1; j++) {
			if (strcmp(userArray[j], userArray[j+1]) > 0) {
				tmp = userArray [j];
				userArray[j] = userArray[j + 1];
				userArray[j + 1] = tmp;
			}
		}
	}

	FILE * fp = fdopen(fd, "w");
	for (i = 0; i < n; i++) {
		fprintf(fp, "%s\r\n", userArray[i]);
	}

	/*
	// print list of users
	UserEntry * userNode = roomNode->users->head;
	while (userNode != NULL) {
		fprintf(fp, "%s\r\n", userNode->name);
		userNode = userNode->next;
	}
	*/
	fprintf(fp, "\r\n");
	fflush(fp);
}

void IRCServer::getOnlineUsersRooms(int fd, const char * user, const char * password,const  char * args)
{
	// read users into array and sort it
	int maxArray = 10;
	char ** userArray = (char **) malloc(sizeof(char *) * maxArray);
	char ** roomArray = (char **) malloc(sizeof(char *) * maxArray);
	int n = 0;
	int i;

	int found;
	RoomEntry * roomNode = rooms->head;
	while (roomNode != NULL) {
		UserEntry * userNode = roomNode->users->head;
		while (userNode != NULL) {
			found = 0;
			if (n == maxArray) {
				maxArray *= 2;
				userArray = (char **) realloc(userArray, sizeof(char *) * maxArray);
				userArray = (char **) realloc(roomArray, sizeof(char *) * maxArray);
			}
			// check if already in array
			for (i = 0; i < n; i++) {
				if (!strcmp(userArray[i], userNode->name)) {
					found = 1;
					break;
				}
			}
			if (!found) {
				userArray[n] = userNode->name;
				roomArray[n] = roomNode->name;
				n++;
			}
			userNode = userNode->next;
		}
		roomNode = roomNode->next;
	}
	
	/*
	// read users into array and sort it
	int maxArray = 10;
	char ** userArray = (char **) malloc(sizeof(char *) * maxArray);
	int n = 0;
	UserEntry * userNode = users->head;
	while (userNode != NULL) {
		if (n == maxArray) {
			maxArray *= 2;
			userArray = (char **) realloc(userArray, sizeof(char *) * maxArray);
		}
		userArray[n] = userNode->name;
		n++;
		userNode = userNode->next;
	}
	*/
	
	int j;
	char * tmp;
	for (i = 0; i < n - 1; i++) {
		for (j = 0; j < n - i - 1; j++) {
			if (strcmp(userArray[j], userArray[j+1]) > 0) {
				tmp = userArray [j];
				userArray[j] = userArray[j + 1];
				userArray[j + 1] = tmp;
			}
		}
	}

	FILE * fp = fdopen(fd, "w");
	for (i = 0; i < n; i++) {
		fprintf(fp, "%s %s\r\n", userArray[i], roomArray[i]);
	}
	fprintf(fp, "\r\n");
	fflush(fp);

	/*
	FILE * fp = fdopen(fd, "w");

	UserEntry * node = users->head;
	while(node != NULL) {
		fprintf(fp, "%s\r\n", node->name);
		node = node->next;
	}
	fflush(fp);
	*/
}

void IRCServer::getOnlineUsers(int fd, const char * user, const char * password,const  char * args)
{
	// read users into array and sort it
	int maxArray = 10;
	char ** userArray = (char **) malloc(sizeof(char *) * maxArray);
	int n = 0;
	int i;

	int found;
	RoomEntry * roomNode = rooms->head;
	while (roomNode != NULL) {
		UserEntry * userNode = roomNode->users->head;
		while (userNode != NULL) {
			found = 0;
			if (n == maxArray) {
				maxArray *= 2;
				userArray = (char **) realloc(userArray, sizeof(char *) * maxArray);
			}
			// check if already in array
			for (i = 0; i < n; i++) {
				if (!strcmp(userArray[i], userNode->name)) {
					found = 1;
					break;
				}
			}
			if (!found) {
				userArray[n] = userNode->name;
				n++;
			}
			userNode = userNode->next;
		}
		roomNode = roomNode->next;
	}
	
	/*
	// read users into array and sort it
	int maxArray = 10;
	char ** userArray = (char **) malloc(sizeof(char *) * maxArray);
	int n = 0;
	UserEntry * userNode = users->head;
	while (userNode != NULL) {
		if (n == maxArray) {
			maxArray *= 2;
			userArray = (char **) realloc(userArray, sizeof(char *) * maxArray);
		}
		userArray[n] = userNode->name;
		n++;
		userNode = userNode->next;
	}
	*/
	
	int j;
	char * tmp;
	for (i = 0; i < n - 1; i++) {
		for (j = 0; j < n - i - 1; j++) {
			if (strcmp(userArray[j], userArray[j+1]) > 0) {
				tmp = userArray [j];
				userArray[j] = userArray[j + 1];
				userArray[j + 1] = tmp;
			}
		}
	}

	FILE * fp = fdopen(fd, "w");
	for (i = 0; i < n; i++) {
		fprintf(fp, "%s\r\n", userArray[i]);
	}
	fprintf(fp, "\r\n");
	fflush(fp);

	/*
	FILE * fp = fdopen(fd, "w");

	UserEntry * node = users->head;
	while(node != NULL) {
		fprintf(fp, "%s\r\n", node->name);
		node = node->next;
	}
	fflush(fp);
	*/
}

void
IRCServer::getAllUsers(int fd, const char * user, const char * password,const  char * args)
{
	/*
	// read users into array and sort it
	int maxArray = 10;
	char ** userArray = (char **) malloc(sizeof(char *) * maxArray);
	int n = 0;
	int i;

	int found;
	RoomEntry * roomNode = rooms->head;
	while (roomNode != NULL) {
		UserEntry * userNode = roomNode->users->head;
		while (userNode != NULL) {
			found = 0;
			if (n == maxArray) {
				maxArray *= 2;
				userArray = (char **) realloc(userArray, sizeof(char *) * maxArray);
			}
			// check if already in array
			for (i = 0; i < n; i++) {
				if (!strcmp(userArray[i], userNode->name)) {
					found = 1;
					break;
				}
			}
			if (!found) {
				userArray[n] = userNode->name;
				n++;
			}
			userNode = userNode->next;
		}
		roomNode = roomNode->next;
	}
	*/
	
	// read users into array and sort it
	int maxArray = 10;
	int i;
	char ** userArray = (char **) malloc(sizeof(char *) * maxArray);
	int n = 0;
	UserEntry * userNode = users->head;
	while (userNode != NULL) {
		if (n == maxArray) {
			maxArray *= 2;
			userArray = (char **) realloc(userArray, sizeof(char *) * maxArray);
		}
		userArray[n] = userNode->name;
		n++;
		userNode = userNode->next;
	}
	
	int j;
	char * tmp;
	for (i = 0; i < n - 1; i++) {
		for (j = 0; j < n - i - 1; j++) {
			if (strcmp(userArray[j], userArray[j+1]) > 0) {
				tmp = userArray [j];
				userArray[j] = userArray[j + 1];
				userArray[j + 1] = tmp;
			}
		}
	}

	FILE * fp = fdopen(fd, "w");
	for (i = 0; i < n; i++) {
		fprintf(fp, "%s\r\n", userArray[i]);
	}
	fprintf(fp, "\r\n");
	fflush(fp);

	/*
	FILE * fp = fdopen(fd, "w");

	UserEntry * node = users->head;
	while(node != NULL) {
		fprintf(fp, "%s\r\n", node->name);
		node = node->next;
	}
	fflush(fp);
	*/
}
