#include <time.h>
//#include <curses.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <ctype.h>

#define FRIENDS_FILE "friends.txt"
#define SOUND_IMG "sound.png"
#define MUTE_IMG "mute.png"

class TextWindow {
private:
	GtkWidget * window;
	GtkWidget * view;
	GtkTextBuffer * buffer;
	GtkTextIter iter;

public:
	TextWindow();
	GtkWidget * getView();
	char * getText();
	GtkWidget * getWindow();

	// append the string to the text view
	void append(const char * str);

	// append the string in bold to the text view
	void appendBold(const char * str);

	// clear all of the text fromt the text view
	void clear();
};

class ListWindow {
private:
	GtkWidget * window;
	GtkListStore * list;
	GtkWidget * tree;
	GtkTreeIter iter;
	GtkCellRenderer * cell;
	GtkTreeViewColumn * column;

public:
	ListWindow(char * title);

	// remove all entries from the tree list
	void clear();

	// append a new entry to the tree list
	void append(const char * str);
	GtkWidget * getWindow();
	GtkWidget * getTree();
};

struct ListEntry {
	char * str;
	struct ListEntry * next;
};
typedef struct ListEntry ListEntry;

struct List {
	ListEntry * head;
};
typedef struct List List;

// struct that keeps track of a user's session information
struct Login {
	const char * user;
	const char * pass;
	const char * host;
	const char * room;
	int port;
	int messages;
	bool sound;
	TextWindow * recvWindow;
	TextWindow * sendWindow;
	ListWindow * roomWindow;
	ListWindow * usersWindow;
	ListWindow * friendsWindow;
	List * rooms;
	List * friends;
};
typedef struct Login Login;

// open a window to get the user's login info
void openLogin();

// open a messenger session using the user info entered into the login window
static void openMessenger(const char * user, const char * pass, const char * host, int * port, GtkWidget * loginWindow);

// create a socket to communicate with the server
int createSock(const char * host, int port);

// send a command and store the response from the server
int sendCommand(const char * host, int port, const char * command, char * response);

// callback function to send a message
static void sendMsg(GtkWidget * widget, gpointer data);

// open a window to create a new room
static void openCreateRoom(GtkWidget * widget, gpointer data);

// read the user's friend list from the friends file
void readFriends(List * friends);

// open a window to add a friend to the friend list and file
static void addFriend(GtkWidget * widget, gpointer data);

// toggle if the bell sound is played upon receiving messages and swap the toggle button image
static void toggleSound(GtkWidget * widget, gpointer data);

//
static gboolean delete_event(GtkWidget * widget, GdkEvent * event, gpointer data);

// close the messenger session and return to the login window
static gboolean delete_messenger_event(GtkWidget * widget, GdkEvent * event, gpointer data);

//
static void destroy(GtkWidget * widget, gpointer data);

// update the messages, rooms, users, and friends lists
static void time_handler(gpointer data);

// poll server to see if new messages have come in if so display them
static void getMessages(Login * login);

// leave the previous room and enter the selected room
static void enterRoom(GtkWidget * widget, GtkTreePath * path, GtkTreeViewColumn * column, gpointer data);

// callback to hide the password
static void hidePassword(GtkWidget * widget, GdkEvent * event, gpointer data);

// callback to login to the server and start a messenger session
static gboolean login(GtkWidget * widget, GdkEvent * event, gpointer data);

// callback to add a new user to the server
static gboolean signUp(GtkWidget * widget, GdkEvent * event, gpointer data);

// callback to open a window to enter a password protected private room
static void enter_room_from_dialog(GtkWidget * widget, gpointer data);

// callback to create a new room using information from the create room window
static void create_room(GtkWidget * widget, gpointer data);

// callback to toggle the password field of the create room window
static void tolle_private(GtkWidget * widget, gpointer data);

// callback to leave the current room
static void leave_room(GtkWidget * widget, gpointer data);

// add an emoticon to the message
void addEmote(GtkButton * widget, gpointer data);

// open an emoticon window which can be used to add emoticons to messages
void openEmoticon(GtkWidget * widget, gpointer data);

// open a new user window to get and check new user credentials
void openNewUser(GtkWidget * widget, GdkEvent * event, gpointer data);

// open a window to get credentials to enter a private room
static void openEnterPrivate(GtkWidget * widget, gpointer data);

// open a window to get a username to add to one's friend list
static void openAddFriend(GtkWidget * widget, gpointer data);
