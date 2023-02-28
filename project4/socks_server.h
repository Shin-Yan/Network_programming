#pragma once

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <memory>
#include <utility>
#include <sys/types.h>
#include <sys/wait.h>

using boost::asio::ip::tcp;

class server
{
public:
	server(boost::asio::io_context& io_context, short port);
private:
	void do_accept();

private:
	tcp::acceptor acceptor_;
	std::vector<pid_t> child_pids;
};


class session : public std::enable_shared_from_this<session> {
public:
 	session(tcp::socket socket);
	void start();
	void do_read();
	void do_reply();
	void do_reject();
	void do_connect();
	void do_bind();
	void do_read_from_web();
	void do_read_from_client();
	void do_write_to_client(int length);
	void do_request_to_web(int length);
	
	static void set_exe_context(boost::asio::io_context* io_context);
	static boost::asio::io_context* io_context_;
	void parse_sock4_request(int);
	void print_sock4_info();
	bool firewall(std::string,std::string);
	

private:
 	tcp::socket socket_;
	tcp::socket *web_socket;
	tcp::acceptor *acceptor_;
	enum { max_length = 1024 };
	char data_[max_length];
	char reply_msg[8];
	char from_web[max_length];
	char from_client[max_length];
	std::string request_method;
	std::string request_uri;
	std::string query_string;
	std::string server_protocol;
	std::string http_host;
	std::string server_addr;
	std::string server_port;
	std::string remote_addr;
	std::string remote_port;
	std::string exec_file;
	std::string http_request_string;
	// std::string return_header;

private:
	std::string command;
	std::string reply;
	std::string destination_port;
	std::string destination_IP;
	std::string source_port;
	std::string source_IP;
	std::string domain_name;
	std::string domain_to_ip;
};