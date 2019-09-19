// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "image-burner/image_burner.h"

namespace imageburn {

// Register with the glib type system.
// This macro automatically defines a number of functions and variables
// which are required to make session_manager functional as a GObject:
// - image_burner_parent_class
// - image_burner_get_type()
// - dbus_glib_image__burner_object_info
// It also ensures that the structs are setup so that the initialization
// functions are called in the appropriate way by g_object_new().
G_DEFINE_TYPE(ImageBurner, image_burner, G_TYPE_OBJECT);

// This file is eerily reminiscent of cryptohome/interface.cc.

GObject* image_burner_constructor(GType gtype,
                                  guint n_properties,
                                  GObjectConstructParam* properties) {
  GObject* obj;
  GObjectClass* parent_class;
  // Instantiate using the parent class, then extend for local properties.
  parent_class = G_OBJECT_CLASS(image_burner_parent_class);
  obj = parent_class->constructor(gtype, n_properties, properties);

  ImageBurner* image_burner = reinterpret_cast<ImageBurner*>(obj);
  image_burner->service = NULL;

  // We don't have any thing we care to expose to the glib class system.
  return obj;
}

void image_burner_class_init(ImageBurnerClass* real_class) {
  // Called once to configure the "class" structure.
  GObjectClass* gobject_class = G_OBJECT_CLASS(real_class);
  gobject_class->constructor = image_burner_constructor;
}

void image_burner_init(ImageBurner* self) {}

gboolean image_burner_burn_image(ImageBurner* self,
                                 gchar* from_path,
                                 gchar* to_path,
                                 DBusGMethodInvocation* context) {
  if (!self->service)
    return false;
  return self->service->BurnImageAsync(from_path, to_path, context);
}

}  // namespace imageburn
