// Copyright 2010 Google Inc. All Rights Reserved.
// Author: tbarzic@google.com (Toni Barzic)


#ifndef BURNIMAGE_INTERFACE_H_
#define BURNIMAGE_INTERFACE_H_

#include "image_burner/service.h"


// Helpers for using GObjects until we can get a C++ wrapper going.
namespace imageburn {
namespace gobject {  // Namespace hiding the GObject type data.

struct ImageBurner {
  GObject parent_instance;
  ImageBurnService *service;  // pointer to implementing service.
};
struct ImageBurnerClass { GObjectClass parent_class; };

// image_burner_get_type() is defined in interface.cc by the
// G_DEFINE_TYPE() macro.  This macro defines a number of other GLib
// class system specific functions and variables discussed in
// interface.cc.
GType image_burner_get_type();  // defined by G_DEFINE_TYPE

// Interface function prototypes which wrap service.
gboolean image_burner_burn_image(ImageBurner *self,
                                 gchar *from_path,
                                 gchar *to_path,
                                 GError **error);

}  // namespace gobject

}  // namespace image_burner

#endif  // BURNIMAGE_INTERFACE_H_

