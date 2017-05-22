# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include common.mk

### Rules to generate the mojom header and source files.

GEN_MOJO_TEMPLATES_DIR := $(OUT)/common/templates
MOJOM_BINDINGS_GENERATOR := \
	$(SYSROOT)/usr/src/libmojo-$(BASE_VER)/mojo/mojom_bindings_generator.py
COMMON_MOJOM_FILES := \
	common/mojo/camera_algorithm.mojom
GENERATED_SOURCES := $(patsubst %.mojom,%.mojom.cc,$(COMMON_MOJOM_FILES))
$(GENERATED_SOURCES):
	$(QUIET)echo generate_mojo_templates: $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)rm -rf $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)mkdir -p $(GEN_MOJO_TEMPLATES_DIR)
	$(QUIET)python $(MOJOM_BINDINGS_GENERATOR) \
		--use_bundled_pylibs precompile -o $(GEN_MOJO_TEMPLATES_DIR)
	cd $(SRC) && \
		python $(abspath $(MOJOM_BINDINGS_GENERATOR)) \
		--use_bundled_pylibs generate \
		$(COMMON_MOJOM_FILES) \
		-o $(SRC) \
		--bytecode_path $(abspath $(GEN_MOJO_TEMPLATES_DIR)) \
		-g c++

common/mojo/mojo_templates: \
	$(MOJOM_BINDINGS_GENERATOR) $(GENERATED_SOURCES)

clean: CLEAN($(patsubst %,%.h,$(COMMON_MOJOM_FILES)))
clean: CLEAN($(patsubst %,%.cc,$(COMMON_MOJOM_FILES)))
clean: CLEAN($(patsubst %,%-internal.h,$(COMMON_MOJOM_FILES)))

.PHONY: common/mojo/mojo_templates
