#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <czmq.h>
#include <string.h>
#include <iwinfo.h>

extern void print_scanlist(const struct iwinfo_ops *iw, const char *ifname);
extern void print_assoclist(const struct iwinfo_ops *iw, const char *ifname);
extern char * format_bssid(unsigned char *mac);
extern char * format_signal(int sig);

void *send_weak_clients_thread( void *ptr );
void *sub_check_signal_thread(void *ptr);
void *sub_disconnect_signal_thread( void *ptr);
void response_check_signal(char *check_client_MAC);
void send_check_result(char *check_client_MAC, char *check_client_SIG);
// char *read_client_list();
void initialize_this_router_MAC();
void disconnect_client(char *disconnect_client_MAC);


#define MIN_SIGNAL_VALUE -10
#define CLIENT_BAN_TIME "1000"

// zsock_t *pull ;
zsock_t *push ;
zsock_t *response;

char this_router_MAC_in_ath9k_format[30];
static char this_router_MAC_in_driver_format[18] = { 0 };

char *substr (char *source, int start, int count, char *result)
{
    int i;
    for (i = 0; i < (count); i++) {
        result[i] = source[i + start];
    }
    result[i] = '\0';
    return result;
}

void concat(char **head_ptr, char *tail) {
    char *outstr;
    char *head = *head_ptr;
    int head_len = 0, tail_len = 0;

    if (head != NULL) 
        head_len = strlen(head);
    if (tail != NULL) 
        tail_len = strlen(tail);
    outstr = malloc(head_len + tail_len + 1);
    outstr[0] = '\0';
    if (head != NULL)
        strcat(outstr, head);
    if (tail != NULL)
        strcat(outstr, tail);
    free (head);
    *head_ptr = outstr;
}

void initialize_this_router_MAC() {
    const struct iwinfo_ops *iw = iwinfo_backend("wlan0");
    char *ifname = "wlan0";
    if (iw->bssid(ifname, this_router_MAC_in_driver_format))
        snprintf(this_router_MAC_in_driver_format, sizeof(this_router_MAC_in_driver_format), "00:00:00:00:00:00");
}

char *convert_MAC_format_driver_to_ath9k(char *this_router_MAC_in_ath9k_format, char *this_router_MAC_in_driver_format) {
    int i = 0;
    for(i = 0; i < 6; i++) {
        this_router_MAC_in_ath9k_format[5*i] = '0';
        this_router_MAC_in_ath9k_format[5*i + 1] = 'x';
        this_router_MAC_in_ath9k_format[5*i + 2] = this_router_MAC_in_driver_format[3*i];
        this_router_MAC_in_ath9k_format[5*i + 3] = this_router_MAC_in_driver_format[3*i + 1];
        if (i < 5) {
            this_router_MAC_in_ath9k_format[5*i + 4] = this_router_MAC_in_driver_format[3*i + 2];
        } else this_router_MAC_in_ath9k_format[5*i + 4] = '\0';
    }
}

char *client_weak_signal_list() {
    const struct iwinfo_ops *iw = iwinfo_backend("wlan0");
    char *ifname = "wlan0";
    char *output = NULL;
    int i, len;
	char buf[IWINFO_BUFSIZE];
    struct iwinfo_assoclist_entry *e;
    if (!iw->assoclist(ifname, buf, &len)) {
        for (i = 0; i < len; i += sizeof(struct iwinfo_assoclist_entry))
        {
            e = (struct iwinfo_assoclist_entry *) &buf[i];
            if (e->signal < MIN_SIGNAL_VALUE) {
                concat(&output, "{");
                concat(&output, format_bssid(e->mac));
                concat(&output, ",");
                concat(&output, format_signal(e->signal));
                concat(&output, ",");
                concat(&output, this_router_MAC_in_ath9k_format);
                if(i == len - sizeof(struct iwinfo_assoclist_entry)) {
                    concat(&output, "}");
                } else {
                    concat(&output, "};");
                }
            }
        }
    }
    iwinfo_finish();
    return output;
}

