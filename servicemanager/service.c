// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "simplebinder.h"

typedef int (*do_cmd_func)(struct binder_state* bs);

void print_string16(uint16_t* str, size_t len) {
  size_t i;
  for (i = 0; i < len; i++)
    printf("%c", (char)(str[i]));
  printf("\n");
}

uint16_t* list_service(struct binder_state* bs,
                       uint32_t index,
                       size_t* result_len) {
  unsigned iodata[512 / 4];
  struct binder_io msg, reply;
  uint16_t* result;

  bio_init(&msg, iodata, sizeof(iodata), 4);
  bio_put_uint32(&msg, 0);
  bio_put_string16_x(&msg, SVC_MGR_NAME);
  bio_put_uint32(&msg, index);
  if (binder_call(bs, &msg, &reply, BINDER_SERVICE_MANAGER,
                  SVC_MGR_LIST_SERVICES))
    return NULL;

  result = bio_get_string16(&reply, result_len);
  binder_done(bs, &msg, &reply);
  return result;
}

int do_list(struct binder_state* bs) {
  uint32_t index = 0;
  uint16_t* name;
  size_t name_len = 0;
  while ((name = list_service(bs, index, &name_len)) != NULL) {
    printf("%d\t", index);
    print_string16(name, name_len);
    index++;
  }
  return 0;
}

int do_ping(struct binder_state* bs) {
  unsigned iodata[512 / 4];
  struct binder_io msg, reply;
  bio_init(&msg, iodata, sizeof(iodata), 0);
  if (binder_call(bs, &msg, &reply, BINDER_SERVICE_MANAGER, PING_TRANSACTION)) {
    printf("ServiceManger failed to respond\n");
  } else {
    printf("ServiceManager is ready\n");
  }
  binder_done(bs, &msg, &reply);
  return 0;
}

struct cmd_t {
  char* name;
  do_cmd_func func;
  char* help;
};

struct cmd_t g_cmds[] = {{"ping", do_ping, "Ping ServiceManager"},
                         {"list", do_list, "List Registered Services"}};
#define NUM_CMDS (sizeof(g_cmds) / sizeof(struct cmd_t))

void show_help(char* name) {
  int i;
  printf("Usage:\n");
  printf("%s <command>\n", name);
  printf("commands:\n");
  for (i = 0; i < NUM_CMDS; i++)
    printf("\t%s:\t%s\n", g_cmds[i].name, g_cmds[i].help);
}

do_cmd_func get_cmd_func(char* cmd) {
  int i;
  for (i = 0; i < NUM_CMDS; i++) {
    if (strcmp(cmd, g_cmds[i].name) == 0)
      return g_cmds[i].func;
  }
  return NULL;
}

int main(int argc, char** argv) {
  struct binder_state* bs;
  do_cmd_func func;

  if (argc != 2) {
    show_help(argv[0]);
    return 1;
  }

  func = get_cmd_func(argv[1]);
  if (func == NULL) {
    printf("Unknown Command\n");
    show_help(argv[0]);
    return 1;
  }

  bs = binder_open(128 * 1024);
  if (!bs) {
    printf("Failed to open binder\n");
    return 1;
  }

  func(bs);

  binder_close(bs);
  return 0;
}