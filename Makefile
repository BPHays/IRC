
CXX = g++ -fPIC

all: IRCServer IRCClient

IRCServer: IRCServer.cc
	gcc -g -o IRCServer IRCServer.cc

IRCClient: IRCClient.cc
	g++ -g -o IRCClient IRCClient.cc `pkg-config --cflags --libs gtk+-2.0`

clean:
	rm -f friends.txt password.txt *.o  IRCServer IRCClient
