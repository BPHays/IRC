#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include "IRCClient.h"
#include <assert.h>

#define MAX_PASS 32
#define MAX_USER 32
#define MAX_RESPONSE (10 * 1024)
#define MAX_ROOM_NAME 32

int createSock(const char * host, int port) {
	struct sockaddr_in sockAddr;
	memset((char *) &sockAddr, 0, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons((u_short) port);
	struct hostent * ptrh = gethostbyname(host);
	if (ptrh == NULL) {
		perror("gethostbyname");
		return -1;
	}
	memcpy(&sockAddr.sin_addr, ptrh->h_addr, ptrh->h_length);
	struct protoent *ptrp = getprotobyname("tcp");
	if (ptrp == NULL) {
		perror("getprotobyname");
		return -1;
	}
	int sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sock < 0) {
		perror("socket");
		return -1;
	}

	printf("%s:%d\n", host, port);
	if (connect(sock, (struct sockaddr *) &sockAddr, sizeof(sockAddr)) < 0) {
		perror("connect");
		return -1;
	}
	return sock;
}

int sendCommand(const char * host, int port, const char * command, char * response) {
	int sock = createSock(host, port);
	if (sock < 0) {
		return 0;
	}

	write(sock, command, strlen(command));
	write(sock, "\r\n", 2);
	write(1, command, strlen(command));
	write(1, "\r\n", 2);

	int n = 0;
	int len = 0;
	while ((n = read(sock, response + len, MAX_RESPONSE - len)) > 0) {
		len += n;
	}
	response[len] = 0;
	printf("response:\n%s\n", response);
	close(sock);
	return 1;
}

void readFriends(List * friends) {
	char * name = (char *) malloc(MAX_USER);
	ListEntry * friendNode = friends->head;
	FILE * fp;
	fp = fopen(FRIENDS_FILE, "a+");
	fclose(fp);
	fp = fopen(FRIENDS_FILE, "r");
	ListEntry * newNode;
	while (fscanf(fp, " %s", name) == 1) {
		newNode = (ListEntry *) malloc(sizeof(ListEntry));
		newNode->str = strdup(name);
		newNode->next = friends->head;
		friends->head = newNode;
	}
}

static void addFriend(GtkWidget * widget, gpointer data) {
	Login * login = (Login *) data;
	GtkWidget * name_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "name"));
	GtkWidget * error_lbl = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "error"));
	GtkWidget * window = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "window"));
	const char * name = gtk_entry_get_text(GTK_ENTRY(name_entry));

	for (int i = 0; i < strlen(name); i++) {
		if (isspace(name[i]) || iscntrl(name[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Username may not conatain spaces");
			return;
		}
	}

	// add friend to linked list
	ListEntry * node = (ListEntry *) malloc(sizeof(ListEntry));
	node->str = strdup(name);
	node->next = login->friends->head;
	login->friends->head = node->next;

	// add friend to friends file
	FILE * fp;
	fp = fopen(FRIENDS_FILE, "a+");
	fprintf(fp, "%s\n", name);
	fclose(fp);

	gtk_widget_destroy(window);
}

static void toggleSound(GtkWidget * widget, gpointer data) {
	Login * login = (Login *) data;
	char * filename;
	GtkWidget * img;
	if (login->sound) {
		img = gtk_image_new_from_file(MUTE_IMG);
		login->sound = false;
	} else {
		img = gtk_image_new_from_file(SOUND_IMG);
		login->sound = true;
	}
	gtk_button_set_image(GTK_BUTTON(widget), img);
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
	return FALSE;
}

static gboolean delete_messenger_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
	g_print("delete event ocurred\n");
	GtkWidget * loginWindow = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "loginWindow"));
	gtk_widget_show(loginWindow);
	//TODO add code to stop timer code from executing after a messenger session is over
	g_source_remove(GPOINTER_TO_UINT(data));
	return FALSE;
}


static void destroy(GtkWidget *widget, gpointer data) {
	gtk_main_quit();
}

static void time_handler(gpointer data) {
	char * command = (char *) malloc(MAX_RESPONSE);
	char * resp = (char *) malloc(MAX_RESPONSE);
	char * msg = (char *) malloc(MAX_RESPONSE);
	int msgNum;
	char * name = (char *) malloc(MAX_RESPONSE);
	char * formatted = (char *) malloc(MAX_RESPONSE);
	Login * login = (Login *) data;
	ListEntry * node;
	int found;
	int bytes;
	int byteSum = 0;
	// check if the user is in a room
	if (login->room != NULL) {
		// get new messages in the room
		sprintf(command, "GET-MESSAGES %s %s %d %s", login->user, login->pass, login->messages, login->room);
		sendCommand(login->host, login->port, command, resp);
		if (strcmp("NO-NEW-MESSAGES\r\n", resp)) {
			while (sscanf(resp + byteSum, " %d %s %[^\r]%n", &msgNum, name, msg, &bytes) != -1) {
				byteSum += bytes;
				if (login->sound && strcmp(name, login->user)) {
					printf("\a");
				}
				printf("%s\n", msg);
				/*
				sprintf(formatted, "%s: %s\n", name, msg);
				login->recvWindow->append(formatted);
				*/
				if (!strncmp("<META>", msg, 6)) {
					login->recvWindow->appendBold(msg + 7);
					login->recvWindow->appendBold("\n");
				} else {
					sprintf(formatted, "%s: ", name);
					login->recvWindow->appendBold(formatted);
					sprintf(formatted, "%s\n", msg);
					login->recvWindow->append(formatted);
				}
				login->messages++;
			}
		}	
		
		// update the user list
		sprintf(command, "GET-USERS-IN-ROOM %s %s %s", login->user, login->pass, login->room);
		sendCommand(login->host, login->port, command, resp);
		List * tmp = (List *) malloc(sizeof(List));
		tmp->head = NULL;
		ListEntry * node;;
		byteSum = 0;

		login->usersWindow->clear();
		byteSum = 0;
		while (sscanf(resp + byteSum, " %[^\r]%n", msg, &bytes) != -1) {
			byteSum += bytes;
			login->usersWindow->append(msg);
		}
	}

	// see what rooms are available
	sprintf(command, "LIST-ROOMS %s %s", login->user, login->pass);
	sendCommand(login->host, login->port, command, resp);
	byteSum = 0;
	while (sscanf(resp + byteSum, " %[^\r]%n", msg, &bytes) != -1) {
		byteSum += bytes;
		node = login->rooms->head;
		found = 0;
		while (node != NULL) {
			if(!strcmp(node->str, msg)) {
				found = 1;
				break;
			}
			node = node->next;
		}
		if (!found) {
			ListEntry * newNode = (ListEntry *) malloc(sizeof(ListEntry));
			newNode->str = strdup(msg);
			newNode->next = login->rooms->head;
			login->rooms->head = newNode;
			login->roomWindow->append(msg);
		}
	}

	// see which friends are online
	login->friendsWindow->clear();
	sprintf(command, "GET-ONLINE-USERS-ROOMS %s %s", login->user, login->pass);
	sendCommand(login->host, login->port, command, resp);
	byteSum = 0;
	char * msg2 = (char *) malloc(MAX_RESPONSE);
	while(sscanf(resp + byteSum, " %s  %[^\r]%n", msg, msg2, &bytes) != -1) {
		sprintf(formatted, "%s (%s)", msg, msg2);
		byteSum += bytes;
		node = login->friends->head;
		while (node != NULL) {
			if (!strcmp(node->str, msg)) {
				login->friendsWindow->append(formatted);
				break;
			}
			node = node->next;
		}
	}	
	free(msg2);
	msg2 = NULL;

	free(command);
	command = NULL;
	free(resp);
	resp = NULL;
	free(msg);
	msg = NULL;
	free(formatted);
	formatted = NULL;
}

