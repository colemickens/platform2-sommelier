# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
include common.mk

PROTOC ?= protoc
GLIB_GENMARSHAL ?= glib-genmarshal
DBUS_BINDING_TOOL ?= dbus-binding-tool

PKG_CONFIG ?= pkg-config

# When you uprev this, please grep for TODOBASE to find stuff you might want to
# clean up or fix.
BASE_VER = 125070
PC_DEPS = dbus-1 dbus-glib-1 glib-2.0 nss \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS += -Wall -Werror -fno-exceptions
CPPFLAGS += -I$(OUT) $(PC_CFLAGS)
LDLIBS += -lpolicy-$(BASE_VER) -lmetrics $(PC_LIBS) \
	-lcrypto -lecryptfs -levent -lkeyutils -lchaps -lprotobuf \
	-lpthread -lscrypt -ltspi

PROTOS := install_attributes tpm_status vault_keyset attestation
PB_HDRS := $(patsubst %,%.pb.h,$(PROTOS))
PB_SRCS := $(patsubst %,%.pb.cc,$(PROTOS))
PB_OBJS := $(patsubst %.cc,%.o,$(PB_SRCS))
MARSHALS := marshal
MARSHAL_HDRS := $(patsubst %,%.glibmarshal.h,$(MARSHALS))
MARSHAL_SRCS := $(patsubst %,%.glibmarshal.c,$(MARSHALS))
MARSHAL_OBJS := $(patsubst %.c,%.o,$(MARSHAL_SRCS))
BINDING_HDRS := bindings/client.h bindings/server.h
GEN_HDRS := $(PB_HDRS) $(MARSHAL_HDRS) $(BINDING_HDRS)

SHARED_OBJS := $(filter-out cryptohomed.o cryptohome.o cryptohome-path.o \
                  %_tests.o %_unittest.o cryptohome_testrunner.o, $(CXX_OBJECTS))
TEST_OBJS := $(filter %_unittest.o cryptohome_testrunner.o %_tests.o, $(CXX_OBJECTS))

# Used to cause an object to depend on all the generated headers. Since the
# headers are generated, there's no way for common.mk's dependency detection to
# spot them, so just have everything depend on them to ensure they're built
# first.
define add_gen_hdrs
$(1).depends: $$(GEN_HDRS)

endef

$(eval \
    $(foreach obj, \
        $(CXX_OBJECTS), \
        $(call add_gen_hdrs,$(obj))))
# Add after we add the generated rules to CXX_OBJECTS.
$(eval $(call add_object_rules,$(MARSHAL_OBJS),CC,c))
$(eval $(call add_object_rules,$(PB_OBJS),CXX,cc))
SHARED_OBJS += $(MARSHAL_OBJS) $(PB_OBJS)

all: CXX_BINARY(cryptohomed) CXX_BINARY(cryptohome) CXX_BINARY(cryptohome-path)
tests: TEST(CXX_BINARY(cryptohome_testrunner))

%.pb.cc: $(SRC)/%.proto
	$(PROTOC) $^ --proto_path $(dir $^) --cpp_out $(dir $@)
%.pb.h: $(SRC)/%.proto
	$(PROTOC) $^ --proto_path $(dir $^) --cpp_out $(dir $@)
%.glibmarshal.c: $(SRC)/%.list
	$(GLIB_GENMARSHAL) --body --prefix=cryptohome $^ > $@
%.glibmarshal.h: $(SRC)/%.list
	$(GLIB_GENMARSHAL) --header --prefix=cryptohome $^ > $@

bindings/client.h: $(SRC)/cryptohome.xml
	mkdir -p $(dir $@)
	$(DBUS_BINDING_TOOL) --mode=glib-client --prefix=cryptohome $^ > $@

bindings/server.h: $(SRC)/cryptohome.xml
	mkdir -p $(dir $@)
	$(DBUS_BINDING_TOOL) --mode=glib-server --prefix=cryptohome $^ > $@

CXX_BINARY(cryptohomed): $(SHARED_OBJS) cryptohomed.o
CXX_BINARY(cryptohome): $(SHARED_OBJS) cryptohome.o
CXX_BINARY(cryptohome-path): $(SHARED_OBJS) cryptohome-path.o
CXX_BINARY(cryptohome_testrunner): LDLIBS += -lgtest -lgmock
CXX_BINARY(cryptohome_testrunner): $(SHARED_OBJS) $(TEST_OBJS)

clean: CLEAN(*.pb.cc) CLEAN(*.pb.h) CLEAN(cryptohomed)
clean: CLEAN(cryptohome) CLEAN(cryptohome-path) CLEAN(cryptohome_testrunner)
