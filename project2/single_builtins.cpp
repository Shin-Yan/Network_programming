#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <iostream>
#include "single_builtins.h"

#define MAX_LEN 15000
#define MYSH_TOKEN_DELIM " \t\n\r"
#define STDERR_FILENO 2
#define MAX_CLIENT_NUM 30
using namespace std;

std::string builtin_str[] = {
  "printenv",
  "setenv",
  "exit"
};

int mysh_printenv(char **args, int sockfd,std::vector<ClientData>::iterator currentClientIt)
{
    char *var;
    if(args[1] == NULL)
      return 1;
    else if(args[2])
      return 1;
   
    var = getenv(args[1]);
    if(var)
      cout<<var<<endl;
    return 1;
}

int mysh_setenv(char **args,int sockfd,std::vector<ClientData>::iterator currentClientIt)
{   
    std::string envName(args[1]);
    std::string envVar(args[2]);
    EraseEnv(envName,currentClientIt);
    struct EnvVar temp;
    temp.name = envName;
    temp.value = envVar;
    (*currentClientIt).envVarTable.push_back(temp);
    // cout<<currentClientIt->envVarTable[0].name<<endl;
    // cout<<currentClientIt->envVarTable[0].value<<endl;
    setenv(args[1],args[2],1);
    return 1;
}

int mysh_exit(char **args,int sockfd,std::vector<ClientData>::iterator currentClientIt)
{
  return 0;
}
int (*builtin_func[]) (char **,int,std::vector<ClientData>::iterator) = {
  &mysh_printenv,
  &mysh_setenv,
  &mysh_exit
};

int mysh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

char* mysh_read_line(int sockfd)
{
  char *line = NULL;
  size_t bufsize = 0; // have getline allocate a buffer for us
  FILE *CLIENT_IN = fdopen(sockfd,"r"); 
  if (getline(&line, &bufsize, CLIENT_IN) == -1) {
    if (feof(CLIENT_IN)){
      exit(EXIT_SUCCESS);  // We received an EOF
    } 
    else{
      perror("mysh: getline\n");
      exit(EXIT_FAILURE);
    }
  }
  return line;
}

int mysh_file_redirection(int in,cmd_struct* command,char* filename,int sockfd){
  pid_t pid;
  int status;

  while((pid = fork())<0){
    ;
  }
  if (pid == 0) {
    if(in != STDIN_FILENO){
      dup2(in,STDIN_FILENO);
      close(in);
    }
    int fd;
    if ((fd = creat(filename, S_IRUSR | S_IWUSR)) < 0)
      perror("creat() error");
    dup2(fd, STDOUT_FILENO);
    dup2(sockfd,STDERR_FILENO);
    close(fd);
    if (execvp(command->args[0], command->args) == -1) {
      //printf("Unknown command: [%s].\n",cmd->args[0]);
        char errstr[300] = "Unknown command: [";
        char errstr2[100] = "].\n";
        strcat(errstr,command->progname);
        strcat(errstr,errstr2);
        write(STDERR_FILENO,errstr,strlen(errstr));
    }
    exit(EXIT_FAILURE);
  } 
  else {
    waitpid(pid, &status, WUNTRACED);
  }
  return 1;
}

