#ifndef SINGLE_BUILTINS_H
#define SINGLE_BUILTINS_H

#define MAX_LEN 15000
#define TOKEN_SEP " \t\n\r"

typedef struct {
  char* progname;
  int redirect[2];
  char* args[];
} cmd_struct;

typedef struct {
  int numberpiped;
  int pipeInNum;
  bool userpipeIn;
  bool userpipeOut;
  int n_cmds;
  char *pipes_and_FR;
  cmd_struct* cmds[];
} pipeline_struct;

struct pipeFd{
  int pipeRead;
  int pipeWrite;
  int ownerClientfd;
};

struct ClientTranInfo
{
    int TargetClientUid;
    // int SourceClientFd;
    int pipeRead;
};

struct EnvVar{
    std::string name ;
    std::string value ;
};

struct ClientData{
    int fdNum;
    int uid;
    int** numberpipe_table;
    std::string name;
    std::string ip;
    unsigned short port;
    std::string sendMsg;
    std::vector<ClientTranInfo> sendTable;   // 紀錄User Pipe out的收件人 .
    // std::vector<ClientTranInfo> recvTable;   // 紀錄User Pipe in的寄件人 .
    std::vector<EnvVar> envVarTable;
};

char *mysh_read_line(int);
char *mysh_next_non_empty(char **);
int mysh_execute(pipeline_struct* , int** ,int,std::vector<ClientData>::iterator,std::vector<pipeFd>&,std::vector<ClientData>&,int*,char*);
int mysh_launch(int,int,cmd_struct*,int,int);
int mysh_file_redirection(int,cmd_struct*,char*,int);
int mysh_printenv(char **,int,std::vector<ClientData>::iterator);
int mysh_setenv(char **,int,std::vector<ClientData>::iterator);
int mysh_exit(char **,int,std::vector<ClientData>::iterator);
int mysh_number_pipe(cmd_struct*,int,int,int,char,char*,int**,int);
int  mysh_userpipeout(int,int,int,int,cmd_struct*,std::vector<ClientData>::iterator,int*,std::vector<ClientData>&,std::string);
int mysh_num_builtins();
cmd_struct* mysh_parse_command(char*);
pipeline_struct* mysh_parse_pipeline(char*);
void print_command(cmd_struct*);
void print_pipeline(pipeline_struct*);
void sig_handler();

int search(int**, int);

int AssignId(int*, int);
void PrintWelcome(int);
// void BroadCast(int,std::string,std::vector<ClientData>::iterator,int,int,fd_set,std::vector<ClientData> &clientTable);
void EraseExitClient(std::vector<ClientData>&,std::vector<ClientData>::iterator);
void EraseEnv(std::string,std::vector<ClientData>::iterator);
void BroadCast(int action,std::string msg,std::vector<ClientData>::iterator,int);
void EraseExitSenderUserPipe(std::vector<ClientData>::iterator currentClientIt);
void EraseExitRecvUserPipe(std::vector<ClientData>::iterator,std::vector<ClientData>::iterator);

// bool IsInRecvTable(int,std::vector<ClientData>::iterator);
bool IsInSendTable(int,std::vector<ClientData>::iterator);
bool ClientExist(int,int*);


std::vector<ClientData>::iterator GetClientByFd(std::vector<ClientData>&,int);
std::string GetEnvValue(std::string,std::vector<ClientData>::iterator);
std::vector<ClientData>::iterator GetClientByUid(std::vector<ClientData>&,int uid);
std::vector<ClientTranInfo>::iterator GetSendIt(int,std::vector<ClientData>::iterator);
// int** mysh_numberpipe_table;
// char *builtin_str[3];

/* For debugging purposes. */
void print_command(cmd_struct* command);
void print_pipeline(pipeline_struct* pipeline);


#endif  /*BUILTINS_H */