int main()
{
    initialize_this_router_MAC();
    convert_MAC_format_driver_to_ath9k(this_router_MAC_in_ath9k_format, this_router_MAC_in_driver_format);
    response = zsock_new_push (">tcp://192.168.1.1:5561");
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;
    pthread_t thread4;

    const char *message1 = "Thread send message";
    int  iret1;
    iret1 = pthread_create( &thread1, NULL, send_weak_clients_thread, (void*) message1);
    if(iret1)
    {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
        exit(EXIT_FAILURE);
    }

    printf("pthread_create() for thread 1 returns: %d\n",iret1);

    const char *message2 = "Thread sub TOPIC message";
    int  iret2;
    iret2 = pthread_create( &thread2, NULL, sub_check_signal_thread, (void*) message2);
    if(iret2)
    {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret2);
        exit(EXIT_FAILURE);
    }

    printf("pthread_create() for thread 2 returns: %d\n",iret2);

    const char *message3 = "Thread sub DISCONNECT message";
    int  iret3;
    iret3 = pthread_create( &thread3, NULL, sub_disconnect_signal_thread, (void*) message3);
    if(iret3)
    {
        fprintf(stderr,"Error - pthread_create() return code: %d\n",iret3);
        exit(EXIT_FAILURE);
    }

    printf("pthread_create() for thread 3 returns: %d\n",iret3);

    pthread_join( thread1, NULL);
    pthread_join( thread2, NULL);
    pthread_join( thread3, NULL);
    
    exit(EXIT_SUCCESS);
}

void *send_weak_clients_thread( void *ptr ) {
    int i = 0;
    push = zsock_new_push (">tcp://192.168.1.1:5560");
    char * send_msg = NULL;
    while(1){
        char* weak_client_list = client_weak_signal_list();
        printf("\nSEND_MESSAGE:Send weak clients to server: %s", weak_client_list);
        zstr_send (push, weak_client_list);
        free(weak_client_list);
        zclock_sleep(1000);
    }
    return 0;
}

void *sub_check_signal_thread( void* ptr) {
    zsock_t *socket = zsock_new_sub(">tcp://192.168.1.1:5556", "TOPIC");

    assert(socket);
    while(1) {
        char *topic;
        zmsg_t *msg;
        int rc = zsock_recv(socket, "sm", &topic, &msg);
        assert(rc == 0);
      
        zsys_info("Recv on %s", topic);
        if(!strcmp(topic, "TOPIC")) {
            char *check_client_MAC = zmsg_popstr(msg);
            char *check_client_SIG = zmsg_popstr(msg);
            send_check_result(check_client_MAC, check_client_SIG);
            zmsg_destroy(&msg);
        }       
    }
    zsock_destroy(&socket);
    return 0;
}

void *sub_disconnect_signal_thread( void *ptr) {
    zsock_t *socket = zsock_new_sub(">tcp://192.168.1.1:5557", "DISCONNECT");
    assert(socket);
    while(1) {
        char *topic;
        zmsg_t *msg;
        int rc = zsock_recv(socket, "sm", &topic, &msg);
        assert(rc == 0);
      
        zsys_info("Recv on %s", topic);
       
        if(!strcmp(topic, "DISCONNECT")) {
            char *disconnect_client_MAC = zmsg_popstr(msg);
            char *disconnect_sender_MAC = zmsg_popstr(msg);
            if (!strcmp(this_router_MAC_in_ath9k_format, disconnect_sender_MAC)) {
                //printf("\nSUB_MESSAGE_DISCONNECT:weak client: %s, from sender: %s\n", disconnect_client_MAC, disconnect_sender_MAC);
		//For test purpose, avoid your computer is disconnected by your router
                if (!(disconnect_client_MAC[2] == '5' && disconnect_client_MAC[3] == '4')) {
                    printf("\nSUB_MESSAGE_DISCONNECT:weak client: %s, from sender: %s\n", disconnect_client_MAC, disconnect_sender_MAC);
                    disconnect_client(disconnect_client_MAC);
                }
            }
            free(disconnect_client_MAC);
            free(disconnect_sender_MAC);
            free(topic);
            zmsg_destroy(&msg);
        }
        
    }
    zsock_destroy(&socket);
    return 0;
}

