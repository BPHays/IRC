
#ifndef IRC_SERVER
#define IRC_SERVER

#define PASSWORD_FILE "password.txt"
#define MAX_USERNAME 100
#define MAX_PASSWORD 100
#define MAX_MESSAGES 100
#define MAX_ROOMNAME 100

struct Message {
	char * msg;
	char * user;
};

struct UserEntry {
	char * name;
	char * pass;
	struct UserEntry * next;
};

typedef struct UserEntry UserEntry;

struct UserList {
	UserEntry * head;
};

typedef struct UserList UserList;

struct RoomEntry {
	char * name;
	UserList * users;
	Message * messages;
	int msgNum;
	struct RoomEntry * next;
	char * pass;
};

typedef struct RoomEntry RoomEntry;

struct RoomList {
	RoomEntry * head;
};

typedef struct RoomList RoomList;

class IRCServer {
	// Add any variables you need

private:
	int open_server_socket(int port);

	// variables
	RoomList * rooms;
	UserList * users;

public:
	void initialize();
	bool checkPassword(int fd, const char * user, const char * password);
	void processRequest( int socket );
	void addUser(int fd, const char * user, const char * password, const char * args);
	void createRoom(int fd, const char * user, const char * password, const char * args);
	void listRooms(int fd, const char * user, const char * password, const char * args);
	void enterRoom(int fd, const char * user, const char * password, const char * args);
	void leaveRoom(int fd, const char * user, const char * password, const char * args);
	void sendMessage(int fd, const char * user, const char * password, const char * args);
	void getMessages(int fd, const char * user, const char * password, const char * args);
	void getUsersInRoom(int fd, const char * user, const char * password, const char * args);
	void getAllUsers(int fd, const char * user, const char * password, const char * args);
	void getOnlineUsers(int fd, const char * user, const char * password, const char * args);
	void getOnlineUsersRooms(int fd, const char * user, const char * password, const char * args);
	void runServer(int port);
};

#endif
