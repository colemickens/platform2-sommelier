// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "simplebinder.h"

#define critical(_msg, ...) \
  syslog(LOG_CRIT, "servicemanager: " _msg, ##__VA_ARGS__)

#define warn(_msg, ...) \
  syslog(LOG_WARNING, "servicemanager: " _msg, ##__VA_ARGS__)

#define info(_msg, ...) syslog(LOG_INFO, "servicemanager: " _msg, ##__VA_ARGS__)

#define BINDER_MAP_LEN (128 * 1024)  // 128kb

// TODO(leecam): What shall we name our service manager?
uint16_t svcmgr_id[] = {'a', 'n', 'd', 'r', 'o', 'i', 'd', '.', 'o',
                        's', '.', 'I', 'S', 'e', 'r', 'v', 'i', 'c',
                        'e', 'M', 'a', 'n', 'a', 'g', 'e', 'r'};

struct svcinfo {
  struct svcinfo* next;
  uint32_t handle;
  struct binder_death death;
  size_t len;
  uint16_t name[0];
};

struct svcinfo* svclist = NULL;

const char* str8(const uint16_t* x, size_t x_len) {
  static char buf[128];
  size_t max = 127;
  char* p = buf;

  if (x_len < max) {
    max = x_len;
  }

  if (x) {
    while ((max > 0) && (*x != '\0')) {
      *p++ = *x++;
      max--;
    }
  }
  *p++ = 0;
  return buf;
}

static int svc_can_find(const uint16_t* name, size_t name_len, pid_t spid) {
  // TODO(leecam): Implement actual perm checks.
  return 1;
}

static int svc_can_register(const uint16_t* name, size_t name_len, pid_t spid) {
  // TODO(leecam): Implement actual perm checks.
  return 1;
}

static int svc_can_list(pid_t spid) {
  // TODO(leecam): Implement actual perm checks.
  return 1;
}

void svcinfo_death(struct binder_state* bs, void* ptr) {
  struct svcinfo* si = (struct svcinfo*)ptr;

  info("service '%s' died\n", str8(si->name, si->len));
  if (si->handle) {
    binder_release(bs, si->handle);
    si->handle = 0;
  }
}

struct svcinfo* find_svc(const uint16_t* s16, size_t len) {
  struct svcinfo* si;

  for (si = svclist; si; si = si->next) {
    if ((len == si->len) && !memcmp(s16, si->name, len * sizeof(uint16_t))) {
      return si;
    }
  }
  return NULL;
}

uint32_t do_find_service(struct binder_state* bs,
                         const uint16_t* s,
                         size_t len,
                         uid_t uid,
                         pid_t spid) {
  struct svcinfo* si;

  if (!svc_can_find(s, len, spid)) {
    warn("find_service('%s') uid=%d - PERMISSION DENIED\n", str8(s, len), uid);
    return 0;
  }

  si = find_svc(s, len);

  if (si && si->handle) {
    // TODO(leecam): Android does an allow_isolated check here
    // which has no meaning in Brillo. Maybe do something similar?
    return si->handle;
  } else {
    return 0;
  }
}

int do_add_service(struct binder_state* bs,
                   const uint16_t* s,
                   size_t len,
                   uint32_t handle,
                   uid_t uid,
                   pid_t spid) {
  struct svcinfo* si;

  if (!handle || (len == 0) || (len > 127))
    return -1;

  if (!svc_can_register(s, len, spid)) {
    warn("add_service('%s',%x) uid=%d - PERMISSION DENIED\n", str8(s, len),
         handle, uid);
    return -1;
  }
  si = find_svc(s, len);
  if (si) {
    if (si->handle) {
      warn("add_service('%s',%x) uid=%d - ALREADY REGISTERED, OVERRIDE\n",
           str8(s, len), handle, uid);
      svcinfo_death(bs, si);
    }
    si->handle = handle;
  } else {
    si = malloc(sizeof(*si) + (len + 1) * sizeof(uint16_t));
    if (!si) {
      warn("add_service('%s',%x) uid=%d - OUT OF MEMORY\n", str8(s, len),
           handle, uid);
      return -1;
    }
    si->handle = handle;
    si->len = len;
    memcpy(si->name, s, (len + 1) * sizeof(uint16_t));
    si->name[len] = '\0';
    si->death.func = (void*)svcinfo_death;
    si->death.ptr = si;
    // si->allow_isolated = allow_isolated;
    si->next = svclist;
    svclist = si;
  }

  binder_acquire(bs, handle);
  binder_link_to_death(bs, handle, &si->death);
  return 0;
}

int svcmgr_handler(struct binder_state* bs,
                   struct binder_transaction_data* txn,
                   struct binder_io* msg,
                   struct binder_io* reply) {
  struct svcinfo* si;
  uint32_t handle;
  size_t len = 0;
  uint16_t* s;

  if (txn->target.handle != BINDER_SERVICE_MANAGER)
    return -1;

  if (txn->code == PING_TRANSACTION)
    return 0;

  // TODO(leecam): This first param in Android is 'strict_policy'.
  // It's ignored by Android's service manager so is this needed in Brillo?
  bio_get_uint32(msg);
  s = bio_get_string16(msg, &len);

  if (s == NULL)
    return -1;

  if ((len != (sizeof(svcmgr_id) / 2)) ||
      memcmp(svcmgr_id, s, sizeof(svcmgr_id))) {
    warn("invalid id %s\n", str8(s, len));
    return -1;
  }

  switch (txn->code) {
    case SVC_MGR_GET_SERVICE:
    case SVC_MGR_CHECK_SERVICE:
      s = bio_get_string16(msg, &len);
      if (s == NULL)
        return -1;
      handle = do_find_service(bs, s, len, txn->sender_euid, txn->sender_pid);
      if (!handle)
        break;
      bio_put_ref(reply, handle);
      break;

    case SVC_MGR_ADD_SERVICE:
      s = bio_get_string16(msg, &len);
      if (s == NULL)
        return -1;
      handle = bio_get_ref(msg);

      // allow_isolated = bio_get_uint32(msg) ? 1 : 0;
      if (do_add_service(bs, s, len, handle, txn->sender_euid,
                         txn->sender_pid)) {
        return -1;
      }
      break;

    case SVC_MGR_LIST_SERVICES: {
      uint32_t n = bio_get_uint32(msg);
      if (!svc_can_list(txn->sender_pid)) {
        warn("list_service() uid=%d - PERMISSION DENIED\n", txn->sender_euid);
        return -1;
      }
      si = svclist;
      while ((n-- > 0) && si)
        si = si->next;
      if (si) {
        bio_put_string16(reply, si->name);
        return 0;
      }
      return -1;
    }

    default:
      warn("unknown code\n");
      return -1;
  }
  bio_put_uint32(reply, 0);
  return 0;
}

int main(int argc, char** argv) {
  struct binder_state* bs;

  bs = binder_open(BINDER_MAP_LEN);
  if (!bs) {
    critical("failed to open binder driver\n");
    return -1;
  }

  if (binder_become_context_manager(bs)) {
    critical("cannot become context manager (%s)\n", strerror(errno));
    binder_close(bs);
    return -1;
  }

  binder_loop(bs, svcmgr_handler);

  return 0;
}
