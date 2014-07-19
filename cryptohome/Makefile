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
BASE_VER ?= 271506
PC_DEPS = dbus-1 dbus-glib-1 glib-2.0 gthread-2.0 openssl \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS += -Wall -Werror -fno-exceptions -Wno-deprecated-register --std=gnu++11
CPPFLAGS += -I"$(SRC)/.." -I$(OUT) $(PC_CFLAGS)
LDLIBS += -lpolicy-$(BASE_VER) -lmetrics-$(BASE_VER) $(PC_LIBS) \
	-lcrypto -lecryptfs -levent -lkeyutils -lchaps -lprotobuf \
	-lpthread -lscrypt -ltspi -lvboot_host -lm

SHARED_PROTOS := key rpc signed_secret
SHARED_PROTO_FILES := $(patsubst %,%.proto,$(SHARED_PROTOS))
SHARED_PROTO_PATH := $(SYSROOT)/usr/include/chromeos/dbus/cryptohome
SHARED_PB_HDRS := $(patsubst %,%.pb.h,$(SHARED_PROTOS))
SHARED_PB_SRCS := $(patsubst %,%.pb.cc,$(SHARED_PROTOS))
SHARED_PB_OBJS := $(patsubst %,%.pb.o,$(SHARED_PROTOS))

PROTOS := install_attributes tpm_status vault_keyset attestation \
          boot_lockbox_key
PB_HDRS := $(patsubst %,%.pb.h,$(PROTOS))
PB_SRCS := $(patsubst %,%.pb.cc,$(PROTOS))
PB_OBJS := $(patsubst %.cc,%.o,$(PB_SRCS))
MARSHALS := marshal
MARSHAL_HDRS := $(patsubst %,%.glibmarshal.h,$(MARSHALS))
MARSHAL_SRCS := $(patsubst %,%.glibmarshal.c,$(MARSHALS))
MARSHAL_OBJS := $(patsubst %.c,%.o,$(MARSHAL_SRCS))
BINDING_HDRS := bindings/client.h bindings/server.h
GEN_HDRS := $(SHARED_PB_HDRS) $(PB_HDRS) $(MARSHAL_HDRS) $(BINDING_HDRS)
ME_OBJS := mount-encrypted.o mount-helpers.o

TEST_OBJS := $(filter %_unittest.o cryptohome_testrunner.o %_tests.o mock_%.o, \
                      $(CXX_OBJECTS))
SHARED_OBJS := $(filter-out cryptohomed.o cryptohome.o cryptohome-path.o \
                            lockbox-cache-main.o $(ME_OBJS) $(TEST_OBJS) \
                            tpm.o cryptolib.o, $(CXX_OBJECTS))

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
$(eval $(call add_object_rules,$(SHARED_PB_OBJS),CXX,cc))
GEN_OBJS := $(MARSHAL_OBJS) $(PB_OBJS) $(SHARED_PB_OBJS)
SHARED_OBJS += $(GEN_OBJS)
$(eval \
    $(foreach obj, \
        $(GEN_OBJS), \
        $(call add_gen_hdrs,$(obj))))

all: libcrostpm.a
all: CXX_BINARY(cryptohomed) CXX_BINARY(cryptohome) CXX_BINARY(cryptohome-path)
all: CXX_BINARY(lockbox-cache) CC_BINARY(mount-encrypted)
tests: TEST(CXX_BINARY(cryptohome_testrunner))

# Pull in any shared proto files so they can have their optimization changed.
%.proto: $(SHARED_PROTO_PATH)/%.proto
	sed -e 's/LITE_RUNTIME/CODE_SIZE/g' $< > $@

$(SHARED_PB_SRCS): %.pb.cc: %.proto $(SHARED_PB_HDRS)
	$(QUIET)# No work.
$(SHARED_PB_HDRS): %.pb.h: %.proto $(SHARED_PROTO_FILES)
	$(PROTOC) $< --proto_path=$(dir $<) --proto_path=$(PWD) \
		--cpp_out=$(dir $@)

%.pb.cc: $(SRC)/%.proto $(PB_HDRS)
	$(QUIET)# No work.
%.pb.h: $(SRC)/%.proto $(SHARED_PROTO_FILES)
	$(PROTOC) $< --proto_path=$(dir $<) --proto_path=$(PWD) \
		--cpp_out=$(dir $@)

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

CXX_STATIC_LIBRARY(libcrostpm.pie.a): tpm.o cryptolib.o
libcrostpm.a: CXX_STATIC_LIBRARY(libcrostpm.pie.a)
	cp $(OUT)libcrostpm.pie.a $(OUT)libcrostpm.a
CXX_BINARY(cryptohomed): $(SHARED_OBJS) cryptohomed.o libcrostpm.a
CXX_BINARY(cryptohome): $(SHARED_OBJS) cryptohome.o libcrostpm.a
CXX_BINARY(cryptohome-path): $(SHARED_OBJS) cryptohome-path.o libcrostpm.a
CXX_BINARY(cryptohome_testrunner): LDLIBS += -lgtest -lgmock
CXX_BINARY(cryptohome_testrunner): $(SHARED_OBJS) $(TEST_OBJS) libcrostpm.a
CC_BINARY(mount-encrypted): $(ME_OBJS)
CXX_BINARY(lockbox-cache): tpm_status.pb.o lockbox.o lockbox-cache-tpm.o \
                           lockbox-cache.o lockbox-cache-main.o platform.o \
                           libcrostpm.a

clean: CLEAN(*.pb.cc) CLEAN(*.pb.h) CLEAN(cryptohomed) CLEAN(lockbox-cache)
clean: CLEAN(cryptohome) CLEAN(cryptohome-path) CLEAN(cryptohome_testrunner)
clean: CLEAN(mount-encrypted) CLEAN($(SHARED_PROTO_FILES))
clean: CLEAN(libcrostpm.pie.a) libcrostpm.a
