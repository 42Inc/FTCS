#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "inet.h"
#include "game.h"
#include "display.h"

#ifndef _CLIENT_H_
#define _CLIENT_H_

#define PERR(...) fprintf(log, "Error: "__VA_ARGS__); fflush(log)
#define PINF(...) fprintf(log, "Notice: "__VA_ARGS__); fflush(log)

FILE *log;

#define SLNode struct inet_client_ServersListNode

SLNode {
        char ip[16];
        unsigned int port;
        SLNode *left;
        SLNode *right;
};

#define LM_EN 1
#define LM_MY 2

#define GS_INIT 0
#define GS_WAIT 1
#define GS_PROCES 2
#define GS_STOP 3
#define GS_START 4

#define MS_NMOVE 0
#define MS_MMOVE 1
#define MS_WMOVE 2
#define MS_OMOVE 3

#define MAIN_TICK_MAX 128
#define EBMA 1                  //ошибка связанная с сокетом главного сервера
#define EBEX 2                  //ошибка связанная с сокетом дополнительного сервера
#define ENLI 4                  //ошибка связанная со списком серверов (локальным)
#define EGLI 8                  //в списке серверов образовалась "дыра" (в глобальном смысле)
#define EPLI 16                 //ошибка связанная со списком игроков/игр


#define Message struct inet_client_pthread_message
#define TO_DYS 1
#define TO_SRV 2
Message {
        /*
                dest - адресат сообщения
                TO_DYS - сообщение должен оброботать поток client_game
                TO_SRV - сообщение должен оброботать поток client_server

                TO_DYS
                act     действие                        uint            char_buff
                1       напечатать сообщение в чат                      сообщение
                2       сделать ход(отрисовать)         координата      символ
                3       начать игру
                4       закончить игру                  причина
                                                        0 - победа
                                                        1 - поражение
                                                        2 - ничья
                5       сервер недоступен
                6       ошибка от сервера                               сообщение
                7       чей сейчас ход                  символ

                TO_SRV
                act     действие                        uint            char_buff
                1       отправить сообщение чата                        сообщение
                2       отправить ход                   координата
                3       завершение потока client_game
                4       зарегистрироваться              тип подключения
                                                        0 - подключиться к свободной
                                                        1 - создать игру

        */
        unsigned char dest;
        unsigned char act;
        unsigned int uint;
        char char_buff[CHAR_BUFF_SIZE];

        Message *next;
};

#define Client struct inet_client

Client {
        char name[NAME_LEN];
        char oname[NAME_LEN];
        unsigned char symbol;
        unsigned int game_id;
        unsigned int id;
        char board[10];
        pthread_t game_pid;

        char server_ip[16];
        unsigned int server_port;
        int server_socket;
        SLNode *servers;
        unsigned int servers_count;
        pthread_t server_pid;

        Message *que;                   //очередь сообщений для синхронизации потоков
        Message *que_end;
        pthread_mutex_t mutex;
};

/*
        Файл client.c
*/
void WRND(Data *data);
void client_push_Message(Client *clt, unsigned char dest, unsigned char act, unsigned int uint, char *buff);
Message *client_pop_Message(Client *clt, unsigned char dest);

/*
        Файл pth_game.c
*/
void *client_game(void *arg);

/*
        Файл pth_server.c
*/
void *client_server(void *arg);
#endif