static void getMessages(Login * login) {
	char * command = (char *) malloc(MAX_RESPONSE);
	char * resp = (char *) malloc(MAX_RESPONSE);
	char * msg = (char *) malloc(MAX_RESPONSE);
	int msgNum;
	char * name = (char *) malloc(MAX_RESPONSE);
	char * formatted = (char *) malloc(MAX_RESPONSE);
	int bytes;
	int byteSum = 0;
	if (login->room != NULL) {
		// get new messages in the room
		sprintf(command, "GET-MESSAGES %s %s %d %s", login->user, login->pass, login->messages, login->room);
		sendCommand(login->host, login->port, command, resp);
		if (strcmp("NO-NEW-MESSAGES\r\n", resp)) {
			while (sscanf(resp + byteSum, " %d %s %[^\r]%n", &msgNum, name, msg, &bytes) != -1) {
				byteSum += bytes;
				if (login->sound && strcmp(name, login->user)) {
					printf("\a");
				}
				printf("%s\n", msg);
				/*
				sprintf(formatted, "%s: %s\n", name, msg);
				login->recvWindow->append(formatted);
				*/
				if (!strncmp("<META>", msg, 6)) {
					login->recvWindow->appendBold(msg + 7);
					login->recvWindow->appendBold("\n");
				} else {
					sprintf(formatted, "%s: ", name);
					login->recvWindow->appendBold(formatted);
					sprintf(formatted, "%s\n", msg);
					login->recvWindow->append(formatted);
				}
				login->messages++;
			}
		}	
	}	
	free(command);
	command = NULL;
	free(resp);
	resp = NULL;
	free(msg);
	msg = NULL;
	free(formatted);
	formatted = NULL;

}

static void enterRoom(GtkWidget * widget, GtkTreePath * path, GtkTreeViewColumn * column, gpointer data) {
	GtkTreeSelection * selection;
	GtkTreeModel * model;
	GtkTreeIter iter;
	char * room = (char *) malloc(MAX_ROOM_NAME);
	char * resp = (char *) malloc(MAX_RESPONSE);
	char * command = (char * ) malloc(MAX_RESPONSE);
	Login * login = (Login *) data;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	gtk_tree_selection_get_selected(selection, &model, &iter);
	gtk_tree_model_get(model, &iter, 0, &room, -1); 
	// leave last room if still in it
	if (login->room != NULL) {
		sprintf(command, "SEND-MESSAGE %s %s %s <META> %s leaves the room", login->user, login->pass, login->room, login->user);
		sendCommand(login->host, login->port, command, resp);
		sprintf(command, "LEAVE-ROOM %s %s %s", login->user, login->pass, login->room);
		sendCommand(login->host, login->port, command, resp);
		login->recvWindow->clear();
		login->usersWindow->clear();
		login->messages = 0;
	}
	login->room = room;
	sprintf(command, "ENTER-ROOM %s %s %s", login->user, login->pass, room);
	sendCommand(login->host, login->port, command, resp);
	sprintf(command, "SEND-MESSAGE %s %s %s <META> %s entered the room", login->user, login->pass, login->room, login->user);
	sendCommand(login->host, login->port, command, resp);
	getMessages(login);

	free(resp);
	resp = NULL;
	free(command);
	command = NULL;
}

static void hidePassword(GtkWidget * widget, GdkEvent * event, gpointer data) {	
	gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
}