void send_check_result(char *check_client_MAC, char *check_client_SIG) {
 
    char client_list[2025];
    FILE *f1 = fopen("/proc/check_client_signal","w");
    fputs("checkpacket", f1);
    fclose(f1);
    FILE *f2 = fopen("/proc/check_client_signal", "r");
    size_t n = fread( client_list, 1, 2024, f2);
    fclose(f2);
    client_list[n] = '\0';
    int i, j;
	for (i = 2; i < strlen(client_list); i+=34) {
		for (j = 0; j < 34; j = j+5) {
			client_list[i+j] = toupper(client_list[i+j]);
			client_list[i+j+1] = toupper(client_list[i+j+1]);
		}
	}
	char *line = malloc(34*sizeof(char));
    char *token;
    char *client_MAC;
    char *client_SIG;
	char buf[1025];

	for (line = strtok (client_list, "#"); line != NULL && strlen(line) == 33; line = strtok (line + strlen (line) + 1, "#"))
	{
		strncpy (buf, line, sizeof (buf));
        client_MAC = strtok(buf, ";");
        client_SIG = strtok(client_MAC + strlen(client_MAC) +1, ";");
        if(check_client_MAC == NULL) {
            return;
        }
        if (!strcmp(client_MAC, check_client_MAC) && atoi(client_SIG) > atoi(check_client_SIG)) {
            printf("\n This router has a better signal for client: %s, old signal: %s, current signal: %s", check_client_MAC,check_client_SIG, client_SIG);
            response_check_signal(check_client_MAC); 
            free(client_SIG);
            free(client_MAC);
            return;
        }
	}
    free(client_SIG);
    free(client_MAC);
    return;
}


void response_check_signal(char* check_client_MAC) {
    printf("Start RESPONSE message %s", check_client_MAC);
    char* response_msg = NULL;
    concat(&response_msg, "{");
    concat(&response_msg, check_client_MAC);
    concat(&response_msg, ",");
    concat(&response_msg, this_router_MAC_in_ath9k_format);
    concat(&response_msg, "}");
    printf("\nRES: %s", response_msg);
    printf("Start response \n");
    printf("Send: %s", response_msg);
    char response_message[strlen(response_msg)];
    strncpy(response_message, response_msg, strlen(response_msg));
    free(response_msg);
    zstr_send (response, response_message);
    zclock_sleep(1000);    
    return;
}


void reconstruct_MAC_address(char* disconnect_client_MAC, char* reconstruct_MAC_address) {
    int i = 0;
    for (i = 0; i < 6; i++) {
        reconstruct_MAC_address[3*i] = disconnect_client_MAC[5*i+2];
        reconstruct_MAC_address[3*i+1] = disconnect_client_MAC[5*i+3];
        reconstruct_MAC_address[3*i+2] = disconnect_client_MAC[5*i+4];
    }
    reconstruct_MAC_address[18] = '\0';
}   

void disconnect_client(char *disconnect_client_MAC) {
    char MAC_address[18];
    reconstruct_MAC_address(disconnect_client_MAC, MAC_address);
    printf("AT line %d", __LINE__);
    char *disconnect_command = NULL;
    concat(&disconnect_command, "ubus call hostapd.wlan0 del_client \'{\"addr\":\"");
    concat(&disconnect_command, MAC_address);
    concat(&disconnect_command, "\", \"reason\":1, \"deauth\":true, \"ban_time\":");
    concat(&disconnect_command, CLIENT_BAN_TIME);
    concat(&disconnect_command, "}\'");
    printf("%s ", disconnect_command);
    system(disconnect_command);
    free(disconnect_command);
}

