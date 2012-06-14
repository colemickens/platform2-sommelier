# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Top-level Makefile for chaps.

# Pull in chromium os defaults
OUT ?= $(PWD)/build-opt-local

include common.mk

PKG_CONFIG ?= pkg-config
DBUSXX_XML2CPP = dbusxx-xml2cpp
PROTOC = protoc

BASE_VER ?= 125070
PC_DEPS = dbus-c++-1 protobuf openssl \
	libchrome-$(BASE_VER) libchromeos-$(BASE_VER)
PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell $(PKG_CONFIG) --libs $(PC_DEPS))

CXXFLAGS += -I$(SRC)/.. -I$(OUT) $(PC_CFLAGS) -DNDEBUG
LDLIBS += $(PC_LIBS)

# Rules for Generated Code
chaps_proxy_generated.h: $(SRC)/chaps_interface.xml
	$(DBUSXX_XML2CPP) $< --proxy=$@
clean: CLEAN(chaps_proxy_generated.h)

chaps_proxy.o.depends \
chapsd_test.o.depends \
chaps.o.depends \
chaps_event_generator.o.depends \
login_event_client.o.depends: chaps_proxy_generated.h

chaps_adaptor_generated.h: $(SRC)/chaps_interface.xml
	$(DBUSXX_XML2CPP) $< --adaptor=$@
clean: CLEAN(chaps_adaptor_generated.h)

chaps_adaptor.o.depends \
chapsd.o.depends : chaps_adaptor_generated.h

attributes.pb.h attributes.pb.cc: $(SRC)/attributes.proto
	$(PROTOC) -I$(SRC) --cpp_out=$(OUT) $<
clean: CLEAN(attributes.pb.h attributes.pb.cc)
$(eval $(call add_object_rules,attributes.pb.o,CXX,cc))

object_pool_impl.o.depends \
object_pool_test.o.depends \
attributes.o.depends: attributes.pb.h

# Common Files
COMMON_OBJS = chaps_utility.o attributes.pb.o attributes.o

# Chaps Daemon
chapsd_OBJS = $(COMMON_OBJS) \
              chapsd.o \
              chaps_service.o \
              chaps_service_redirect.o \
              chaps_adaptor.o \
              slot_manager_impl.o \
              session_impl.o \
              object_impl.o \
              object_policy_common.o \
              object_policy_data.o \
              object_policy_cert.o \
              object_policy_key.o \
              object_policy_public_key.o \
              object_policy_private_key.o \
              object_policy_secret_key.o \
              object_pool_impl.o \
              tpm_utility_impl.o \
              chaps_factory_impl.o \
              object_store_impl.o \
              opencryptoki_importer.o
chapsd_LIBS = -ldl -ltspi -lleveldb -lmemenv
CXX_BINARY(chapsd): $(chapsd_OBJS)
CXX_BINARY(chapsd): LDLIBS += $(chapsd_LIBS)
clean: CLEAN(chapsd)
all: CXX_BINARY(chapsd)

# Chaps Client Library
libchaps_OBJS = $(COMMON_OBJS) chaps.o chaps_proxy.o login_event_client.o
CXX_LIBRARY(libchaps.so): $(libchaps_OBJS)
clean: CLEAN(libchaps.so)
all: CXX_LIBRARY(libchaps.so)

# Chaps Client CLI App
chaps_client_OBJS = $(COMMON_OBJS) chaps_client.o chaps_proxy.o \
                    login_event_client.o
CXX_BINARY(chaps_client): $(chaps_client_OBJS)
clean: CLEAN(chaps_client)
all: CXX_BINARY(chaps_client)

# PKCS #11 Replay Utility
p11_replay_OBJS = p11_replay.o
CXX_BINARY(p11_replay): $(p11_replay_OBJS) CXX_LIBRARY(libchaps.so)
clean: CLEAN(p11_replay)
all: CXX_BINARY(p11_replay)

# Common Test Dependencies
MOCK_OBJS = chaps_factory_mock.o \
            object_mock.o \
            object_policy_mock.o \
            object_pool_mock.o \
            object_store_mock.o \
            session_mock.o \
            slot_manager_mock.o \
            tpm_utility_mock.o \
            object_importer_mock.o

GMOCK_LIBS = -lgtest -lgmock

# QEMU will not run death-tests, generate RSA keys, or handle the import sample
# tarball. These tests need to be filtered out.
qemu_test_exclusions = *DeathTest* \
                       *ImportSample* \
                       TestSession.RSA* \
                       TestSession.KeyTypeMismatch \
                       TestSession.KeyFunctionPermission \
                       TestSession.BadKeySize \
                       TestSession.BadSignature
# We want $(space)=' '.
space :=
space +=
qemu_test_filter := -$(subst $(space),:,$(strip $(qemu_test_exclusions)))
tests: override GTEST_ARGS.qemu.arm := --gtest_filter=$(qemu_test_filter)

# Unit Tests
chaps_test_OBJS = $(COMMON_OBJS) chaps_test.o chaps_proxy_mock.o
chaps_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(chaps_test): $(chaps_test_OBJS) CXX_LIBRARY(libchaps.so)
CXX_BINARY(chaps_test): LDLIBS += $(chaps_test_LIBS)
clean: CLEAN(chaps_test)
tests: TEST(CXX_BINARY(chaps_test))

