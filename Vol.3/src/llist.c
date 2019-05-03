#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/llist.h"

struct lroot *init() {
  struct lroot *root;
  root = (struct lroot *)malloc(sizeof(struct lroot));
  root->first_node = NULL;
  return (root);
}

struct list *addelem(lroot *root, int fd, int chk_fd, int sid, char *name) {
  struct list *temp, *p;

  root->count = root->count + 1;

  temp = (struct list *)malloc(sizeof(list));
  p = root->first_node;
  root->first_node = temp;
  temp->fd = fd;
  temp->chk_fd = chk_fd;
  temp->sd = sid;
  strcpy(temp->name, name);
  temp->ptr = p;
  return (temp);
}

struct list *deletelem(list *lst, lroot *root) {
  struct list *temp;
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

void listprint(lroot *root) {
  struct list *p;
  p = root->first_node;
  do {
    printf("%d ", p->fd);
    printf("%s\n", p->name);
    p = p->ptr;
  } while (p != NULL);
}

struct list *listfind(lroot *root, char *name) {
  struct list *p;
  p = root->first_node;
  if (p != NULL) {
    do {
      if (!strcmp(name, p->name)) {
        return (p);
      }
      p = p->ptr;
    } while (p != NULL);
  }
  return (NULL);
}
