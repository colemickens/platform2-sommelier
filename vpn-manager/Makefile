# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

BASE_VER ?= 125070

LIBDIR ?= lib
IPSEC_STARTER = /usr/libexec/ipsec/starter
IPSEC_WHACK = /usr/libexec/ipsec/whack
IPSEC_UPDOWN = /usr/libexec/l2tpipsec_vpn/pluto_updown
L2TPD = /usr/sbin/xl2tpd
L2TPIPSEC_OBJS = \
	ipsec_manager.o \
	l2tp_manager.o \
	service_manager.o
PKCS11_LIB = $(LIBDIR)/libchaps.so
TEST_OBJS = $(L2TPIPSEC_OBJS)

PC_DEPS = libchrome-$(BASE_VER) libchromeos-$(BASE_VER) openssl
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

LDLIBS = -lgflags $(PC_LIBS)

TEST_LIBS = -lgtest -lgmock

CPPFLAGS += \
	-I$(SRC)/.. \
	-DIPSEC_STARTER=\"$(IPSEC_STARTER)\" \
	-DIPSEC_WHACK=\"$(IPSEC_WHACK)\" \
	-DIPSEC_UPDOWN=\"$(IPSEC_UPDOWN)\" \
	-DL2TPD=\"$(L2TPD)\" \
	-DPKCS11_LIB=\"$(PKCS11_LIB)\" \
	$(PC_CFLAGS)

all: CXX_BINARY(l2tpipsec_vpn)

CXX_BINARY(l2tpipsec_vpn): l2tpipsec_vpn.o $(L2TPIPSEC_OBJS)

clean: CLEAN(l2tpipsec_vpn)

tests: \
	TEST(CXX_BINARY(ipsec_manager_test)) \
	TEST(CXX_BINARY(l2tp_manager_test)) \
	TEST(CXX_BINARY(service_manager_test))

CXX_BINARY(ipsec_manager_test): ipsec_manager_test.o $(TEST_OBJS)
CXX_BINARY(ipsec_manager_test): LDLIBS += $(TEST_LIBS)
CXX_BINARY(l2tp_manager_test): l2tp_manager_test.o $(TEST_OBJS)
CXX_BINARY(l2tp_manager_test): LDLIBS += $(TEST_LIBS)
CXX_BINARY(service_manager_test): service_manager_test.o $(TEST_OBJS)
CXX_BINARY(service_manager_test): LDLIBS += $(TEST_LIBS)
