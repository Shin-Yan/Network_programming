#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "builtins.h"

void mysh_loop(void)
{
  char *line;
  int status;
  pipeline_struct* pipeline;
  int **mysh_numberpipe_table = calloc(3*sizeof(int*),1);
  int zero_index=0;
  for(int i=0;i<3;i++){
    mysh_numberpipe_table[i] = calloc(1000,sizeof(int));
  }
  for(int i = 0; i < 3 ; i++ ){
    for(int j = 0; j < 1000 ; j++ ){
      mysh_numberpipe_table[i][j] = -1;
    }
  }
  do {
    // printf("in the new loop!\n");
    
    printf("%% ");
    line = mysh_read_line();
    
    pipeline = mysh_parse_pipeline(line);
    status = mysh_execute(pipeline,mysh_numberpipe_table);
    if(pipeline->numberpiped == 0)
      wait(NULL);
    free(line);
    free(pipeline);
  } while (status);
}

int main(int argc, char **argv)
{
  // initialize the path to /bin and ./
  char *initial_setting_env[]={"setenv","PATH","bin:.",NULL};
  mysh_setenv(initial_setting_env);
  // Run command loop.
  mysh_loop();
  
  // Perform any shutdown/cleanup.
  return EXIT_SUCCESS;
}