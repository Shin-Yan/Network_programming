#include "console.h"

#include<iostream>
#include<algorithm>
#include<string>
#include<sstream>
#include<cstdlib>
#include<fstream>
#include <boost/algorithm/string/replace.hpp>

std::vector<Client> clients_info;
boost::asio::io_context console_io_context;
std::string old_sym[7]= {"\"","&","\'","<",">","\n","\r\n"};
std::string new_sym[7] = {"&quot","&amp;", "&#39;", "&lt;", "&gt;", "&NewLine;", "&NewLine;"};

Client::Client(std::string hostname, std::string port, std::string file,boost::asio::io_context &io_context)
    : server_hostname(hostname), server_port_num(port), file_to_open(file), tcp_socket(io_context){}

void parse_query_string(std::string query_string){
    std::istringstream myStream(query_string);
    std::string server_info,host,file;
    std::string port;

    for(int i = 0; i<5 ; i ++){
        std::getline(myStream,server_info,'&');
        if(server_info.length()!=0) 
            host = server_info.substr(3,server_info.length()-3);
        if(host.length()==0)
            break;
        // std::cout<<"host = "<<host<<std::endl;
        
        std::getline(myStream,server_info,'&');
        if(server_info.length()!=0) 
            port = server_info.substr(3,server_info.length()-3);
        else
            break;
        // std::cout<<"port = "<<port<<std::endl;

        std::getline(myStream,server_info,'&');
        if(server_info.length()!=0) 
            file = server_info.substr(3,server_info.length()-3);
        else
            break;
        // std::cout<<"file = "<<file<<std::endl;
        server_info = "";

        if(host.length() != 0 ) {
            clients_info.push_back(Client(host,port,file,console_io_context));
        }
    }  
}

void sendHtml(){
    std::cout<<"Content-type: text/html\r\n\r\n";
    fflush(stdout);
    std::string html = "<!DOCTYPE html>\
<html lang=\"en\">\
  <head>\
    <meta charset=\"UTF-8\" />\
    <title>NP Project 3 Sample Console</title>\
    <link\
      rel=\"stylesheet\"\
      href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
      integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
      crossorigin=\"anonymous\"\
    />\
    <link\
      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
      rel=\"stylesheet\"\
    />\
    <link\
      rel=\"icon\"\
      type=\"image/png\"\
      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\
    />\
    <style>\
      * {\
        font-family: 'Source Code Pro', monospace;\
        font-size: 1rem !important;\
      }\
      body {\
        background-color: #212529;\
      }\
      pre {\
        color: #cccccc;\
      }\
      b {\
        color: #01b468;\
      }\
    </style>\
  </head>\
  <body>\
    <table class=\"table table-dark table-bordered\">\
      <thead>\
        <tr>";
    std::vector<Client>::iterator it = clients_info.begin();
    for(;it!=clients_info.end();it++){
        html = html + "<th scope=\"col\">" + it->server_hostname + ":" +it->server_port_num+"</th>";
    }
    html = html+"</tr>\
      </thead>\
      <tbody>\
        <tr>";
    it = clients_info.begin();
    int i=0;
    for(;it!=clients_info.end();it++){
        html = html + "<td><pre id=\"s" + std::to_string(i) + "\" class=\"mb-0\"></pre></td>";
        i++;
    }
    html+="</tr>\
      </tbody>\
    </table>\
  </body>\
</html>";
    std::cout<<html;
    fflush(stdout);
}
std::vector<std::string> parse_txt_input(std::string file){
        std::ifstream fileStream("./test_case/"+file);
        // std::cout<<"open success"<<std::endl;
        std::vector<std::string> input_vector;
        for(std::string line;std::getline(fileStream, line);){
            // std::cout<<line+"\n";
            input_vector.push_back(line+"\n");
        }
    return input_vector;
}

