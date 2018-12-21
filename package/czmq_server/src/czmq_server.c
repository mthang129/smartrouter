#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <czmq.h>
#include <sys/queue.h>

void *receive_weak_clients_thread( void *ptr );
void *pub_request_thread(void *ptr);
void *receive_response_thread(void *ptr);
void prepare_disconnect_client(char *sender_MAC,  char *weak_client_MAC);

zsock_t *pull_weak_clients ;
// zsock_t *push ;
zsock_t *pull_response;

char weak_client_MAC[30];
char weak_client_SIG[4];
bool weak_client_check = false;
pthread_mutex_t weak_client_mutex = PTHREAD_MUTEX_INITIALIZER;

char disconnect_sender_MAC[30];
char disconnect_client_MAC[30];
bool disconnect_check = false;
pthread_mutex_t disconnect_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t weak_list_mutex = PTHREAD_MUTEX_INITIALIZER;

// struct weak_client {
//    char weak_client_MAC[30];
//    char sender_MAC[30];
//    int signal;
// };

typedef struct Weak_clients{
    LIST_ENTRY(Weak_clients) pointers;
    char weak_client_MAC[30];
    char sender_MAC[30];
    int signal;
} Weak_client;

Weak_client *create_weak_client(char *weak_client_MAC, char *sender_MAC, int signal) {
    Weak_client *weak_client = (Weak_client *)malloc(sizeof(Weak_client));
    strcpy(weak_client->weak_client_MAC, weak_client_MAC);
    strcpy(weak_client->sender_MAC, sender_MAC);
    weak_client->signal = signal;
    return weak_client;
}
LIST_HEAD(weak_client_list, Weak_clients) weak_clients;