static gboolean login(GtkWidget * widget, GdkEvent * event, gpointer data) {
	// get values from the login entry fields

	int * port = (int *) malloc(sizeof(int));
	char * msg = (char *) malloc(MAX_RESPONSE);
	char * resp = (char *) malloc(MAX_RESPONSE); 
	GtkEntry * user_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "user"));
	GtkEntry * pass_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "pass"));
	GtkEntry * host_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "host"));
	GtkEntry * port_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "port"));
	GtkLabel * error_lbl = GTK_LABEL(g_object_get_data(G_OBJECT(data), "error"));
	char * host = (char *) gtk_entry_get_text(host_entry);
	char * user = (char *) gtk_entry_get_text(user_entry);
	char * pass = (char *) gtk_entry_get_text(pass_entry);
	char * port_text = (char *) gtk_entry_get_text(port_entry);

	// check that the port is actually a number
	for (int i = 0; i < strlen(port_text); i++) {
		if (!isdigit(port_text[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Port must be a number");
			return false;
		}
	}
	*port = atoi(port_text);

	// check that port is in proper range
	if (*port < 1 || *port > 65535) {
		gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Port must be in the range 1-65535");
		return false;
	}

	// check that the user and pass contain no space or control chars
	for (int i = 0; i < strlen(user); i++) {
		if (isspace(user[i]) || iscntrl(user[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Username may not conatain spaces");
			return false;
		}
	}
	for (int i = 0; i < strlen(pass); i++) {
		if (isspace(pass[i]) || iscntrl(pass[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Password may not conatain spaces");
			return false;
		}
	}

	GtkWidget * loginWindow = GTK_WIDGET(g_object_get_data(G_OBJECT(data), "window"));
	// for testing purposes only
	if (!strcmp(user, "admin")) {
		openMessenger(user, pass, host, port, widget);
		gtk_label_set_text(GTK_LABEL(error_lbl), "");
	} else {
		sprintf(msg, "GET-ALL-USERS %s %s", user, pass);
		if(sendCommand(host, *port, msg, resp) == 0) {	
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Unable to connect");
		} else {
			if (strcmp(resp, "ERROR (Wrong password)\r\n")) {
				gtk_label_set_text(GTK_LABEL(error_lbl), "");
				openMessenger(user, pass, host, port, loginWindow);
				gtk_widget_hide(GTK_WIDGET(loginWindow));
			} else {
				gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Incorrect Password");
			}
		}
	}
	free(msg);
	msg = NULL;
	free(resp);
	resp = NULL;
}

static gboolean signUp(GtkWidget * widget, GdkEvent * event, gpointer data) {
	char msg[MAX_USER + MAX_PASS + 13];
	char * resp = (char *) malloc(MAX_RESPONSE);
	int port;
	// get values from entry fields
	GtkEntry * user_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "user"));
	GtkEntry * pass1_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "pass1"));
	GtkEntry * pass2_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "pass2"));
	GtkEntry * host_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "host"));
	GtkEntry * port_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "port"));
	GtkWidget * window = GTK_WIDGET(g_object_get_data(G_OBJECT(data), "window"));
	GtkLabel * error_lbl = GTK_LABEL(g_object_get_data(G_OBJECT(data), "error"));
	char * host = (char *) gtk_entry_get_text(host_entry);
	char * port_text = (char *) gtk_entry_get_text(port_entry);
	char * user = (char *) gtk_entry_get_text(user_entry);
	char * pass1 = (char *) gtk_entry_get_text(pass1_entry);
	char * pass2 = (char *) gtk_entry_get_text(pass2_entry);

	// check that the port is actually a number
	for (int i = 0; i < strlen(port_text); i++) {
		if (!isdigit(port_text[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Port must be a number");
			return false;
		}
	}
	port = atoi(port_text);

	// check that port is in proper range
	if (port < 1 || port > 65535) {
		gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Port must be in the range 1-65535");
		return false;
	}

	// check that the user and pass contain no space or control chars
	for (int i = 0; i < strlen(user); i++) {
		if (isspace(user[i]) || iscntrl(user[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Username may not conatain spaces");
			return false;
		}
	}
	for (int i = 0; i < strlen(pass1); i++) {
		if (isspace(pass1[i]) || iscntrl(pass1[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Password may not conatain spaces");
			return false;
		}
	}
	
	// make sure the user entered the same password both times
	if (strcmp(pass1, pass2)) {
		gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Passwords do not match");
		return false;
	} else {
		gtk_label_set_text(GTK_LABEL(error_lbl), "");
		sprintf(msg, "ADD-USER %s %s\r\n", user, pass1);
		sendCommand(host, port, msg, resp);
		if (!strncmp("ERROR", resp, 5)) {
		gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Username taken");
		return false;

		}
		//g_signal_emit_by_name(G_OBJECT(window), "delete-event");
		gtk_widget_destroy(window);
	}
}

static void sendMsg(GtkWidget * widget, gpointer data) {
	Login * login = (Login *) data;
	if (login->room != NULL) {
		char * body;

		char * msg = (char *) malloc(MAX_RESPONSE);
		char * resp = (char * ) malloc(MAX_RESPONSE);
		/*
		char * user = (char *) g_object_get_data(G_OBJECT(data), "user");
		char * pass = (char *) g_object_get_data(G_OBJECT(data), "pass");
		char * host = (char *) g_object_get_data(G_OBJECT(data), "host");
		int  * port = (int *) g_object_get_data(G_OBJECT(data), "port");
		*/

		//TextWindow * window  = (TextWindow *) g_object_get_data(G_OBJECT(data), "window");
		body = login->sendWindow->getText();
		sprintf(msg, "SEND-MESSAGE %s %s %s %s", login->user, login->pass, login->room, body);
		login->sendWindow->clear();
		sendCommand(login->host, login->port, msg, resp);
		getMessages(login);

		// set focus back to text entry
		gtk_widget_grab_focus(login->sendWindow->getView());
		free(msg);
		msg = NULL;
		free(resp);
		resp = NULL;
	}
}

static void enter_room_from_dialog(GtkWidget * widget, gpointer data) {
	Login * login = (Login *) data;
	char * command = (char *) malloc(MAX_RESPONSE);
	char * resp = (char *) malloc(MAX_RESPONSE);
	GtkWidget * name_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "name"));
	GtkWidget * pass_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "pass"));
	GtkWidget * window = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "window"));
	GtkWidget * error_lbl = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "error"));
	const char * name = gtk_entry_get_text(GTK_ENTRY(name_entry));
	const char * roomPass = gtk_entry_get_text(GTK_ENTRY(pass_entry));
	
	// check that the user and pass contain no space or control chars
	for (int i = 0; i < strlen(name); i++) {
		if (isspace(name[i]) || iscntrl(name[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Room name may not conatain spaces");
			return;
		}
	}
	for (int i = 0; i < strlen(roomPass); i++) {
		if (isspace(roomPass[i]) || iscntrl(roomPass[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Password may not conatain spaces");
			return;
		}
	}

	/*
	if (login->room != NULL) {
		sprintf(command, "SEND-MESSAGE %s %s %s <META> %s leaves the room", login->user, login->pass, login->room, login->user);
		sendCommand(login->host, login->port, command, resp);
		sprintf(command, "LEAVE-ROOM %s %s %s", login->user, login->pass, login->room);
		sendCommand(login->host, login->port, command, resp);
		login->room = NULL;
	}
	*/

	sprintf(command, "ENTER-ROOM %s %s %s %s", login->user, login->pass, name, roomPass);
	sendCommand(login->host, login->port, command, resp);
	if (strncmp("ERROR", resp, 5)) {
		login->room = strdup(name);
		sprintf(command, "SEND-MESSAGE %s %s %s <META> %s entered the room", login->user, login->pass, login->room, login->user);
		sendCommand(login->host, login->port, command, resp);
		gtk_widget_destroy(window);
	} else {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Room/Password combo not found");
	}
	getMessages(login);

	free(resp);
	resp = NULL;
	free(command);
	command = NULL;
}

static void create_room(GtkWidget * widget, gpointer data) {
	Login * login = (Login *) data;
	char * command = (char *) malloc(MAX_RESPONSE);
	char * resp = (char *) malloc(MAX_RESPONSE);
	GtkWidget * name_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "name"));
	GtkWidget * pass_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "pass"));
	GtkWidget * window = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "window"));
	GtkWidget * error_lbl = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "error"));
	const char * name = gtk_entry_get_text(GTK_ENTRY(name_entry));

	// check that the room name is valid
	for (int i = 0; i < strlen(name); i++) {
		if (isspace(name[i]) || iscntrl(name[i])) {
			gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Room name may not conatain spaces");
			return;
		}
	}

	if (gtk_widget_is_sensitive(pass_entry)) {
		const char * roomPass = gtk_entry_get_text(GTK_ENTRY(pass_entry));
		// check that the password is valid
		for (int i = 0; i < strlen(roomPass); i++) {
			if (isspace(roomPass[i]) || iscntrl(roomPass[i])) {
				gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Password may not conatain spaces");
				return;
			}
		}
		sprintf(command, "CREATE-ROOM %s %s %s %s", login->user, login->pass, name, roomPass);
	} else {
		sprintf(command, "CREATE-ROOM %s %s %s", login->user, login->pass, name);
	}
	sendCommand(login->host, login->port, command, resp);
	if (!strncmp(resp, "ERROR", 5)) {
		gtk_label_set_text(GTK_LABEL(error_lbl), "ERROR: Room already exists");
	} else {
		gtk_widget_destroy(window);
	}
	free(resp);
	resp = NULL;
	free(command);
	command = NULL;
}

