# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Makefile for bootstat utilities
#

OPT_FLAGS = -O
CFLAGS += -Wall -Werror -fPIC -fno-exceptions $(OPT_FLAGS)

OBJS = \
	bootstat.o \
	bootstat_log.o

COMMAND = bootstat

all: $(COMMAND)

$(COMMAND): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(OBJS)

clean:
	rm -f $(COMMAND) $(OBJS)
