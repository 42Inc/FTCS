#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define CHECK_PORT "3489"
#define NAME_LEN 21
#define MSG_LEN 64
#define SERVER_COUNT 3

struct my_rows {
  char name[20];
};

struct server_addr {
  char ip[16];
  char port[6];
  char chk_port[6];
};

typedef struct {
  GtkWidget *entry, *textview;
} Widgets;

struct server_addr servers[3] = {{"127.0.0.1", "65500", "65501"},
                                 {"127.0.0.1", "65502", "65503"},
                                 {"127.0.0.1", "65504", "65505"}};
struct my_rows names[10];
char side;
int id, sockfd, chk_sockfd, numbytes, course,
        cur_serv = -1, flg = 0, host = 0, end_game = 0, exit_flag = 0,
        connect_flag = 0, server_status = 0;
char cmd[3], message_send[MSG_LEN], message_recv[MSG_LEN];
const gchar matrx[] = "000102101112202122";

GtkWidget *contain1, *contain11;
GtkWidget *game_field1[3][3];
GtkWidget *game_field2[3][3];
GtkWidget *game_field_pix[3][3];
GtkWidget *field_col[3];
GtkWidget *window;
GtkWidget *entry;
GtkWidget *contain;
GtkWidget *contain2;
GtkWidget *button;
Widgets *chat;
fd_set master, read_fds;

static void insert_entry_text(GtkButton *, Widgets *);
GtkWidget *xpm_label_box(GtkWidget *parent, gchar *xpm_filename);
void callback_join_game(GtkWidget *widget, gpointer data);
void callback_my_crs(GtkWidget *widget, gpointer data);
void crt_field();
void crt_end_menu(int command);
void crt_main_menu();
void callback_empty(GtkWidget *widget, gpointer data);
int connect_to_server();
int connect_to_check_port();
gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);
gboolean his_crs(gpointer data);

GtkWidget *xpm_label_box(GtkWidget *parent, gchar *xpm_filename) {
  GtkWidget *box1;
  GtkWidget *pixmapwid;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkStyle *style;

  box1 = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box1), 2);
  style = gtk_widget_get_style(parent);
  pixmap = gdk_pixmap_create_from_xpm(
          parent->window, &mask, &style->bg[GTK_STATE_NORMAL], xpm_filename);
  pixmapwid = gtk_pixmap_new(pixmap, mask);
  gtk_box_pack_start(GTK_BOX(box1), pixmapwid, FALSE, FALSE, 3);
  gtk_widget_show(pixmapwid);

  return (box1);
}

void insert_recv_text(gchar *text) {
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;
  char fulltext[74];
  sprintf(fulltext, "Opponent: %s", text);

  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat->textview));
  mark = gtk_text_buffer_get_insert(buffer);
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

  if (gtk_text_buffer_get_char_count(buffer))
    gtk_text_buffer_insert(buffer, &iter, "\n", 1);
  gtk_text_buffer_insert(buffer, &iter, fulltext, -1);
}

static void insert_entry_text(GtkButton *button, Widgets *w) {
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;
  const gchar *text;
  char fulltext[70];
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->textview));
  text = gtk_entry_get_text(GTK_ENTRY(w->entry));

  memset(message_send, 0, MSG_LEN);
  message_send[0] = 55;
  strcpy(message_send + 4, text);
  message_send[63] = '\0';

  sprintf(fulltext, "Вы: %s", message_send + 4);
  fulltext[69] = '\0';

  if (send(sockfd, message_send, MSG_LEN, 0) == -1) {
    perror("send");
  }

  mark = gtk_text_buffer_get_insert(buffer);
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

  if (gtk_text_buffer_get_char_count(buffer))
    gtk_text_buffer_insert(buffer, &iter, "\n", 1);

  gtk_text_buffer_insert(buffer, &iter, fulltext, -1);
  gtk_editable_delete_text(GTK_EDITABLE(w->entry), 0, -1);
}

