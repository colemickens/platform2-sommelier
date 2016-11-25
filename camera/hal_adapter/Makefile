# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

PC_DEPS := libbrillo-$(BASE_VER) libchrome-$(BASE_VER) libmojo-$(BASE_VER)
PC_CFLAGS := $(shell SYSROOT=$(SYSROOT) $(PKG_CONFIG) --cflags $(PC_DEPS))
PC_LIBS := $(shell SYSROOT=$(SYSROOT) $(PKG_CONFIG) --libs $(PC_DEPS))

ANDROID_INCLUDE := $(abspath $(SRC)/../android/include)
ANDROID_INCLUDE_PATHS := \
	-I$(ANDROID_INCLUDE) \
	-I$(ANDROID_INCLUDE)/hardware/libhardware/include \
	-I$(ANDROID_INCLUDE)/system/core/include \
	-I$(ANDROID_INCLUDE)/system/core/libsync/include \
	-I$(ANDROID_INCLUDE)/system/media/camera/include \
	-I$(ANDROID_INCLUDE)/system/media/private/camera/include

CPPFLAGS += $(PC_CFLAGS) -I$(abspath $(SRC)/../) \
	    -I$(abspath $(SRC)/../include) $(ANDROID_INCLUDE_PATHS)
CXXFLAGS += -std=c++11 -Wall -Wpointer-arith
CFLAGS += -Wall -std=gnu99

LDLIBS += $(PC_LIBS) -ldl

MOJOM_FILES := arc_camera3.mojom

MOJOM_BINDINGS_GENERATOR := \
	$(SYSROOT)/usr/src/libmojo-$(BASE_VER)/mojo/mojom_bindings_generator.py

GEN_MOJO_TEMPLATES_DIR := $(OUT)/templates

mojo_templates: $(MOJOM_BINDINGS_GENERATOR) $(MOJOM_FILES)
	$(QUIET)echo generate_mojo_templates: $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)rm -rf $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)mkdir -p $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)python $(MOJOM_BINDINGS_GENERATOR) --use_bundled_pylibs precompile \
		-o $(GEN_MOJO_TEMPLATES_DIR)
	cd $(SRC) && \
		python $(abspath $(MOJOM_BINDINGS_GENERATOR)) \
		--use_bundled_pylibs generate \
		$(MOJOM_FILES) \
		-o $(abspath $(SRC)) \
		--bytecode_path $(abspath $(GEN_MOJO_TEMPLATES_DIR)) \
		-g c++

clean: CLEAN($(patsubst %,%.h,$(MOJOM_FILES)))
clean: CLEAN($(patsubst %,%.cc,$(MOJOM_FILES)))
clean: CLEAN($(patsubst %,%-internal.h,$(MOJOM_FILES)))

GENERATED_OBJECTS := $(patsubst %.mojom,%.mojom.o,$(MOJOM_FILES))

ANDROID_SRC = $(abspath $(SRC)/../android/src)
ANDROID_C_OBJECTS = \
	$(ANDROID_SRC)/camera_metadata.o \
	$(ANDROID_SRC)/native_handle.o \
	$(ANDROID_SRC)/sync.o

$(eval $(call add_object_rules,$(ANDROID_C_OBJECTS),CC,c,CFLAGS))

clean: CLEAN($(ANDROID_C_OBJECTS))
clean: CLEAN($(patsubst %.o,%.pic.o,$(ANDROID_C_OBJECTS)))
clean: CLEAN($(patsubst %.o,%.pic.d,$(ANDROID_C_OBJECTS)))
clean: CLEAN($(patsubst %.o,%.pie.o,$(ANDROID_C_OBJECTS)))
clean: CLEAN($(patsubst %.o,%.pie.d,$(ANDROID_C_OBJECTS)))
clean: CLEAN($(patsubst %.o,%.o.depends,$(ANDROID_C_OBJECTS)))

CXX_BINARY(arc_camera3_service): $(ANDROID_C_OBJECTS) $(CXX_OBJECTS) $(GENERATED_OBJECTS)

clean: CLEAN(arc_camera3_service)

all: CXX_BINARY(arc_camera3_service)

install: all
	mkdir -p $(DESTDIR)/usr/bin
	install -m 755 $(OUT)/arc_camera3_service $(DESTDIR)/usr/bin
