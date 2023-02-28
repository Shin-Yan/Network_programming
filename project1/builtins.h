#ifndef BUILTINS_H
#define BUILTINS_H

#define MAX_LEN 15000
#define TOKEN_SEP " \t\n\r"

typedef struct {
  char* progname;
  int redirect[2];
  char* args[];
} cmd_struct;

typedef struct {
  int numberpiped;
  int n_cmds;
  char *pipes_and_FR;
  cmd_struct* cmds[];
} pipeline_struct;

char *mysh_read_line(void);
char *mysh_next_non_empty(char **line);
int mysh_execute(pipeline_struct* pipeline, int** numberpipe_table);
int mysh_launch(int in, int out, cmd_struct* command,int final);
int mysh_file_redirection(int in,cmd_struct* cmd,char* filename);
int mysh_printenv(char **args);
int mysh_setenv(char **args);
int mysh_exit(char **args);
int mysh_number_pipe(cmd_struct* command,int in, int read , int write, char symbol, char* number , int** table);
int mysh_num_builtins();
cmd_struct* mysh_parse_command(char* str);
pipeline_struct* mysh_parse_pipeline(char* str);
void print_command(cmd_struct* command);
void print_pipeline(pipeline_struct* pipeline);
void sig_handler();

int search(int** table, int num);

// int** mysh_numberpipe_table;
// char *builtin_str[3];

/* For debugging purposes. */
//void print_command(cmd_struct* command);
//void print_pipeline(pipeline_struct* pipeline);


#endif  /*BUILTINS_H */
