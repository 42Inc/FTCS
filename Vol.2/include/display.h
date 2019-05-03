#include <ncurses.h>
#include <string.h>
#include <termios.h>

#include "inet.h"

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

/*
        X
        up:     00000000110000110110011000011000        12805656
        down:   00011000011001101100001100000000        409387776
        O
        up:     00000000011111100100001001000010        8274498
        down:   01000010010000100111111000000000        1111653888
*/

#define DSM '#'

#define GTL 1 // to left
#define GTU 2 // to up
#define GTR 3 // to right
#define GTD 4 // to down
#define GTM 5 // to middle

#define CHAT_OUT_BUFF_SIZE 256

#define Symbol struct game_simbol

Symbol {
  unsigned int up;
  unsigned int down;
};

#define BoardW struct game_board_window

BoardW {
  WINDOW *bord;
  WINDOW *cell[9];
  WINDOW *butns[9];
  WINDOW *label_up;
  WINDOW *label_down;
  unsigned int coord;
  unsigned char blink_f;
};

#define ChatW struct game_chat_window

ChatW {
  WINDOW *bord;
  WINDOW *out;
  WINDOW *inp;
  WINDOW *line;
  char out_buff[CHAT_OUT_BUFF_SIZE];
  char inp_buff[CHAR_BUFF_SIZE];
  unsigned int inp_pos;
};

void init_BoardW(BoardW *bw);
void print_symbol(BoardW *bw, int coord, Symbol sm);
void go_to(BoardW *bw, unsigned char dir);
void set_text(BoardW *bw, char *buff1, char *buff2);
void blink_BoardW(BoardW *bw);

void init_ChatW(ChatW *cw);
void add_str_ChatW(ChatW *cw, char *buff);
void add_ch_ChatW(ChatW *cw, char sm);
void erase_ch_ChatW(ChatW *cw);
void clear_inp_CharW(ChatW *cw);
#endif
