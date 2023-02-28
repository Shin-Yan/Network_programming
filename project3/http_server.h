#pragma once

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <memory>
#include <utility>

using boost::asio::ip::tcp;

class server
{
public:
	server(boost::asio::io_context& io_context, short port);
private:
	void do_accept();

private:
	tcp::acceptor acceptor_;
};


class session : public std::enable_shared_from_this<session> {
public:
 	session(tcp::socket socket);
	void start();
	void do_read();
	void do_write(std::size_t);
	void set_envs();
	void get_envs();
	void print_envs();
	void connect_to_cgi();
	static void set_exe_context(boost::asio::io_context* io_context);
	static boost::asio::io_context* io_context_;

private:
 	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];
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
	
};