chaps_service_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                          chaps_service_test.o chaps_service.o
chaps_service_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(chaps_service_test): $(chaps_service_test_OBJS)
CXX_BINARY(chaps_service_test): LDLIBS += $(chaps_service_test_LIBS)
clean: CLEAN(chaps_service_test)
tests: TEST(CXX_BINARY(chaps_service_test))

slot_manager_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                         slot_manager_test.o slot_manager_impl.o
slot_manager_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(slot_manager_test): $(slot_manager_test_OBJS)
CXX_BINARY(slot_manager_test): LDLIBS += $(slot_manager_test_LIBS)
clean: CLEAN(slot_manager_test)
tests: TEST(CXX_BINARY(slot_manager_test))

session_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) session_test.o session_impl.o
session_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(session_test): $(session_test_OBJS)
CXX_BINARY(session_test): LDLIBS += $(session_test_LIBS)
clean: CLEAN(session_test)
tests: TEST(CXX_BINARY(session_test))

object_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) object_test.o object_impl.o
object_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(object_test): $(object_test_OBJS)
CXX_BINARY(object_test): LDLIBS += $(object_test_LIBS)
clean: CLEAN(object_test)
tests: TEST(CXX_BINARY(object_test))

object_policy_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                          object_policy_test.o \
                          object_policy_common.o \
                          object_policy_data.o \
                          object_policy_cert.o \
                          object_policy_key.o \
                          object_policy_public_key.o \
                          object_policy_private_key.o \
                          object_policy_secret_key.o
object_policy_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(object_policy_test): $(object_policy_test_OBJS)
CXX_BINARY(object_policy_test): LDLIBS += $(object_policy_test_LIBS)
clean: CLEAN(object_policy_test)
tests: TEST(CXX_BINARY(object_policy_test))

object_pool_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                        object_pool_test.o object_pool_impl.o
object_pool_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(object_pool_test): $(object_pool_test_OBJS)
CXX_BINARY(object_pool_test): LDLIBS += $(object_pool_test_LIBS)
clean: CLEAN(object_pool_test)
tests: TEST(CXX_BINARY(object_pool_test))

object_store_test_OBJS = $(COMMON_OBJS) object_store_test.o object_store_impl.o
object_store_test_LIBS = -lgtest -lleveldb -lmemenv
CXX_BINARY(object_store_test): $(object_store_test_OBJS)
CXX_BINARY(object_store_test): LDLIBS += $(object_store_test_LIBS)
clean: CLEAN(object_store_test)
tests: TEST(CXX_BINARY(object_store_test))

opencryptoki_importer_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                                  opencryptoki_importer_test.o \
                                  opencryptoki_importer.o
opencryptoki_importer_test_LIBS = $(GMOCK_LIBS)
CXX_BINARY(opencryptoki_importer_test): $(opencryptoki_importer_test_OBJS)
CXX_BINARY(opencryptoki_importer_test): LDLIBS += \
                                        $(opencryptoki_importer_test_LIBS)
clean: CLEAN(opencryptoki_importer_test)
tests: $(OUT)/opencryptoki_sample_token.tgz
$(OUT)/opencryptoki_sample_token.tgz:
	cp $(SRC)/opencryptoki_sample_token.tgz $@
clean: CLEAN(opencryptoki_sample_token.tgz)
tests: TEST(CXX_BINARY(opencryptoki_importer_test))

import_random: override GTEST_ARGS := \
    --gtest_repeat=100 \
    --gtest_break_on_failure \
    --gtest_filter=RandomizedTests/TestImporterWithModifier.ImportSample/*
import_random: override GTEST_ARGS.qemu.arm := --gtest_filter=-*
import_random: TEST(CXX_BINARY(opencryptoki_importer_test))
more_tests: import_random

# Live Tests
# Note: These tests require a live system with gtest and gmock installed. These
# cannot be run without a real TPM and cannot be run with autotest. These tests
# do not need to be run regularly but may be useful in the future and so have
# been kept around. To run these tests:
# 1) Add the targets to 'all'.
# 2) Add 'dobin' instructions to the ebuild and install on target device.
# 3) On the target system, login as any user and open a shell.
# 4) Run 'sudo tpm_utility_test'.
# 5) Run 'sudo chapsd_test'.
# 6) Run 'chapsd_test --use_dbus'.
chapsd_test_OBJS = $(COMMON_OBJS) chapsd_test.o chaps_proxy.o \
                   chaps_service_redirect.o
chapsd_test_LIBS = -lgtest -lgflags -ldl
CXX_BINARY(chapsd_test): $(chapsd_test_OBJS) CXX_LIBRARY(libchaps.so)
CXX_BINARY(chapsd_test): LDLIBS += $(chapsd_test_LIBS)
clean: CLEAN(chapsd_test)

tpm_utility_test_OBJS = $(COMMON_OBJS) $(MOCK_OBJS) \
                        tpm_utility_test.o \
                        tpm_utility_impl.o
tpm_utility_test_LIBS = $(GMOCK_LIBS) -ltspi
CXX_BINARY(tpm_utility_test): $(tpm_utility_test_OBJS)
CXX_BINARY(tpm_utility_test): LDLIBS += $(tpm_utility_test_LIBS)
clean: CLEAN(tpm_utility_test)

