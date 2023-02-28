CXXFLAGS = -lpthread -lboost_system -lboost_filesystem
CC = g++
all:socks_server hw4.cgi

socks_server: socks_server.cpp
	$(CC) -std=c++11 socks_server.cpp -o socks_server $(CXXFLAGS)
hw4.cgi: console.cpp
	$(CC) -std=c++11 console.cpp -o hw4.cgi $(CXXFLAGS)

clean:
	rm -f socks_server hw4.cgi
