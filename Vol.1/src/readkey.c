/*
  Created by JIexa24 (Alexey R.)
*/
#include "./../include/main.h"
/*

  tcflag_t c_iflag;      // режимы ввода
  tcflag_t c_oflag;      // режимы вывода
  tcflag_t c_cflag;      // режимы управления
  tcflag_t c_lflag;      // режимы локали
  cc_t c_cc[NCCS];       // управляющие символы

  c_iflag - флаги констант:
*/
int rk_readkey(enum keys *key, int echo) {
  struct termios orig_options;
  char buf[16] = {0};
  int readNum = 0;

  int poll_return;
  struct pollfd pfd;
  poll_return = 0;
  pfd.events = POLLIN | POLLHUP | POLLRDNORM;
  pfd.fd = STDIN_FILENO;
  if (tcgetattr(STDIN_FILENO, &orig_options) != 0) {
    return -1;
  }
  if (rk_mytermregime(0, 0, 1, echo, 1) != 0) {
    return -1;
  }
  if ((poll_return = poll(&pfd, 1, 100)) > 0) {
    readNum = read(STDIN_FILENO, buf, 5);
  }
  if (readNum <= 0) {
    return -1;
  }

  buf[readNum] = '\0';

  if ((strcmp(buf, "\n")) == 0) {
    *key = KEY_enter;
  } else if ((strcmp(buf, "\E[A")) == 0) {
    *key = KEY_up;
  } else if ((strcmp(buf, "\E[B")) == 0) {
    *key = KEY_down;
  } else if ((strcmp(buf, "\E[C")) == 0) {
    *key = KEY_right;
  } else if ((strcmp(buf, "\E[D")) == 0) {
    *key = KEY_left;
  } else if ((strcmp(buf, "\E")) == 0) {
    *key = KEY_esc;
  } else if ('a' <= buf[0] && buf[0] <= 'z') {
    *key = KEY_alpha;
  } else {
    *key = KEY_other;
  }

  if (tcsetattr(STDIN_FILENO, TCSANOW, &orig_options) != 0) {
    return -1;
  }

  return (*key == KEY_alpha) ? buf[0] : 0;
}
/*---------------------------------------------------------------------------*/
int rk_mytermsave(void) {
  struct termios options;
  FILE *save = NULL;

  if (tcgetattr(STDIN_FILENO, &options) != 0) {
    return -1;
  }
  if ((save = fopen("termsettings", "wb")) == NULL) {
    return -1;
  }
  fwrite(&options, sizeof(options), 1, save);
  fclose(save);

  return 0;
}
/*---------------------------------------------------------------------------*/
int rk_mytermrestore(void) {
  struct termios options;
  FILE *data = NULL;
  ;

  if ((data = fopen("termsettings", "rb")) == NULL) {
    return -1;
  } else {
    if (fread(&options, sizeof(options), 1, data) > 0) {
      if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &options) != 0) {
        fclose(data);
        return -1;
      } else {
        fclose(data);
        return -1;
      }
    }
    fclose(data);
  }

  return 0;
}
/*---------------------------------------------------------------------------*/
int rk_mytermregime(int regime, int vtime, int vmin, int echo, int sigint) {
  struct termios options;

  if (tcgetattr(STDIN_FILENO, &options) != 0) {
    return -1;
  }

  if (regime == 1) {
    options.c_lflag |= ICANON;
  } else if (regime == 0) {
    options.c_lflag &= ~ICANON;
  } else {
    return -1;
  }

  if (regime == 0) {
    options.c_cc[VTIME] = vtime;
    options.c_cc[VMIN] = vmin;

    if (echo == 1) {
      options.c_lflag |= ECHO;
    } else if (echo == 0) {
      options.c_lflag &= ~ECHO;
    } else {
      return -1;
    }

    if (sigint == 1) {
      options.c_lflag |= ISIG;
    } else if (sigint == 0) {
      options.c_lflag &= ~ISIG;
    } else {
      return -1;
    }
  }

  if (tcsetattr(STDIN_FILENO, TCSANOW, &options) != 0) {
    return -1;
  }

  return 0;
}
