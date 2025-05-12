#include "proxy_parse.h"
#include <arpa/inet.h>
#include <bits/pthreadtypes.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_CLIENTS 10

typedef struct cache_element cache_element;
// datType for a particular cache element
struct cache_element {
  char *data; // incoming data
  int len;
  char *url;
  time_t lru_time_track; // time based lru least recently used one is removed
  cache_element *next;   // to maintain a link btw cache elements all cache are
                         // stored like a linked list
};

cache_element *find(char *url); /*defined function to find url */
int add_cache_element(char *data, int size, char *url);
void remove_cache_element();

int port_number = 8080;
int proxy_socketId; // each client will have its own socket id while connecting
                    // to the server
pthread_t tid[MAX_CLIENTS]; // we are spawning a thread for each client
/* here 10 clients as defined above an array of threadid for each client */
sem_t semaphore;
pthread_mutex_t lock;

cache_element *head; // head to keep track of linked list
int cache_size;

int main(int argc, char *argv[]) {
  int client_socketId, client_len;
  struct sockaddr_in server_addr, client_addr;
  sem_init(&semaphore, 0,
           MAX_CLIENTS); // intitializing semaphore with value 10 MAX_CLIENTS
  pthread_mutex_init(&lock, NULL); // initializing the mutex lock not actually
                                   // locking the critical section
  if (argc == 2) {
    port_number = atoi(argv[1]); // atoi taken input from cmd line ./proxy 8080
                                 // here 8080 is 1 element like a array
  } else {
    printf("too few args\n");
    exit(1); // system call
  }

  printf("starting proxy server at port number: %d\n", port_number);
  proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);

  if (proxy_socketId < 0) {
    perror("failedl to create a socket\n");
    exit(1);
  }

  int reuse = 1;
  if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
                 sizeof(reuse) < 0)) {
    perror("setSocket failed\n");
  }

  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port_number);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(proxy_socketId, (struct sockaddr *)&server_addr,
           sizeof(server_addr) < 0)) {
    perror("port is not available ");
    exit(1);
  }

  printf("Binding on port %d\n", port_number);
  int listen_status = listen(proxy_socketId, MAX_CLIENTS);
  if (listen_status < 0) {
    perror("error in listening\n");
    exit(1);
  }

  int i = 0;
  int Connected_socketId[MAX_CLIENTS];

  while (1) {
    bzero((char *)&client_addr, sizeof(client_addr));
    client_len = sizeof(client_addr);
    client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr,
                             (socklen_t *)&client_len);
    if (client_socketId < 0) {
      printf("not able to connect");
      exit(1);
    } else {
      Connected_socketId[i] = client_socketId;
    }

    struct sockaddr *client_pt = (struct sockaddr_in *)&client_addr;
    struct in_addr ip_addr = client_pt->sin_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
    printf("client is connected with port number %d and ip address is %s\n",
           ntohs(client_addr.sin_port, str));

    pthread_create(&tid[i], NULL, thread_fn, (void *)&Connected_socketId[i]);
    i++;
  }
  close(proxy_socketId);
  return 0;
}
