# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

CXXFLAGS += -std=c++11 -Wall -Wpointer-arith

### Rules to generate the mojom header and source files.

GEN_MOJO_TEMPLATES_DIR := $(OUT)/templates
MOJOM_BINDINGS_GENERATOR := \
	$(SYSROOT)/usr/src/libmojo-$(BASE_VER)/mojo/mojom_bindings_generator.py
MOJOM_FILES := arc_camera.mojom
MOJO_SRCS := $(patsubst %,%.cc,$(MOJOM_FILES))
MOJO_HEADERS := \
	$(patsubst %,%.h,$(MOJOM_FILES)) \
	$(patsubst %,%-internal.h,$(MOJOM_FILES))
MOJO_OBJECTS := $(patsubst %,%.o,$(MOJOM_FILES))

$(MOJO_SRCS) $(MOJO_HEADERS): $(MOJOM_BINDINGS_GENERATOR) $(MOJOM_FILES)
	$(QUIET)echo generate_mojo_templates: $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)rm -rf $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)mkdir -p $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)python $(MOJOM_BINDINGS_GENERATOR) \
		--use_bundled_pylibs precompile -o $(GEN_MOJO_TEMPLATES_DIR)
	cd $(SRC) && python $(abspath $(MOJOM_BINDINGS_GENERATOR)) \
		--use_bundled_pylibs generate \
		$(MOJOM_FILES) \
		-o $(abspath $(SRC)) \
		--bytecode_path $(abspath $(GEN_MOJO_TEMPLATES_DIR)) \
		-g c++

clean: CLEAN($(MOJO_SRCS))
clean: CLEAN($(MOJO_HEADERS)))

### Rules to generate the arc_camera_service binary.

hal_usb_v1_PC_DEPS := \
	libbrillo-$(BASE_VER) libchrome-$(BASE_VER) libmojo-$(BASE_VER)
hal_usb_v1_CPPFLAGS := $(call get_pc_cflags,$(hal_usb_v1_PC_DEPS))
hal_usb_v1_LDLIBS := $(call get_pc_libs,$(hal_usb_v1_PC_DEPS))

CXX_BINARY(arc_camera_service): CPPFLAGS += $(hal_usb_v1_CPPFLAGS)
CXX_BINARY(arc_camera_service): LDLIBS += $(hal_usb_v1_LDLIBS)
# Use path relative to arc-camera in include.
CXX_BINARY(arc_camera_service): CPPFLAGS += -I $(SRC)/../..
CXX_BINARY(arc_camera_service): \
	$(MOJO_OBJECTS) \
	$(CXX_OBJECTS) \

arc_camera_service: CXX_BINARY(arc_camera_service)

clean: CLEAN(arc_camera_service)

.PHONY: arc_camera_service