void change_butt(int i, int j, int crs) {
  GtkWidget *button;
  gtk_widget_destroy(game_field2[i][j]);
  game_field2[i][j] = gtk_hbox_new(0, 0);
  game_field_pix[i][j] = gtk_hbox_new(0, 0);
  button = gtk_button_new();

  gtk_signal_connect(
          GTK_OBJECT(button),
          "clicked",
          GTK_SIGNAL_FUNC(callback_empty),
          (gpointer)(gpointer)(matrx + ((3 * i + j) * 2)));

  if (crs) {
    if (side == 1)
      game_field_pix[i][j] = xpm_label_box(window, "./pics/x.png");
    else
      game_field_pix[i][j] = xpm_label_box(window, "./pics/o.png");
  } else {
    if (side == 2)
      game_field_pix[i][j] = xpm_label_box(window, "./pics/x.png");
    else
      game_field_pix[i][j] = xpm_label_box(window, "./pics/o.png");
  }
  gtk_widget_show(game_field_pix[i][j]);
  gtk_container_add(GTK_CONTAINER(button), game_field_pix[i][j]);

  gtk_box_pack_start(GTK_BOX(game_field2[i][j]), button, TRUE, 10, 0);
  gtk_widget_show(button);
  gtk_box_pack_start(
          GTK_BOX(game_field1[i][j]), game_field2[i][j], TRUE, 10, 0);
  gtk_widget_show(game_field2[i][j]);
}

gboolean check_player(gpointer data) {
  read_fds = master;
  if (select(sockfd + 1, &read_fds, NULL, NULL, 0) == -1) {
    perror("select");
  }

  if (FD_ISSET(sockfd, &read_fds)) {
    if ((numbytes = recv(sockfd, message_recv, MSG_LEN, 0)) <= 0) {
      perror("recv");
      crt_end_menu(80);
      return FALSE;
    } else {
      id = message_recv[1];
      printf("Game id %d\n", id);
      message_send[0] = 100;
      if (send(sockfd, message_send, MSG_LEN, 0) == -1)
        perror("send");

      gtk_widget_destroy(contain);
      crt_field();
      return FALSE;
    }
  } else {
    return TRUE;
  }
}

void crt_end_menu(int command) {
  GtkWidget *label;
  GtkWidget *button;
  exit_flag = 1;
  if (command == 20 || command == 30) {
    if ((host == 1 && command == 20) || (host == 0 && command == 30)) {
      label = gtk_label_new("You win!");
    } else
      label = gtk_label_new("You lose!");
  }
  if (command == 40) {
    label = gtk_label_new("Draw!");
  }
  if (command == 60) {
    label = gtk_label_new("No games were created!\nCheck out later...");
  }
  if (command == 61) {
    label = gtk_label_new("Game is busy!");
  }
  if (command == 80) {
    label = gtk_label_new("Server is temporarily unreachable!\nTry again...");
  }
  if (command == 85) {
    label = gtk_label_new(
            "Name of the room must not be empty\nor be more than 20"
            "symbols");
  }
  if (command == 66) {
    label = gtk_label_new("You win!");
  }

  if (command != 61) {
    gtk_widget_destroy(GTK_WIDGET(contain));
  }

  host = 0;

  contain = gtk_vbox_new(0, 0);
  contain1 = gtk_vbox_new(0, 0);
  contain2 = gtk_vbox_new(0, 0);

  gtk_box_pack_start(GTK_BOX(contain1), label, TRUE, 10, 0);
  gtk_widget_show(label);

  gtk_container_add(GTK_CONTAINER(contain), contain1);

  button = gtk_button_new_with_label("Main menu");

  gtk_signal_connect(
          GTK_OBJECT(button),
          "clicked",
          GTK_SIGNAL_FUNC(crt_main_menu),
          GTK_OBJECT(window));

  gtk_box_pack_start(GTK_BOX(contain2), button, TRUE, 10, 10);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Exit");

  gtk_signal_connect(
          GTK_OBJECT(button),
          "clicked",
          GTK_SIGNAL_FUNC(delete_event),
          GTK_OBJECT(window));

  gtk_box_pack_start(GTK_BOX(contain2), button, TRUE, 10, 10);
  gtk_widget_show(button);

  gtk_container_add(GTK_CONTAINER(contain), contain2);

  gtk_container_add(GTK_CONTAINER(window), contain);
  gtk_widget_show(contain);
  gtk_widget_show(contain1);
  gtk_widget_show(contain2);

  close(sockfd);
  close(chk_sockfd);
}