int mysh_execute(pipeline_struct* pipeline,int** numberpipe_table,int sockfd,std::vector<ClientData>::iterator currentClientIt,std::vector<pipeFd>&userPipeTable,std::vector<ClientData> &clientTable,int* uidTable,char* line){
  int zero_index = -1;
  int n = pipeline->n_cmds;
  int i=0;
  pid_t pid;
  int in,fd[2];
  // cout<<sockfd<<endl;
  if (pipeline->cmds[0]->args[0]==NULL)
    return 1;
    // cout<<"execute pipetable addr= "<<numberpipe_table<<endl;;
  for(i=0 ; i<1000 ; i++){
      if(numberpipe_table[0][i] > 0)
        numberpipe_table[0][i]--;
      if(numberpipe_table[0][i] == 0)
        zero_index = i;
    }
  // cout<<"brefore builtins"<<endl;
  for (int j = 0; j < 3; j++) {
    // cout<<" j = " <<j<<endl;
    if (strcmp(pipeline->cmds[0]->args[0], builtin_str[j].c_str()) == 0) {
      return (*builtin_func[j])(pipeline->cmds[0]->args,sockfd,currentClientIt);
    }
  }
  // cout<<"not builtins"<<endl;
  if(zero_index == -1)
    in = STDIN_FILENO;
  else{
    numberpipe_table[0][zero_index]--;
    in = numberpipe_table[1][zero_index];
    close(numberpipe_table[2][zero_index]);
  }

  if(pipeline->userpipeIn){
    //To Do
    int senderUid = pipeline->pipeInNum;
    // cout<< "sender Uid = "<<senderUid<<endl;
    vector<ClientData>::iterator senderIt = GetClientByUid(clientTable,senderUid);
    if(!ClientExist(senderUid,uidTable)){
      cout <<"*** Error: user #"<<senderUid<<" does not exist yet. ***" << endl;
      fflush(stdout);
      int devNull = open("/dev/null",O_RDONLY);
      in = devNull;
      // return 1;
    }
    else if(!IsInSendTable(currentClientIt->uid,senderIt)){
      cout <<"*** Error: the pipe #"<<senderUid<<"->#"<<currentClientIt->uid<<" does not exist yet. ***"  << endl;
      fflush(stdout);
      int devNull = open("/dev/null",O_RDONLY);
      in = devNull;
      // return 1;
    }
    else{
      vector<ClientTranInfo>::iterator senderTransfor = GetSendIt(currentClientIt->uid, senderIt);
      in = senderTransfor->pipeRead;
      //clean the recv data of current client
      // vector<ClientTranInfo>::iterator itr = currentClientIt->recvTable.begin();
      // while(itr != currentClientIt->recvTable.end()){
      //   if(itr->SourceClientFd == senderUid){
      //     (*currentClientIt).recvTable.erase(itr);
      //     break;
      //   }
      //   itr ++;
      // }
      //clean the sender data of sender client
      vector<ClientTranInfo>::iterator its = senderIt->sendTable.begin();
      while(its != senderIt->sendTable.end()){
        if(its->TargetClientUid == currentClientIt->uid){
          senderIt->sendTable.erase(its);
          break;
        }
        its ++;
      }
      string broadcastline(line);
      BroadCast(5, broadcastline, currentClientIt,senderUid );
    }
  }
  // cout<<"starting execute"<<endl;
  int status;

  // define j as pipe index
  int j ;
  for(i = 0, j = 0; i < n-1; ++i,++j){
    if(pipeline->pipes_and_FR[j]== '<'){
      if(i+1==n-1){
        // cout<<"break!"<<endl;
        break; //the last pipe is in pipe
      }
      else
        j++; //let the pipe index to the next
    }
      
    if(pipeline->pipes_and_FR[j]=='>' && !pipeline->userpipeOut){
      return mysh_file_redirection(in,pipeline->cmds[i],pipeline->cmds[j+1]->args[0],sockfd);
    }
    pipe(fd);
    if((pipeline->numberpiped==1) &&(j == n-2)){
      status = mysh_number_pipe(pipeline->cmds[i],in,fd[0],fd[1],pipeline->pipes_and_FR[j],pipeline->cmds[j+1]->args[0],numberpipe_table,sockfd);
      if(in != STDIN_FILENO)
        close(in);
      
      return status;
    }
    if((pipeline->userpipeOut)&&pipeline->pipes_and_FR[j]=='>'){
      int recvnum = atoi(pipeline->cmds[j+1]->args[0]);
      string wholeLine(line);
      status = mysh_userpipeout(in,fd[0],fd[1],recvnum,pipeline->cmds[i],currentClientIt,uidTable,clientTable,wholeLine);
      if(in != STDIN_FILENO)
       close(in);
      close(fd[1]);
      return status;
    }
    
    mysh_launch(in,fd[1],pipeline->cmds[i],0,sockfd);
    if(in != STDIN_FILENO)
      close(in);
    close(fd[1]);
    in = fd[0];
    if(pipeline->pipes_and_FR[i]== '<')
      i++;
  }
  // cout<<"last launch"<<endl;
  mysh_launch(in,sockfd,pipeline->cmds[i],1,sockfd);
  if(in != STDIN_FILENO)
    close(in);
  return 1;
}
int search(int** table, int num){
  for(int i=0 ; i<1000; i++){
    if(table[0][i] == num)
      return i;
  }
  return -1;
}

