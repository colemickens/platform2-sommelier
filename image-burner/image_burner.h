// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGE_BURNER_IMAGE_BURNER_H_
#define IMAGE_BURNER_IMAGE_BURNER_H_

#include "image-burner/image_burn_service.h"

// Helpers for using GObjects until we can get a C++ wrapper going.
namespace imageburn {

struct ImageBurner {
  GObject parent_instance;
  ImageBurnService* service;  // pointer to implementing service.
};

struct ImageBurnerClass {
  GObjectClass parent_class;
};

// image_burner_get_type() is defined in interface.cc by the
// G_DEFINE_TYPE() macro.  This macro defines a number of other GLib
// class system specific functions and variables discussed in
// interface.cc.
GType image_burner_get_type();  // defined by G_DEFINE_TYPE

// Interface function prototypes which wrap service.
gboolean image_burner_burn_image(ImageBurner* self,
                                 gchar* from_path,
                                 gchar* to_path,
                                 DBusGMethodInvocation* context);
}  // namespace imageburn

#endif  // IMAGE_BURNER_IMAGE_BURNER_H_
