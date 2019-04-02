#include "../include/client.h"

void WRND(Data *data)
{
        PINF("wrong data:\n\t%d\n\t%d\n\t%s\n\t%d\n\t%d\n\t%d\n",
                data->dtype,
                data->type,
                data->char_buff,
                data->uchar_1,
                data->uchar_2,
                data->uchar_3);
}

/*
        Добавить в очередь сообщений
        Использует мютекс клиента
*/
void client_push_Message(Client *clt, unsigned char dest, unsigned char act, unsigned int uint, char *buff)
{
        Message *msg;

        msg = malloc(sizeof(Message));
        if(msg == NULL){
                PERR("create Message\n");
                return;
        }

        msg->dest = dest;
        msg->act = act;
        msg->uint = uint;
        if(buff != NULL)
                strcpy(msg->char_buff, buff);
        msg->next = NULL;

        pthread_mutex_lock(&clt->mutex);
        if(clt->que_end == NULL){
                clt->que_end = msg;
                clt->que = msg;
        } else {
                clt->que_end->next = msg;
                clt->que_end = msg;
        }
        pthread_mutex_unlock(&clt->mutex);
}

/*
        Забрать из очереди сообщений
        Использует мютекс клиента
        Сообщения удаляются вручную
*/
Message *client_pop_Message(Client *clt, unsigned char dest)
{
        Message *msg;
        Message *prv;

        msg = NULL;
        prv = NULL;

        if( pthread_mutex_trylock(&clt->mutex) == EBUSY )
                return NULL;

        msg = clt->que;
        while(msg != NULL){
                if(msg->dest == dest)
                        break;
                prv = msg;
                msg = msg->next;
        }
        if(msg != NULL){
                if(prv != NULL){
                        prv->next = msg->next;
                        if(prv->next == NULL)
                                clt->que_end = prv;
                } else {
                        clt->que = msg->next;
                        if(msg->next == NULL)
                                clt->que_end = NULL;
                }
        }
        pthread_mutex_unlock(&clt->mutex);

        return msg;
}

Client *client_init(char *ip, char *port_s)
{
        Client *clt;
        int i;

        clt = malloc(sizeof(Client));

        if(clt == NULL){
                PERR("Client struct is NULL\n");

                return NULL;
        }

        sprintf(clt->name, "Nobody");
        clt->server_socket = -1;
        clt->servers = NULL;
        clt->servers_count = 0;
        clt->que = NULL;
        clt->que_end = NULL;
        for(i = 0; i < 10; i ++)
                clt->board[i] = BN;
        clt->board[9] = '\0';
        pthread_mutex_init(&clt->mutex, NULL);

        strcpy(clt->server_ip, ip);
        sscanf(port_s, "%d", &clt->server_port);

        pthread_create(&clt->server_pid, NULL, client_server, clt);
        pthread_create(&clt->game_pid, NULL, client_game, clt);

        return clt;
}

unsigned int rand_range(unsigned int min, unsigned int max){
    return rand() % (max - min + 1) + min;
}

void *UI_job(void *arg)
{
        Client *clt = (Client *)arg;
        unsigned int rnd;

        srand(time(NULL));

        rnd = rand_range(0, 1);
        PINF("UI: %u register...\n", rnd);
        client_push_Message(clt, TO_SRV, 4, rnd, NULL);

        sleep(5);
        while(1){
                rnd = rand_range(0, 10);
                switch(rnd){
                        case 0:
                                PINF("UI: exit...\n");
                                client_push_Message(clt, TO_SRV, 3, 0, NULL);
                                pthread_exit(0);
                                break;
                        default:
                                PINF("UI: move...\n");
                                rnd = rand_range(0, 9);
                                client_push_Message(clt, TO_SRV, 2, rnd, NULL);
                                break;
                }
                sleep(1);
        }

        client_push_Message(clt, TO_SRV, 3, 0, NULL);
        pthread_exit(0);
}

Client *client_init_UI(char *ip, char *port_s)
{
        Client *clt;
        int i;

        clt = malloc(sizeof(Client));

        if(clt == NULL){
                PERR("Client struct is NULL\n");

                return NULL;
        }

        sprintf(clt->name, "Nobody");
        clt->server_socket = -1;
        clt->servers = NULL;
        clt->servers_count = 0;
        clt->que = NULL;
        clt->que_end = NULL;
        for(i = 0; i < 10; i ++)
                clt->board[i] = BN;
        clt->board[9] = '\0';
        pthread_mutex_init(&clt->mutex, NULL);

        strcpy(clt->server_ip, ip);
        sscanf(port_s, "%d", &clt->server_port);

        pthread_create(&clt->server_pid, NULL, client_server, clt);
        pthread_create(&clt->game_pid, NULL, UI_job, clt);

        return clt;
}

static char fname[512];

void create_log_file()
{
        time_t now_time;
        struct tm * time_info;
        char *buf;
        int i;


        now_time = time(NULL);
        time_info = localtime(&now_time);
        buf = asctime(time_info);
        for(i = 0; buf[i] != '\0'; i++)
                if(buf[i] == ' ')
                        buf[i] = '_';
        buf[i-1] = '\0';
        sprintf(fname, "logs/%s_CLIENT.txt", buf);
        log = fopen(fname, "w");
        if(log == NULL){
                fprintf(stderr, "Error: create log file %s\n", fname);
                exit(1);
        }
}

int main(int args, char **arg)
{
        Client *clt;
        pid_t pid;

        create_log_file();
        switch(args - 1){
                case 2:
                        clt = client_init(arg[1], arg[2]);
                        break;
                case 3:
                        /*if(log != NULL){
                                pid = fork();
                                if(pid == 0){
                                        close(0);
                                        execlp("tail", "tail", "-f", fname, (char*)NULL);
                                        exit(1);
                                }
                        }*/
                        clt = client_init_UI(arg[1], arg[2]);
                        break;
                default:
                        PERR("Needs arguments: server IP, server PORT\n");
                        return -1;
        }
        if(clt != NULL)
                pthread_join(clt->server_pid, 0);

        PINF("client is down\n");
        fclose(log);

        return 0;
}