void sig_handler(int num){
      wait(NULL);
}

int mysh_launch(int in,int out,cmd_struct* command,int last,int sockfd)
{
  pid_t pid;
  int status;

  while((pid = fork())<0){
    ;
  }
    if (pid == 0) {
      if(in != STDIN_FILENO){
        dup2(in,STDIN_FILENO);
        close(in);
      }
      if(out != STDOUT_FILENO){
        dup2(out,STDOUT_FILENO);
        if(out!=sockfd)
          close(out);
      }
      // dup2(sockfd,STDERR_FILENO);
      if (execvp(command->args[0],command->args) == -1) {
          char errstr[300] = "Unknown command: [";
          char errstr2[100] = "].\n";
          strcat(errstr,command->progname);
          strcat(errstr,errstr2);
          write(STDERR_FILENO,errstr,strlen(errstr));
      }
      exit(EXIT_FAILURE);
    } 
    else {
      if(last == 0){
        signal(SIGCHLD,sig_handler);
        // cout<<"signal and wait"<<endl;
      }
      else{
        // cout<<"start waiting"<<endl;
        waitpid(pid, &status, WUNTRACED);
        // cout<<"end of waiting"<<endl;
      }
    }
  return 1;
}

int mysh_number_pipe(cmd_struct* command,int in,int read,int writer, char symbol, char* number, int** table,int sockfd){
  int pipe_number = atoi(number);
  int pipe_index;
  pid_t pid;
  int status;

  if((pipe_index=search(table,pipe_number))==-1){
    pipe_index = search(table,-1);
    while((pid = fork())<0){
      ;
    }
    if(pid == 0){
      if(in != STDIN_FILENO){
        dup2(in,STDIN_FILENO);
        close(in);
      }
      dup2(writer,STDOUT_FILENO);
      if(symbol == '!')
        dup2(writer,STDERR_FILENO);
      // else
        // dup2(sockfd,STDERR_FILENO);
      if (execvp(command->args[0],command->args) == -1) {
        // printf("Unknown command: [%s].\n",command->progname);
        char errstr[300] = "Unknown command: [";
        char errstr2[100] = "].\n";
        strcat(errstr,command->progname);
        strcat(errstr,errstr2);
        write(STDERR_FILENO,errstr,strlen(errstr));
      }
      exit(EXIT_FAILURE);
    }
    else{
      signal(SIGCHLD,sig_handler);
      table[0][pipe_index] = pipe_number;
      table[1][pipe_index] = read;
      table[2][pipe_index] = writer;
    }
  }
  else{
    while((pid = fork())<0){
      ;
    }
    if(pid == 0){
      if(in != STDIN_FILENO){
        dup2(in,STDIN_FILENO);
        close(in);
      }
      dup2(table[2][pipe_index],STDOUT_FILENO);
      if(symbol == '!')
        dup2(table[2][pipe_index],STDERR_FILENO);
      // else
        // dup2(sockfd,STDERR_FILENO);
      if (execvp(command->args[0],command->args) == -1) {
        char errstr[300] = "Unknown command: [";
        char errstr2[100] = "].\n";
        strcat(errstr,command->progname);
        strcat(errstr,errstr2);
        write(STDERR_FILENO,errstr,strlen(errstr));
      }
      exit(EXIT_FAILURE);
    }
    
    else{
      signal(SIGCHLD,sig_handler);
      close(read);
      close(writer);
    }
  }
  return 1;
}