gboolean check_server(gpointer data) {
  if (end_game)
    return FALSE;
  memset(message_send, 0, MSG_LEN);
  message_send[0] = 100;
  if (send(chk_sockfd, message_send, MSG_LEN, 0) == -1) {
    perror("check send");
    return FALSE;
  }

  memset(message_recv, 0, MSG_LEN);
  if ((numbytes = recv(chk_sockfd, message_recv, MSG_LEN, 0)) <= 0) {
    perror("check recv");
    if (connect_to_server() != 0) {
      crt_end_menu(80);
      return FALSE;
    } else {
      memset(message_send, 0, MSG_LEN);
      message_send[0] = 7;
      message_send[1] = id;
      if (host)
        message_send[2] = 1;
      else
        message_send[2] = 2;
      sleep(1);
      if (send(sockfd, message_send, MSG_LEN, 0) == -1) {
        perror("check send");
      }
      return TRUE;
    }
  }
  return TRUE;
}

gboolean his_crs(gpointer data) {
  int cmd, ij, i, j;
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  read_fds = master;

  if (select(sockfd + 1, &read_fds, NULL, NULL, &tv) == -1) {
    if (!end_game)
      perror("select");
  }
  memset(message_recv, 0, MSG_LEN);
  if (FD_ISSET(sockfd, &read_fds)) {
    if ((numbytes = recv(sockfd, message_recv, MSG_LEN, 0)) <= 0) {
      if (!end_game)
        perror("his recv");
      if (end_game)
        return FALSE;
      else
        return TRUE;
    } else {
      cmd = message_recv[0];
      if (cmd == 10) {
        ij = (int)message_recv[1];
        i = ij / 10;
        j = ij % 10;

        change_butt(i, j, 1);
        memset(message_recv, 0, MSG_LEN);
        if ((numbytes = recv(sockfd, message_recv, MSG_LEN, 0)) <= 0) {
          perror("recv");
          return TRUE;
        }
        cmd = message_recv[0];
        if (cmd == 50)
          course = 1;
        else {
          end_game = 1;
          crt_end_menu(cmd);
          return FALSE;
        }
        return TRUE;
      } else if (cmd == 66) {
        end_game = 1;
        crt_end_menu(cmd);
        return FALSE;
      } else if (cmd == 55) {
        insert_recv_text(message_recv + 4);
        memset(message_recv, 0, MSG_LEN);
        return TRUE;
      } else {
        printf("Com %d Check message recv!\n", cmd);
        return TRUE;
      }
    }
  } else
    return TRUE;
}

void callback_empty(GtkWidget *widget, gpointer data) {
}

void callback_my_crs(GtkWidget *widget, gpointer data) {
  gchar *name1;
  name1 = (gchar *)data;
  int ij, cmd;
  int i = name1[0] - '0';
  int j = name1[1] - '0';

  if (course == 1) {
    memset(message_send, 0, MSG_LEN);
    ij = i * 10 + j;
    message_send[0] = (char)10;
    message_send[1] = (char)ij;
    if (send(sockfd, message_send, MSG_LEN, 0) == -1) {
      perror("send");
    }
    memset(message_recv, 0, MSG_LEN);

    if ((numbytes = recv(sockfd, message_recv, MSG_LEN, 0)) == -1) {
      perror("recv");
      exit(1);
    }
    change_butt(i, j, 0);
    cmd = message_recv[0];
    if (cmd == 50)
      course = 2;
    else {
      end_game = 1;
      crt_end_menu(cmd);
    }
  }
}