void sendInputToNPserverAndBrowser(int client_id,std::vector<std::string> input){
    if(input.size()==0) return;

    boost::asio::async_write(clients_info[client_id].tcp_socket, boost::asio::buffer(input[0],input[0].length()),
      [input,client_id](boost::system::error_code ec, std::size_t){

      std::string toBrowser = input[0];
      for(int i=0;i<7;i++){
          size_t pos = 0;
          pos = toBrowser.find(old_sym[i],pos);
          while(pos != std::string::npos){
            toBrowser.replace(pos, old_sym[i].length(), new_sym[i]);
					  pos += new_sym[i].length();
					  pos = toBrowser.find(old_sym[i], pos);
          }
          pos = 0;
      }
      boost::replace_all(toBrowser, "\r", "");
      std::string output_string = "<script>document.getElementById(\'s" + std::to_string(client_id) + "\').innerHTML += \'<b>" + toBrowser + "</b>\';</script>\n";
      std::cout<< output_string ;
      fflush(stdout);
      std::vector<std::string> tmp_input = input;
      std::vector<std::string>::iterator it = tmp_input.begin();
			tmp_input.erase(it);			

			getOutputFromNPserver(client_id,tmp_input);
      }
    );
}

void getOutputFromNPserver(int client_id,std::vector<std::string> input){
    clients_info[client_id].tcp_socket.async_read_some(boost::asio::buffer(clients_info[client_id].read_buffer, 10000),
    [input,client_id](boost::system::error_code ec,std::size_t length){
        if(!ec){
            // std::string outstr = "receive something";
            // sendToBrowser(client_id,outstr);
            std::string receive_str(clients_info[client_id].read_buffer);
            sendToBrowser(client_id,receive_str);
            memset(clients_info[client_id].read_buffer,'\0',10000);
            size_t prompt_position;
            prompt_position = receive_str.find("%");
            if(prompt_position == std::string::npos){
              // there is no prompt in return, so just call this function again to continue reading the output
              getOutputFromNPserver(client_id,input);
            }
            else{
              // there is a prompt in return, so we shall send the command to server
              sendInputToNPserverAndBrowser(client_id,input);
            }
        }
    }
    );
}

void sendToBrowser(int client_id, std::string send_string){
  
  for(int i=0;i<6;i++){
    // no return have the style \r\n, hence we have to check only 6 members
    size_t pos = 0;
    // find the old symbol position first
    pos = send_string.find(old_sym[i],pos);
    while(pos != std::string::npos){
      // replace the symbol to html style one by one
      send_string.replace(pos,old_sym[i].length(),new_sym[i]);
      pos += new_sym[i].length();
      pos = send_string.find(old_sym[i],pos);
    }
    
  }
  std::string output_string = "<script>document.getElementById(\'s" + std::to_string(client_id) + "\').innerHTML += \'" + send_string + "\';</script>\n";
  std::cout<< output_string ;
  fflush(stdout);
}

int main(int argc, char* argv[]){
    query_string = getenv("QUERY_STRING");
    // parse the query string and create client objects
    parse_query_string(query_string);

    std::vector<Client>::iterator it = clients_info.begin();
    // establish tcp connection with np_single_golden

    // std::cout<<"starting to connect"<<std::endl;
    for(;it!=clients_info.end();it++){
        // std::cout<<it->server_hostname<<" "<<it->server_port_num<<std::endl;
        // std::cout<<"resolving"<<std::endl;
        boost::asio::ip::tcp::resolver resolver(console_io_context);
        // std::cout<<"querying"<<std::endl;
        boost::asio::ip::tcp::resolver::query my_query(it->server_hostname, it->server_port_num);
        // std::cout<<"getting iterator"<<std::endl;
        boost::asio::ip::tcp::resolver::iterator endpoint_it = resolver.resolve(my_query);
        // std::cout<<"getting endpoint"<<std::endl;
        boost::asio::ip::tcp::endpoint endpoint = *endpoint_it;
        // std::cout<<"connecting to endpoint"<<std::endl;
        it->tcp_socket.connect(endpoint);
    }
    // std::cout<<"connecting success"<<std::endl;
    sendHtml();
    
    int i=0;
    it = clients_info.begin();
    for(;it!=clients_info.end();it++){
        std::vector<std::string> input = parse_txt_input(it->file_to_open);
        std::vector<std::string>::iterator iter = input.begin();
        // for(;iter!=input.end();iter++){
        //   sendToBrowser(i,*iter);
        //   ;
        // }
        getOutputFromNPserver(i,input);
        
        
        i++;
    }
    console_io_context.run();


}
