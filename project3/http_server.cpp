#include "http_server.h"

boost::asio::io_context *session::io_context_;

server::server(boost::asio::io_context& io_context, short port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port)){
	session::set_exe_context(&io_context);
	do_accept();
}


void server::do_accept(){
	acceptor_.async_accept(
 		[this](boost::system::error_code ec, tcp::socket socket) {
 			if (!ec) {
 				std::make_shared<session>(std::move(socket))->start();
 			}
 		do_accept();
 		}
	);
}
session::session(tcp::socket socket) :
	socket_(std::move(socket)){
	}

void session::start(){
	do_read();
}

void session::do_read() {
	auto self(shared_from_this());
	socket_.async_read_some(
 	boost::asio::buffer(data_, max_length),
 		[this, self](boost::system::error_code ec, std:: size_t length) {
			if (ec) return;

			std::string receiveString(data_);
			// http_request_string += receiveString;
			// size_t s_len = http_request_string.length();
			// if(http_request_string.substr(s_len-4,4) != "\r\n\r\n"){
			// 	do_read();
			// 	return ;
			// }
			std::string user_agent;
			std::istringstream myStream(receiveString);

			myStream >> request_method ;
			myStream >> request_uri ;
			myStream >> server_protocol ;
			myStream >> user_agent ;
			myStream >> http_host ;

			server_addr = socket_.local_endpoint().address().to_string();
			remote_addr = socket_.remote_endpoint().address().to_string();
			server_port = std::to_string(socket_.local_endpoint().port());
			remote_port = std::to_string(socket_.remote_endpoint().port());

			myStream = std::istringstream(request_uri);
			std::getline(myStream,request_uri,'?');
			std::getline(myStream,query_string);


			boost::filesystem::path full_path(boost::filesystem::current_path());
			exec_file = full_path.string() + request_uri;

			set_envs();
			get_envs();
			print_envs();

			connect_to_cgi();
		 }
	);
 }

// void session::do_write(std:: size_t length) {
//  	auto self(shared_from_this());
//  	boost::asio::async_write(
//  	socket_, boost::asio::buffer(return_header, return_header.length()),
//  		[this, self](boost::system::error_code ec, std:: size_t length) {
//  			if (!ec) connect_to_cgi();
//  		}
// 	);
//  }

void session::set_envs(){
	setenv("REQUEST_METHOD", request_method.c_str(), 1);
	setenv("REQUEST_URI", request_uri.c_str(), 1);
	setenv("QUERY_STRING", query_string.c_str(), 1);
	setenv("SERVER_PROTOCOL", server_protocol.c_str(), 1);
	setenv("HTTP_HOST", http_host.c_str(), 1);
	setenv("SERVER_ADDR", server_addr.c_str(), 1);
	setenv("SERVER_PORT", server_port.c_str(), 1);
	setenv("REMOTE_ADDR", remote_addr.c_str(), 1);
	setenv("REMOTE_PORT", remote_port.c_str(), 1);
	setenv("EXEC_FILE", exec_file.c_str(), 1);
}

void session::get_envs(){
	request_method = getenv("REQUEST_METHOD");
	request_uri = getenv("REQUEST_URI");
	query_string = getenv("QUERY_STRING");
	server_protocol = getenv("SERVER_PROTOCOL");
	http_host = getenv("HTTP_HOST");
	server_addr = getenv("SERVER_ADDR");
	server_port = getenv("SERVER_PORT");
	remote_addr = getenv("REMOTE_ADDR");
	remote_port = getenv("REMOTE_PORT");
	exec_file = getenv("EXEC_FILE");
}

void session::print_envs(){
	std::cout << "--------------------------------------------" << std::endl;
	std::cout << "REQUEST_METHOD: " << request_method << std::endl;
	std::cout << "REQUEST_URI: " << request_uri << std::endl;
	std::cout << "QUERY_STRING: " << query_string << std::endl;
	std::cout << "SERVER_PROTOCOL: " << server_protocol << std::endl;
	std::cout << "HTTP_HOST: " <<  http_host << std::endl;
	std::cout << "SERVER_ADDR: " << server_addr << std::endl;
	std::cout << "SERVER_PORT: " << server_port << std::endl;
	std::cout << "REMOTE_ADDR: " << remote_addr << std::endl;
	std::cout << "REMOTE_PORT: " << remote_port << std::endl;
	std::cout << "EXEC_FILE: " << exec_file << std::endl;
	std::cout << "--------------------------------------------" << std::endl;
}

void session::connect_to_cgi(){
	std::cout<<"connecting to cgi"<<std::endl;
	session::io_context_->notify_fork(boost::asio::io_context::fork_prepare);
	if (fork() == 0){
		std::cout<<"fork successfully"<<std::endl;
  	// This is the child process.
	  	session::io_context_->notify_fork(boost::asio::io_context::fork_child);
		int sockfd = socket_.native_handle();

		std::string ok_string ="HTTP/1.1 200 OK\r\n";

		dup2(sockfd, STDIN_FILENO);
		dup2(sockfd, STDOUT_FILENO);
		// dup2(sockfd, STDERR_FILENO);
		
		// socket_.close();
		
		std::cout<<ok_string;
		fflush(stdout);

		if(execlp(exec_file.c_str(),exec_file.c_str(),NULL)<0){
			std::cout << "Content-type:text/html\r\n\r\n<h1>Fail</h1>";
			fflush(stdout);
		}
	}
	else{
  	// This is the parent process.
  		session::io_context_->notify_fork(boost::asio::io_context::fork_parent);
		socket_.close();
	}
}

void session::set_exe_context(boost::asio::io_context* io_context){
	io_context_ = io_context;
	
}

int main(int argc, char* argv[1]){
	try{
		if (argc != 2){
			std::cerr << "Usage: async_tcp_echo_server <port>\n";
      		return 1;
		}
		boost::asio::io_context my_exe_context;
		server my_server(my_exe_context, std::atoi(argv[1]));
		my_exe_context.run();
	}

	catch (std::exception& e){
    	std::cerr << "Exception: " << e.what() << "\n";
  	}

	return 0;
}