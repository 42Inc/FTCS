#include "../include/client.h"

void *client_game(void *arg)
{
        Client *clt = (Client *)arg;
        BoardW bw;
        ChatW cw;
        Symbol client_symbol, opponent_symbol;
        int epl, epl_count;
        struct epoll_event events[4], ev[4];
        Message *msg;
        unsigned char gstatus = GS_INIT;        //Состояние игры
        unsigned char mstatus = MS_NMOVE;       //Состояние хода
        int key;
        char char_buff[CHAR_BUFF_SIZE];
        unsigned int uint1;

        epl = epoll_create(4);
        initscr();
        keypad(stdscr, 1);
        noecho();
        refresh();
        PINF("client_game service is running\n");
        ev[0].events = EPOLLIN;//stdin
        ev[0].data.fd = 1;
        epoll_ctl(epl, EPOLL_CTL_ADD, 1, &ev[0]);

        init_BoardW(&bw);
        init_ChatW(&cw);

        add_str_ChatW(&cw, "Server: Input your Name");

        while(1){
                epl_count = epoll_wait(epl, events, 1, 500);
                while(epl_count > 0){
                        epl_count--;
                        if(events[epl_count].data.fd == 1){
                                key = getch();
                                switch(key){
                                        case KEY_F(10):
                                                goto pth_exit;
                                                break;
                                        case KEY_BACKSPACE:
                                                erase_ch_ChatW(&cw);
                                                break;
                                        case 10: //ENTER
                                                if(cw.inp_buff[0] == '\0'){
                                                        //if(gstatus == GS_PROCES && mstatus == MS_MMOVE){
                                                        if(gstatus == GS_PROCES){
                                                                /*
                                                                        Сделать ход
                                                                */
                                                                if(clt->board[bw.coord] == BN){
                                                                        //mstatus = MS_WMOVE;
                                                                        client_push_Message(clt, TO_SRV, 2, bw.coord, NULL);
                                                                }
                                                        }
                                                } else {
                                                        switch(gstatus){
                                                                case GS_INIT:
                                                                        /*
                                                                                Ввод имени
                                                                        */
                                                                        cw.inp_buff[NAME_LEN - 1] = '\0';
                                                                        strcpy(clt->name, cw.inp_buff);
                                                                        sprintf(char_buff, "Server: Welcome, %s!", cw.inp_buff);
                                                                        add_str_ChatW(&cw, char_buff);
                                                                        add_str_ChatW(&cw, "Server: Choose: \n\t1 - new game\n\t2 - any game");
                                                                        gstatus = GS_START;
                                                                        break;
                                                                case GS_START:
                                                                        /*
                                                                                Опции выбора игры
                                                                        */
                                                                        sscanf(cw.inp_buff, "%u", &uint1);
                                                                        switch(uint1){
                                                                                case 1:
                                                                                        add_str_ChatW(&cw, "Server: Waiting for an opponent...");
                                                                                        gstatus = GS_WAIT;
                                                                                        client_push_Message(clt, TO_SRV, 4, 1, NULL);
                                                                                        break;
                                                                                case 2:
                                                                                        add_str_ChatW(&cw, "Server: Waiting for an opponent...");
                                                                                        gstatus = GS_WAIT;
                                                                                        client_push_Message(clt, TO_SRV, 4, 0, NULL);
                                                                                        break;
                                                                        }
                                                                        break;
                                                                case GS_PROCES:
                                                                        /*
                                                                                Отправить сообщение чата
                                                                        */
                                                                        client_push_Message(clt, TO_SRV, 1, 0, cw.inp_buff);
                                                                        sprintf(char_buff, "%s: %s", clt->name, cw.inp_buff);
                                                                        add_str_ChatW(&cw, char_buff);
                                                                        break;
                                                                case GS_STOP:
                                                                        /*
                                                                                Отправить сообщение чата
                                                                        */
                                                                        client_push_Message(clt, TO_SRV, 1, 0, cw.inp_buff);
                                                                        sprintf(char_buff, "%s: %s", clt->name, cw.inp_buff);
                                                                        add_str_ChatW(&cw, char_buff);
                                                                        break;
                                                        }
                                                        clear_inp_CharW(&cw);
                                                }
                                                break;
                                        case KEY_UP:
                                                go_to(&bw, GTU);
                                                break;
                                        case KEY_LEFT:
                                                go_to(&bw, GTL);
                                                break;
                                        case KEY_RIGHT:
                                                go_to(&bw, GTR);
                                                break;
                                        case KEY_DOWN:
                                                go_to(&bw, GTD);
                                                break;
                                        default:
                                                if( (key >= 'a' && key <= 'z') ||
                                                    (key >= 'A' && key <= 'Z') ||
                                                    (key >= '0' && key <= '9') ||
                                                    (strchr(".?! ,", (char)key) != NULL)){
                                                        add_ch_ChatW(&cw, (char)key);
                                                }
                                }
                        }
                }
                msg = client_pop_Message(clt, TO_DYS);
                while(msg != NULL){
                        switch(msg->act){
                                case 1:
                                        add_str_ChatW(&cw, msg->char_buff);
                                        break;
                                case 2:
                                        sscanf(msg->char_buff, "%u", &uint1);
                                        if(uint1 == clt->symbol){
                                                print_symbol(&bw, msg->uint, client_symbol);
                                        } else {
                                                print_symbol(&bw, msg->uint, opponent_symbol);
                                        }
                                        /*switch(mstatus){
                                                case MS_WMOVE:
                                                        print_symbol(&bw, msg->uint, client_symbol);
                                                        mstatus = MS_OMOVE;
                                                        PINF("client_game: next move - opponent\n");
                                                        break;
                                                case MS_OMOVE:
                                                        print_symbol(&bw, msg->uint, opponent_symbol);
                                                        mstatus = MS_MMOVE;
                                                        PINF("client_game: next move - client\n");
                                                        break;
                                                default:
                                                        PERR("client_game: wrong mstatus(read msg): %u\n", mstatus);
                                                        goto pth_exit;
                                        }*/
                                        break;
                                case 3:
                                        sprintf(char_buff, "Server: %s connected", clt->oname);
                                        add_str_ChatW(&cw, char_buff);
                                        gstatus = GS_STOP;
                                        switch(clt->symbol){
                                                case BO:
                                                        client_symbol.up = 8274498;
                                                        client_symbol.down = 1111653888;
                                                        opponent_symbol.up = 12805656;
                                                        opponent_symbol.down = 409387776;
                                                        sprintf(char_buff, "O - %s", clt->name);
                                                        set_text(&bw, char_buff, NULL);
                                                        sprintf(char_buff, "X - %s", clt->oname);
                                                        set_text(&bw, NULL, char_buff);
                                                        break;
                                                case BX:
                                                        client_symbol.up = 12805656;
                                                        client_symbol.down = 409387776;
                                                        opponent_symbol.up = 8274498;
                                                        opponent_symbol.down = 1111653888;
                                                        sprintf(char_buff, "X - %s", clt->name);
                                                        set_text(&bw, char_buff, NULL);
                                                        sprintf(char_buff, "O - %s", clt->oname);
                                                        set_text(&bw, NULL, char_buff);
                                                        break;
                                                default:
                                                        PERR("client_game: wrong client symbol: %u\n", clt->symbol);
                                                        goto pth_exit;
                                        }
                                        PINF("client_game: the game has begun\n");
                                        break;
                                case 4:
                                        switch(msg->uint){
                                                case 0:
                                                        sprintf(char_buff, "Server: You Win!");
                                                        break;
                                                case 1:
                                                        sprintf(char_buff, "Server: %s Win!", clt->oname);
                                                        break;
                                                case 2:
                                                        sprintf(char_buff, "Server: Dead heat");
                                                        break;
                                        }
                                        mstatus = MS_NMOVE;
                                        gstatus = GS_STOP;
                                        add_str_ChatW(&cw, char_buff);
                                        break;
                                case 5:
                                        sprintf(char_buff, "Server: server unreachable");
                                        add_str_ChatW(&cw, char_buff);
                                        mstatus = MS_NMOVE;
                                        gstatus = GS_WAIT;
                                        break;
                                case 6:
                                        add_str_ChatW(&cw, msg->char_buff);
                                        mstatus = MS_NMOVE;
                                        gstatus = GS_WAIT;
                                        break;
                                case 7:
                                        gstatus = GS_PROCES;
                                        if(msg->uint != clt->symbol){
                                                mstatus = MS_OMOVE;
                                                PINF("client_game: next move - opponent\n");
                                        } else {
                                                mstatus = MS_MMOVE;
                                                PINF("client_game: next move - client\n");
                                        }
                                        break;
                        }
                        free(msg);
                        msg = client_pop_Message(clt, TO_DYS);
                }
                blink_BoardW(&bw);
        }

        endwin();
        PINF("client_game service: Unexpected exit\n");
        pthread_exit(0);

        pth_exit:
                endwin();
                PINF("client_game service is down\n");
                client_push_Message(clt, TO_SRV, 3, 0, NULL);
                pthread_exit(0);
}
