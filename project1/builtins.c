#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "builtins.h"

#define MAX_LEN 15000
#define MYSH_TOKEN_DELIM " \t\n\r"
#define STDERR_FILENO 2

char *builtin_str[] = {
  "printenv",
  "setenv",
  "exit"
};

int mysh_printenv(char **args)
{
    char *var;
    if(args[1] == NULL)
      return 1;
    else if(args[2])
      return 1;
    var = getenv(args[1]);
    if(var)
      printf("%s\n",var);
    return 1;
}

int mysh_setenv(char **args)
{
    setenv(args[1],args[2],1);
    return 1;
}

int mysh_exit(char **args)
{
  return 0;
}

int (*builtin_func[]) (char **) = {
  &mysh_printenv,
  &mysh_setenv,
  &mysh_exit
};

int mysh_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

char* mysh_read_line(void)
{
  char *line = NULL;
  ssize_t bufsize = 0; // have getline allocate a buffer for us
  if (getline(&line, &bufsize, stdin) == -1) {
    if (feof(stdin)){
      exit(EXIT_SUCCESS);  // We received an EOF
    } 
    else{
      perror("mysh: getline\n");
      exit(EXIT_FAILURE);
    }
  }
  return line;
}

int mysh_file_redirection(int in,cmd_struct* cmd,char* filename){
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0) {
    if(in != STDIN_FILENO){
      dup2(in,STDIN_FILENO);
      close(in);
    }
    int fd;
    if ((fd = creat(filename, S_IRUSR | S_IWUSR)) < 0)
      perror("creat() error");
    dup2(fd, STDOUT_FILENO);
    close(fd);
    if (execvp(cmd->args[0], cmd->args) == -1) {
      printf("Unknown command: [%s].\n",cmd->args[0]);
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    return mysh_file_redirection(in,cmd,filename);
  } else {
    waitpid(pid, &status, WUNTRACED);
  }
  return 1;
}

int mysh_execute(pipeline_struct* pipeline,int** numberpipe_table){
  int zero_index = -1;
  int n = pipeline->n_cmds;
  int i=0;
  pid_t pid;
  int in,fd[2];
  if (pipeline->cmds[0]->args[0]==NULL)
    return 1;
  for(int i=0 ; i<1000 ; i++){
      if(numberpipe_table[0][i] > 0)
        numberpipe_table[0][i]--;
      if(numberpipe_table[0][i] == 0)
        zero_index = i;
    }
  for (int j = 0; j < mysh_num_builtins(); j++) {
    if (strcmp(pipeline->cmds[i]->args[0], builtin_str[j]) == 0) {
      return (*builtin_func[j])(pipeline->cmds[i]->args);
    }
  }
  
  if(zero_index == -1)
    in = STDIN_FILENO;
  else{
    numberpipe_table[0][zero_index]--;
    in = numberpipe_table[1][zero_index];
    close(numberpipe_table[2][zero_index]);
  }
    
  for(i = 0; i < n-1; ++i){
    if(pipeline->pipes_and_FR[i]=='>'){
      return mysh_file_redirection(in,pipeline->cmds[i],pipeline->cmds[i+1]->args[0]);
    }
    pipe(fd);
    if((pipeline->numberpiped==1) &&(i == n-2)){
      return mysh_number_pipe(pipeline->cmds[n-2],in,fd[0],fd[1],pipeline->pipes_and_FR[n-2],pipeline->cmds[n-1]->args[0],numberpipe_table);
    }
    mysh_launch(in,fd[1],pipeline->cmds[i],0);
    close(fd[1]);
    in = fd[0];
  }
  mysh_launch(in,STDOUT_FILENO,pipeline->cmds[n-1],1);
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


int mysh_number_pipe(cmd_struct* command,int in,int read,int writer, char symbol, char* number, int** table){
  int pipe_number = atoi(number);
  int pipe_index;
  pid_t pid;
  int status;

  if((pipe_index=search(table,pipe_number))==-1){
    pipe_index = search(table,-1);
    pid = fork();
    if(pid == 0){
      if(in != STDIN_FILENO){
        dup2(in,STDIN_FILENO);
        close(in);
      }
      dup2(writer,STDOUT_FILENO);
      if(symbol == '!')
        dup2(writer,STDERR_FILENO);
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
    else if(pid<0){
      return mysh_number_pipe(command,in,read,writer,symbol,number,table);
    }
    else{
      signal(SIGCHLD,sig_handler);
      table[0][pipe_index] = pipe_number;
      table[1][pipe_index] = read;
      table[2][pipe_index] = writer;
    }
  }
  else{
    pid = fork();
    if(pid == 0){
      if(in != STDIN_FILENO){
        dup2(in,STDIN_FILENO);
        close(in);
      }
      dup2(table[2][pipe_index],STDOUT_FILENO);
      if(symbol == '!')
        dup2(table[2][pipe_index],STDERR_FILENO);
      if (execvp(command->args[0],command->args) == -1) {
        char errstr[300] = "Unknown command: [";
        char errstr2[100] = "].\n";
        strcat(errstr,command->progname);
        strcat(errstr,errstr2);
        write(STDERR_FILENO,errstr,strlen(errstr));
      }
      exit(EXIT_FAILURE);
    }
    else if(pid<0){
      return mysh_number_pipe(command,in,read,writer,symbol,number,table);
    }
    else{
      signal(SIGCHLD,sig_handler);
      close(read);
      close(writer);
    }
  }
  return 1;
}

int mysh_launch(int in,int out,cmd_struct* command,int last)
{
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0) {
    
    if(in != STDIN_FILENO){
      dup2(in,STDIN_FILENO);
      close(in);
    }
    if(out != STDOUT_FILENO){
      dup2(out,STDOUT_FILENO);
      close(out);
    }
    if (execvp(command->args[0],command->args) == -1) {
        char errstr[300] = "Unknown command: [";
        char errstr2[100] = "].\n";
        strcat(errstr,command->progname);
        strcat(errstr,errstr2);
        write(STDERR_FILENO,errstr,strlen(errstr));
    }
    exit(EXIT_FAILURE);
  } 
  else if (pid < 0) {
    return mysh_launch(in,out,command,last);
  } 
  else {
    if(last == 0)
      signal(SIGCHLD,sig_handler);
    else
      waitpid(pid, &status, WUNTRACED);
  }
  return 1;
}

cmd_struct* mysh_parse_command(char* str) {
  char* copy = strndup(str, MAX_LEN);
  char* token;
  int position = 0;
  cmd_struct* command = calloc(sizeof(cmd_struct) + MAX_LEN * sizeof(char*), 1);

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
  pipeline_struct* ret;
  for (char* cur = copy; *cur; cur++) {
    if (*cur == '|' || *cur == '!' || *cur == '>')
      ++n_cmds;
    if ((*cur == '|' || *cur == '!') && *(cur+1)>='0' && *(cur+1)<='9')
      numberpipe_or_not = 1;
  }
  ++n_cmds;
  //printf("%d",n_cmds);
  ret = calloc(sizeof(pipeline_struct) + (n_cmds + 1) * sizeof(cmd_struct*), 1);
  ret->pipes_and_FR = malloc((n_cmds-1)*sizeof(char));
  ret->n_cmds = n_cmds;
  ret->numberpiped = numberpipe_or_not;
  for (char* cur = copy; *cur; cur++) {
    if (*cur == '|' || *cur == '!' || *cur == '>') 
      ret->pipes_and_FR[i++] = *cur;
  }
  i=0;
  while((cmd_str = strsep(&copy, "|!>"))) {
    ret->cmds[i++] = mysh_parse_command(cmd_str);
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