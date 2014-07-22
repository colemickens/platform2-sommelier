// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <va/va.h>
#include <va/va_drm.h>

#include "label_detect.h"

/* Returns true if given VA profile |va_profile| has |entrypoint| and the entry
 * point supports given raw |format|.
 */
static bool has_vaapi_entrypoint(VADisplay va_display, VAProfile va_profile,
    VAEntrypoint entrypoint, unsigned int format) {
  VAStatus va_res;
  VAConfigAttrib attrib = {VAConfigAttribRTFormat, 0};
  va_res = vaGetConfigAttributes(va_display, va_profile, entrypoint,
      &attrib, 1);
  if (va_res != VA_STATUS_SUCCESS) {
    TRACE("vaGetConfigAttributes failed (%d)\n", va_res);
    return false;
  }

  return attrib.value & format;
}

/* Returns true if the current platform supports at least one of the
 * |required_profiles| and |entrypoint| for that profile supports given raw
 * |format|.
 */
static bool match_vaapi_capabilities(VADisplay va_display,
    VAProfile* required_profiles,
    VAEntrypoint entrypoint, unsigned int format) {
  int i;
  bool found = false;
  int num_supported_profiles;
  VAStatus va_res;
  VAProfile* profiles;
  int max_profiles = vaMaxNumProfiles(va_display);
  if (max_profiles < 0) {
    TRACE("vaMaxNumProfiles returns negative number\n");
    return false;
  }

  profiles = (VAProfile*) alloca(sizeof(VAProfile) * max_profiles);
  if (!profiles) {
    TRACE("alloca failed\n");
    return false;
  }
  va_res = vaQueryConfigProfiles(va_display, profiles, &num_supported_profiles);
  if (va_res != VA_STATUS_SUCCESS) {
    TRACE("vaQueryConfigProfiles failed (%d)\n", va_res);
    return false;
  }
  for (i = 0; i < num_supported_profiles; i++) {
    int j;
    VAProfile profile = profiles[i];
    TRACE("supported profile: %d\n", profile);
    for (j = 0; required_profiles[j] != VAProfileNone; j++) {
      if (required_profiles[j] == profile &&
          has_vaapi_entrypoint(va_display, profile, entrypoint, format)) {
        found = true;
        /* continue the loop in order to output all supported profiles */
      }
    }
  }
  return found;
}

/* Returns true if libva supports any given profiles. And that profile has said
 * entrypoint with format.
 */
bool is_vaapi_support_formats(int fd, VAProfile* profiles,
    VAEntrypoint entrypoint, unsigned int format) {
  bool found = false;
  VAStatus va_res;
  VADisplay va_display;
  int major_version, minor_version;

  va_display = vaGetDisplayDRM(fd);
  if (!vaDisplayIsValid(va_display)) {
    TRACE("vaGetDisplay returns invalid display\n");
    return false;
  }

  va_res = vaInitialize(va_display, &major_version, &minor_version);
  if (va_res != VA_STATUS_SUCCESS) {
    TRACE("vaInitialize failed\n");
    return false;
  }

  if (match_vaapi_capabilities(va_display, profiles, entrypoint, format))
    found = true;

  vaTerminate(va_display);

  return found;
}