int  mysh_userpipeout(int in,int pipeout,int out,int recvUid,cmd_struct* command,vector<ClientData>::iterator currentClientIt,int* uidTable,std::vector<ClientData> &clientTable,string line)
{
  // cout<<"recvUid = "<<recvUid<<endl;
  bool exist = true;
  bool insend = false;
  int devNull = -1;
  vector<ClientData>::iterator recvClientIt = GetClientByUid(clientTable,recvUid);
  if(!(exist = ClientExist(recvUid,uidTable))){
    devNull = open("/dev/null", O_WRONLY); 
    out = devNull;
    close(pipeout);
    cout <<"*** Error: user #"<<recvUid<<" does not exist yet. ***" << endl;
    fflush(stdout);
    // return 1;
  }
  else if((insend = IsInSendTable(recvClientIt->uid, currentClientIt))){
    devNull = open("/dev/null", O_WRONLY);
    out = devNull;
    close(pipeout);
    cout << "*** Error: the pipe #"<<currentClientIt->uid<<"->#"<<recvUid<<" already exists. ***" <<endl;
    fflush(stdout);
    // return 1;
  }
  pid_t pid;
  int status;

  while((pid = fork())<0){
    ;
  }
    if (pid == 0) {
      if(in != STDIN_FILENO){
        dup2(in,STDIN_FILENO);
        close(in);
      }
      if(out != STDOUT_FILENO){
        dup2(out,STDOUT_FILENO);
        close(out);
      }
      // dup2(sockfd,STDERR_FILENO);
      if (execvp(command->args[0],command->args) == -1) {
          char errstr[300] = "Unknown command: [";
          char errstr2[100] = "].\n";
          strcat(errstr,command->progname);
          strcat(errstr,errstr2);
          write(STDERR_FILENO,errstr,strlen(errstr));
      }
      exit(EXIT_FAILURE);
    } 
    else {
      if(out == devNull){
        waitpid(pid, &status, WUNTRACED);
        // cout<<"end of waiting"<<endl;
      }
      else{
        signal(SIGCHLD,sig_handler);
        // cout<<"signal and wait"<<endl;
      }
      if(!exist){
        close(out);
        // cout <<"*** Error: user #"<<recvUid<<" does not exist yet. ***" << endl;
        // fflush(stdout);
        return 1;
      }
      else if(insend){
        close(out);
        // cout << "*** Error: the pipe #"<<currentClientIt->uid<<"->#"<<recvUid<<" already exists. ***" <<endl;
        // fflush(stdout);
        return 1;
      }
      else{
        ClientTranInfo temp;
        temp.TargetClientUid = recvUid;
        temp.pipeRead = pipeout;
        currentClientIt->sendTable.push_back(temp);
        BroadCast(4, line, currentClientIt, recvUid);
      }
    }
  return 1;
}

cmd_struct* mysh_parse_command(char* str) {
  char* copy = strndup(str, MAX_LEN);
  char* token;
  int position = 0;
  cmd_struct* command = (cmd_struct*)calloc(sizeof(cmd_struct) + MAX_LEN * sizeof(char*), 1);

  token = strtok(str, MYSH_TOKEN_DELIM);
  while (token != NULL) {
    command->args[position++] = token;
    token = strtok(NULL, MYSH_TOKEN_DELIM);
  }
  command->args[position] = NULL;
  command->progname = command->args[0];
  command->redirect[0] = command->redirect[1] = -1;
  return command;
}