bool is_client_existed(char* weak_client_MAC) {
    pthread_mutex_lock(&weak_list_mutex);
    Weak_client *temp;
    LIST_FOREACH(temp, &weak_clients, pointers) {
        if( !strcmp(temp->weak_client_MAC, weak_client_MAC)){
            pthread_mutex_unlock(&weak_list_mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&weak_list_mutex);
    return false;
}

char *substr (char *source, int start, int count, char *result)
{
    int i;
    for (i = 0 ; i < (count) ; i++) {
        result[i] = source[i + start];
    }
    result[i] = '\0';
    return result;
}

int main()
{
    LIST_INIT(&weak_clients);
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;
    const char *message1 = "Thread 1";
    int  iret1;
    iret1 = pthread_create( &thread1, NULL, receive_weak_clients_thread, (void*) message1);
    if(iret1)
    {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
        exit(EXIT_FAILURE);
    }

    printf("pthread_create() for thread 1 returns: %d\n",iret1);


const char *message2 = "Thread 2";
    int  iret2;
    iret2 = pthread_create( &thread2, NULL, pub_request_thread, (void*) message2);
    if(iret2)
    {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
        exit(EXIT_FAILURE);
    }

    printf("pthread_create() for thread 2 returns: %d\n",iret2);

    const char *message3 = "Thread 3";
    int  iret3;
    iret3 = pthread_create( &thread3, NULL, receive_response_thread, (void*) message3);
    if(iret3)
    {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret3);
        exit(EXIT_FAILURE);
    }

    printf("pthread_create() for thread 3 returns: %d\n",iret3);

    zactor_t *auth = zactor_new (zauth, NULL);
    assert (auth);
    zstr_sendx (auth, "ALLOW", "192.168.1.1","192.168.1.2", NULL);
    zsock_wait (auth);



    pthread_join( thread1, NULL);
    pthread_join( thread2, NULL);
    pthread_join( thread3, NULL);
    exit(EXIT_SUCCESS);
}

void *receive_weak_clients_thread( void *ptr )
{
        pull_weak_clients = zsock_new_pull ("@tcp://192.168.1.1:5560");
        zsock_set_zap_domain (pull_weak_clients, "global");

        while(1) {
            char *string = zstr_recv (pull_weak_clients);
            puts (string);
            const char s[2] = ";";
            char *token;
            token = strtok(string, s);
            while( token != NULL )
            {
                char client_MAC[30];
                char client_SIG[4];
                char sender_MAC[30];
                substr(token,1,29,client_MAC);
                substr(token,31,3,client_SIG);
                substr(token,35,29,sender_MAC);

               
                if (!is_client_existed(client_MAC)) {
                    Weak_client *new_weak_client  =  create_weak_client(client_MAC, sender_MAC, atoi(client_SIG));
                    
                    pthread_mutex_lock(&weak_list_mutex);
                    LIST_INSERT_HEAD(&weak_clients, new_weak_client, pointers);
                    
                    pthread_mutex_unlock(&weak_list_mutex);
                }
                
                pthread_mutex_lock( &weak_client_mutex);
                
                strcpy(weak_client_MAC, client_MAC);
                strcpy(weak_client_SIG, client_SIG);
                weak_client_check = true;
                
                pthread_mutex_unlock( &weak_client_mutex);
                
                printf("\nreceive_weak_clients_thread WEAK: :  MAC %s, SIG %s", weak_client_MAC, weak_client_SIG);
                token = strtok(NULL, s);
            }

            zstr_free (&string);
        }
    return 0;
}

void *pub_request_thread ( void *ptr) {

    zsock_t *check_request_socket = zsock_new_pub("@tcp://192.168.1.1:5556");
    assert(check_request_socket);

    zsock_t *disconnect_socket = zsock_new_pub("@tcp://192.168.1.1:5557");
    assert(disconnect_socket);

    zsock_set_zap_domain (check_request_socket, "global");
    zsock_set_zap_domain (disconnect_socket, "global");

    while(!zsys_interrupted ) {
        zclock_sleep(10);

        pthread_mutex_lock(&weak_client_mutex);
        if (weak_client_check) {
            zsys_info("\npub_request_thread: SEND TOPIC");
            zsock_send(check_request_socket, "sss", "TOPIC", weak_client_MAC, weak_client_SIG);
            weak_client_check = false;
        }
        pthread_mutex_unlock(&weak_client_mutex);

        pthread_mutex_lock(&disconnect_mutex);
       
        if (disconnect_check) {
            zsys_info("\n pub_request_thread: SEND DISCONNECT PUB mac dis: %s mac sender %s", disconnect_client_MAC, disconnect_sender_MAC);
            int i = zsock_send(disconnect_socket, "sss", "DISCONNECT", disconnect_client_MAC, disconnect_sender_MAC);
            zsys_info("i: %d", i);
            zsys_info("\npub_request_thread: SEND DISCONNECT DONE");
            disconnect_check = false;
        }
        pthread_mutex_unlock(&disconnect_mutex);
    }
    zsock_destroy(&check_request_socket);
    zsock_destroy(&disconnect_socket);
    return 0;
}


void *receive_response_thread(void *ptr) {
    pull_response = zsock_new_pull ("@tcp://192.168.1.1:5561");
    zsock_set_zap_domain (pull_response, "global");

    while(1) {
        char *res_msg = zstr_recv (pull_response);
        printf("\n RECEIVE_RESPONSE: RESPONSE MESSAGE RECEIVE: ");
        puts (res_msg);
        char weak_client_MAC[30];
        char sender_MAC[30];

        substr(res_msg, 1, 29,weak_client_MAC);
        substr(res_msg, 31, 29, sender_MAC );
        printf("\n sender mac: %s", sender_MAC);

        pthread_mutex_lock(&weak_list_mutex);

        Weak_client *temp;
        LIST_FOREACH(temp, &weak_clients, pointers) {
            printf("trong LIST FOREACH %s", temp->sender_MAC);
            if(!strcmp(temp->weak_client_MAC, weak_client_MAC) && (strcmp(temp->sender_MAC, sender_MAC)!=0)) {
            // if(!strcmp(temp->weak_client_MAC, weak_client_MAC) ) {
                prepare_disconnect_client(temp->sender_MAC, weak_client_MAC);
                LIST_REMOVE(temp, pointers);
            }
        }
        pthread_mutex_unlock(&weak_list_mutex);
        zstr_free (&res_msg);
    }
return 0;
}

void prepare_disconnect_client(char* sender_MAC, char* weak_client_MAC) {
    pthread_mutex_lock(&disconnect_mutex);
    printf("\n SEND DISCONNECT SIGNAL");
    printf("\n Disconnect client: %s", weak_client_MAC);
    printf("\n Disconnect from router: %s ", sender_MAC);
    strcpy(disconnect_sender_MAC, sender_MAC);
    strcpy(disconnect_client_MAC, weak_client_MAC);
    disconnect_check = true;
    pthread_mutex_unlock(&disconnect_mutex);
}