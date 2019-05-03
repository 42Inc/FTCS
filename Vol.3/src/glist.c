#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/glist.h"

struct glroot *ginit() {
  struct glroot *root;
  root = (struct glroot *)malloc(sizeof(struct glroot));
  root->first_node = NULL;
  return (root);
}

struct glist *gaddelem(glroot *root, int number, int sockfd) {
  struct glist *temp, *p;
  int i, j;

  root->count = root->count + 1;

  temp = (struct glist *)malloc(sizeof(glist));
  p = root->first_node;
  root->first_node = temp;
  temp->number = number;
  temp->fd = sockfd;
  temp->chk_fd = -1;
  temp->flg = 0;
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) { temp->field[i][j] = 0; }
  }
  temp->ptr = p;
  return (temp);
}

struct glist *gdeletelem(glist *lst, glroot *root) {
  struct glist *temp;
  temp = root->first_node;
  root->count = root->count - 1;
  if (temp != lst) {
    while (temp->ptr != lst) { temp = temp->ptr; }
    temp->ptr = lst->ptr;
    free(lst);
  } else {
    root->first_node = lst->ptr;
    temp->ptr = lst->ptr;
    free(lst);
  }
  return (temp);
}

void glistprint(glroot *root) {
  struct glist *p;
  int i, j;

  p = root->first_node;
  do {
    printf("№%d ", p->number);
    for (i = 0; i < 3; i++) {
      for (j = 0; j < 3; j++) { printf("%d ", p->field[i][j]); }
    }
    printf("\n");
    p = p->ptr;
  } while (p != NULL);
}

struct glist *glistfind(glroot *root, int number) {
  struct glist *p;
  p = root->first_node;
  if (p != NULL) {
    do {
      if (p->number == number) {
        return (p);
      }
      p = p->ptr;
    } while (p != NULL);
  }
  return (NULL);
}
