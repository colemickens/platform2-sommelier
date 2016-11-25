# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk
include pc_utils.mk

### Rules to generate the mojom header and source files.

GEN_MOJO_TEMPLATES_DIR := $(OUT)/hal_adapter/templates
MOJOM_BINDINGS_GENERATOR := \
	$(SYSROOT)/usr/src/libmojo-$(BASE_VER)/mojo/mojom_bindings_generator.py
MOJOM_FILES := hal_adapter/arc_camera3.mojom
GENERATED_SOURCES := $(patsubst %.mojom,%.mojom.cc,$(MOJOM_FILES))

$(GENERATED_SOURCES):
	$(QUIET)echo generate_mojo_templates: $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)rm -rf $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)mkdir -p $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)python $(MOJOM_BINDINGS_GENERATOR) \
		--use_bundled_pylibs precompile -o $(GEN_MOJO_TEMPLATES_DIR)
	cd $(SRC) && \
		python $(abspath $(MOJOM_BINDINGS_GENERATOR)) \
		--use_bundled_pylibs generate \
		$(MOJOM_FILES) \
		-o $(SRC) \
		--bytecode_path $(abspath $(GEN_MOJO_TEMPLATES_DIR)) \
		-g c++

hal_adapter/mojo_templates: $(MOJOM_BINDINGS_GENERATOR) $(GENERATED_SOURCES)

clean: CLEAN($(patsubst %,%.h,$(MOJOM_FILES)))
clean: CLEAN($(patsubst %,%.cc,$(MOJOM_FILES)))
clean: CLEAN($(patsubst %,%-internal.h,$(MOJOM_FILES)))

.PHONY: hal_adapter/mojo_templates

### Rules to generate the hal_adapter/arc_camera3_service binary.

hal_adapter_PC_DEPS := libbrillo-$(BASE_VER) libmojo-$(BASE_VER)
hal_adapter_CPPFLAGS := $(call get_pc_cflags,$(hal_adapter_PC_DEPS))
hal_adapter_LDLIBS := $(call get_pc_libs,$(hal_adapter_PC_DEPS)) -ldl

CXX_BINARY(hal_adapter/arc_camera3_service): CPPFLAGS += $(hal_adapter_CPPFLAGS)
CXX_BINARY(hal_adapter/arc_camera3_service): LDLIBS += $(hal_adapter_LDLIBS)
CXX_BINARY(hal_adapter/arc_camera3_service): \
	$(ANDROID_OBJECTS) \
	$(hal_adapter_CXX_OBJECTS)

hal_adapter/arc_camera3_service: CXX_BINARY(hal_adapter/arc_camera3_service)

clean: CLEAN(hal_adapter/arc_camera3_service)

.PHONY: hal_adapter/arc_camera3_service
