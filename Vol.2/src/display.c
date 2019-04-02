#include "../include/display.h"

void init_BoardW(BoardW *bw)
{
        int i, j;
        int y = 2, x = 3;

        bw->coord = 4;
        bw->blink_f = 0;
        //h, w, y, x
        bw->bord = newwin(34, 38, 1, 1);
        box(bw->bord, '*', '*');

        bw->label_up = derwin(bw->bord, 1, 31, 1, 5);

        bw->label_down = derwin(bw->bord, 1, 31, y + 30, 5);

        for(i = 0, j = 0; i < 3; i++, j += 11){
                bw->butns[i] = derwin(bw->bord, 10, 10, y, x + j);
        }
        for(i = 3, j = 0; i < 6; i++, j += 11){
                bw->butns[i] = derwin(bw->bord, 10, 10, y + 10, x + j);
        }
        for(i = 6, j = 0; i < 9; i++, j += 11){
                bw->butns[i] = derwin(bw->bord, 10, 10, y + 20, x + j);
        }

        for(i = 0; i < 9; i++){
                bw->cell[i] = derwin(bw->butns[i], 8, 8, 1, 1);
                box(bw->butns[i], ' ', ' ');
                wrefresh(bw->butns[i]);
        }

        wrefresh(bw->bord);
}

void blink_BoardW(BoardW *bw)
{
        switch(bw->blink_f){
                case 0:
                        bw->blink_f = 1;
                        box(bw->butns[bw->coord], '*', '*');
                        wrefresh(bw->butns[bw->coord]);
                        break;
                case 1:
                        bw->blink_f = 0;
                        box(bw->butns[bw->coord], ' ', ' ');
                        wrefresh(bw->butns[bw->coord]);
                        break;
        }
}

void print_symbol(BoardW *bw, int coord, Symbol sm)
{
        int i;
        char buf[65];

        if(0 > coord || coord > 8)
                return;

        for(i = 31; i >= 0; i--){
                if( (sm.up & 1) == 1)
                        buf[i] = DSM;
                else
                        buf[i] = ' ';
                sm.up = sm.up >> 1;
        }
        for(i = 63; i >= 32; i--){
                if( (sm.down & 1) == 1)
                        buf[i] = DSM;
                else
                        buf[i] = ' ';
                sm.down = sm.down >> 1;
        }
        buf[64] = '\0';

        wmove(bw->cell[coord], 0, 0);
        waddstr(bw->cell[coord], buf);
        wrefresh(bw->cell[coord]);
}

void go_to(BoardW *bw, unsigned char dir)
{

        box(bw->butns[bw->coord], ' ', ' ');
        wrefresh(bw->butns[bw->coord]);
        switch(dir){
                case GTR:
                        if(bw->coord + 1 < 9)
                                bw->coord += 1;
                        else
                                bw->coord = 0;
                        break;
                case GTU:
                        if(bw->coord > 2)
                                bw->coord -= 3;
                        break;
                case GTL:
                        if(bw->coord != 0)
                                bw->coord -= 1;
                        else
                                bw->coord = 8;
                        break;
                case GTD:
                        if(bw->coord < 6)
                                bw->coord += 3;
                        break;
                case GTM:
                        bw->coord = 4;
                        break;
        }
        //box(bw->butns[bw->coord], '*', '*');
        //wrefresh(bw->butns[bw->coord]);
}

void set_text(BoardW *bw, char *buff1, char *buff2)
{
        if(buff1 != NULL){
                wmove(bw->label_up, 0, 0);
                waddstr(bw->label_up, buff1);
                wrefresh(bw->label_up);
        }
        if(buff2 != NULL){
                wmove(bw->label_down, 0, 0);
                waddstr(bw->label_down, buff2);
                wrefresh(bw->label_down);
        }
}

void init_ChatW(ChatW *cw)
{
        //h, w, y, x
        cw->bord = newwin(20, 38, 1, 39);
        box(cw->bord, '*', '*');

        cw->out_buff[0] = '\0';
        cw->inp_buff[0] = '\0';
        cw->inp_pos = 0;
        cw->out = derwin(cw->bord, 14, 35, 1, 2);
        cw->line = derwin(cw->bord, 1, 23, 15, 7);
        box(cw->line, ' ', '*');
        wrefresh(cw->line);
        cw->inp = derwin(cw->bord, 3, 35, 16, 2);

        wrefresh(cw->bord);
}

void add_str_ChatW(ChatW *cw, char *buff)
{
        size_t size;
        char tmp[CHAT_OUT_BUFF_SIZE];

        size = strlen(buff);
        if(size >= CHAT_OUT_BUFF_SIZE - 1)
                buff[CHAT_OUT_BUFF_SIZE - 2] = '\0';
        strcpy(tmp, cw->out_buff);
        strcpy(cw->out_buff, buff);
        cw->out_buff[size] = '\n';
        cw->out_buff[size + 1] = '\0';
        tmp[(CHAT_OUT_BUFF_SIZE - 2) - (size + 1)] = '\0';
        if(tmp[0] != '\0')
                strcpy(cw->out_buff + size + 1, tmp);

        wmove(cw->out, 0, 0);
        wprintw(cw->out, cw->out_buff);
        wrefresh(cw->out);
}

void add_ch_ChatW(ChatW *cw, char sm)
{
        if(cw->inp_pos >= CHAR_BUFF_SIZE - 1)
                return;

        cw->inp_buff[cw->inp_pos] = sm;
        cw->inp_pos += 1;
        cw->inp_buff[cw->inp_pos] = '\0';
        wclear(cw->inp);
        wmove(cw->inp, 0, 0);
        wprintw(cw->inp, cw->inp_buff);
        wrefresh(cw->inp);
}

void erase_ch_ChatW(ChatW *cw)
{
        if(cw->inp_pos == 0)
                return;

        cw->inp_pos -= 1;
        cw->inp_buff[cw->inp_pos] = '\0';
        wclear(cw->inp);
        wmove(cw->inp, 0, 0);
        wprintw(cw->inp, cw->inp_buff);
        wrefresh(cw->inp);
}

void clear_inp_CharW(ChatW *cw)
{
        cw->inp_pos = 0;
        cw->inp_buff[0] = '\0';
        wclear(cw->inp);
        wmove(cw->inp, 0, 0);
        wrefresh(cw->inp);
}
