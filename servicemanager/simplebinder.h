// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICEMANAGER_SIMPLEBINDER_H_
#define SERVICEMANAGER_SIMPLEBINDER_H_

#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
// Out of order due to this bad header requiring sys/types.h
#include <linux/android/binder.h>

#define BINDER_EXPORT __attribute__((visibility("default")))

struct binder_state;

struct binder_io {
  char* data;          /* pointer to read/write from */
  binder_size_t* offs; /* array of offsets */
  size_t data_avail;   /* bytes available in data buffer */
  size_t offs_avail;   /* entries available in offsets array */

  char* data0;          /* start of data buffer */
  binder_size_t* offs0; /* start of offsets buffer */
  uint32_t flags;
  uint32_t unused;
};

struct binder_death {
  void (*func)(struct binder_state* bs, void* ptr);
  void* ptr;
};

/* the one magic handle */
#define BINDER_SERVICE_MANAGER 0U

#define SVC_MGR_NAME "android.os.IServiceManager"

enum {
  /* Must match definitions in IBinder.h and IServiceManager.h */
  PING_TRANSACTION = B_PACK_CHARS('_', 'P', 'N', 'G'),
  SVC_MGR_GET_SERVICE = 1,
  SVC_MGR_CHECK_SERVICE,
  SVC_MGR_ADD_SERVICE,
  SVC_MGR_LIST_SERVICES,
};

typedef int (*binder_handler)(struct binder_state* bs,
                              struct binder_transaction_data* txn,
                              struct binder_io* msg,
                              struct binder_io* reply);

BINDER_EXPORT struct binder_state* binder_open(size_t mapsize);
BINDER_EXPORT void binder_close(struct binder_state* bs);

/* initiate a blocking binder call
 * - returns zero on success
 */
BINDER_EXPORT int binder_call(struct binder_state* bs,
                              struct binder_io* msg,
                              struct binder_io* reply,
                              uint32_t target,
                              uint32_t code);

/* release any state associate with the binder_io
 * - call once any necessary data has been extracted from the
 *   binder_io after binder_call() returns
 * - can safely be called even if binder_call() fails
 */
BINDER_EXPORT void binder_done(struct binder_state* bs,
                               struct binder_io* msg,
                               struct binder_io* reply);

/* manipulate strong references */
BINDER_EXPORT void binder_acquire(struct binder_state* bs, uint32_t target);
BINDER_EXPORT void binder_release(struct binder_state* bs, uint32_t target);

BINDER_EXPORT void binder_link_to_death(struct binder_state* bs,
                                        uint32_t target,
                                        struct binder_death* death);

BINDER_EXPORT void binder_loop(struct binder_state* bs, binder_handler func);

BINDER_EXPORT int binder_become_context_manager(struct binder_state* bs);

/* allocate a binder_io, providing a stack-allocated working
 * buffer, size of the working buffer, and how many object
 * offset entries to reserve from the buffer
 */
BINDER_EXPORT void bio_init(struct binder_io* bio,
                            void* data,
                            size_t maxdata,
                            size_t maxobjects);

BINDER_EXPORT void bio_put_obj(struct binder_io* bio, void* ptr);
BINDER_EXPORT void bio_put_ref(struct binder_io* bio, uint32_t handle);
BINDER_EXPORT void bio_put_uint32(struct binder_io* bio, uint32_t n);
BINDER_EXPORT void bio_put_string16(struct binder_io* bio, const uint16_t* str);
BINDER_EXPORT void bio_put_string16_x(struct binder_io* bio, const char* _str);

BINDER_EXPORT uint32_t bio_get_uint32(struct binder_io* bio);
BINDER_EXPORT uint16_t* bio_get_string16(struct binder_io* bio, size_t* sz);
BINDER_EXPORT uint32_t bio_get_ref(struct binder_io* bio);

#endif  // SERVICEMANAGER_SIMPLEBINDER_H_
