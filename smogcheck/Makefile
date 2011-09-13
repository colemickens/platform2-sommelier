# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CC?=cc
CFLAGS+=-g -Iinclude/ -Wimplicit -fPIC -Wall
LDFLAGS+=-luuid
EXEC_NAME=smogcheck
SONAME=libsmogcheck.so

all: binary

libs:
	$(CC) $(CFLAGS) -o lib/lib_smogcheck.o -c lib/lib_smogcheck.c

shared_libs: libs
	$(CC) -shared -fPIC -o ${SONAME} lib/lib_smogcheck.o -W1,-h${SONAME} \
	 -lpthread -lrt

binary: shared_libs
	$(CC) $(CFLAGS) -o smogcheck.o -c smogcheck.c
	$(CC) $(CFLAGS) -o smogcheck smogcheck.o lib/lib_smogcheck.o

install:
	install -D ${EXEC_NAME} $(DESTDIR)/usr/local/sbin/${EXEC_NAME}
	install -D -m 0644 $(SONAME) $(DESTDIR)/usr/lib/$(SONAME)
	ln -s $(SONAME) $(DESTDIR)/usr/lib/$(SONAME).0

.PHONY: clean cov all

clean:
	rm -f *.o lib/*.o ${EXEC_NAME} ${SONAME}
