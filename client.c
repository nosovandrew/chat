#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>

#define BUFFER_SZ 256

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];

void str_overwrite_stdout() {
  printf("%s", "> ");
  fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) {
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

void send_msg_handler() {
  char message[BUFFER_SZ] = {};
	char buffer[BUFFER_SZ + 32] = {};

  while(1) {
  	str_overwrite_stdout();
    fgets(message, BUFFER_SZ, stdin);
    str_trim_lf(message, BUFFER_SZ);

    if (strcmp(message, "exit") == 0) {
			break;
    } else {
      sprintf(buffer, "%s: %s\n", name, message);
      send(sockfd, buffer, strlen(buffer), 0);
    }

		bzero(message, BUFFER_SZ);
    bzero(buffer, BUFFER_SZ + 32);
  }
  catch_ctrl_c_and_exit(2);
}

void recv_msg_handler() {
	char message[BUFFER_SZ] = {};
  while (1) {
		int receive = recv(sockfd, message, BUFFER_SZ, 0);
    if (receive > 0) {
      printf("%s", message);
      str_overwrite_stdout();
    } else if (receive == 0) {
			break;
    } else {
			// -1
		}
		memset(message, 0, sizeof(message));
  }
}

int main(int argc, char **argv) {
  struct hostent *server;

  /* get client params */
	if(argc < 2){
		printf("Usage: %s <port>\n", argv[0]);
		return 0;
	}

	server = gethostbyname(argv[1]);
  if (server == NULL) {
    fprintf(stderr, "ERROR, no such host\n");
    return 0;
  }

	uint16_t port = (uint16_t) atoi(argv[2]);

	signal(SIGINT, catch_ctrl_c_and_exit);

	if (strlen(argv[3]) > 32 || strlen(argv[3]) < 2){
		printf("Name must be between 2 and 30 characters.\n");
		return 0;
	}
  strcpy(name, argv[3]);

	struct sockaddr_in server_addr;

	/* Configure socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  bcopy(server->h_addr, (char *) &server_addr.sin_addr.s_addr, (size_t) server->h_length);
  server_addr.sin_port = htons(port);


  /* Connect to server */
  int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (err == -1) {
		printf("ERROR: connect\n");
		return 0;
	}

	/* Send name to server */
	send(sockfd, name, 32, 0);
  
  /* Threads for sending and recieving messages */
	pthread_t send_msg_thread;
  if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
    return 0;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return 0;
	}

  /* Checking ctrl + c */
	while (1){
		if(flag){
			printf("\nGood Bye!\n");
			break;
    }
	}

	close(sockfd);

	return 0;
}