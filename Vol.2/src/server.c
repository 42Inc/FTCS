#include "../include/server.h"

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
        Использует мютекс сервера
*/
void server_push_Message(Server *srv, unsigned char act, unsigned int uint, SLNode *node)
{
        Message *msg;

        msg = malloc(sizeof(Message));
        if(msg == NULL){
                PERR("create Message\n");
                return;
        }

        msg->act = act;
        msg->uint = uint;
        msg->node = node;
        msg->next = NULL;

        pthread_mutex_lock(&srv->mutex);
        if(srv->que_end == NULL){
                srv->que_end = msg;
                srv->que = msg;
        } else {
                srv->que_end->next = msg;
                srv->que_end = msg;
        }
        pthread_mutex_unlock(&srv->mutex);
}

/*
        Забрать из очереди сообщений
        Использует мютекс сервера
        Сообщения удаляются вручную
*/
Message *server_pop_Message(Server *srv)
{
        Message *msg;

        msg = NULL;

        if( pthread_mutex_trylock(&srv->mutex) == EBUSY )
                return NULL;

        if(srv->que != NULL){
                msg = srv->que;
                srv->que = msg->next;
                if(msg->next == NULL)
                        srv->que_end = NULL;
        }
        pthread_mutex_unlock(&srv->mutex);

        return msg;
}

int create_socket(char *ip, unsigned int port)
{
        int sock;
        struct sockaddr_in addr;
        socklen_t addr_len;

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sock <= 0){
                PERR("open socket\n");

                return -1;
        }
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if(inet_aton(ip, &addr.sin_addr) == 0){
                PERR("set ip addres %s\n", ip);
                close(sock);
                return -1;
        }
        addr_len = sizeof(struct sockaddr_in);

        if(bind(sock, (struct sockaddr *) &addr, addr_len) != 0){
                PERR("bind to socked %s:%d\n", ip, port);
                close(sock);
                return -1;
        }
        PINF("new socket: %s:%d\n", ip, port);

        return sock;
}

Server *server_init(char *ip, char *port_s, char *main_ip, char *main_port_s)
{
        Server *srv;
        unsigned int port;
        unsigned int max_port;
        int i;

        srv = malloc(sizeof(Server));

        if(srv == NULL){
                PERR("Server struct is NULL\n");

                return NULL;
        }

        strcpy(srv->ip, ip);
        srv->servers = NULL;
        srv->servers_count = 0;
        srv->que = NULL;
        srv->que_end = NULL;
        srv->last_game_id = 0;
        srv->last_player_id = 0;
        pthread_mutex_init(&srv->mutex, NULL);
        for(i = 0; i < MAX_GAMES; i++)
                srv->games[i] = NULL;
        for(i = 0; i < MAX_GAMES * 2; i++)
                srv->players[i] = NULL;

        sscanf(port_s, "%d", &port);

        /*
                Игровой сокет
        */
        srv->clients_socket = create_socket(ip, port);
        if(!(srv->clients_socket > 0))
                goto error_exit;
        srv->games_port = port;
        /*
                Серверный сокет
        */
        for(max_port = port + MAX_PORTS, port += 1; port <= max_port; port++){
                srv->servers_socket = create_socket(ip, port);
                if(srv->servers_socket > 0)
                        break;
        }
        if(!(srv->servers_socket > 0))
                goto error_exit;
        srv->servers_port = port;

        if(main_ip != NULL && main_port_s != NULL){
                /*
                        Подключение к главному серверу
                */
                sscanf(main_port_s, "%d", &port);
                srv->main_server_socket = connect_to_main_server(main_ip, port);
                if(srv->main_server_socket <= 0){
                        goto error_exit;
                }
                strcpy(srv->main_server_info.ip, main_ip);
                srv->main_server_info.s_port = port;
                PINF("update main_server_info %s:%d\n", srv->main_server_info.ip, srv->main_server_info.s_port);
        } else {
                srv->main_server_socket = 0;
                server_add_SLNode(srv, ip, srv->servers_port, srv->games_port);
        }
        pthread_create(&srv->servers_pid, NULL, server_servers, srv);
        pthread_create(&srv->games_pid, NULL, server_games, srv);

        PINF("Server is running: %s:%d(s)/%d(g)\n", ip, srv->servers_port, srv->games_port);

        return srv;

        error_exit:
                free(srv);
                return NULL;
}

void create_log_file()
{
        time_t now_time;
        struct tm * time_info;
        char *buf;
        int i;
        char fname[512];
        pid_t pid;

        now_time = time(NULL);
        time_info = localtime(&now_time);
        buf = asctime(time_info);
        for(i = 0; buf[i] != '\0'; i++)
                if(buf[i] == ' ')
                        buf[i] = '_';
        buf[i-1] = '\0';
        sprintf(fname, "logs/%s_SERVER.txt", buf);
        log = fopen(fname, "w");
        if(log == NULL){
                fprintf(stderr, "Error: create log file %s\n", fname);
                exit(1);
        } else {
                pid = fork();
                if(pid == 0){
                        close(0);
                        execlp("tail", "tail", "-f", fname, (char*)NULL);
                        exit(1);
                }
        }
}

int main(int args, char **arg)
{
        Server *srv;

        create_log_file();
        switch(args - 1){
                case 2:
                        srv = server_init(arg[1], arg[2], NULL, NULL);
                        break;
                case 4:
                        srv = server_init(arg[1], arg[2], arg[3], arg[4]);
                        break;
                default:
                        PERR("Needs arguments:\n\t ip, port\n\tor\n\t ip, port, main server ip, main server port\n");
                        return -1;
        }
        if(srv != NULL)
                pthread_join(srv->servers_pid, 0);

        PINF("server is down\n");
        fclose(log);

        return 0;
}
