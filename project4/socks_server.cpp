#include "socks_server.h"

#include<fstream>
#include<regex>

boost::asio::io_context *session::io_context_;

server::server(boost::asio::io_context& io_context, short port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port)){
	session::set_exe_context(&io_context);
	do_accept();
}


void server::do_accept(){
	acceptor_.async_accept(
 		[this](boost::system::error_code ec, tcp::socket socket) {
 			if (ec) 
				return;
			session::io_context_->notify_fork(boost::asio::io_context::fork_prepare);
			pid_t pid = fork();
			if(pid == 0){
				//child process
				session::io_context_->notify_fork(boost::asio::io_context::fork_child);
				std::make_shared<session>(std::move(socket))->start();
			}
 			else{
				 //parent process
				 session::io_context_->notify_fork(boost::asio::io_context::fork_parent);
				 socket.close();

				 child_pids.push_back(pid);

				 int waitPID, status;
				for (int i = 0; i < (int)child_pids.size(); ++i){
					waitPID = waitpid(child_pids[i], &status, WNOHANG);
					if (waitPID == child_pids[i]) child_pids.erase(child_pids.begin() + i, child_pids.begin() + i + 1);
				}
				 do_accept();
			 }
 		
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

	memset(data_,'\0',max_length);
	socket_.async_read_some(
 	boost::asio::buffer(data_, max_length),
 		[this, self](boost::system::error_code ec, std:: size_t length) {
			if (ec) return;

			parse_sock4_request(length);
			do_reply();
		 }
	);
 }

void session::do_reply(){
	reply_msg[0] = 0;
	std::string destIP;
	if(domain_name == "")
		destIP = destination_IP;
	else
		destIP = domain_to_ip;
	if(data_[0] != 4 || destination_IP =="0.0.0.0" || !firewall(command,destIP)){
		reply = "Reject";
		reply_msg[1]=91;
		do_reject();
	}
	else if(command == "CONNECT"){
		reply = "Accept";
		reply_msg[1]=90;
		do_connect();
	}
	else if(command == "BIND"){
		reply = "Accept";
		reply_msg[1]=90;
		do_bind();
	}
	source_port = std::to_string(socket_.remote_endpoint().port());
	source_IP = socket_.remote_endpoint().address().to_string();
	print_sock4_info();
}

void session::do_reject(){
	auto self(shared_from_this());
	boost::asio::async_write(socket_, boost::asio::buffer(reply_msg,8),
		[this,self](boost::system::error_code ec, std::size_t){
			if(ec) 
				return;
		}
	);
}

void session::do_connect(){
	for(int i =2 ; i < 8 ; i++)
		reply_msg[i] = 0;
	
	auto self(shared_from_this());
	boost::asio::async_write(socket_, boost::asio::buffer(reply_msg, 8),
		[this, self](boost::system::error_code ec, std::size_t)
		{
			if (ec) return;

			web_socket = new tcp::socket(*session::io_context_);

			tcp::endpoint endpoint;

			if (domain_name != "")
			{
				tcp::resolver resolver(*session::io_context_);
				tcp::resolver::query query(domain_name, destination_port);
				tcp::resolver::iterator iter = resolver.resolve(query);
				endpoint = *iter;
			}
			else{
				endpoint = tcp::endpoint(boost::asio::ip::address::from_string(destination_IP), stoi(destination_port));
			}

			web_socket->connect(endpoint);

			do_read_from_web();		
			do_read_from_client();
		});
}

void session::do_bind(){
	// std::cout <<"do binding" <<std::endl;
	tcp::endpoint endpoint(boost::asio::ip::address::from_string("0.0.0.0"),0);
	
	acceptor_ = new tcp::acceptor(*session::io_context_);
	acceptor_->open(tcp::v4());
	acceptor_->set_option(tcp::acceptor::reuse_address(true));
	acceptor_->bind(endpoint);
	acceptor_->listen(boost::asio::socket_base::max_connections);

	reply_msg[2] = acceptor_->local_endpoint().port() / 256;
	reply_msg[3] = acceptor_->local_endpoint().port() % 256;

	for(int i = 4; i < 8; i++)
		reply_msg[i] = 0;
	
	auto self(shared_from_this());
	boost::asio::async_write(socket_, boost::asio::buffer(reply_msg, 8),
		[this, self](boost::system::error_code ec, std::size_t)
		{
			if (ec) return;

			web_socket = new tcp::socket(*session::io_context_);
			
			acceptor_->accept(*web_socket);

			boost::asio::write(socket_, boost::asio::buffer(reply_msg, 8));

			do_read_from_web();
			do_read_from_client();
		});
}
void session::do_read_from_web(){
	auto self(shared_from_this());

	memset(from_web,'\0',max_length);
	web_socket->async_read_some(boost::asio::buffer(from_web),
		[this,self](boost::system::error_code ec, std::size_t length){
			if(ec){
				if(ec == boost::asio::error::eof){
					web_socket->close();
					socket_.close();
				}
				return;
			}
			do_write_to_client(length);
		}
	);
}

void session::do_read_from_client(){
	auto self(shared_from_this());

	memset(from_client,'\0', max_length);
	socket_.async_read_some(boost::asio::buffer(from_client),
		[this, self](boost::system::error_code ec, std::size_t length)
		{
			if (ec) 
			{
				if (ec == boost::asio::error::eof)
				{
					web_socket->close();
					socket_.close();
				}
				return;
			}
								
			do_request_to_web(length);
		});
}

void session::do_write_to_client(int length){
	auto self(shared_from_this());
	boost::asio::async_write(socket_, boost::asio::buffer(from_web, length),
		[this, self](boost::system::error_code ec, std::size_t len)
		{
			if (ec) return;

			do_read_from_web();
		});
}

void session::do_request_to_web(int length){
	auto self(shared_from_this());

	boost::asio::async_write((*web_socket), boost::asio::buffer(from_client, length),
		[this, self](boost::system::error_code ec, std::size_t len)
		{
			if (ec) return;

			do_read_from_client();
		});
}

void session::set_exe_context(boost::asio::io_context* io_context){
	io_context_ = io_context;
	
}

void session::parse_sock4_request(int length){
	int port,d[6];
	for(int i = 0; i < 6 ; i++ ){
		d[i] = data_[i+2];
		if(data_[i+2]<0)
			d[i] += 256;
	}
	//Get dest port
	port = d[0]*256+d[1];
	destination_port = std::to_string(port);

	//Get dest IP
	destination_IP = std::to_string(d[2]);
	for(int i = 3 ; i < 6 ; i++ ){
		destination_IP += ".";
		destination_IP += std::to_string(d[i]);
	}

	//Get domain name
	bool DN = false; //first will get userid
	int count = 0;
	domain_name="";
	// std::string str(data_);
	// std::cout<<str<<std::endl;
	// std::cout << length << std::endl;
	for(int i = 8 ; i < length ; i++ ){
		if(data_[i] == 0){
			DN = true;
		}
		else if(DN){
			domain_name+=data_[i];
		}
	}
	// std::cout<<"domain name = "<<domain_name<<std::endl;
	//Get Command
	// std::cout <<"command int = "<< (int)data_[1] << std::endl;
	if(data_[1] == 1) command = "CONNECT";
	if(data_[1] == 2) command = "BIND";

	// std::cout<<"command = " <<command << std::endl;
	domain_to_ip = "";
	if(domain_name !=""){
		tcp::resolver resolver(*session::io_context_);
		tcp::resolver::query query(domain_name, destination_port);
		tcp::resolver::iterator iter = resolver.resolve(query);
		tcp::endpoint endpoint = *iter;	
		domain_to_ip = endpoint.address().to_string();
	}
	// std::cout<<"socks4a IP = "<<domain_to_ip<<std::endl;

}

void session::print_sock4_info(){
	std::cout<<"------------------------------"<<std::endl;
	std::cout<<"<S_IP>: "<<source_IP<<std::endl;
	std::cout<<"<S_PORT>: "<<source_port<<std::endl;

	if(domain_to_ip == "")
		std::cout<<"<D_IP>: "<<destination_IP<<std::endl;
	else
		std::cout<<"<D_IP>: "<<domain_to_ip<<std::endl;

	std::cout<<"<D_PORT>: "<<destination_port<<std::endl;
	std::cout<<"<Command>: "<<command<<std::endl;
	std::cout<<"<Reply>: "<<reply<<std::endl;
	std::cout<<"------------------------------"<<std::endl;
}

bool session::firewall(std::string command, std::string dest_IP){
	std::ifstream firewall_config;
	firewall_config.open("socks.conf");
	if(!firewall_config){
		// std::cout<<"No firewall" <<std::endl;
		return true;
	}

	std::string permit_string;
	while(std::getline(firewall_config,permit_string)){
		std::istringstream rule_Stream(permit_string);
		std::string rule, cmd , ip_addr;
		std::getline(rule_Stream,rule,' ');
		// std::cout<<"rule  = "<<rule<<std::endl;
		std::getline(rule_Stream,cmd,' ');
		// std::cout<<"cmd  = "<<cmd<<std::endl;
		std::getline(rule_Stream,ip_addr);
		// std::cout<<"ip_addr  = "<<ip_addr<<std::endl;

		if(rule == "permit" && ((cmd == "c" && command == "CONNECT") || (cmd =="b" && command == "BIND"))){
			// std::cout<<"pass the first check" <<std::endl;
			std::string accept_ip = ip_addr.substr(0,ip_addr.find('*'));
			// std::cout<<"accept ip = "<<accept_ip<<std::endl;
			// std::cout<<"dest ip = "<<dest_IP.substr(0,accept_ip.size())<<std::endl;
			if(dest_IP.substr(0,accept_ip.size()) == accept_ip)
				return true;
		}
	}
	// std::cout<< "do not pass the firewall" <<std::endl;
	return false; 
}

int main(int argc, char* argv[1]){
	try{
		if (argc != 2){
			std::cerr << "Usage: socks_server <port>\n";
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