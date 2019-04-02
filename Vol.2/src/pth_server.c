#include "../include/client.h"

/*
        Поиск в списке серверов
*/
SLNode *client_find2_SLNode(Client *clt, char *ip, unsigned int port)
{
        SLNode *node;

        if(clt->servers == NULL){
                PERR("servers list is empty\n");
                return NULL;
        }
        node = clt->servers;
        while(node != NULL){
                if(strcmp(node->ip, ip) == 0 && node->port == port)
                        break;
                node = node->right;
        }
        if(node == NULL){
                PERR("not found2 SLNode: %s:%d\n", ip, port);
                return NULL;
        }

        return node;
}

SLNode *client_find1_SLNode(Client *clt, int id)
{
        int i;
        SLNode *node;

        if(clt->servers == NULL){
                PERR("servers list is empty\n");
                return NULL;
        }
        node = clt->servers;
        for(i = 0; i != id && i <= clt->servers_count; i++){
                node = node->right;
        }
        if(node == NULL){
                PERR("not found1 SLNode %d\n", id);
                return NULL;
        }

        return node;
}

/*
        Добавить в список серверов
*/
void client_add_SLNode(Client *clt, char *ip, unsigned int port)
{
        SLNode *node;

        if(clt->servers == NULL){
                clt->servers = malloc(sizeof(SLNode));
                if(clt->servers == NULL){
                        PERR("create first SLNode\n");
                        exit(1);
                }
                strcpy(clt->servers->ip, ip);
                clt->servers->port = port;
                clt->servers->left = NULL;
                clt->servers->right = NULL;
        } else {
                node = clt->servers;
                while(node->right != NULL){
                        node = node->right;
                }
                node->right = malloc(sizeof(SLNode));
                if(node->right == NULL){
                        PERR("create next SLNode\n");
                        exit(1);
                }
                strcpy(node->right->ip, ip);
                node->right->port = port;
                node->right->left = node;
                node->right->right = NULL;
        }
        clt->servers_count += 1;
        PINF("included new server to list. List size: %d. Server: %s:%u\n", clt->servers_count, ip, port);
}

/*
        Убрать из списка серверов
*/
void client_erase1_SLNode(Client *clt, int id)
{
        SLNode *node;

        node = client_find1_SLNode(clt, id);
        if(node == NULL)
                return;
        if(node->left != NULL)
                node->left->right = node->right;
        else
                clt->servers = node->right;
        if(node->right != NULL)
                node->right->left = node->left;

        clt->servers_count -= 1;
        PINF("erased server from list. List size: %d. Server: %s:%u\n", clt->servers_count, node->ip, node->port);
        free(node);
}
void client_erase2_SLNode(Client *clt, char *ip, unsigned int port)
{
        SLNode *node;

        node = client_find2_SLNode(clt, ip, port);
        if(node == NULL)
                return;
        if(node->left != NULL)
                node->left->right = node->right;
        else
                clt->servers = node->right;
        if(node->right != NULL)
                node->right->left = node->left;

        clt->servers_count -= 1;
        PINF("erased server from list. List size: %d. Server: %s:%u\n", clt->servers_count, node->ip, node->port);
        free(node);
}

void client_clear_SLNode(Client *clt)
{
        SLNode *node;

        node = clt->servers;
        while(clt->servers != NULL){
                node = clt->servers;
                clt->servers = node->right;
                free(node);
                clt->servers_count -= 1;
        }

        PINF("clear servers list. List size: %d.\n", clt->servers_count);
}