static void toggle_private(GtkWidget * widget, gpointer data) {
	GtkWidget * pass_entry = GTK_WIDGET(data);
	if (gtk_widget_is_sensitive(pass_entry)) {
		gtk_widget_set_sensitive(pass_entry, FALSE);
		gtk_entry_set_text(GTK_ENTRY(data), "");
	} else {
		gtk_widget_set_sensitive(pass_entry, TRUE);
	}
}

TextWindow::TextWindow() {
	window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	view = gtk_text_view_new();
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	gtk_container_add(GTK_CONTAINER(window), view);
	gtk_widget_show_all(window);
	gtk_widget_show(window);
	GtkTextTag * tag = gtk_text_buffer_create_tag(buffer, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
}

static void leave_room(GtkWidget * widget, gpointer data) {
	Login * login = (Login *) data;
	if (login->room != NULL) {
		char * command = (char *) malloc(MAX_RESPONSE);
		char * resp = (char *) malloc(MAX_RESPONSE);
		sprintf(command, "SEND-MESSAGE %s %s %s <META> %s leaves the room", login->user, login->pass, login->room, login->user);
		sendCommand(login->host, login->port, command, resp);
		sprintf(command, "LEAVE-ROOM %s %s %s", login->user, login->pass, login->room);
		sendCommand(login->host, login->port, command, resp);
		login->usersWindow->clear();
		login->room = NULL;	
		login->messages = 0;
		login->recvWindow->clear();
		free(command);
		command = NULL;
		free(resp);
		resp = NULL;
	}
}

void TextWindow::append(const char * str) {
	//gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);
	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert(buffer, &iter, str, -1);
	gtk_widget_show_all(window);
}

void TextWindow::appendBold(const char * str) {
	//gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);
	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, str, -1, "bold", NULL);
	gtk_widget_show_all(window);
}

char * TextWindow::getText() {
	GtkTextIter start;
	GtkTextIter end;
	char * msg;
	gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(buffer), &start, &end);
	msg = (char *) gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer), &start, &end, FALSE);
	puts(msg);
	return msg;
}

void TextWindow::clear() {
	GtkTextIter start;
	GtkTextIter end;
	gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(buffer), &start, &end);
	gtk_text_buffer_delete(GTK_TEXT_BUFFER(buffer), &start, &end);
}

GtkWidget * TextWindow::getWindow() {
	return this->window;
}

GtkWidget * TextWindow::getView() {
	return this->view;
}

ListWindow::ListWindow(char * title) {
	this->window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	list = gtk_list_store_new(1, G_TYPE_STRING);
	this->tree = gtk_tree_view_new();
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(list));
	gtk_container_add(GTK_CONTAINER(window), tree);

	/*
	int i;
	for (i = 0; i < 10; i++) {
		gchar *msg = g_strdup_printf("Room%d", i);
		gtk_list_store_append(GTK_LIST_STORE(list), &iter);
		gtk_list_store_set(GTK_LIST_STORE(list), &iter, 0, msg, -1);
		g_free(msg);
	}
	*/

	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(title, cell, "text", 0 , NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), GTK_TREE_VIEW_COLUMN(column));
	
	gtk_widget_show(tree);
	gtk_widget_show(window);
}

