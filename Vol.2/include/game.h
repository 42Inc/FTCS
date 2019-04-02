#ifndef _GAME_H_
#define _GAME_H_

#define NAME_LEN 64

#define BX 3
#define BO 2
#define BN 1
#define BD 4

unsigned char get_next_player(char *board);
unsigned char get_winner(char *board);

#endif
