#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#define MAX_CLIENTS 29
#define BUFFER_SZ 256
#define NAME_SZ 32

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

/* Client structure */
typedef struct {
	struct sockaddr_in address;
	int sockfd; 
	int uid;
	char name[NAME_SZ];
} client_t;

void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout () {
    printf("\r%s", "> ");
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

/* Add clients to queue */
void queue_add(client_t *client) {
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (!clients[i]) {
			clients[i] = client;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Del clients from queue */
void queue_remove(int uid) {
	pthread_mutex_lock(&clients_mutex);

	for (int i=0; i < MAX_CLIENTS; ++i) {
		if (clients[i]) {
			if (clients[i] -> uid == uid) {
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients (not to sender) */
void send_message(char *s, int uid) {
	pthread_mutex_lock(&clients_mutex);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		if (clients[i]) {
			if (clients[i] -> uid != uid) {
				if (write(clients[i] -> sockfd, s, strlen(s)) < 0) {
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Handle communication with client */
void *handle_client(void *arg) {
	char buff_out[BUFFER_SZ];
	char name[NAME_SZ];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	if (recv(cli -> sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32 - 1) {
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else {
		strcpy(cli->name, name);
		sprintf(buff_out, "%s has joined\n", cli->name);
		printf("%s", buff_out);
		send_message(buff_out, cli->uid);
	}

	bzero(buff_out, BUFFER_SZ);

	while(1){
		if (leave_flag) {
			break;
		}

		int receive = recv(cli -> sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if (strlen(buff_out) > 0) {
				send_message(buff_out, cli->uid);

				str_trim_lf(buff_out, strlen(buff_out));
				printf("%s -> %s\n", buff_out, cli->name);
			}
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0) {
			sprintf(buff_out, "%s has left\n", cli -> name);
			printf("%s", buff_out);
			send_message(buff_out, cli -> uid);
			leave_flag = 1;
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}
		bzero(buff_out, BUFFER_SZ);
	}

  /* Delete client from queue and yield thread */
	close(cli -> sockfd);
  queue_remove(cli -> uid);
  free(cli);
  cli_count--;
  pthread_detach(pthread_self());

	return 0;
}

int main(int argc, char *argv[]) {
  int sockfd, newsockfd;
  int option = 1;
  pthread_t tid;

  /* Check port number */
  if(argc<2){
    fprintf(stderr, "usage %s hostname port\n", argv[0]);
    return 0;
  }

  /* First call to socket() function */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    perror("ERROR opening socket");
    return 0;
  }

  struct sockaddr_in serv_addr, cli_addr;

  /* Initialize socket structure */
  bzero((char *) &serv_addr, sizeof(serv_addr));
  uint16_t portno = (uint16_t) atoi(argv[1]);

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  /* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR , &option, sizeof(option)) < 0) {
		perror("ERROR: setsockopt failed");
    return 0;
	}

  /* Now bind the host address using bind() call.*/
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: Binding");
    return 0;
  }

  /* Now start listening for the clients, here process will
    * go in sleep mode and will wait for the incoming connection
  */

  if (listen(sockfd, 5) < 0) {
    perror("ERROR: Socket listening failed");
    return 0;
	}

  while(1) {

    socklen_t clilen = sizeof(cli_addr);

    /* Accept actual connection from the client */
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    /* Checking if the chat is full */
    if ((cli_count) == MAX_CLIENTS) {
      perror("The chat is full. Connection refused: ");
      print_client_addr(cli_addr);
      printf(":%d\n", cli_addr.sin_port);
      close(newsockfd);
      continue;
    }

    /* Creating client for server */
    client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli -> address = cli_addr;
		cli -> sockfd = newsockfd;
		cli -> uid = uid++;

		/* Add client to the queue*/
		queue_add(cli);
    /* Create thread */
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Sleep mode for reduce CPU load */
		sleep(1);
  } 

  return 0;
}