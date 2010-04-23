// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_PLUGIN_H_
#define CROMO_PLUGIN_H_

class ModemManagerServer;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _cromo_plugin_descriptor {
  const char* name;
  void (*onload)(ModemManagerServer*);
  void (*onunload)(void);
} cromo_plugin_descriptor;

#define CROMO_DEFINE_PLUGIN(name, onload, onunload) \
    extern cromo_plugin_descriptor plugin_descriptor \
        __attribute__((visibility("default"))); \
    cromo_plugin_descriptor plugin_descriptor = { \
         #name, onload, onunload \
    };


#ifdef __cplusplus
}
#endif
#endif  // CROMO_PLUGIN_H_
