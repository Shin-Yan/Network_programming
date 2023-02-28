#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <vector>
#include <string>
#include <malloc.h>
#include <arpa/inet.h>
#include <iostream>
#include <fcntl.h>
#include <errno.h>
#include <iomanip>

#include "single_builtins.h"
using namespace std;
/* Global Variables*/

#define BUFSIZE 4096
#define MAX_CLIENT_NUM 30
#define MYSH_TOKEN_DELIM " \t\n\r"
#define STDERR_FILENO 2

fd_set afds;
int sockfd;
int nfds;
int port;
int uidTable[MAX_CLIENT_NUM+1];
vector<ClientData> clientTable;
vector<pipeFd> userPipeTable;

bool HasClient(int uid)
{
    if(uidTable[uid] != 0)
        return true;
    return false;
}


void BroadCast(int action,string msg,vector<ClientData>::iterator currentClient,int targetfd)
{
    int fd ;
    string temp ;
    vector<ClientData>::iterator targetClient ;
    for(fd = 0 ; fd < nfds ; fd ++)
    {
        temp = "";
        if (FD_ISSET(fd, &afds))
        {
            // cout <<fd <<"is in set" <<endl;
            if (fd != sockfd)   /* Don't send to msock */
            {
                
                switch (action) {
                    case 0:
                        temp = "*** "+(*currentClient).name+" yelled ***: " + msg + "\n";
                        break;
                    case 1:
                        temp = "*** User from " +(*currentClient).ip+":"+to_string((*currentClient).port)+" is named '"+(*currentClient).name+"'. ***\n";
                        break;
                    case 2:
                        temp = "*** User '(no name)' entered from "+(*currentClient).ip+":"+to_string((*currentClient).port)+". ***\n";
                        break;
                    case 3:
                        temp = "*** User '"+(*currentClient).name+"' left. ***\n";
                        break;
                    case 4:
                        targetClient = GetClientByUid(clientTable,targetfd);
                        temp = "*** "+(*currentClient).name+" (#"+to_string((*currentClient).uid)+") just piped '"+msg+"' to "+(*targetClient).name+" (#"+to_string((*targetClient).uid)+") ***\n";
                        break;
                    case 5:
                        targetClient= GetClientByUid(clientTable,targetfd);
                        temp = "*** "+(*currentClient).name+" (#"+to_string((*currentClient).uid)+") just received from "+(*targetClient).name+" (#"+to_string((*targetClient).uid)+") by '"+msg+"' ***\n";
                        break;
                    default:
                        break;
                        
                }
                
                if (write(fd, temp.c_str(), temp.length()) == -1)
                    // perror("send");
                    {;}
            }
        }
    }
}

int** init_pipeTable(int** table){
  table = (int**)calloc(sizeof(int*),3);
  for(int i=0;i<3;i++){
    table[i] = (int*)calloc(sizeof(int),1000);
  }
  for(int i = 0; i < 3 ; i++ ){
    for(int j = 0; j < 1000 ; j++ ){
      table[i][j] = -1;
    }
  }
  return table;
}

void free_pipeTable(int**table ){
  for(int i=0;i<3;i++){
    free(table[i]);
  }
  free(table);
}