void crt_field() {
  int i, j;
  GtkWidget *button;
  GtkWidget *table;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;
  GtkWidget *scrolled_win;
  char txt[] = "Chat:\n";
  exit_flag = 0;

  chat = g_slice_new(Widgets);
  contain11 = gtk_hbox_new(0, 0);
  contain = gtk_hbox_new(0, 0);
  table = gtk_table_new(2, 2, FALSE);

  for (i = 0; i < 3; i++) {
    field_col[i] = gtk_vbox_new(0, 0);
    gtk_box_pack_start(GTK_BOX(contain11), field_col[i], TRUE, 10, 0);
    gtk_widget_show(field_col[i]);
  }

  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      game_field1[i][j] = gtk_hbox_new(0, 0);
      gtk_box_pack_start(GTK_BOX(field_col[i]), game_field1[i][j], TRUE, 10, 0);
      gtk_widget_show(game_field1[i][j]);
    }
  }
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) {
      game_field2[i][j] = gtk_hbox_new(0, 0);

      button = gtk_button_new();
      gtk_signal_connect(
              GTK_OBJECT(button),
              "clicked",
              GTK_SIGNAL_FUNC(callback_my_crs),
              (gpointer)(matrx + ((3 * i + j) * 2)));

      game_field_pix[i][j] = xpm_label_box(window, "./pics/bg.png");
      gtk_widget_show(game_field_pix[i][j]);
      gtk_container_add(GTK_CONTAINER(button), game_field_pix[i][j]);

      gtk_box_pack_start(GTK_BOX(game_field2[i][j]), button, TRUE, 10, 0);
      gtk_widget_show(button);
      gtk_box_pack_start(
              GTK_BOX(game_field1[i][j]), game_field2[i][j], TRUE, 10, 0);
      gtk_widget_show(game_field2[i][j]);
    }
  }
  gtk_table_attach_defaults(GTK_TABLE(table), contain11, 0, 1, 0, 1);
  gtk_widget_show(contain11);
  gtk_widget_show(contain);

  button = gtk_button_new_with_label("\t\tSend\t\t");

  g_signal_connect(
          G_OBJECT(button),
          "clicked",
          G_CALLBACK(insert_entry_text),
          (gpointer)chat);
  g_signal_connect(
          G_OBJECT(button),
          "activate",
          G_CALLBACK(insert_entry_text),
          (gpointer)chat);

  gtk_table_attach_defaults(GTK_TABLE(table), button, 1, 2, 1, 2);
  gtk_widget_show(button);

  chat->textview = gtk_text_view_new();
  chat->entry = gtk_entry_new();

  scrolled_win = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(scrolled_win), chat->textview);

  gtk_table_attach_defaults(GTK_TABLE(table), scrolled_win, 1, 2, 0, 1);

  gtk_table_attach_defaults(GTK_TABLE(table), chat->entry, 0, 1, 1, 2);
  gtk_widget_show(chat->entry);
  gtk_widget_show(scrolled_win);
  gtk_widget_show(chat->textview);

  gtk_container_add(GTK_CONTAINER(contain), table);
  gtk_container_add(GTK_CONTAINER(window), contain);
  gtk_widget_show(table);

  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat->textview));
  mark = gtk_text_buffer_get_insert(buffer);
  gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

  gtk_text_buffer_insert(buffer, &iter, txt, -1);

  gtk_widget_set_state(chat->textview, GTK_STATE_INSENSITIVE);

  g_timeout_add_seconds(1, his_crs, NULL);
  g_timeout_add_seconds(1, check_server, NULL);
}

void callback_connect(GtkWidget *widget, gpointer data) {
  GtkWidget *label;
  contain11 = gtk_vbox_new(0, 0);
  const gchar *name;

  if (connect_to_server() != 0) {
    crt_end_menu(80);
  } else {
    name = gtk_entry_get_text(GTK_ENTRY(entry));

    if (strlen(name) == 0 || strlen(name) > 20) {
      crt_end_menu(85);
    } else {
      memset(message_send, 0, MSG_LEN);

      if (side == 1)
        message_send[0] = 1;
      else
        message_send[0] = 3;
      course = 1;

      strcpy(message_send + 4, name);

      if (send(sockfd, message_send, MSG_LEN, 0) == -1)
        perror("send");

      gtk_widget_destroy(GTK_WIDGET(data));
      gtk_widget_destroy(contain2);

      label = gtk_label_new("Searching for the opponent...");
      gtk_box_pack_start(GTK_BOX(contain11), label, TRUE, 10, 0);
      gtk_widget_show(label);

      gtk_box_pack_start(GTK_BOX(contain1), contain11, TRUE, 10, 10);
      gtk_widget_show(contain11);

      g_timeout_add_seconds(1, check_player, NULL);
    }
  }
}

void callback_enter_name(GtkWidget *widget, gpointer data) {
  gtk_widget_destroy(GTK_WIDGET(contain11));
  gchar *tmp = (gchar *)data;
  if (tmp[0] == '1')
    side = 1;
  else
    side = 2;
  host = 1;
  exit_flag = 1;
  GtkWidget *label;
  GtkWidget *button;

  contain11 = gtk_vbox_new(0, 0);

  button = gtk_button_new_with_mnemonic("Host");
  entry = gtk_entry_new();

  label = gtk_label_new("Enter the room's name:");
  gtk_box_pack_start(GTK_BOX(contain11), label, TRUE, 10, 0);
  gtk_widget_show(label);

  g_signal_connect(button, "clicked", G_CALLBACK(callback_connect), contain11);
  gtk_box_pack_start(GTK_BOX(contain11), entry, TRUE, 10, 0);
  gtk_box_pack_start(GTK_BOX(contain11), button, TRUE, 10, 0);
  gtk_widget_show(entry);
  gtk_widget_show(button);

  gtk_box_pack_start(GTK_BOX(contain1), contain11, TRUE, 10, 10);
  gtk_widget_show(contain11);
}

