# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

BINS = l2tpipsec_vpn
IPSEC_STARTER = /usr/libexec/ipsec/starter
IPSEC_WHACK = /usr/libexec/ipsec/whack
IPSEC_UPDOWN = /usr/libexec/l2tpipsec_vpn/pluto_updown
L2TPD = /usr/sbin/xl2tpd
L2TPIPSEC_OBJS = \
	ipsec_manager.o \
	l2tp_manager.o \
	service_manager.o
PKCS11_LIB = /usr/lib/libchaps.so
TEST_OBJS = $(L2TPIPSEC_OBJS)
TEST_BINS = \
	ipsec_manager_test \
	l2tp_manager_test \
	service_manager_test

LDCONFIG = \
	$(shell $(PKG_CONFIG) --libs libchromeos openssl)

# -lglib-2.0 is needed by libbase.a now.
COMMON_LIBS = -lchromeos -lbase -lpthread -lglib-2.0 -lgflags -lrt $(LDCONFIG)

TEST_LIBS = $(COMMON_LIBS) -lgtest -lgmock
INCLUDE_DIRS = -I..

CXXFLAGS += -Wall -Werror -DIPSEC_STARTER=\"$(IPSEC_STARTER)\" \
	-DIPSEC_WHACK=\"$(IPSEC_WHACK)\" \
	-DIPSEC_UPDOWN=\"$(IPSEC_UPDOWN)\" \
	-DL2TPD=\"$(L2TPD)\" \
	-DPKCS11_LIB=\"$(PKCS11_LIB)\" \
	$(shell $(PKG_CONFIG) --cflags libchromeos openssl)

all: $(BINS)

l2tpipsec_vpn: l2tpipsec_vpn.o $(L2TPIPSEC_OBJS)
	$(CXX) $(CXXFLAGS) $^ $(COMMON_LIBS) -o $@

tests: $(TEST_BINS)

%_test: %_test.o $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(LIB_DIRS) $^ $(TEST_LIBS) -o $@

.cc.o:
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

clean:
	rm -rf *.o $(BINS) $(TEST_BINS)
