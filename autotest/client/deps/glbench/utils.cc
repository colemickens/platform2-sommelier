// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "utils.h"

#include "base/logging.h"
#include "main.h"

void *MmapFile(const char *name, size_t *length) {
  int fd = open(name, O_RDONLY);
  if (fd == -1)
    return NULL;

  struct stat sb;
  CHECK(fstat(fd, &sb) != -1);

  char *mmap_ptr = static_cast<char *>(
    mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));

  close(fd);

  if (mmap_ptr)
    *length = sb.st_size;

  return mmap_ptr;
}


static void print_info_log(int obj)
{
  char info_log[4096];
  int length;
  glGetInfoLogARB(obj, sizeof(info_log)-1, &length, info_log);
  char *p = info_log;
  while (p < info_log + length) {
    char *newline = strchr(p, '\n');
    if (newline)
      *newline = '\0';
    printf("# Log: %s\n", p);
    if (!newline)
      break;
    p = newline + 1;
  }
}


GLuint InitShaderProgram(const char *vertex_src, const char *fragment_src) {
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(vertex_shader, 1, &vertex_src, NULL);
  glShaderSource(fragment_shader, 1, &fragment_src, NULL);

  glCompileShader(vertex_shader);
  print_info_log(vertex_shader);
  glCompileShader(fragment_shader);
  print_info_log(fragment_shader);

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  print_info_log(program);
  glUseProgram(program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}