int connect_to_main_server(char *ip, unsigned int port)
{
        int sock;
        struct sockaddr_in addr;
        socklen_t addr_len;

        PINF("connect to server %s:%u\n", ip, port);

        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(sock <= 0){
                PERR("open socket\n");
                return -1;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if(inet_aton(ip, &addr.sin_addr) == 0){
                PERR("set ip addres %s\n", ip);
                PERR("connect to main server %s:%u\n", ip, port);
                close(sock);
                return -1;
        } else {
                addr_len = sizeof(struct sockaddr_in);
                if(connect(sock, (struct sockaddr *) &addr, addr_len) == 0){
                        PINF("connected to main server %s:%u\n", ip, port);
                } else {
                        PERR("connect to main server %s:%u\n", ip, port);
                        close(sock);
                        return -1;
                }
        }

        return sock;
}

void *client_server(void *arg)
{
        Client *clt = (Client *)arg;
        int epl, epl_count;
        struct epoll_event events[1], ev;
        Message *msg;
        Data data;
        char char_buff[CHAR_BUFF_SIZE];
        unsigned int uint_1, uint_2, uint_3;
        unsigned char fl_err = 0;
        unsigned int main_tick = 0;
        SLNode *slnode;

        epl = epoll_create(1);
        ev.events = EPOLLIN;
        PINF("client_server service is running\n");

        while(1){
                epl_count = epoll_wait(epl, events, 1, 500);
                if(clt->server_socket > 0)
                        main_tick += 1;
                while(epl_count > 0){
                        epl_count -= 1;
                        if(events[epl_count].data.fd == clt->server_socket){
                                /*
                                        Сообщение от основного сервера
                                */
                                if( recv(clt->server_socket, &data, Data_size, 0) <= 0 ){
                                        PERR("receive data from server_socket\n");
                                        fl_err = fl_err | EBMA;
                                } else
                                switch(data.dtype){
                                        case 1:
                                                switch(data.type){
                                                        case 1:
                                                                main_tick = 0;
                                                                clt->game_id = data.uchar_2;
                                                                clt->id = data.uchar_1;
                                                                clt->symbol = data.uchar_3;
                                                                break;
                                                        case 2:
                                                                main_tick = 0;
                                                                switch(data.uchar_1){
                                                                        default:
                                                                                client_push_Message(clt, TO_DYS, 6, 0, "Server: unexpected error");
                                                                }
                                                                break;
                                                        case 3:
                                                                main_tick = 0;
                                                                data.dtype = 1;
                                                                data.type = 4;
                                                                if( send(clt->server_socket, &data, Data_size, 0) <= 0 ){
                                                                        PERR("send data to server_socket\n");
                                                                        fl_err = fl_err | EBMA;
                                                                }
                                                                break;
                                                        case 4:
                                                                main_tick = 0;
                                                                sscanf(data.char_buff, "%s %u", char_buff, &uint_1);
                                                                client_add_SLNode(clt, char_buff, uint_1);
                                                                break;
                                                        case 5:
                                                                main_tick = 0;
                                                                sscanf(data.char_buff, "%s %u", char_buff, &uint_1);
                                                                client_erase2_SLNode(clt, char_buff, uint_1);
                                                                break;
                                                        default:
                                                                WRND(&data);
                                                }
                                                break;
                                        case 2:
                                                switch(data.type){
                                                        case 1:
                                                                main_tick = 0;
                                                                if(data.uchar_1 == BD){
                                                                        client_push_Message(clt, TO_DYS, 4, 2, NULL);
                                                                        break;
                                                                }
                                                                if(data.uchar_1 == clt->symbol)
                                                                        client_push_Message(clt, TO_DYS, 4, 0, NULL);
                                                                else
                                                                        client_push_Message(clt, TO_DYS, 4, 1, NULL);
                                                                break;
                                                        case 2:
                                                                main_tick = 0;
                                                                PINF("print move to %u: %u\n", data.uchar_1, data.uchar_2);
                                                                sprintf(char_buff, "%u ", data.uchar_2);
                                                                client_push_Message(clt, TO_DYS, 2, data.uchar_1, char_buff);
                                                                break;
                                                        case 3:
                                                                main_tick = 0;
                                                                client_push_Message(clt, TO_DYS, 7, data.uchar_1, NULL);
                                                                break;
                                                        case 4:
                                                                main_tick = 0;
                                                                sprintf(clt->oname, "%s", data.char_buff);
                                                                client_push_Message(clt, TO_DYS, 3, 0, NULL);
                                                                break;
                                                        default:
                                                                WRND(&data);
                                                }
                                                break;
                                        case 3:
                                                switch(data.type){
                                                        case 1:
                                                                main_tick = 0;
                                                                sprintf(char_buff, "%s: %s", clt->oname, data.char_buff);
                                                                client_push_Message(clt, TO_DYS, 1, 0, char_buff);
                                                                break;
                                                        case 2:
                                                                main_tick = 0;
                                                                sprintf(char_buff, "Server: %s", data.char_buff);
                                                                client_push_Message(clt, TO_DYS, 1, 0, char_buff);
                                                                break;
                                                        case 3:
                                                                main_tick = 0;
                                                                PINF("message not accepted: %s\n", data.char_buff);
                                                                client_push_Message(clt, TO_SRV, 1, 0, data.char_buff);
                                                                break;
                                                        default:
                                                                WRND(&data);
                                                }
                                                break;
                                        default:
                                                WRND(&data);
                                }
                        }
                }//while(epl_count > 0)
                if(main_tick == MAIN_TICK_MAX){
                        fl_err = fl_err | EBMA;
                        main_tick = 0;
                }
                if((fl_err & ENLI) == ENLI && (fl_err & EBMA) != EBMA){
                        /*
                                Если список серверов некорректный
                        */
                        fl_err = fl_err & (~ENLI);
                        if(clt->server_socket > 0){
                                PINF("bad servers list\n");
                                client_clear_SLNode(clt);
                                data.dtype = 1;
                                data.type = 3;
                                if( send(clt->server_socket, &data, Data_size, 0) <= 0 ){
                                        PERR("send data to server_socket\n");
                                        fl_err = fl_err | EBMA;
                                }
                        }
                }
                if((fl_err & EBMA) == EBMA){
                        /*
                                Если отвалился главный сервер
                        */
                        fl_err = fl_err & (~EBMA);
                        if(clt->server_socket > 0){
                                sleep(2);
                                PINF("bad server_socket\n");
                                epoll_ctl(epl, EPOLL_CTL_DEL, clt->server_socket, &ev);
                                close(clt->server_socket);
                                clt->server_socket = -1;
                                client_erase1_SLNode(clt, 0);
                                slnode = clt->servers;
                                while(slnode != NULL){
                                        clt->server_socket = connect_to_main_server(slnode->ip, slnode->port);
                                        if(clt->server_socket > 0){
                                                data.dtype = 1;
                                                data.type = 2;
                                                data.uchar_1 = clt->id;
                                                data.uchar_2 = clt->game_id;
                                                if( send(clt->server_socket, &data, Data_size, 0) <= 0 ){
                                                        PERR("send data to server_socket\n");
                                                } else {
                                                        ev.data.fd = clt->server_socket;
                                                        epoll_ctl(epl, EPOLL_CTL_ADD, clt->server_socket, &ev);
                                                        fl_err = fl_err | ENLI;
                                                        break;
                                                }
                                        }
                                        PINF("server unreachable: %s:%u\n", slnode->ip, slnode->port);
                                        slnode = slnode->left;
                                }
                                if(slnode == NULL){
                                        PERR("main server unreachable\n");
                                        client_push_Message(clt, TO_DYS, 5, 0, NULL);
                                }
                        }
                }
                msg = client_pop_Message(clt, TO_SRV);
                while(msg != NULL){
                        switch(msg->act){
                                case 1:
                                        if(clt->server_socket > 0){
                                                data.dtype = 3;
                                                data.type = 1;
                                                sprintf(data.char_buff, "%s", msg->char_buff);
                                                if( send(clt->server_socket, &data, Data_size, 0) <= 0 ){
                                                        PERR("send data to server_socket\n");
                                                        fl_err = fl_err | EBMA;
                                                        client_push_Message(clt, TO_SRV, 1, 0, msg->char_buff);
                                                }
                                        } else {
                                                client_push_Message(clt, TO_SRV, 1, 0, msg->char_buff);
                                        }
                                        break;
                                case 2:
                                        if(clt->server_socket > 0){
                                                data.dtype = 2;
                                                data.type = 1;
                                                data.uchar_1 = msg->uint;
                                                if( send(clt->server_socket, &data, Data_size, 0) <= 0 ){
                                                        PERR("send data to server_socket\n");
                                                        fl_err = fl_err | EBMA;
                                                        client_push_Message(clt, TO_SRV, 2, msg->uint, NULL);
                                                } else
                                                        PINF("send \"move to %u\": %u\n", msg->uint, clt->symbol);
                                        } else {
                                                client_push_Message(clt, TO_SRV, 2, msg->uint, NULL);
                                        }
                                        break;
                                case 3:
                                        goto pth_exit;
                                        break;
                                case 4:
                                        clt->server_socket = connect_to_main_server(clt->server_ip, clt->server_port);
                                        if(clt->server_socket > 0){
                                                ev.data.fd = clt->server_socket;
                                                epoll_ctl(epl, EPOLL_CTL_ADD, clt->server_socket, &ev);
                                                data.dtype = 1;
                                                data.type = 1;
                                                sprintf(data.char_buff, "%s", clt->name);
                                                data.uchar_1 = msg->uint;
                                                if( send(clt->server_socket, &data, Data_size, 0) <= 0 ){
                                                        PERR("send data to server_socket\n");
                                                        client_push_Message(clt, TO_DYS, 5, 0, NULL);
                                                } else
                                                        fl_err = fl_err | ENLI;
                                        } else {
                                                client_push_Message(clt, TO_DYS, 5, 0, NULL);
                                        }
                                        break;
                                default:
                                        PERR("error msg data\n");
                        }
                        free(msg);
                        msg = client_pop_Message(clt, TO_SRV);
                }
        }

        pthread_exit(0);

        pth_exit:
                PINF("client_server service is down\n");
                pthread_exit(0);
}
