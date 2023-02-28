CXXFLAGS = -lpthread -lboost_system -lboost_filesystem
CC = g++
all:http_server console.cgi

http_server: http_server.cpp
	$(CC) -std=c++11 http_server.cpp -o http_server $(CXXFLAGS)

console.cgi: console.cpp
	$(CC) -std=c++11 console.cpp -o console.cgi $(CXXFLAGS)