void ListWindow::append(const char * str) {
	gtk_list_store_append(GTK_LIST_STORE(list), &iter);
	gtk_list_store_set(GTK_LIST_STORE(list), &iter, 0, str, -1);
}

void ListWindow::clear() {
	gtk_list_store_clear(GTK_LIST_STORE(list));
}

GtkWidget * ListWindow::getWindow() {
	return this->window;
}

GtkWidget * ListWindow::getTree() {
	return this->tree;
}

void addEmote(GtkButton * widget, gpointer data) {
	Login * login = (Login *) data;
	login->sendWindow->append(gtk_button_get_label(widget));
}

void openEmoticon(GtkWidget * widget, gpointer data) {
	GtkWidget * window;
	GtkWidget * box;
	GtkWidget * smile;
	GtkWidget * frown;
	GtkWidget * cry;
	GtkWidget * worried;
	GtkWidget * sunglasses;
	GtkWidget * wink;

	// create the window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	gtk_window_set_title(GTK_WINDOW(window), "Emoticons");
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);

	smile = gtk_button_new_with_mnemonic(" :) ");
	frown = gtk_button_new_with_mnemonic(" :( ");
	cry = gtk_button_new_with_mnemonic(" :'( ");
	worried = gtk_button_new_with_mnemonic(" :/ ");
	sunglasses = gtk_button_new_with_mnemonic(" B) ");
	wink = gtk_button_new_with_mnemonic(" ;) ");

	g_signal_connect(G_OBJECT(smile), "clicked", G_CALLBACK(addEmote), data);
	g_signal_connect(G_OBJECT(frown), "clicked", G_CALLBACK(addEmote), data);
	g_signal_connect(G_OBJECT(cry), "clicked", G_CALLBACK(addEmote), data);
	g_signal_connect(G_OBJECT(worried), "clicked", G_CALLBACK(addEmote), data);
	g_signal_connect(G_OBJECT(sunglasses), "clicked", G_CALLBACK(addEmote), data);
	g_signal_connect(G_OBJECT(wink), "clicked", G_CALLBACK(addEmote), data);

	box = gtk_hbox_new(TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), smile, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), frown, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), cry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), worried, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), sunglasses, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), wink, TRUE, TRUE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	// show widgets
	gtk_widget_show_all(window);
}


void openNewUser(GtkWidget * widget, GdkEvent * event, gpointer data) {
	GtkWidget * window;
	GtkWidget * box;
	GtkWidget * user_entry;
	GtkWidget * pass1_entry;
	GtkWidget * pass2_entry;
	GtkWidget * sign_up_btn;
	GtkWidget * error_lbl;
	GtkEntry * host_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "host"));
	GtkEntry * port_entry = GTK_ENTRY(g_object_get_data(G_OBJECT(data), "port"));

	// create the window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);

	box = gtk_vbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	// add entry fields
	user_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(user_entry), "Username");
	pass1_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(pass1_entry), "Password");
	pass2_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(pass2_entry), "Retype Password");
	gtk_box_pack_start(GTK_BOX(box), user_entry, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), pass1_entry, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), pass2_entry, FALSE, FALSE, 5);
	
	g_signal_connect(G_OBJECT(pass1_entry), "focus-in-event", G_CALLBACK(hidePassword), NULL);
	g_signal_connect(G_OBJECT(pass2_entry), "focus-in-event", G_CALLBACK(hidePassword), NULL);
	//gtk_entry_set_visibility(GTK_ENTRY(pass1_entry), FALSE);
	//gtk_entry_set_visibility(GTK_ENTRY(pass2_entry), FALSE);

	// error label
	error_lbl = gtk_label_new(NULL);

	// add sign up button
	sign_up_btn = gtk_button_new_with_label("Sign up");
	g_object_set_data(G_OBJECT(sign_up_btn), "user", user_entry);
	g_object_set_data(G_OBJECT(sign_up_btn), "pass1", pass1_entry);
	g_object_set_data(G_OBJECT(sign_up_btn), "pass2", pass2_entry);
	g_object_set_data(G_OBJECT(sign_up_btn), "host", host_entry);
	g_object_set_data(G_OBJECT(sign_up_btn), "port", port_entry);
	g_object_set_data(G_OBJECT(sign_up_btn), "error", error_lbl);
	g_object_set_data(G_OBJECT(sign_up_btn), "window", window);
	g_signal_connect(G_OBJECT(sign_up_btn), "clicked", G_CALLBACK(signUp), sign_up_btn);
	gtk_box_pack_start(GTK_BOX(box), sign_up_btn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), error_lbl, FALSE, FALSE, 5);

	// show widgets
	gtk_widget_show_all(window);
	/*
	gtk_widget_show(error_lbl);
	gtk_widget_show(user_entry);
	gtk_widget_show(pass1_entry);
	gtk_widget_show(pass2_entry);
	gtk_widget_show(sign_up_btn);
	gtk_widget_show(box);
	gtk_widget_show(window);
	*/
}

