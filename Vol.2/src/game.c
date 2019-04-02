#include "../include/game.h"

unsigned char get_next_player(char *board)
{
        unsigned int x, o, i;

        for(i = 0, o = 0, x = 0; i < 9; i++){
                switch(board[i]){
                        case BX:
                                x++;
                                break;
                        case BO:
                                o++;
                                break;
                }
        }

        if(o > x)
                return BX;
        if(o == x)
                return BO;

        return BN;
}

unsigned char get_winner(char *board)
{
        unsigned int i;

        if(board[0] != BN){
                /*
                        * * *
                        ? ? ?
                        ? ? ?
                */
                if(board[0] == board[1] && board[0] == board[2])
                        return board[0];
                /*
                        * ? ?
                        * ? ?
                        * ? ?
                */
                if(board[0] == board[3] && board[0] == board[6])
                        return board[0];
                /*
                        * ? ?
                        ? * ?
                        ? ? *
                */
                if(board[0] == board[4] && board[0] == board[8])
                        return board[0];
        }
        if(board[1] != BN){
                /*
                        ? * ?
                        ? * ?
                        ? * ?
                */
                if(board[1] == board[4] && board[1] == board[7])
                        return board[1];
        }
        if(board[2] != BN){
                /*
                        ? ? *
                        ? ? *
                        ? ? *
                */
                if(board[2] == board[5] && board[2] == board[8])
                        return board[2];
                /*
                        ? ? *
                        ? * ?
                        * ? ?
                */
                if(board[2] == board[4] && board[2] == board[6])
                        return board[2];
        }
        if(board[3] != BN){
                /*
                        ? ? ?
                        * * *
                        ? ? ?
                */
                if(board[3] == board[4] && board[3] == board[5])
                        return board[3];
        }
        if(board[6] != BN){
                /*
                        ? ? ?
                        ? ? ?
                        * * *
                */
                if(board[6] == board[7] && board[6] == board[8])
                        return board[6];
        }

        for(i = 0; i < 9; i++)
                if(board[i] == BN)
                        return BN;

        return BD;
}
