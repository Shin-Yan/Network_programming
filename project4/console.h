#pragma once

#include<boost/asio.hpp>
#include<vector>
#include<string>

class Client{
    public:
        Client(std::string hostname,std::string port,std::string file,boost::asio::io_context&);
    public:
        std::string server_hostname;
        std::string server_port_num;
        std::string file_to_open;
        boost::asio::ip::tcp::socket tcp_socket;
        char read_buffer[10000];
};

void parse_query_string(std::string);
void sendHtml();
std::string query_string;
std::vector<std::string> parse_txt_input(std::string);
void sendInputToNPserverAndBrowser(int,std::vector<std::string>);
void getOutputFromNPserver(int,std::vector<std::string>);
void sendToBrowser(int,std::string);
void getSocksReply(int);
std::string sh;
std::string sp;
