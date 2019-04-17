/*
  Created by JIexa24 (Alexey R.)
*/
#include "./../include/main.h"

int mt_clrscr() {
  fprintf(stdout, "\E[H\E[2J");
  return 0;
}
/*---------------------------------------------------------------------------*/
int mt_gotoXY(int x, int y) {
  int rows = 0;
  int cols = 0;
  mt_getscreensize(&rows, &cols);
  if (((y < rows) && (y >= 0)) && ((x < cols) && (x >= 0))) {
    fprintf(stdout, "\E[%d;%dH", y, x);
    return 0;
  } else {
    return -1;
  }
}
/*---------------------------------------------------------------------------*/
int mt_getscreensize(int *rows, int *cols) {
  struct winsize w;
  if (!ioctl(STDOUT_FILENO, TIOCGWINSZ, &w)) {
    *rows = w.ws_row;
    *cols = w.ws_col;
    return 0;
  } else {
    return -1;
  }
}
/*---------------------------------------------------------------------------*/
int mt_setfgcolor(enum colors color) {
  if (color >= 0 && color <= 9) {
    fprintf(stdout, "\E[%dm", 30 + color);
    return 0;
  } else {
    return -1;
  }
}
/*---------------------------------------------------------------------------*/
int mt_setbgcolor(enum colors color) {
  if (color >= 0 && color <= 9) {
    fprintf(stdout, "\E[%dm", 40 + color);
    return 0;
  } else {
    return -1;
  }
}
/*---------------------------------------------------------------------------*/
