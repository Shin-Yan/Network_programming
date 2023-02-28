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

#include "simple_builtins.h"

int sockfd;

void mysh_loop(int sockfd)
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
    char* prompt="% ";
    write(sockfd,prompt,2);
    line = mysh_read_line(sockfd);
    
    pipeline = mysh_parse_pipeline(line);
    status = mysh_execute(pipeline,mysh_numberpipe_table,sockfd);
    free(line);
    free(pipeline);
  } while (status);
}
void Int_sig_handle(int num){
  close(sockfd);
  exit(0);
}
int main(int argc, char **argv)
{
  // initialize the path to /bin and ./
  char *initial_setting_env[]={"setenv","PATH","bin:.",NULL};
  int connfd, len, port;
	struct sockaddr_in serv_addr, cli;
  bool bOptVal = false;
  int bOptLen = sizeof (bool);

  // Set the port to argv[1]
  port = atoi(argv[1]);

  // socket create and verification
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  
  if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");

  bOptVal = true;
  signal(SIGINT,Int_sig_handle);
  setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(char*)&bOptVal,bOptLen);

	bzero(&serv_addr, sizeof(serv_addr));
  // assign IP, PORT
  serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

  // Binding newly created socket to given IP and verification
	if ((bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) != 0) {
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
	while(1){
    mysh_setenv(initial_setting_env,0);
    len = sizeof(cli);

    // Accept the data packet from client and verification
	  connfd = accept(sockfd,(struct sockaddr *)&cli, &len);
	  if (connfd < 0) {
		  printf("server accept failed...\n");
		  exit(0);
	  }
	  else
		  printf("server accept the client...\n");

    // Run command loop.
    mysh_loop(connfd);
    printf("client exits.\n");
    close(connfd);
  }
  
  // Perform any shutdown/cleanup.
  close(sockfd);
  return EXIT_SUCCESS;
}