void callback_choice_side(GtkWidget *widget, gpointer data) {
  gtk_widget_destroy(GTK_WIDGET(data));

  GtkWidget *label;
  GtkWidget *pix;
  GtkWidget *button;
  GtkWidget *box = gtk_hbox_new(0, 0);

  contain11 = gtk_vbox_new(0, 0);
  exit_flag = 1;

  label = gtk_label_new("Choose your sight:");
  gtk_box_pack_start(GTK_BOX(contain11), label, TRUE, 10, 0);
  gtk_box_pack_start(GTK_BOX(contain11), box, TRUE, 10, 0);
  gtk_widget_show(label);

  button = gtk_button_new();
  g_signal_connect(
          G_OBJECT(button),
          "clicked",
          G_CALLBACK(callback_enter_name),
          (gpointer) "1");
  pix = xpm_label_box(window, "./pics/o.png");
  gtk_widget_show(pix);
  gtk_container_add(GTK_CONTAINER(button), pix);
  gtk_box_pack_start(GTK_BOX(box), button, FALSE, 10, 0);
  gtk_widget_show(button);

  button = gtk_button_new();
  g_signal_connect(
          G_OBJECT(button),
          "clicked",
          G_CALLBACK(callback_enter_name),
          (gpointer) "2");
  pix = xpm_label_box(window, "./pics/x.png");

  gtk_widget_show(pix);
  gtk_container_add(GTK_CONTAINER(button), pix);
  gtk_box_pack_end(GTK_BOX(box), button, FALSE, 10, 0);
  gtk_widget_show(button);

  gtk_widget_show(box);

  gtk_box_pack_start(GTK_BOX(contain1), contain11, TRUE, 10, 10);
  gtk_widget_show(contain11);
}

void callback_choice_game(GtkWidget *widget, gpointer data) {
  gtk_widget_destroy(GTK_WIDGET(contain));
  memset(message_send, 0, MSG_LEN);
  message_send[0] = 2;
  strcpy(message_send + 4, (char *)data);
  if (send(sockfd, message_send, MSG_LEN, 0) == -1)
    perror("send");

  memset(message_recv, 0, MSG_LEN);
  if ((numbytes = recv(sockfd, message_recv, MSG_LEN, 0)) == -1) {
    perror("recv");
    exit(1);
  }
  if (message_recv[0] == 61) {
    crt_end_menu(61);
  } else {
    if (message_recv[0] == 1)
      side = 2;
    else
      side = 1;
    exit_flag = 0;
    id = message_recv[1];
    printf("Game id %d\n", id);

    crt_field();
  }
}

void read_list(int count) {
  int i;

  for (i = 0; i < count; i++) {
    if ((numbytes = recv(sockfd, names[i].name, NAME_LEN - 1, 0)) == -1) {
      perror("recv");
      exit(1);
    }
    names[i].name[numbytes] = '\0';
    if (send(sockfd, message_send, MSG_LEN, 0) == -1)
      perror("send");
  }
}

void callback_join_game(GtkWidget *widget, gpointer data) {
  if (connect_to_server() != 0) {
    crt_end_menu(80);
  } else {
    gtk_widget_destroy(GTK_WIDGET(data));
    int i, count = 0;
    GtkWidget *button;
    GtkWidget *label;
    contain11 = gtk_vbox_new(0, 0);
    exit_flag = 0;

    memset(message_send, 0, MSG_LEN);
    memset(message_recv, 0, MSG_LEN);

    message_send[0] = 2;
    course = 2;

    if (send(sockfd, message_send, MSG_LEN, 0) == -1)
      perror("send");

    label = gtk_label_new("Room's list:");
    gtk_box_pack_start(GTK_BOX(contain11), label, TRUE, 10, 0);
    gtk_widget_show(label);

    if ((numbytes = recv(sockfd, message_recv, MSG_LEN, 0)) == -1) {
      perror("recv");
      exit(1);
    }
    count = message_recv[1];

    if (count != 60) {
      read_list(count);
      for (i = 0; i < count; i++) {
        button = gtk_button_new_with_label(names[i].name);
        gtk_signal_connect(
                GTK_OBJECT(button),
                "clicked",
                GTK_SIGNAL_FUNC(callback_choice_game),
                (gpointer)names[i].name);
        gtk_box_pack_start(GTK_BOX(contain11), button, TRUE, 10, 0);
        gtk_widget_show(button);
      }
      gtk_box_pack_start(GTK_BOX(contain1), contain11, TRUE, 10, 10);
      gtk_widget_show(contain11);
    } else
      crt_end_menu(60);
  }
}

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET)
    return &(((struct sockaddr_in *)sa)->sin_addr);
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
  gtk_main_quit();
  if (exit_flag == 0) {
    message_send[0] = (char)66;
    if (send(sockfd, message_send, MSG_LEN, 0) == -1) {
      perror("send");
    }
  }
  return FALSE;
}