pipeline_struct* mysh_parse_pipeline(char* str){
  char* copy = strndup(str, MAX_LEN);
  char* cmd_str;
  int n_cmds = 0;
  int i = 0;
  int numberpipe_or_not = 0;
  bool userpipeIn=false,userpipeOut=false;
  pipeline_struct* ret;
  for (char* cur = copy; *cur; cur++) {
    if (*cur == '|' || *cur == '!' || *cur == '>' || *cur == '<')
      ++n_cmds;
    if ((*cur == '|' || *cur == '!') && *(cur+1)>='0' && *(cur+1)<='9')
      numberpipe_or_not = 1;
    if ((*cur == '<') && *(cur+1)>='0' && *(cur+1)<='9')
      userpipeIn = true;
    if ((*cur == '>') && *(cur+1)>='0' && *(cur+1)<='9')
      userpipeOut = true;
  }
  ++n_cmds;

  // cout<<"There are "<<n_cmds<<" cmds"<<endl;
  ret = (pipeline_struct*)calloc(sizeof(pipeline_struct) + (n_cmds + 1) * sizeof(cmd_struct*), 1);
  ret->pipes_and_FR = (char*)malloc((n_cmds-1)*sizeof(char));
  ret->n_cmds = n_cmds;
  ret->numberpiped = numberpipe_or_not;
  ret->userpipeIn = userpipeIn;
  ret->userpipeOut = userpipeOut;
  
  // cout<<"ret create success"<<endl;
  i=0;
  while((cmd_str = strsep(&copy, "|!><"))) {
    ret->cmds[i++] = mysh_parse_command(cmd_str);
  }

  // cout<<"parse command success!"<<endl;
  copy = strndup(str, MAX_LEN);
  i=0;
  for (char* cur = copy; *cur; cur++) {
    // cout<<*cur<<" ";
    if (*cur == '|' || *cur == '!' || *cur == '>' || *cur == '<') 
      ret->pipes_and_FR[i++] = *cur;
    if (*cur == '<' && userpipeIn)
      ret->pipeInNum = atoi(ret->cmds[i]->args[0]);
  }
  return ret;
}

void print_command(cmd_struct* command) {
  char** arg = command->args;
  int i = 0;

  fprintf(stderr, "progname: %s\n", command->progname);

  for (i = 0, arg = command->args; *arg; ++arg, ++i) {
    fprintf(stderr, " args[%d]: %s\n", i, *arg);
  }
}


void print_pipeline(pipeline_struct* pipeline) {
  cmd_struct** cmd = pipeline->cmds;
  int i = 0;

  fprintf(stderr, "n_cmds: %d\n", pipeline->n_cmds);

  for (i = 0; i < pipeline->n_cmds; ++i) {
    fprintf(stderr, "cmds[%d]:\n", i);
    if(i < pipeline->n_cmds-1)
      printf("%c ",pipeline->pipes_and_FR[i]);
    print_command(cmd[i]);
  }
    printf("numberpiped = %d\n",pipeline->numberpiped);
  printf("\n");
}

int AssignId(int* uidTable, int max_cli){
  for(int i=1 ; i < max_cli+1 ; i ++)
    {
        if(uidTable[i] == 0)
        {
            uidTable[i] = 1 ;
            return i;
        }
    }
    return 0;
}

void PrintWelcome(int sockfd){
    
    dup2(sockfd,1);
    cout << "****************************************" << endl;
    cout << "** Welcome to the information server. **" << endl;
    cout << "****************************************" << endl;
}

void EraseExitClient(std::vector<ClientData>&clientTable,std::vector<ClientData>::iterator currentClientIt)
{
    clientTable.erase(currentClientIt);
}

std::vector<ClientData>::iterator GetClientByFd(std::vector<ClientData> &clientTable,int fd)
{
    vector<ClientData>::iterator it = clientTable.begin();
    while( it != clientTable.end())
    {
      
        if(it->fdNum == fd){
            // cout<<"In function fd = "<<it->fdNum<<endl;
            // cout<<"In function uid = "<<it->uid<<endl;
            // cout<<"In function ip = "<<it->ip<<endl;
            // cout<<"In function port = "<<it->port<<endl;
            // cout<<"In function envvalue = "<<GetEnvValue("PATH",it)<<endl;
            return  it;
        }
        it ++;
    }
    // cout<<it->fdNum<<endl;
    return it;
}