void openLogin() {
	GtkWidget * window;
	GtkWidget * title_lbl;
	GtkWidget * sign_up_btn;
	GtkWidget * sign_in_btn;
	GtkWidget * user_entry;
	GtkWidget * pass_entry;
	GtkWidget * box;
	GtkWidget * serv_box;
	GtkWidget * serv_host;
	GtkWidget * serv_colon_lbl;
	GtkWidget * serv_port;
	GtkWidget * error_lbl;
	
	// create the sign in window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Login");
	g_signal_connect (window, "delete-event", G_CALLBACK(delete_event), NULL);
	g_signal_connect (window, "destroy", G_CALLBACK(destroy), NULL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);

	box = gtk_vbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	// add the server fields
	serv_box = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), serv_box, TRUE, TRUE, 5);
	serv_host = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(serv_host), "Hostname");
	serv_port = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(serv_port), "Port");
	serv_colon_lbl = gtk_label_new_with_mnemonic(":");
	gtk_box_pack_start(GTK_BOX(serv_box), serv_host, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(serv_box), serv_colon_lbl, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(serv_box), serv_port, FALSE, FALSE, 5);

	// add a username textbox and label
	user_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(user_entry), "Username");
	gtk_box_pack_start(GTK_BOX(box), user_entry, FALSE, FALSE, 5);

	// add a password textbox and label
	pass_entry = gtk_entry_new();	
	g_signal_connect(G_OBJECT(pass_entry), "focus-in-event", G_CALLBACK(hidePassword), NULL);
	gtk_entry_set_text(GTK_ENTRY(pass_entry), "Password");
	gtk_box_pack_start(GTK_BOX(box), pass_entry, FALSE, FALSE, 5);
	//gtk_entry_set_visibility(GTK_ENTRY(pass_entry), FALSE);

	// error label at the bottom of the window
	error_lbl = gtk_label_new(NULL);

	// add a sign in button
	sign_in_btn = gtk_button_new_with_label("Sign In");
	g_object_set_data(G_OBJECT(sign_in_btn), "host", serv_host);
	g_object_set_data(G_OBJECT(sign_in_btn), "port", serv_port);
	g_object_set_data(G_OBJECT(sign_in_btn), "user", user_entry);
	g_object_set_data(G_OBJECT(sign_in_btn), "pass", pass_entry);
	g_object_set_data(G_OBJECT(sign_in_btn), "window", window);
	g_object_set_data(G_OBJECT(sign_in_btn), "error", error_lbl);
	g_signal_connect_swapped(G_OBJECT(sign_in_btn), "clicked", G_CALLBACK(login), sign_in_btn);
	gtk_box_pack_start(GTK_BOX(box), sign_in_btn, FALSE, FALSE, 5);

	// add a sign up button to bring up sing up window
	sign_up_btn = gtk_button_new_with_label("Sign Up");
	g_object_set_data(G_OBJECT(sign_up_btn), "host", serv_host);
	g_object_set_data(G_OBJECT(sign_up_btn), "port", serv_port);
	g_signal_connect(sign_up_btn, "clicked", G_CALLBACK(openNewUser), sign_up_btn);
	gtk_box_pack_start(GTK_BOX(box), sign_up_btn, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(box), error_lbl, FALSE, FALSE, 5);
	
	// show widgets
	gtk_widget_show_all(window);
	/*
	gtk_widget_show(error_lbl);
	gtk_widget_show(serv_host);
	gtk_widget_show(serv_port);
	gtk_widget_show(serv_colon_lbl);
	gtk_widget_show(serv_box);
	gtk_widget_show(sign_up_btn);
	gtk_widget_show(sign_in_btn);
	gtk_widget_show(user_entry);
	gtk_widget_show(pass_entry);
	gtk_widget_show(box);
	gtk_widget_show(window);
	*/
}

static char * openServerSelect(GtkWidget * widget, GdkEvent * event, gpointer data) {
	GtkWidget * window;
	GtkWidget * OK_btn;
	GtkWidget * server_lbl;
	GtkWidget * server_entry;
	GtkWidget * port_lbl;
	GtkWidget * port_entry;
	GtkWidget * box;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect (window, "delete-event", G_CALLBACK(delete_event), NULL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);

	box = gtk_vbox_new(TRUE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	// set up data entry fields
	server_lbl = gtk_label_new_with_mnemonic("Hostname");
	server_entry = gtk_entry_new();
	port_lbl = gtk_label_new_with_mnemonic("Port");
	port_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(box), server_lbl, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), server_entry, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), port_lbl, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), port_entry, FALSE, FALSE, 5);

	OK_btn = gtk_button_new_with_label("OK");
	gtk_box_pack_start(GTK_BOX(box), OK_btn, FALSE, FALSE, 5);

	gtk_widget_show_all(window);
	/*
	gtk_widget_show(OK_btn);
	gtk_widget_show(server_lbl);
	gtk_widget_show(server_entry);
	gtk_widget_show(port_lbl);
	gtk_widget_show(port_entry);
	gtk_widget_show(box);
	gtk_widget_show(window);
	*/
}

static void openCreateRoom(GtkWidget * widget, gpointer data) {
	GtkWidget * window;
	GtkWidget * name_entry;
	GtkWidget * submit_btn;
	GtkWidget * box;
	GtkWidget * is_private_btn;
	GtkWidget * pass_entry;
	GtkWidget * error_lbl;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	g_signal_connect (window, "delete-event", G_CALLBACK(delete_event), NULL);
	gtk_window_set_title(GTK_WINDOW(window), "Create Room");

	// name entry
	name_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(name_entry), "Name");

	// password entry
	pass_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(pass_entry), "password");
	g_signal_connect(G_OBJECT(pass_entry), "focus-in-event", G_CALLBACK(hidePassword), NULL);
	//gtk_entry_set_visibility(GTK_ENTRY(pass_entry), FALSE);
	gtk_widget_set_sensitive(pass_entry, FALSE);

	// private check box
	is_private_btn = gtk_check_button_new_with_label("Private?");
	g_signal_connect(is_private_btn, "toggled", G_CALLBACK(toggle_private), (gpointer) pass_entry);

	// error label
	error_lbl = gtk_label_new(NULL);

	// submit button
	submit_btn = gtk_button_new_with_mnemonic("Submit");
	g_object_set_data(G_OBJECT(submit_btn), "name", name_entry);
	g_object_set_data(G_OBJECT(submit_btn), "pass", pass_entry);
	g_object_set_data(G_OBJECT(submit_btn), "window", window);
	g_object_set_data(G_OBJECT(submit_btn), "error", error_lbl);
	g_signal_connect(submit_btn, "clicked", G_CALLBACK(create_room), (gpointer) data);

	// pack box
	box = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), name_entry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), is_private_btn, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), pass_entry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), submit_btn, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), error_lbl, TRUE, TRUE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	gtk_widget_show_all(window);
	/*
	gtk_widget_show(error_lbl);
	gtk_widget_show(is_private_btn);
	gtk_widget_show(pass_entry);
	gtk_widget_show(name_entry);
	gtk_widget_show(submit_btn);
	gtk_widget_show(box);
	gtk_widget_show(window);
	*/
}

