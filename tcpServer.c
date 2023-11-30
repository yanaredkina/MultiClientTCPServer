// task: tcp-server interacting with multiple clients
// creating clients: nc 127.0.0.1 5000
// monitoring sockets status: netstat -an | grep 5000

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#define SERVER_PORT (5000)
#define MAX_LINE (1024)
#define MAX_THREADS (5)


struct Connections {
    int storage[MAX_THREADS];
    pthread_mutex_t mutex;
};

void set(struct Connections *clients, int id, int socket) {
    pthread_mutex_lock(&clients->mutex);
  	clients->storage[id] = socket;
  	pthread_mutex_unlock(&clients->mutex);
}

void unset(struct Connections *clients, int id) {
  	pthread_mutex_lock(&clients->mutex);
  	clients->storage[id] = 0;
  	pthread_mutex_unlock(&clients->mutex);
}

void elements(struct Connections *clients, int * result) {
    pthread_mutex_lock(&clients->mutex);
    memcpy(result, clients->storage, MAX_THREADS);
    pthread_mutex_unlock(&clients->mutex);
}

typedef struct Node *Link; 
typedef struct Node {
    int clientSocket;
    Link next;
} node;

struct WorkQueue {
    Link first;
    Link last;
  	pthread_mutex_t mutex;
};

void enqueue(int clientSocket, struct WorkQueue * queue) {
  	pthread_mutex_lock(&queue->mutex);
    Link tmp = (Link) malloc(sizeof(node));
    tmp->clientSocket = clientSocket;
    tmp->next = NULL;
    if (queue->first == NULL) {
        queue->first = tmp;
        queue->last = tmp;
        
    } else {
        queue->last->next = tmp;
        queue->last = tmp;
    }
    pthread_mutex_unlock(&queue->mutex);
}

int dequeue(struct WorkQueue * queue) {
    pthread_mutex_lock(&queue->mutex);
    if (queue->first == NULL) {
      	pthread_mutex_unlock(&queue->mutex);
        return 0;
    } else {
        int res = queue->first->clientSocket;
        Link tmp = queue->first;
        queue->first = queue->first->next;
        free(tmp);

        if (queue->first == NULL)
        {
            queue->last = NULL;
        }
        pthread_mutex_unlock(&queue->mutex);
        return res;
    }
}

struct Context {
    int id;
    struct WorkQueue* workQueue;
    struct Connections* clients;
};


void* processingService(void* args) {
    struct Context* context = (struct Context*) args;
  	int id = context->id;
  	struct WorkQueue* queue = context->workQueue;
  	struct Connections* clients = context->clients;
  
    int thisThread = (int) pthread_self();
    printf("Tread %d start working...\n", thisThread);
    
    while(1) {
        int clientSocket;
        while((clientSocket = dequeue(queue)) < 1) {
            sleep(5);
        }
        set(clients, id, clientSocket);
        write(clientSocket, "Enter your message:\n", 20);
    
        unsigned char buffer[MAX_LINE] = {0};
        unsigned char q[MAX_LINE] = "quit\n";
        ssize_t size;
    
        while((size = read(clientSocket, buffer, MAX_LINE - 1)) > 0) {
            printf("Tread %d reading...\n", thisThread);

            if (strcmp((const char*)buffer, (const char*) q) == 0) {
                break;
            }
          	int result[MAX_THREADS];
          	elements(clients, result);
            for (int i = 0; i < MAX_THREADS; i++) {
              	if (result[i] > 0 && i != id) {
                  write(result[i], buffer, size);
                }
            }
        }
  			
      	unset(clients, id);
        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
        printf("Tread %d closing client %d\n", thisThread, clientSocket);
    }
    pthread_exit(NULL);
}

void sig_handler(int signum) {
    exit(0);
}
    

int main(int argc, char** argv) {
    int serverPort = 5000;
    int ttl = 0;
    
    int option;
    while((option = getopt(argc, argv, "pw")) != -1) {
        switch(option) {
            case 'p': 
                serverPort = atoi(argv[optind]);
                optind++;
                break;
            case 'w':
                ttl = atoi(argv[optind]);
                optind++;
                break;
            
            default: 
                return 1;
        }
    }
    
    signal(SIGALRM, sig_handler);
    if (ttl > 0) {
        alarm(ttl);
    }
    
    printf("TCP SERVER is starting\n");
    
    int serverSocket;
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket < 0) {
        perror("ERROR: error in calling socket()\n");
        exit(1);
    }
    
    struct sockaddr_in servaddr;
    bzero((void *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(serverPort);
    
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("ERROR: error in calling setsockopt()\n");
        exit(1);
    }
    
    if (bind(serverSocket, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("ERROR: error in calling bind()\n");
        exit(1);
    }
    
    if (listen(serverSocket, 5) < 0) {
        perror("ERROR: error in calling listen()\n");
        exit(1);
    }
    printf("listening ...\n");
    

    pthread_t threads[MAX_THREADS];
    int status;
    int count = 0;
    
    struct WorkQueue queue;
    memset(&queue, 0, sizeof(queue));
  	pthread_mutex_init(&queue.mutex, NULL);
  
  	struct Connections clients;
  	memset(&clients, 0, sizeof(clients));
    pthread_mutex_init(&clients.mutex, NULL);
    
    int clientSocket;
    
    while (1) {
        
        clientSocket = accept(serverSocket, (struct sockaddr *)NULL, (socklen_t *)NULL);
        if (clientSocket < 0) {
            perror("ERROR on accept\n");
            continue;
        }
        printf("Client %d connected to server\n", clientSocket);
        enqueue(clientSocket, &queue);
      	
        
        if (count < MAX_THREADS) {
          	struct Context context = {count, &queue, &clients};
            status = pthread_create(&threads[count++], NULL, processingService, &context);
            if (status != 0) {
                perror("ERROR: error in calling pthread_create()\n");
                exit(1);
            }
            pthread_detach(threads[count - 1]);
        }
    }
    
  	pthread_mutex_destroy(&queue.mutex);
    close(serverSocket);
    return 0;
    
}