std::string GetEnvValue(string envname,std::vector<ClientData>::iterator currentClient)
{
    vector<EnvVar>::iterator it = (*currentClient).envVarTable.begin();
    while(it != (*currentClient).envVarTable.end())
    {
      // cout<<envname<<endl;
        if(envname == (*it).name){
            // cout<<"in function name = "<<it->name<<endl;
            return (*it).value;
        }
        it ++;
    }
    // cout<<"in function name = "<<it->name<<endl;
    return "";
}

std::vector<ClientData>::iterator GetClientByUid(std::vector<ClientData>&clientTable,int uid)
{
    vector<ClientData>::iterator it = clientTable.begin();
    while( it != clientTable.end())
    {
        if((*it).uid == uid)
            return  it;
        it ++;
    }
    return it;
}

void EraseEnv(string envname,std::vector<ClientData>::iterator currentClient)
{
    vector<EnvVar>::iterator it = (*currentClient).envVarTable.begin();
    while(it != (*currentClient).envVarTable.end())
    {
        if(envname == (*it).name)
        {
            // cout<<"erase success"<<endl;
            (*currentClient).envVarTable.erase(it);
            return;
        }
        it ++;
    }
}

bool IsInSendTable(int receiverfd,vector<ClientData>::iterator sender)
{
    vector<ClientTranInfo>::iterator it = sender->sendTable.begin();
    while(it != sender->sendTable.end())
    {
        if(it->TargetClientUid == receiverfd)
            return true;
        it ++;
    }
    return false;
}

// bool IsInRecvTable(int senderfd,std::vector<ClientData>::iterator receiver)
// {
//     vector<ClientTranInfo>::iterator it = receiver->recvTable.begin();
//     while(it != receiver->recvTable.end())
//     {
//         if(it->SourceClientFd == senderfd)
//             return true;
//         it ++;
//     }
//     return false;
// }

bool ClientExist(int uid,int* uidTable)
{
    if (uid <= 0 || uid > MAX_CLIENT_NUM)
      return false;
    if(uidTable[uid] != 0)
        return true;
    return false;
}


vector<ClientTranInfo>::iterator GetSendIt(int receiverUid,vector<ClientData>::iterator sender)
{
    vector<ClientTranInfo>::iterator it = sender->sendTable.begin();
    while(it != sender->sendTable.end())
    {
        if(it->TargetClientUid == receiverUid)
            return it;
        it ++;
    }
    return it;
}
// int GetPipeFdBySenderFd(int senderFd,std::vector<pipeFd> &userPipeTable){
//   vector<pipeFd>
// }

void EraseExitSenderUserPipe(vector<ClientData>::iterator currentClientIt){
  vector<ClientTranInfo> clientSendTable = currentClientIt->sendTable;
  vector<ClientTranInfo>::iterator it = clientSendTable.begin();
  while(it != clientSendTable.end()){
    if(it->pipeRead>2){
      close(it->pipeRead);
    }
    it++;
  }
}

void EraseExitRecvUserPipe(vector<ClientData>::iterator currentClientIt,vector<ClientData>::iterator senderClientIt){
  // cout<<"enter erasing"<<endl;
  // int i =0;
  if (currentClientIt == senderClientIt)
    return;
  int recvUid = currentClientIt->uid;
  // cout<<"Start to clean "<<senderClientIt->uid<<"'s sneding msg"<<endl;
  vector<ClientTranInfo>::iterator it = senderClientIt->sendTable.begin();
  while(it != senderClientIt->sendTable.end()){
    if(it->TargetClientUid == recvUid){
      // cout<<"erasing client "<<senderClientIt->uid<<" to "<<currentClientIt->uid<<"'s sending msg"<<endl;
      close(it->pipeRead);
      // cout<<"closing success"<<endl;
      senderClientIt->sendTable.erase(it);
      // cout<<"erasing success"<<endl;
      break;
    }
    // i++;
    // cout<<i<<endl;
    it++;
  }
}