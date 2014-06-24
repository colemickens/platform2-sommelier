// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Public API for the C and C++ bindings to the Chromium OS
// 'bootstat' facility.  The facility is a simple timestamp
// mechanism to associate a named event with the time that it
// occurred and with other relevant statistics.

#ifndef BOOTSTAT_H_
#define BOOTSTAT_H_

#if defined(__cplusplus)
extern "C" {
#endif

//
// Length of the longest valid string naming an event, including the
// terminating NUL character.  Clients of bootstat_log() can use
// this value for the size of buffers to hold event names; names
// that exceed this buffer size will be truncated.
//
// This value is arbitrarily chosen, but see comments in
// bootstat_log.c regarding implementation assumptions for this
// value.
//
#define BOOTSTAT_MAX_EVENT_LEN  64

// Log an event.  Event names should be composed of characters drawn
// from this subset of 7-bit ASCII:  Letters (upper- or lower-case),
// digits, dot ('.'), dash ('-'), and underscore ('_').  Case is
// significant.  Behavior in the presence of other characters is
// unspecified - Caveat Emptor!
//
// Applications are responsible for establishing higher-level naming
// conventions to prevent name collisions.
extern void bootstat_log(const char* event_name);

#if defined(__cplusplus)
}
#endif
#endif  // BOOTSTAT_H_