static void openEnterPrivate(GtkWidget * widget, gpointer data) {
	GtkWidget * window;
	GtkWidget * name_entry;
	GtkWidget * pass_entry;
	GtkWidget * submit_btn;
	GtkWidget * box;
	GtkWidget * error_lbl;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	g_signal_connect (window, "delete-event", G_CALLBACK(delete_event), NULL);
	gtk_window_set_title(GTK_WINDOW(window), "Enter Private Room");

	// name entry
	name_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(name_entry), "Name");

	// pass entry
	pass_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(pass_entry), "password");
	g_signal_connect(G_OBJECT(pass_entry), "focus-in-event", G_CALLBACK(hidePassword), NULL);
	//gtk_entry_set_visibility(GTK_ENTRY(pass_entry), FALSE);

	// error label
	error_lbl = gtk_label_new(NULL);

	// submit button
	submit_btn = gtk_button_new_with_mnemonic("Submit");
	g_object_set_data(G_OBJECT(submit_btn), "name", name_entry);
	g_object_set_data(G_OBJECT(submit_btn), "pass", pass_entry);
	g_object_set_data(G_OBJECT(submit_btn), "window", window);
	g_object_set_data(G_OBJECT(submit_btn), "error", error_lbl);
	g_signal_connect(submit_btn, "clicked", G_CALLBACK(enter_room_from_dialog), data);

	// pack box
	box = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), name_entry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), pass_entry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), submit_btn, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), error_lbl, TRUE, TRUE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	gtk_widget_show_all(window);
	/*
	gtk_widget_show(error_lbl);
	gtk_widget_show(pass_entry);
	gtk_widget_show(name_entry);
	gtk_widget_show(submit_btn);
	gtk_widget_show(box);
	gtk_widget_show(window);
	*/
}

static void openAddFriend(GtkWidget * widget, gpointer data) {
	GtkWidget * window;
	GtkWidget * name_entry;
	GtkWidget * submit_btn;
	GtkWidget * box;
	GtkWidget * error_lbl;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);
	g_signal_connect (window, "delete-event", G_CALLBACK(delete_event), NULL);
	gtk_window_set_title(GTK_WINDOW(window), "Add Friend");
	name_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(name_entry), "Name");
	submit_btn = gtk_button_new_with_mnemonic("Submit");
	error_lbl = gtk_label_new(NULL);
	g_object_set_data(G_OBJECT(submit_btn), "name", name_entry);
	g_object_set_data(G_OBJECT(submit_btn), "error", error_lbl);
	g_object_set_data(G_OBJECT(submit_btn), "window", window);
	g_signal_connect(submit_btn, "clicked", G_CALLBACK(addFriend), (gpointer) data);
	box = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box), name_entry, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), submit_btn, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), error_lbl, TRUE, TRUE, 5);
	gtk_container_add(GTK_CONTAINER(window), box);

	gtk_widget_show(error_lbl);
	gtk_widget_show(name_entry);
	gtk_widget_show(submit_btn);
	gtk_widget_show(box);
	gtk_widget_show(window);
}