int connect_to_check_port() {
  struct addrinfo hints, *servinfo, *p = NULL;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(
               servers[cur_serv].ip,
               servers[cur_serv].chk_port,
               &hints,
               &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return -1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((chk_sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
        -1) {
      perror("client: socket");
      continue;
    }
    if (connect(chk_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(chk_sockfd);
      perror("client: connect");
      continue;
    }
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect to check port\n");
    return -1;
  }

  freeaddrinfo(servinfo);
  return 0;
}

int connect_to_server() {
  struct addrinfo hints, *servinfo, *p = NULL;
  int rv, i;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if (server_status) {
    cur_serv--;
  }

  for (i = cur_serv + 1; i < SERVER_COUNT; i++) {
    if ((rv = getaddrinfo(servers[i].ip, servers[i].port, &hints, &servinfo)) !=
        0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      continue;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
          -1) {
        perror("client: socket");
        continue;
      }
      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        close(sockfd);
        perror("client: connect");
        continue;
      }
      connect_flag = 1;
      cur_serv = i;
      server_status = 1;
      break;
    }
    if (connect_flag) {
      connect_flag = 0;
      break;
    }
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return -1;
  }
  FD_SET(sockfd, &master);

  freeaddrinfo(servinfo);
  if (server_status)
    return connect_to_check_port();
  else
    return 0;
}

void crt_main_menu() {
  if (flg == 1)
    gtk_widget_destroy(contain);
  else
    flg = 1;
  exit_flag = 1;
  end_game = 0;
  gtk_window_set_default_size(GTK_WINDOW(window), 325, 250);

  contain = gtk_vbox_new(0, 0);
  contain1 = gtk_vbox_new(0, 0);
  contain11 = gtk_vbox_new(0, 0);
  contain2 = gtk_vbox_new(0, 0);

  button = gtk_button_new_with_label("Create game");
  gtk_signal_connect(
          GTK_OBJECT(button),
          "clicked",
          GTK_SIGNAL_FUNC(callback_choice_side),
          (gpointer)contain11);
  gtk_box_pack_start(GTK_BOX(contain11), button, FALSE, TRUE, 0);
  gtk_widget_show(button);

  button = gtk_button_new_with_label("Join game");
  gtk_signal_connect(
          GTK_OBJECT(button),
          "clicked",
          GTK_SIGNAL_FUNC(callback_join_game),
          (gpointer)contain11);
  gtk_box_pack_start(GTK_BOX(contain11), button, FALSE, TRUE, 0);
  gtk_widget_show(button);

  gtk_box_pack_start(GTK_BOX(contain1), contain11, FALSE, TRUE, 0);
  gtk_widget_show(contain11);

  button = gtk_button_new_with_label("Exit");
  gtk_signal_connect(
          GTK_OBJECT(button),
          "clicked",
          GTK_SIGNAL_FUNC(delete_event),
          GTK_OBJECT(window));
  gtk_box_pack_start(GTK_BOX(contain2), button, FALSE, TRUE, 10);
  gtk_widget_show(button);

  gtk_box_pack_start(GTK_BOX(contain), contain1, FALSE, TRUE, 10);
  gtk_box_pack_start(GTK_BOX(contain), contain2, FALSE, TRUE, 10);
  gtk_widget_show(contain1);
  gtk_widget_show(contain2);

  gtk_container_add(GTK_CONTAINER(window), contain);

  gtk_widget_show(contain);
  gtk_widget_show(window);
}

int main(int argc, char *argv[]) {
  gtk_init(&argc, &argv);
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Ti-tac-toe");
  gtk_signal_connect(
          GTK_OBJECT(window),
          "delete_event",
          GTK_SIGNAL_FUNC(delete_event),
          NULL);
  gtk_container_set_border_width(GTK_CONTAINER(window), 20);
  crt_main_menu();
  gtk_main();
  return 0;
}