int RunShell(int connfd,char* line,vector<ClientData>::iterator currentClientIt)
{
  int status=1;
  pipeline_struct* pipeline;
    dup2(connfd,STDOUT_FILENO);
    dup2(connfd,STDERR_FILENO);
    // cout<<"start to parse"<<endl;
    pipeline = mysh_parse_pipeline(line);
    // cout<<"parse success!"<<endl;
    string msgstr;
    string originStr(line);
    if (originStr.length()>5)
      msgstr.assign(originStr.begin()+5,originStr.end());
    if(strncmp(line,"who",3) == 0){
      cout << "<ID>\t<nickname>\t<IP:port>\t<indicate me>" << endl;
      for (int i = 1 ; i < MAX_CLIENT_NUM+1 ; i ++)
      {
        if(uidTable[i] != 0)
        {
            vector<ClientData>::iterator it = GetClientByUid(clientTable,i);
            cout <<it->uid <<"\t"<<it->name<<"\t"<<it->ip<<":"<<it->port;
            fflush(stdout);
            if(it->fdNum == currentClientIt->fdNum)
                cout <<"\t<-me" << endl;
            else
                cout << endl;
        }
      }
    }
    else if(strncmp(line,"tell",4) == 0){
      int tellUid = atoi(pipeline->cmds[0]->args[1]);
      if(tellUid<=30 && tellUid >0 && HasClient(tellUid)){
        string msg;
        if(tellUid >= 10)
          msg.assign(msgstr.begin()+3,msgstr.end());
        else
          msg.assign(msgstr.begin()+2,msgstr.end());
        msg =  "*** "+(*currentClientIt).name+" told you ***: "+ msg +"\n";
        vector<ClientData>::iterator it = GetClientByUid(clientTable,tellUid);
        write(it->fdNum, msg.c_str(), msg.length());
      }
      else
        cout << "*** Error: user #"+to_string(tellUid)+" does not exist yet. ***"<< endl;
    }
    else if(strncmp(line,"yell",4) == 0){
      BroadCast(0,msgstr,currentClientIt,-1);
    }
    else if(strncmp(line,"name",4) == 0){
      bool hasSameName = false;
      vector<ClientData>::iterator it = clientTable.begin();
      while( it != clientTable.end()){
          if(it->name ==msgstr){
              hasSameName = true;
              break;
          }
          it ++;
      }
      if(hasSameName){
          cout << "*** User '"+msgstr+"' already exists. ***" << endl;
      }
      else{
          currentClientIt->name = msgstr;
          BroadCast(1, "", currentClientIt,-1);
      }
    }
    else{
      status = mysh_execute(pipeline,currentClientIt->numberpipe_table,connfd,currentClientIt,userPipeTable,clientTable,uidTable,line);
    }
    send(connfd,"% ",2,0);
    free(pipeline);
  return status;
}
void Int_sig_handle(int num){
  close(sockfd);
  exit(0);
}
int main(int argc, char **argv)
{
  // chdir("../");
  for(int i=1;i<MAX_CLIENT_NUM+1;i++){
    uidTable[i] = 0;
  }
  cout << "get port number" <<endl;
  // Get the port number
  if(argc>1)
    port = atoi(argv[1]);
  else
    port = 8888;

  // initialize the path to /bin and ./
  // string initial_setting_env[]={"setenv","PATH","bin:.",NULL};
  socklen_t len;
  vector<ClientData>::iterator currentClientIt ;
	struct sockaddr_in serv_addr;
  struct sockaddr_in cli;
  
  fd_set rfds;

  // socket create and verification
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  
  if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");

  signal(SIGINT,Int_sig_handle);

	bzero(&serv_addr, sizeof(serv_addr));

  // assign IP, PORT
  serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

  int enable = 1;

  setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&enable,1);
  // Binding newly created socket to given IP and verification
	if (::bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
		printf("socket bind failed...\n");
    close(sockfd);
		exit(0);
	}
	else
		printf("Socket successfully binded..\n");
  
  // Now server is ready to listen and verification
	if ((listen(sockfd, 5)) != 0) {
		printf("Listen failed...\n");
		exit(0);
	}
	else
		printf("Server listening..\n");

  signal(SIGCHLD, SIG_IGN);
  nfds = getdtablesize();
  // cout<<"nfds = "<<nfds<<endl;
  FD_ZERO(&afds);
  FD_SET(sockfd, &afds); 
  

	while(1){
    memcpy(&rfds,&afds,sizeof(rfds));
    int i;
    int connfd;

    if(select(nfds,&rfds,NULL,NULL,(struct timeval*)0) < 0){
      if(errno == EINTR)
        continue;
      perror("select error");
      exit(EXIT_FAILURE);
    }
    
    for(int i=0;i<nfds;i++){
      if(FD_ISSET(i,&rfds)){
        if(i == sockfd){
          //handle new connections
          len = sizeof(cli);
          connfd = accept(sockfd,(struct sockaddr *)&cli, &len);
          if (connfd < 0) {
		        printf("server accept failed...\n");
		        exit(0);
	        }
          else{
		        // printf("server accept the client...\n");
            struct ClientData temp ;
            temp.numberpipe_table = init_pipeTable(temp.numberpipe_table);
            // cout<<"original pipetable addr= "<<temp.numberpipe_table<<endl;
            temp.fdNum = connfd;
            temp.name = "(no name)";
            struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&cli;
            struct in_addr ipAddr = pV4Addr->sin_addr;
            char str[INET_ADDRSTRLEN];
            inet_ntop( AF_INET, &ipAddr, str, INET_ADDRSTRLEN );
            temp.ip = string(str);
            temp.port = ntohs(cli.sin_port);
            temp.sendMsg = "";
            temp.uid = AssignId(uidTable,MAX_CLIENT_NUM);
            struct EnvVar tempEnv;
            tempEnv.name = "PATH";
            tempEnv.value = "bin:.";
            temp.envVarTable.push_back(tempEnv);
                        
            clientTable.push_back(temp);    // 會加在最後面 .
            FD_SET(connfd, &afds);           // 新增到 master set
                        
            PrintWelcome(connfd);
            BroadCast(2,"",(--clientTable.end()),-1);
                        
            send(connfd, "% ", 2,0);
          }       
        }
        else{
          // 處理來自 client 的資料
          int nbytes;
          char buf[BUFSIZE];
                    
          if ((nbytes = recv(i, buf, sizeof(buf),0)) <= 0)
          {
            // got error or connection closed by client
            if (nbytes == 0)
            {
              // 關閉連線
              printf("selectserver: socket %d hung up\n", i);
            }
            else
              perror("recv");
                        
              BroadCast(3,"",currentClientIt,-1);
              free_pipeTable(currentClientIt->numberpipe_table);
              EraseExitSenderUserPipe(currentClientIt);
              vector<ClientData>::iterator it = clientTable.begin();
              while(it != clientTable.end()){
                EraseExitRecvUserPipe(currentClientIt,it);
                it++;
              }
              EraseExitClient(clientTable, currentClientIt);
              
              uidTable[(*currentClientIt).uid] = 0;
              close(i);
              FD_CLR(i, &afds); // 從 master set 中移除
          }
          else{
            // 我們從 client 收到一些資料
            // cout<<"i = "<<i<<endl;
            currentClientIt = GetClientByFd(clientTable,i);
            // if currentClientIt == clientTable.end() => No this client!
            // cout<<"In return , fd = "<<currentClientIt->fdNum<<endl;
            // cout<<"In return , uid = "<<currentClientIt->uid<<endl;
            // cout<<"In return , ip = "<<currentClientIt->ip<<endl;
            // cout<<"In return , port = "<<currentClientIt->port<<endl;
            
            buf[nbytes-1] = '\0';
            // cout << "get msg = "<<buf<<endl;
            string path = GetEnvValue("PATH", currentClientIt);
            // cout<<"PATH = "<<path<<endl;
            // cout<<"In reutrn envvalue = "<<path<<endl;
            setenv("PATH",path.c_str(),1);

            int status = RunShell(i,buf,currentClientIt);
            if(status == 0)
            {
              BroadCast(3,"",currentClientIt,-1);
              uidTable[(*currentClientIt).uid] = 0;
              free_pipeTable(currentClientIt->numberpipe_table);
              EraseExitSenderUserPipe(currentClientIt);
              vector<ClientData>::iterator it = clientTable.begin();
              while(it != clientTable.end()){
                EraseExitRecvUserPipe(currentClientIt,it);
                it++;
              }
              // cout<<"end of erasing user_pipes"<<endl;
              EraseExitClient(clientTable, currentClientIt);
              // cout<<"end of erasing exit client data"<<endl;
                            
              close(i);
              close(1);  // 因為dup到stdout , 也要關 !! 否則 sock 仍存在 !
              close(2);  // 同上
                            
              dup2(0,1); // 先佔住 1 , 否則到時connfd會建立在1 !!
              dup2(0,2); // 同上 .
                            
              FD_CLR(i, &afds); // 從 master set 中移除
            }
          }
        } 
      }
    }
  }
  
  // Perform any shutdown/cleanup.
  close(sockfd);
  return EXIT_SUCCESS;
}