static void openMessenger(const char * user, const char * pass, const char * host, int * port, GtkWidget * loginWindow) {
	GtkWidget * window;
	GtkWidget * pane_main;
	GtkWidget * pane_user_msg;
	GtkWidget * pane_send_recv;
	GtkWidget * send_box;
	GtkWidget * send_btn;
	GtkWidget * sound_img;
	GtkWidget * sound_btn;
	GtkWidget * send_btn_box;
	GtkWidget * leave_room_btn;
	GtkWidget * create_room_btn;
	GtkWidget * enter_private_btn;
	GtkWidget * room_friend_box;
	GtkWidget * room_box;
	GtkWidget * room_btn_box;
	GtkWidget * friend_box;
	GtkWidget * add_friend_btn;
	GtkWidget * emoticon_btn;
	char * rooms_title = strdup("Rooms");
	char * users_title = strdup("Users");
	char * friends_title = strdup("Friends");
	ListWindow * rooms_window = new ListWindow(rooms_title);
	TextWindow * recv_msg_window = new TextWindow();
	TextWindow * send_msg_window = new TextWindow();
	ListWindow * users_window = new ListWindow(users_title);
	ListWindow * friends_window = new ListWindow(friends_title);

	// add info to login struct
	Login * loginInfo = (Login *) malloc(sizeof(Login));
	loginInfo->user = user;
	loginInfo->pass = pass;
	loginInfo->host = host;
	loginInfo->port = *port;
	loginInfo->sound = true;
	loginInfo->roomWindow = rooms_window;
	loginInfo->messages = 0;
	loginInfo->recvWindow = recv_msg_window;
	loginInfo->room = NULL;
	loginInfo->usersWindow = users_window;
	loginInfo->rooms = (List *) malloc(sizeof(List));
	loginInfo->rooms->head = NULL;
	loginInfo->sendWindow = send_msg_window;
	loginInfo->friendsWindow = friends_window;
	loginInfo->friends = (List *) malloc(sizeof(List));
	loginInfo->friends->head = NULL;
	readFriends(loginInfo->friends);


	// set up the main window
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	char * title = (char *) malloc(14 + strlen(host) + 5 + 1);
	sprintf(title, "Messenger on %s:%d", host, *port);
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_window_set_default_size(GTK_WINDOW(window), 500, 600);	
	g_object_set_data(G_OBJECT(window), "loginWindow", (gpointer) loginWindow);
	gtk_container_set_border_width (GTK_CONTAINER(window), 10);

	// set up the paned windows
	pane_main = gtk_vpaned_new();
	pane_user_msg = gtk_hpaned_new();
	pane_send_recv = gtk_vpaned_new();
	gtk_container_add(GTK_CONTAINER(window), pane_main);

	// set up the rooms pane
	room_friend_box = gtk_hbox_new(FALSE, 1);
	room_box = gtk_vbox_new(FALSE, 5);
	room_btn_box = gtk_hbox_new(FALSE, 5);
	create_room_btn = gtk_button_new_with_label("Create room");
	g_signal_connect(create_room_btn, "clicked", G_CALLBACK(openCreateRoom), (gpointer) loginInfo);
	enter_private_btn = gtk_button_new_with_label("Enter Private");
	g_signal_connect(enter_private_btn, "clicked", G_CALLBACK(openEnterPrivate), (gpointer) loginInfo);
	leave_room_btn = gtk_button_new_with_label("Leave room");
	g_signal_connect(leave_room_btn, "clicked", G_CALLBACK(leave_room), (gpointer) loginInfo);
	gtk_box_pack_start(GTK_BOX(room_box), rooms_window->getWindow(), TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(room_btn_box), create_room_btn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(room_btn_box), enter_private_btn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(room_btn_box), leave_room_btn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(room_box), room_btn_box, FALSE, FALSE, 5);
	//gtk_box_pack_start(GTK_BOX(room_box), room_friend_box, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(room_friend_box), room_box, TRUE, TRUE, 1);	
	gtk_paned_add1(GTK_PANED(pane_main), room_friend_box);
	g_signal_connect(G_OBJECT(rooms_window->getTree()), "row-activated", G_CALLBACK(enterRoom), (gpointer) loginInfo);

	// set up friends pane
	friend_box = gtk_vbox_new(FALSE, 5);
	add_friend_btn = gtk_button_new_with_label("Add Friend");
	g_signal_connect(add_friend_btn, "clicked", G_CALLBACK(openAddFriend), (gpointer) loginInfo);
	gtk_box_pack_start(GTK_BOX(friend_box), friends_window->getWindow(), TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(friend_box), add_friend_btn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(room_friend_box), friend_box, TRUE, TRUE, 5);	

	// set up messages pane
	gtk_text_view_set_editable(GTK_TEXT_VIEW(recv_msg_window->getView()), FALSE);
	gtk_text_view_set_accepts_tab(GTK_TEXT_VIEW(send_msg_window->getView()), FALSE);
	send_box = gtk_vbox_new(FALSE, 5);
	send_btn_box = gtk_hbox_new(FALSE, 5);
	send_btn = gtk_button_new_with_label("Send");
	/*
	g_object_set_data(G_OBJECT(send_btn), "user", (gpointer) user);
	g_object_set_data(G_OBJECT(send_btn), "pass", (gpointer) pass);
	g_object_set_data(G_OBJECT(send_btn), "host", (gpointer) host);
	g_object_set_data(G_OBJECT(send_btn), "port", (gpointer) port);
	g_object_set_data(G_OBJECT(send_btn), "window", (gpointer) send_msg_window);
	*/
	//g_signal_connect(G_OBJECT(send_btn), "clicked", G_CALLBACK(sendMsg), send_msg_window);	
	g_signal_connect(G_OBJECT(send_btn), "clicked", G_CALLBACK(sendMsg), (gpointer) loginInfo);
	sound_btn = gtk_button_new();
	sound_img = gtk_image_new_from_file(SOUND_IMG);
	gtk_button_set_image(GTK_BUTTON(sound_btn), sound_img);
	g_signal_connect(G_OBJECT(sound_btn), "clicked", G_CALLBACK(toggleSound), (gpointer) loginInfo);
	emoticon_btn = gtk_button_new_with_label("Emoticons");
	g_signal_connect(G_OBJECT(emoticon_btn), "clicked", G_CALLBACK(openEmoticon), (gpointer) loginInfo);
	gtk_box_pack_start(GTK_BOX(send_box), send_msg_window->getWindow(), TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(send_btn_box), send_btn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(send_btn_box), sound_btn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(send_btn_box), emoticon_btn, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(send_box), send_btn_box, FALSE, FALSE, 5);

	// add windows to panes
	gtk_paned_add1(GTK_PANED(pane_send_recv), recv_msg_window->getWindow());
	gtk_paned_add2(GTK_PANED(pane_send_recv), send_box);
	gtk_paned_add1(GTK_PANED(pane_user_msg), users_window->getWindow());
	gtk_paned_add2(GTK_PANED(pane_user_msg), pane_send_recv);
	gtk_paned_add2(GTK_PANED(pane_main), pane_user_msg);
	gtk_paned_set_position(GTK_PANED(pane_main), 200);
	gtk_paned_set_position(GTK_PANED(pane_user_msg), 100);
	gtk_paned_set_position(GTK_PANED(pane_send_recv), 250);

	//start timer
	guint timeout;
	timeout = g_timeout_add(5000, (GSourceFunc) time_handler, (gpointer) loginInfo);
	time_handler((gpointer) loginInfo);

	g_signal_connect (window, "delete-event", G_CALLBACK(delete_messenger_event), (gpointer) GUINT_TO_POINTER(timeout));

	gtk_widget_show_all(window);
	/*
	gtk_widget_show(enter_private_btn);
	gtk_widget_show(friend_box);
	gtk_widget_show(add_friend_btn);
	gtk_widget_show(room_friend_box);
	gtk_widget_show(room_btn_box);
	gtk_widget_show(leave_room_btn);
	gtk_widget_show(room_box);
	gtk_widget_show(create_room_btn);
	gtk_widget_show(send_box);
	gtk_widget_show(send_btn);
	gtk_widget_show(pane_send_recv);
	gtk_widget_show(pane_user_msg);
	gtk_widget_show(pane_main);
	gtk_widget_show(window);
	*/
}

int main(int argc, char * argv[]) {
	
	gtk_init(&argc, &argv);

	openLogin();
		
	gtk_main();
	return 0;
}
