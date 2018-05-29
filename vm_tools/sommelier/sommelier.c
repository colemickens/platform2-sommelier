// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <libgen.h>
#include <limits.h>
#include <linux/virtwl.h>
#include <math.h>
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xcb/composite.h>
#include <xcb/xfixes.h>

#include "aura-shell-client-protocol.h"
#include "drm-server-protocol.h"
#include "keyboard-extension-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "xdg-shell-unstable-v6-client-protocol.h"

// Check that required macro definitions exist.
#ifndef XWAYLAND_PATH
#error XWAYLAND_PATH must be defined
#endif
#ifndef XWAYLAND_SHM_DRIVER
#error XWAYLAND_SHM_DRIVER must be defined
#endif
#ifndef SHM_DRIVER
#error SHM_DRIVER must be defined
#endif
#ifndef VIRTWL_DEVICE
#error VIRTWL_DEVICE must be defined
#endif
#ifndef PEER_CMD_PREFIX
#error PEER_CMD_PREFIX must be defined
#endif

struct sl_global {
  struct sl_context* ctx;
  const struct wl_interface *interface;
  uint32_t name;
  uint32_t version;
  void *data;
  wl_global_bind_func_t bind;
  struct wl_list link;
};

struct sl_host_registry {
  struct sl_context* ctx;
  struct wl_resource *resource;
  struct wl_list link;
};

struct sl_host_callback {
  struct wl_resource *resource;
  struct wl_callback *proxy;
};

struct sl_compositor {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct sl_global* host_global;
  struct wl_compositor *internal;
};

typedef void (*sl_begin_end_access_func_t)(int fd);

struct sl_mmap {
  int refcount;
  int fd;
  void *addr;
  size_t size;
  size_t offset;
  size_t stride;
  size_t bpp;
  sl_begin_end_access_func_t begin_access;
  sl_begin_end_access_func_t end_access;
  struct wl_resource *buffer_resource;
};

struct sl_output_buffer;

struct sl_output_buffer {
  struct wl_list link;
  uint32_t width;
  uint32_t height;
  uint32_t format;
  struct wl_buffer *internal;
  struct sl_mmap* mmap;
  struct pixman_region32 damage;
  struct sl_host_surface* surface;
};

struct sl_host_region {
  struct sl_context* ctx;
  struct wl_resource *resource;
  struct wl_region *proxy;
};

struct sl_host_compositor {
  struct sl_compositor* compositor;
  struct wl_resource *resource;
  struct wl_compositor *proxy;
};

struct sl_host_shm_pool {
  struct sl_shm* shm;
  struct wl_resource *resource;
  struct wl_shm_pool *proxy;
  int fd;
};

struct sl_host_shm {
  struct sl_shm* shm;
  struct wl_resource* resource;
  struct wl_shm* shm_proxy;
  struct zwp_linux_dmabuf_v1* linux_dmabuf_proxy;
};

struct sl_shm {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
  struct wl_shm *internal;
};

struct sl_host_shell_surface {
  struct wl_resource *resource;
  struct wl_shell_surface *proxy;
};

struct sl_host_shell {
  struct sl_shell* shell;
  struct wl_resource *resource;
  struct wl_shell *proxy;
};

struct sl_shell {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
};

struct sl_output {
  struct sl_context* ctx;
  uint32_t id;
  uint32_t version;
  struct sl_global* host_global;
  struct wl_list link;
};

struct sl_data_source {
  struct sl_context* ctx;
  struct wl_data_source *internal;
};

struct sl_subcompositor {
  struct sl_context* ctx;
  uint32_t id;
  struct sl_global* host_global;
};

struct sl_host_subcompositor {
  struct sl_context* ctx;
  struct wl_resource *resource;
  struct wl_subcompositor *proxy;
};

struct sl_host_subsurface {
  struct sl_context* ctx;
  struct wl_resource *resource;
  struct wl_subsurface *proxy;
};

struct sl_config {
  uint32_t serial;
  uint32_t mask;
  uint32_t values[5];
  uint32_t states_length;
  uint32_t states[3];
};

struct sl_window {
  struct sl_context* ctx;
  xcb_window_t id;
  xcb_window_t frame_id;
  uint32_t host_surface_id;
  int unpaired;
  int x;
  int y;
  int width;
  int height;
  int border_width;
  int depth;
  int managed;
  int realized;
  int activated;
  int allow_resize;
  xcb_window_t transient_for;
  xcb_window_t client_leader;
  int decorated;
  char *name;
  char *clazz;
  char *startup_id;
  uint32_t size_flags;
  int min_width;
  int min_height;
  int max_width;
  int max_height;
  struct sl_config next_config;
  struct sl_config pending_config;
  struct zxdg_surface_v6 *xdg_surface;
  struct zxdg_toplevel_v6 *xdg_toplevel;
  struct zxdg_popup_v6 *xdg_popup;
  struct zaura_surface *aura_surface;
  struct wl_list link;
};

enum {
  PROPERTY_WM_NAME,
  PROPERTY_WM_CLASS,
  PROPERTY_WM_TRANSIENT_FOR,
  PROPERTY_WM_NORMAL_HINTS,
  PROPERTY_WM_CLIENT_LEADER,
  PROPERTY_MOTIF_WM_HINTS,
  PROPERTY_NET_STARTUP_ID,
};

enum {
  SHM_DRIVER_NOOP,
  SHM_DRIVER_DMABUF,
  SHM_DRIVER_VIRTWL,
  SHM_DRIVER_VIRTWL_DMABUF,
};

#define US_POSITION (1L << 0)
#define US_SIZE (1L << 1)
#define P_POSITION (1L << 2)
#define P_SIZE (1L << 3)
#define P_MIN_SIZE (1L << 4)
#define P_MAX_SIZE (1L << 5)
#define P_RESIZE_INC (1L << 6)
#define P_ASPECT (1L << 7)
#define P_BASE_SIZE (1L << 8)
#define P_WIN_GRAVITY (1L << 9)

struct sl_wm_size_hints {
  uint32_t flags;
  int32_t x, y;
  int32_t width, height;
  int32_t min_width, min_height;
  int32_t max_width, max_height;
  int32_t width_inc, height_inc;
  struct {
    int32_t x;
    int32_t y;
  } min_aspect, max_aspect;
  int32_t base_width, base_height;
  int32_t win_gravity;
};

#define MWM_HINTS_FUNCTIONS (1L << 0)
#define MWM_HINTS_DECORATIONS (1L << 1)
#define MWM_HINTS_INPUT_MODE (1L << 2)
#define MWM_HINTS_STATUS (1L << 3)

#define MWM_DECOR_ALL (1L << 0)
#define MWM_DECOR_BORDER (1L << 1)
#define MWM_DECOR_RESIZEH (1L << 2)
#define MWM_DECOR_TITLE (1L << 3)
#define MWM_DECOR_MENU (1L << 4)
#define MWM_DECOR_MINIMIZE (1L << 5)
#define MWM_DECOR_MAXIMIZE (1L << 6)

struct sl_mwm_hints {
  uint32_t flags;
  uint32_t functions;
  uint32_t decorations;
  int32_t input_mode;
  uint32_t status;
};

#define NET_WM_MOVERESIZE_SIZE_TOPLEFT 0
#define NET_WM_MOVERESIZE_SIZE_TOP 1
#define NET_WM_MOVERESIZE_SIZE_TOPRIGHT 2
#define NET_WM_MOVERESIZE_SIZE_RIGHT 3
#define NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define NET_WM_MOVERESIZE_SIZE_BOTTOM 5
#define NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT 6
#define NET_WM_MOVERESIZE_SIZE_LEFT 7
#define NET_WM_MOVERESIZE_MOVE 8

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD 1
#define NET_WM_STATE_TOGGLE 2

#define WM_STATE_WITHDRAWN 0
#define WM_STATE_NORMAL 1
#define WM_STATE_ICONIC 3

#define SEND_EVENT_MASK 0x80

#define MIN_SCALE 0.1
#define MAX_SCALE 10.0

#define INCH_IN_MM 25.4

#define MIN_DPI 72
#define MAX_DPI 9600

#define XCURSOR_SIZE_BASE 24

#define MAX_OUTPUT_SCALE 2

#define MIN_SIZE (INT_MIN / 10)
#define MAX_SIZE (INT_MAX / 10)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#define LOCK_SUFFIX ".lock"
#define LOCK_SUFFIXLEN 5

#define APPLICATION_ID_FORMAT_PREFIX "org.chromium.termina"
#define XID_APPLICATION_ID_FORMAT APPLICATION_ID_FORMAT_PREFIX ".xid.%d"
#define WM_CLIENT_LEADER_APPLICATION_ID_FORMAT \
  APPLICATION_ID_FORMAT_PREFIX ".wmclientleader.%d"
#define WM_CLASS_APPLICATION_ID_FORMAT \
  APPLICATION_ID_FORMAT_PREFIX ".wmclass.%s"

#define MIN_AURA_SHELL_VERSION 6

struct dma_buf_sync {
  __u64 flags;
};

#define DMA_BUF_SYNC_READ (1 << 0)
#define DMA_BUF_SYNC_WRITE (2 << 0)
#define DMA_BUF_SYNC_RW (DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END (1 << 2)

#define DMA_BUF_BASE 'b'
#define DMA_BUF_IOCTL_SYNC _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

static void sl_dmabuf_sync(int fd, __u64 flags) {
  struct dma_buf_sync sync = {0};
  int rv;

  sync.flags = flags;
  do {
    rv = ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
  } while (rv == -1 && errno == EINTR);
}

static void sl_dmabuf_begin_access(int fd) {
  sl_dmabuf_sync(fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW);
}

static void sl_dmabuf_end_access(int fd) {
  sl_dmabuf_sync(fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW);
}

static struct sl_mmap* sl_mmap_create(
    int fd, size_t size, size_t offset, size_t stride, size_t bpp) {
  struct sl_mmap* map;

  map = malloc(sizeof(*map));
  map->refcount = 1;
  map->fd = fd;
  map->size = size;
  map->offset = offset;
  map->stride = stride;
  map->bpp = bpp;
  map->begin_access = NULL;
  map->end_access = NULL;
  map->buffer_resource = NULL;
  map->addr =
      mmap(NULL, size + offset, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  assert(map->addr != MAP_FAILED);

  return map;
}

static struct sl_mmap* sl_mmap_ref(struct sl_mmap* map) {
  map->refcount++;
  return map;
}

static void sl_mmap_unref(struct sl_mmap* map) {
  if (map->refcount-- == 1) {
    munmap(map->addr, map->size + map->offset);
    close(map->fd);
    free(map);
  }
}

static void sl_output_buffer_destroy(struct sl_output_buffer* buffer) {
  wl_buffer_destroy(buffer->internal);
  sl_mmap_unref(buffer->mmap);
  pixman_region32_fini(&buffer->damage);
  wl_list_remove(&buffer->link);
}

static void sl_output_buffer_release(void* data, struct wl_buffer* buffer) {
  struct sl_output_buffer* output_buffer = wl_buffer_get_user_data(buffer);
  struct sl_host_surface* host_surface = output_buffer->surface;

  wl_list_remove(&output_buffer->link);
  wl_list_insert(&host_surface->released_buffers, &output_buffer->link);
}

static const struct wl_buffer_listener sl_output_buffer_listener = {
    sl_output_buffer_release};

static void sl_internal_xdg_shell_ping(void* data,
                                       struct zxdg_shell_v6* xdg_shell,
                                       uint32_t serial) {
  zxdg_shell_v6_pong(xdg_shell, serial);
}

static const struct zxdg_shell_v6_listener sl_internal_xdg_shell_listener = {
    sl_internal_xdg_shell_ping};

static void sl_send_configure_notify(struct sl_window* window) {
  xcb_configure_notify_event_t event = {
      .response_type = XCB_CONFIGURE_NOTIFY,
      .event = window->id,
      .window = window->id,
      .above_sibling = XCB_WINDOW_NONE,
      .x = window->x,
      .y = window->y,
      .width = window->width,
      .height = window->height,
      .border_width = window->border_width,
      .override_redirect = 0,
      .pad0 = 0,
      .pad1 = 0,
  };

  xcb_send_event(window->ctx->connection, 0, window->id,
                 XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char*)&event);
}

static void sl_adjust_window_size_for_screen_size(struct sl_window* window) {
  struct sl_context* ctx = window->ctx;

  // Clamp size to screen.
  window->width = MIN(window->width, ctx->screen->width_in_pixels);
  window->height = MIN(window->height, ctx->screen->height_in_pixels);
}

static void sl_adjust_window_position_for_screen_size(
    struct sl_window* window) {
  struct sl_context* ctx = window->ctx;

  // Center horizontally/vertically.
  window->x = ctx->screen->width_in_pixels / 2 - window->width / 2;
  window->y = ctx->screen->height_in_pixels / 2 - window->height / 2;
}

static void sl_configure_window(struct sl_window* window) {
  assert(!window->pending_config.serial);

  if (window->next_config.mask) {
    int values[5];
    int x = window->x;
    int y = window->y;
    int i = 0;

    xcb_configure_window(window->ctx->connection, window->frame_id,
                         window->next_config.mask, window->next_config.values);

    if (window->next_config.mask & XCB_CONFIG_WINDOW_X)
      x = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_Y)
      y = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_WIDTH)
      window->width = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_HEIGHT)
      window->height = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
      window->border_width = window->next_config.values[i++];

    // Set x/y to origin in case window gravity is not northwest as expected.
    assert(window->managed);
    values[0] = 0;
    values[1] = 0;
    values[2] = window->width;
    values[3] = window->height;
    values[4] = window->border_width;
    xcb_configure_window(
        window->ctx->connection, window->id,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
        values);

    if (x != window->x || y != window->y) {
      window->x = x;
      window->y = y;
      sl_send_configure_notify(window);
    }
  }

  if (window->managed) {
    xcb_change_property(window->ctx->connection, XCB_PROP_MODE_REPLACE,
                        window->id, window->ctx->atoms[ATOM_NET_WM_STATE].value,
                        XCB_ATOM_ATOM, 32, window->next_config.states_length,
                        window->next_config.states);
  }

  window->pending_config = window->next_config;
  window->next_config.serial = 0;
  window->next_config.mask = 0;
  window->next_config.states_length = 0;
}

static void sl_set_input_focus(struct sl_context* ctx,
                               struct sl_window* window) {
  if (window) {
    xcb_client_message_event_t event = {
        .response_type = XCB_CLIENT_MESSAGE,
        .format = 32,
        .window = window->id,
        .type = ctx->atoms[ATOM_WM_PROTOCOLS].value,
        .data.data32 =
            {
                ctx->atoms[ATOM_WM_TAKE_FOCUS].value, XCB_CURRENT_TIME,
            },
    };

    if (!window->managed)
      return;

    xcb_send_event(ctx->connection, 0, window->id,
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (char*)&event);

    xcb_set_input_focus(ctx->connection, XCB_INPUT_FOCUS_NONE, window->id,
                        XCB_CURRENT_TIME);
  } else {
    xcb_set_input_focus(ctx->connection, XCB_INPUT_FOCUS_NONE, XCB_NONE,
                        XCB_CURRENT_TIME);
  }
}

void sl_restack_windows(struct sl_context* ctx, uint32_t focus_resource_id) {
  struct sl_window* sibling;
  uint32_t values[1];

  wl_list_for_each(sibling, &ctx->windows, link) {
    if (!sibling->managed)
      continue;

    // Move focus window to the top and all other windows to the bottom.
    values[0] = sibling->host_surface_id == focus_resource_id
                    ? XCB_STACK_MODE_ABOVE
                    : XCB_STACK_MODE_BELOW;
    xcb_configure_window(ctx->connection, sibling->frame_id,
                         XCB_CONFIG_WINDOW_STACK_MODE, values);
  }
}

void sl_roundtrip(struct sl_context* ctx) {
  free(xcb_get_input_focus_reply(ctx->connection,
                                 xcb_get_input_focus(ctx->connection), NULL));
}

static int sl_process_pending_configure_acks(
    struct sl_window* window, struct sl_host_surface* host_surface) {
  if (!window->pending_config.serial)
    return 0;

  if (window->managed && host_surface) {
    int width = window->width + window->border_width * 2;
    int height = window->height + window->border_width * 2;
    // Early out if we expect contents to match window size at some point in
    // the future.
    if (width != host_surface->contents_width ||
        height != host_surface->contents_height) {
      return 0;
    }
  }

  if (window->xdg_surface) {
    zxdg_surface_v6_ack_configure(window->xdg_surface,
                                  window->pending_config.serial);
  }
  window->pending_config.serial = 0;

  if (window->next_config.serial)
    sl_configure_window(window);

  return 1;
}

static void sl_internal_xdg_surface_configure(
    void* data, struct zxdg_surface_v6* xdg_surface, uint32_t serial) {
  struct sl_window* window = zxdg_surface_v6_get_user_data(xdg_surface);

  window->next_config.serial = serial;
  if (!window->pending_config.serial) {
    struct wl_resource *host_resource;
    struct sl_host_surface* host_surface = NULL;

    host_resource =
        wl_client_get_object(window->ctx->client, window->host_surface_id);
    if (host_resource)
      host_surface = wl_resource_get_user_data(host_resource);

    sl_configure_window(window);

    if (sl_process_pending_configure_acks(window, host_surface)) {
      if (host_surface)
        wl_surface_commit(host_surface->proxy);
    }
  }
}

static const struct zxdg_surface_v6_listener sl_internal_xdg_surface_listener =
    {sl_internal_xdg_surface_configure};

static void sl_internal_xdg_toplevel_configure(
    void* data,
    struct zxdg_toplevel_v6* xdg_toplevel,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  struct sl_window* window = zxdg_toplevel_v6_get_user_data(xdg_toplevel);
  int activated = 0;
  uint32_t *state;
  int i = 0;

  if (!window->managed)
    return;

  if (width && height) {
    int32_t width_in_pixels = width * window->ctx->scale;
    int32_t height_in_pixels = height * window->ctx->scale;
    int i = 0;

    window->next_config.mask = XCB_CONFIG_WINDOW_WIDTH |
                               XCB_CONFIG_WINDOW_HEIGHT |
                               XCB_CONFIG_WINDOW_BORDER_WIDTH;
    if (!(window->size_flags & (US_POSITION | P_POSITION))) {
      window->next_config.mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
      window->next_config.values[i++] =
          window->ctx->screen->width_in_pixels / 2 - width_in_pixels / 2;
      window->next_config.values[i++] =
          window->ctx->screen->height_in_pixels / 2 - height_in_pixels / 2;
    }
    window->next_config.values[i++] = width_in_pixels;
    window->next_config.values[i++] = height_in_pixels;
    window->next_config.values[i++] = 0;
  }

  window->allow_resize = 1;
  wl_array_for_each(state, states) {
    if (*state == ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN) {
      window->allow_resize = 0;
      window->next_config.states[i++] =
          window->ctx->atoms[ATOM_NET_WM_STATE_FULLSCREEN].value;
    }
    if (*state == ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED) {
      window->allow_resize = 0;
      window->next_config.states[i++] =
          window->ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT].value;
      window->next_config.states[i++] =
          window->ctx->atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ].value;
    }
    if (*state == ZXDG_TOPLEVEL_V6_STATE_ACTIVATED)
      activated = 1;
    if (*state == ZXDG_TOPLEVEL_V6_STATE_RESIZING)
      window->allow_resize = 0;
  }

  if (activated != window->activated) {
    if (activated != (window->ctx->host_focus_window == window)) {
      window->ctx->host_focus_window = activated ? window : NULL;
      window->ctx->needs_set_input_focus = 1;
    }
    window->activated = activated;
  }

  window->next_config.states_length = i;
}

static void sl_internal_xdg_toplevel_close(
    void* data, struct zxdg_toplevel_v6* xdg_toplevel) {
  struct sl_window* window = zxdg_toplevel_v6_get_user_data(xdg_toplevel);
  xcb_client_message_event_t event = {
      .response_type = XCB_CLIENT_MESSAGE,
      .format = 32,
      .window = window->id,
      .type = window->ctx->atoms[ATOM_WM_PROTOCOLS].value,
      .data.data32 =
          {
              window->ctx->atoms[ATOM_WM_DELETE_WINDOW].value, XCB_CURRENT_TIME,
          },
  };

  xcb_send_event(window->ctx->connection, 0, window->id,
                 XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
}

static const struct zxdg_toplevel_v6_listener
    sl_internal_xdg_toplevel_listener = {sl_internal_xdg_toplevel_configure,
                                         sl_internal_xdg_toplevel_close};

static void sl_internal_xdg_popup_configure(void* data,
                                            struct zxdg_popup_v6* xdg_popup,
                                            int32_t x,
                                            int32_t y,
                                            int32_t width,
                                            int32_t height) {}

static void sl_internal_xdg_popup_done(void* data,
                                       struct zxdg_popup_v6* zxdg_popup_v6) {}

static const struct zxdg_popup_v6_listener sl_internal_xdg_popup_listener = {
    sl_internal_xdg_popup_configure, sl_internal_xdg_popup_done};

static void sl_window_set_wm_state(struct sl_window* window, int state) {
  struct sl_context* ctx = window->ctx;
  uint32_t values[2];

  values[0] = state;
  values[1] = XCB_WINDOW_NONE;

  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, window->id,
                      ctx->atoms[ATOM_WM_STATE].value,
                      ctx->atoms[ATOM_WM_STATE].value, 32, 2, values);
}

static void sl_window_update(struct sl_window* window) {
  struct wl_resource *host_resource = NULL;
  struct sl_host_surface* host_surface;
  struct sl_context* ctx = window->ctx;
  struct sl_window* parent = NULL;

  if (window->host_surface_id) {
    host_resource = wl_client_get_object(ctx->client, window->host_surface_id);
    if (host_resource && window->unpaired) {
      wl_list_remove(&window->link);
      wl_list_insert(&ctx->windows, &window->link);
      window->unpaired = 0;
    }
  } else if (!window->unpaired) {
    wl_list_remove(&window->link);
    wl_list_insert(&ctx->unpaired_windows, &window->link);
    window->unpaired = 1;
  }

  if (!host_resource) {
    if (window->aura_surface) {
      zaura_surface_destroy(window->aura_surface);
      window->aura_surface = NULL;
    }
    if (window->xdg_toplevel) {
      zxdg_toplevel_v6_destroy(window->xdg_toplevel);
      window->xdg_toplevel = NULL;
    }
    if (window->xdg_popup) {
      zxdg_popup_v6_destroy(window->xdg_popup);
      window->xdg_popup = NULL;
    }
    if (window->xdg_surface) {
      zxdg_surface_v6_destroy(window->xdg_surface);
      window->xdg_surface = NULL;
    }
    window->realized = 0;
    return;
  }

  host_surface = wl_resource_get_user_data(host_resource);
  assert(host_surface);
  assert(!host_surface->has_role);

  assert(ctx->xdg_shell);
  assert(ctx->xdg_shell->internal);

  if (window->managed) {
    if (window->transient_for != XCB_WINDOW_NONE) {
      struct sl_window* sibling;

      wl_list_for_each(sibling, &ctx->windows, link) {
        if (sibling->id == window->transient_for) {
          if (sibling->xdg_toplevel)
            parent = sibling;
          break;
        }
      }
    }
  } else {
    struct sl_window* sibling;
    uint32_t parent_last_event_serial = 0;

    wl_list_for_each(sibling, &ctx->windows, link) {
      struct wl_resource *sibling_host_resource;
      struct sl_host_surface* sibling_host_surface;

      if (!sibling->realized)
        continue;

      sibling_host_resource =
          wl_client_get_object(ctx->client, sibling->host_surface_id);
      if (!sibling_host_resource)
        continue;

      // Any parent will do but prefer last event window.
      sibling_host_surface = wl_resource_get_user_data(sibling_host_resource);
      if (parent_last_event_serial > sibling_host_surface->last_event_serial)
        continue;

      parent = sibling;
      parent_last_event_serial = sibling_host_surface->last_event_serial;
    }
  }

  if (!window->depth) {
    xcb_get_geometry_reply_t* geometry_reply = xcb_get_geometry_reply(
        ctx->connection, xcb_get_geometry(ctx->connection, window->id), NULL);
    if (geometry_reply) {
      window->depth = geometry_reply->depth;
      free(geometry_reply);
    }
  }

  if (!window->xdg_surface) {
    window->xdg_surface = zxdg_shell_v6_get_xdg_surface(
        ctx->xdg_shell->internal, host_surface->proxy);
    zxdg_surface_v6_set_user_data(window->xdg_surface, window);
    zxdg_surface_v6_add_listener(window->xdg_surface,
                                 &sl_internal_xdg_surface_listener, window);
  }

  if (ctx->aura_shell) {
    if (!window->aura_surface) {
      window->aura_surface = zaura_shell_get_aura_surface(
          ctx->aura_shell->internal, host_surface->proxy);
    }
    zaura_surface_set_frame(window->aura_surface,
                            window->decorated
                                ? ZAURA_SURFACE_FRAME_TYPE_NORMAL
                                : window->depth == 32
                                      ? ZAURA_SURFACE_FRAME_TYPE_NONE
                                      : ZAURA_SURFACE_FRAME_TYPE_SHADOW);

    if (ctx->has_frame_color) {
      zaura_surface_set_frame_colors(window->aura_surface, ctx->frame_color,
                                     ctx->frame_color);
    }

    zaura_surface_set_startup_id(window->aura_surface, window->startup_id);

    if (ctx->application_id) {
      zaura_surface_set_application_id(window->aura_surface,
                                       ctx->application_id);
    } else {
      char application_id_str[128];

      if (window->clazz) {
        snprintf(application_id_str, sizeof(application_id_str),
                 WM_CLASS_APPLICATION_ID_FORMAT, window->clazz);
      } else if (window->client_leader != XCB_WINDOW_NONE) {
        snprintf(application_id_str, sizeof(application_id_str),
                 WM_CLIENT_LEADER_APPLICATION_ID_FORMAT, window->client_leader);
      } else {
        snprintf(application_id_str, sizeof(application_id_str),
                 XID_APPLICATION_ID_FORMAT, window->id);
      }

      zaura_surface_set_application_id(window->aura_surface,
                                       application_id_str);
    }
  }

  if (window->managed || !parent) {
    if (!window->xdg_toplevel) {
      window->xdg_toplevel = zxdg_surface_v6_get_toplevel(window->xdg_surface);
      zxdg_toplevel_v6_set_user_data(window->xdg_toplevel, window);
      zxdg_toplevel_v6_add_listener(window->xdg_toplevel,
                                    &sl_internal_xdg_toplevel_listener, window);
    }
    if (parent)
      zxdg_toplevel_v6_set_parent(window->xdg_toplevel, parent->xdg_toplevel);
    if (window->name)
      zxdg_toplevel_v6_set_title(window->xdg_toplevel, window->name);
    if (window->size_flags & P_MIN_SIZE) {
      zxdg_toplevel_v6_set_min_size(window->xdg_toplevel,
                                    window->min_width / ctx->scale,
                                    window->min_height / ctx->scale);
    }
    if (window->size_flags & P_MAX_SIZE) {
      zxdg_toplevel_v6_set_max_size(window->xdg_toplevel,
                                    window->max_width / ctx->scale,
                                    window->max_height / ctx->scale);
    }
  } else if (!window->xdg_popup) {
    struct zxdg_positioner_v6 *positioner;

    positioner = zxdg_shell_v6_create_positioner(ctx->xdg_shell->internal);
    assert(positioner);
    zxdg_positioner_v6_set_anchor(positioner,
                                  ZXDG_POSITIONER_V6_ANCHOR_TOP |
                                      ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    zxdg_positioner_v6_set_gravity(positioner,
                                   ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                                       ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
    zxdg_positioner_v6_set_anchor_rect(
        positioner, (window->x - parent->x) / ctx->scale,
        (window->y - parent->y) / ctx->scale, 1, 1);

    window->xdg_popup = zxdg_surface_v6_get_popup(
        window->xdg_surface, parent->xdg_surface, positioner);
    zxdg_popup_v6_set_user_data(window->xdg_popup, window);
    zxdg_popup_v6_add_listener(window->xdg_popup,
                               &sl_internal_xdg_popup_listener, window);

    zxdg_positioner_v6_destroy(positioner);
  }

  if ((window->size_flags & (US_POSITION | P_POSITION)) && parent &&
      ctx->aura_shell) {
    zaura_surface_set_parent(window->aura_surface, parent->aura_surface,
                             (window->x - parent->x) / ctx->scale,
                             (window->y - parent->y) / ctx->scale);
  }

  wl_surface_commit(host_surface->proxy);
  if (host_surface->contents_width && host_surface->contents_height)
    window->realized = 1;
}

static size_t sl_bpp_for_shm_format(uint32_t format) {
  switch (format) {
  case WL_SHM_FORMAT_RGB565:
    return 2;
  case WL_SHM_FORMAT_ARGB8888:
  case WL_SHM_FORMAT_ABGR8888:
  case WL_SHM_FORMAT_XRGB8888:
  case WL_SHM_FORMAT_XBGR8888:
    return 4;
  }
  assert(0);
  return 0;
}

static uint32_t sl_gbm_format_for_shm_format(uint32_t format) {
  switch (format) {
  case WL_SHM_FORMAT_RGB565:
    return GBM_FORMAT_RGB565;
  case WL_SHM_FORMAT_ARGB8888:
    return GBM_FORMAT_ARGB8888;
  case WL_SHM_FORMAT_ABGR8888:
    return GBM_FORMAT_ABGR8888;
  case WL_SHM_FORMAT_XRGB8888:
    return GBM_FORMAT_XRGB8888;
  case WL_SHM_FORMAT_XBGR8888:
    return GBM_FORMAT_XBGR8888;
  }
  assert(0);
  return 0;
}

static uint32_t sl_drm_format_for_shm_format(int format) {
  switch (format) {
  case WL_SHM_FORMAT_RGB565:
    return WL_DRM_FORMAT_RGB565;
  case WL_SHM_FORMAT_ARGB8888:
    return WL_DRM_FORMAT_ARGB8888;
  case WL_SHM_FORMAT_ABGR8888:
    return WL_DRM_FORMAT_ABGR8888;
  case WL_SHM_FORMAT_XRGB8888:
    return WL_DRM_FORMAT_XRGB8888;
  case WL_SHM_FORMAT_XBGR8888:
    return WL_DRM_FORMAT_XBGR8888;
  }
  assert(0);
  return 0;
}

static void sl_host_surface_destroy(struct wl_client* client,
                                    struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_host_surface_attach(struct wl_client* client,
                                   struct wl_resource* resource,
                                   struct wl_resource* buffer_resource,
                                   int32_t x,
                                   int32_t y) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_buffer* host_buffer =
      buffer_resource ? wl_resource_get_user_data(buffer_resource) : NULL;
  struct wl_buffer *buffer_proxy = NULL;
  struct sl_window* window;
  double scale = host->ctx->scale;

  host->current_buffer = NULL;
  if (host->contents_shm_mmap) {
    sl_mmap_unref(host->contents_shm_mmap);
    host->contents_shm_mmap = NULL;
  }

  if (host_buffer) {
    host->contents_width = host_buffer->width;
    host->contents_height = host_buffer->height;
    buffer_proxy = host_buffer->proxy;
    if (host_buffer->shm_mmap)
      host->contents_shm_mmap = sl_mmap_ref(host_buffer->shm_mmap);
  }

  if (host->contents_shm_mmap) {
    while (!wl_list_empty(&host->released_buffers)) {
      host->current_buffer = wl_container_of(host->released_buffers.next,
                                             host->current_buffer, link);

      if (host->current_buffer->width == host_buffer->width &&
          host->current_buffer->height == host_buffer->height &&
          host->current_buffer->format == host_buffer->shm_format) {
        break;
      }

      sl_output_buffer_destroy(host->current_buffer);
      host->current_buffer = NULL;
    }

    // Allocate new output buffer.
    if (!host->current_buffer) {
      size_t width = host_buffer->width;
      size_t height = host_buffer->height;
      uint32_t shm_format = host_buffer->shm_format;
      size_t bpp = sl_bpp_for_shm_format(shm_format);

      host->current_buffer = malloc(sizeof(struct sl_output_buffer));
      assert(host->current_buffer);
      wl_list_insert(&host->released_buffers, &host->current_buffer->link);
      host->current_buffer->width = width;
      host->current_buffer->height = height;
      host->current_buffer->format = shm_format;
      host->current_buffer->surface = host;
      pixman_region32_init_rect(&host->current_buffer->damage, 0, 0, MAX_SIZE,
                                MAX_SIZE);

      switch (host->ctx->shm_driver) {
        case SHM_DRIVER_DMABUF: {
          struct zwp_linux_buffer_params_v1* buffer_params;
          struct gbm_bo* bo;
          int stride0;
          int fd;

          bo = gbm_bo_create(host->ctx->gbm, width, height,
                             sl_gbm_format_for_shm_format(shm_format),
                             GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
          stride0 = gbm_bo_get_stride(bo);
          fd = gbm_bo_get_fd(bo);

          buffer_params = zwp_linux_dmabuf_v1_create_params(
              host->ctx->linux_dmabuf->internal);
          zwp_linux_buffer_params_v1_add(buffer_params, fd, 0, 0, stride0, 0,
                                         0);
          host->current_buffer->internal =
              zwp_linux_buffer_params_v1_create_immed(
                  buffer_params, width, height,
                  sl_drm_format_for_shm_format(shm_format), 0);
          zwp_linux_buffer_params_v1_destroy(buffer_params);

          host->current_buffer->mmap =
              sl_mmap_create(fd, height * stride0, 0, stride0, bpp);
          host->current_buffer->mmap->begin_access = sl_dmabuf_begin_access;
          host->current_buffer->mmap->end_access = sl_dmabuf_end_access;

          gbm_bo_destroy(bo);
        } break;
        case SHM_DRIVER_VIRTWL: {
          size_t size = host_buffer->shm_mmap->size;
          struct virtwl_ioctl_new ioctl_new = {.type = VIRTWL_IOCTL_NEW_ALLOC,
                                               .fd = -1,
                                               .flags = 0,
                                               .size = size};
          struct wl_shm_pool* pool;
          int rv;

          rv = ioctl(host->ctx->virtwl_fd, VIRTWL_IOCTL_NEW, &ioctl_new);
          assert(rv == 0);
          UNUSED(rv);

          pool =
              wl_shm_create_pool(host->ctx->shm->internal, ioctl_new.fd, size);
          host->current_buffer->internal = wl_shm_pool_create_buffer(
              pool, 0, width, height, host_buffer->shm_mmap->stride,
              shm_format);
          wl_shm_pool_destroy(pool);

          host->current_buffer->mmap = sl_mmap_create(
              ioctl_new.fd, size, 0, host_buffer->shm_mmap->stride, bpp);
        } break;
        case SHM_DRIVER_VIRTWL_DMABUF: {
          uint32_t drm_format = sl_drm_format_for_shm_format(shm_format);
          struct virtwl_ioctl_new ioctl_new = {
              .type = VIRTWL_IOCTL_NEW_DMABUF,
              .fd = -1,
              .flags = 0,
              .dmabuf = {
                  .width = width, .height = height, .format = drm_format}};
          struct zwp_linux_buffer_params_v1* buffer_params;
          int rv;

          rv = ioctl(host->ctx->virtwl_fd, VIRTWL_IOCTL_NEW, &ioctl_new);
          if (rv) {
            fprintf(stderr, "error: virtwl dmabuf allocation failed: %s\n",
                    strerror(errno));
            _exit(EXIT_FAILURE);
          }

          buffer_params = zwp_linux_dmabuf_v1_create_params(
              host->ctx->linux_dmabuf->internal);
          zwp_linux_buffer_params_v1_add(buffer_params, ioctl_new.fd, 0, 0,
                                         ioctl_new.dmabuf.stride0, 0, 0);
          host->current_buffer->internal =
              zwp_linux_buffer_params_v1_create_immed(buffer_params, width,
                                                      height, drm_format, 0);
          zwp_linux_buffer_params_v1_destroy(buffer_params);

          host->current_buffer->mmap =
              sl_mmap_create(ioctl_new.fd, ioctl_new.dmabuf.stride0 * height, 0,
                             ioctl_new.dmabuf.stride0, bpp);
        } break;
      }

      assert(host->current_buffer->internal);
      assert(host->current_buffer->mmap);

      wl_buffer_set_user_data(host->current_buffer->internal,
                              host->current_buffer);
      wl_buffer_add_listener(host->current_buffer->internal,
                             &sl_output_buffer_listener, host->current_buffer);
    }
  }

  x /= scale;
  y /= scale;

  if (host->current_buffer) {
    assert(host->current_buffer->internal);
    wl_surface_attach(host->proxy, host->current_buffer->internal, x, y);
  } else {
    wl_surface_attach(host->proxy, buffer_proxy, x, y);
  }

  wl_list_for_each(window, &host->ctx->windows, link) {
    if (window->host_surface_id == wl_resource_get_id(resource)) {
      while (sl_process_pending_configure_acks(window, host))
        continue;

      break;
    }
  }
}

static void sl_host_surface_damage(struct wl_client* client,
                                   struct wl_resource* resource,
                                   int32_t x,
                                   int32_t y,
                                   int32_t width,
                                   int32_t height) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);
  double scale = host->ctx->scale;
  struct sl_output_buffer* buffer;
  int64_t x1, y1, x2, y2;

  wl_list_for_each(buffer, &host->busy_buffers, link) {
    pixman_region32_union_rect(&buffer->damage, &buffer->damage, x, y, width,
                               height);
  }
  wl_list_for_each(buffer, &host->released_buffers, link) {
    pixman_region32_union_rect(&buffer->damage, &buffer->damage, x, y, width,
                               height);
  }

  x1 = x;
  y1 = y;
  x2 = x1 + width;
  y2 = y1 + height;

  // Enclosing rect after scaling and outset by one pixel to account for
  // potential filtering.
  x1 = MAX(MIN_SIZE, x1 - 1) / scale;
  y1 = MAX(MIN_SIZE, y1 - 1) / scale;
  x2 = ceil(MIN(x2 + 1, MAX_SIZE) / scale);
  y2 = ceil(MIN(y2 + 1, MAX_SIZE) / scale);

  wl_surface_damage(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void sl_frame_callback_done(void* data,
                                   struct wl_callback* callback,
                                   uint32_t time) {
  struct sl_host_callback* host = wl_callback_get_user_data(callback);

  wl_callback_send_done(host->resource, time);
  wl_resource_destroy(host->resource);
}

static const struct wl_callback_listener sl_frame_callback_listener = {
    sl_frame_callback_done};

static void sl_host_callback_destroy(struct wl_resource* resource) {
  struct sl_host_callback* host = wl_resource_get_user_data(resource);

  wl_callback_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_host_surface_frame(struct wl_client* client,
                                  struct wl_resource* resource,
                                  uint32_t callback) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_callback* host_callback;

  host_callback = malloc(sizeof(*host_callback));
  assert(host_callback);

  host_callback->resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);
  wl_resource_set_implementation(host_callback->resource, NULL, host_callback,
                                 sl_host_callback_destroy);
  host_callback->proxy = wl_surface_frame(host->proxy);
  wl_callback_set_user_data(host_callback->proxy, host_callback);
  wl_callback_add_listener(host_callback->proxy, &sl_frame_callback_listener,
                           host_callback);
}

static void sl_host_surface_set_opaque_region(
    struct wl_client* client,
    struct wl_resource* resource,
    struct wl_resource* region_resource) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_region* host_region =
      region_resource ? wl_resource_get_user_data(region_resource) : NULL;

  wl_surface_set_opaque_region(host->proxy,
                               host_region ? host_region->proxy : NULL);
}

static void sl_host_surface_set_input_region(
    struct wl_client* client,
    struct wl_resource* resource,
    struct wl_resource* region_resource) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_region* host_region =
      region_resource ? wl_resource_get_user_data(region_resource) : NULL;

  wl_surface_set_input_region(host->proxy,
                              host_region ? host_region->proxy : NULL);
}

static void sl_host_surface_commit(struct wl_client* client,
                                   struct wl_resource* resource) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);
  struct sl_viewport* viewport = NULL;
  struct sl_window* window;

  if (!wl_list_empty(&host->contents_viewport))
    viewport = wl_container_of(host->contents_viewport.next, viewport, link);

  if (host->contents_shm_mmap) {
    uint8_t *src_base =
        host->contents_shm_mmap->addr + host->contents_shm_mmap->offset;
    uint8_t *dst_base =
        host->current_buffer->mmap->addr + host->current_buffer->mmap->offset;
    size_t src_stride = host->contents_shm_mmap->stride;
    size_t dst_stride = host->current_buffer->mmap->stride;
    size_t bpp = host->contents_shm_mmap->bpp;
    double contents_scale_x = host->contents_scale;
    double contents_scale_y = host->contents_scale;
    double contents_offset_x = 0.0;
    double contents_offset_y = 0.0;
    pixman_box32_t *rect;
    int n;

    // Determine scale and offset for damage based on current viewport.
    if (viewport) {
      double contents_width = host->contents_width;
      double contents_height = host->contents_height;

      if (viewport->src_x >= 0 && viewport->src_y >= 0) {
        contents_offset_x = wl_fixed_to_double(viewport->src_x);
        contents_offset_y = wl_fixed_to_double(viewport->src_y);
      }

      if (viewport->dst_width > 0 && viewport->dst_height > 0) {
        contents_scale_x *= contents_width / viewport->dst_width;
        contents_scale_y *= contents_height / viewport->dst_height;

        // Take source rectangle into account when both destionation size and
        // source rectangle are set. If only source rectangle is set, then
        // it determines the surface size so it can be ignored.
        if (viewport->src_width >= 0 && viewport->src_height >= 0) {
          contents_scale_x *=
              wl_fixed_to_double(viewport->src_width) / contents_width;
          contents_scale_y *=
              wl_fixed_to_double(viewport->src_height) / contents_height;
        }
      }
    }

    if (host->current_buffer->mmap->begin_access)
      host->current_buffer->mmap->begin_access(host->current_buffer->mmap->fd);

    rect = pixman_region32_rectangles(&host->current_buffer->damage, &n);
    while (n--) {
      int32_t x1, y1, x2, y2;

      // Enclosing rect after applying scale and offset.
      x1 = rect->x1 * contents_scale_x + contents_offset_x;
      y1 = rect->y1 * contents_scale_y + contents_offset_y;
      x2 = rect->x2 * contents_scale_x + contents_offset_x + 0.5;
      y2 = rect->y2 * contents_scale_y + contents_offset_y + 0.5;

      x1 = MAX(0, x1);
      y1 = MAX(0, y1);
      x2 = MIN(host->contents_width, x2);
      y2 = MIN(host->contents_height, y2);

      if (x1 < x2 && y1 < y2) {
        uint8_t *src = src_base + y1 * src_stride + x1 * bpp;
        uint8_t *dst = dst_base + y1 * dst_stride + x1 * bpp;
        int32_t width = x2 - x1;
        int32_t height = y2 - y1;
        size_t bytes = width * bpp;

        while (height--) {
          memcpy(dst, src, bytes);
          dst += dst_stride;
          src += src_stride;
        }
      }

      ++rect;
    }

    if (host->current_buffer->mmap->end_access)
      host->current_buffer->mmap->end_access(host->current_buffer->mmap->fd);

    pixman_region32_clear(&host->current_buffer->damage);

    wl_list_remove(&host->current_buffer->link);
    wl_list_insert(&host->busy_buffers, &host->current_buffer->link);
  }

  if (host->contents_width && host->contents_height) {
    double scale = host->ctx->scale * host->contents_scale;

    if (host->viewport) {
      int width = host->contents_width;
      int height = host->contents_height;

      // We need to take the client's viewport into account while still
      // making sure our scale is accounted for.
      if (viewport) {
        if (viewport->src_x >= 0 && viewport->src_y >= 0 &&
            viewport->src_width >= 0 && viewport->src_height >= 0) {
          wp_viewport_set_source(host->viewport, viewport->src_x,
                                 viewport->src_y, viewport->src_width,
                                 viewport->src_height);

          // If the source rectangle is set and the destination size is not
          // set, then src_width and src_height should be integers, and the
          // surface size becomes the source rectangle size.
          width = wl_fixed_to_int(viewport->src_width);
          height = wl_fixed_to_int(viewport->src_height);
        }

        // Use destination size as surface size when set.
        if (viewport->dst_width >= 0 && viewport->dst_height >= 0) {
          width = viewport->dst_width;
          height = viewport->dst_height;
        }
      }

      wp_viewport_set_destination(host->viewport, ceil(width / scale),
                                  ceil(height / scale));
    } else {
      wl_surface_set_buffer_scale(host->proxy, scale);
    }
  }

  // No need to defer client commits if surface has a role. E.g. is a cursor
  // or shell surface.
  if (host->has_role) {
    wl_surface_commit(host->proxy);

    // GTK determines the scale based on the output the surface has entered.
    // If the surface has not entered any output, then have it enter the
    // internal output. TODO(reveman): Remove this when surface-output tracking
    // has been implemented in Chrome.
    if (!host->has_output) {
      struct sl_host_output* output;

      wl_list_for_each(output, &host->ctx->host_outputs, link) {
        if (output->internal) {
          wl_surface_send_enter(host->resource, output->resource);
          host->has_output = 1;
          break;
        }
      }
    }
  } else {
    // Commit if surface is associated with a window. Otherwise, defer
    // commit until window is created.
    wl_list_for_each(window, &host->ctx->windows, link) {
      if (window->host_surface_id == wl_resource_get_id(resource)) {
        if (window->xdg_surface) {
          wl_surface_commit(host->proxy);
          if (host->contents_width && host->contents_height)
            window->realized = 1;
        }
        break;
      }
    }
  }

  if (host->contents_shm_mmap) {
    if (host->contents_shm_mmap->buffer_resource)
      wl_buffer_send_release(host->contents_shm_mmap->buffer_resource);
    sl_mmap_unref(host->contents_shm_mmap);
    host->contents_shm_mmap = NULL;
  }
}

static void sl_host_surface_set_buffer_transform(struct wl_client* client,
                                                 struct wl_resource* resource,
                                                 int32_t transform) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);

  wl_surface_set_buffer_transform(host->proxy, transform);
}

static void sl_host_surface_set_buffer_scale(struct wl_client* client,
                                             struct wl_resource* resource,
                                             int32_t scale) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);

  host->contents_scale = scale;
}

static void sl_host_surface_damage_buffer(struct wl_client* client,
                                          struct wl_resource* resource,
                                          int32_t x,
                                          int32_t y,
                                          int32_t width,
                                          int32_t height) {
  assert(0);
}

static const struct wl_surface_interface sl_surface_implementation = {
    sl_host_surface_destroy,
    sl_host_surface_attach,
    sl_host_surface_damage,
    sl_host_surface_frame,
    sl_host_surface_set_opaque_region,
    sl_host_surface_set_input_region,
    sl_host_surface_commit,
    sl_host_surface_set_buffer_transform,
    sl_host_surface_set_buffer_scale,
    sl_host_surface_damage_buffer};

static void sl_destroy_host_surface(struct wl_resource* resource) {
  struct sl_host_surface* host = wl_resource_get_user_data(resource);
  struct sl_window *window, *surface_window = NULL;
  struct sl_output_buffer* buffer;

  wl_list_for_each(window, &host->ctx->windows, link) {
    if (window->host_surface_id == wl_resource_get_id(resource)) {
      surface_window = window;
      break;
    }
  }

  if (surface_window) {
    surface_window->host_surface_id = 0;
    sl_window_update(surface_window);
  }

  if (host->contents_shm_mmap)
    sl_mmap_unref(host->contents_shm_mmap);

  while (!wl_list_empty(&host->released_buffers)) {
    buffer = wl_container_of(host->released_buffers.next, buffer, link);
    sl_output_buffer_destroy(buffer);
  }
  while (!wl_list_empty(&host->busy_buffers)) {
    buffer = wl_container_of(host->busy_buffers.next, buffer, link);
    sl_output_buffer_destroy(buffer);
  }
  while (!wl_list_empty(&host->contents_viewport))
    wl_list_remove(host->contents_viewport.next);

  if (host->viewport)
    wp_viewport_destroy(host->viewport);
  wl_surface_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_surface_enter(void* data,
                             struct wl_surface* surface,
                             struct wl_output* output) {
  struct sl_host_surface* host = wl_surface_get_user_data(surface);
  struct sl_host_output* host_output = wl_output_get_user_data(output);

  wl_surface_send_enter(host->resource, host_output->resource);
  host->has_output = 1;
}

static void sl_surface_leave(void* data,
                             struct wl_surface* surface,
                             struct wl_output* output) {
  struct sl_host_surface* host = wl_surface_get_user_data(surface);
  struct sl_host_output* host_output = wl_output_get_user_data(output);

  wl_surface_send_leave(host->resource, host_output->resource);
}

static const struct wl_surface_listener sl_surface_listener = {
    sl_surface_enter, sl_surface_leave};

static void sl_region_destroy(struct wl_client* client,
                              struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_region_add(struct wl_client* client,
                          struct wl_resource* resource,
                          int32_t x,
                          int32_t y,
                          int32_t width,
                          int32_t height) {
  struct sl_host_region* host = wl_resource_get_user_data(resource);
  double scale = host->ctx->scale;
  int32_t x1, y1, x2, y2;

  x1 = x / scale;
  y1 = y / scale;
  x2 = (x + width) / scale;
  y2 = (y + height) / scale;

  wl_region_add(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void sl_region_subtract(struct wl_client* client,
                               struct wl_resource* resource,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height) {
  struct sl_host_region* host = wl_resource_get_user_data(resource);
  double scale = host->ctx->scale;
  int32_t x1, y1, x2, y2;

  x1 = x / scale;
  y1 = y / scale;
  x2 = (x + width) / scale;
  y2 = (y + height) / scale;

  wl_region_subtract(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static const struct wl_region_interface sl_region_implementation = {
    sl_region_destroy, sl_region_add, sl_region_subtract};

static void sl_destroy_host_region(struct wl_resource* resource) {
  struct sl_host_region* host = wl_resource_get_user_data(resource);

  wl_region_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_compositor_create_host_surface(struct wl_client* client,
                                              struct wl_resource* resource,
                                              uint32_t id) {
  struct sl_host_compositor* host = wl_resource_get_user_data(resource);
  struct sl_host_surface* host_surface;
  struct sl_window *window, *unpaired_window = NULL;

  host_surface = malloc(sizeof(*host_surface));
  assert(host_surface);

  host_surface->ctx = host->compositor->ctx;
  host_surface->contents_width = 0;
  host_surface->contents_height = 0;
  host_surface->contents_scale = 1;
  wl_list_init(&host_surface->contents_viewport);
  host_surface->contents_shm_mmap = NULL;
  host_surface->has_role = 0;
  host_surface->has_output = 0;
  host_surface->last_event_serial = 0;
  host_surface->current_buffer = NULL;
  wl_list_init(&host_surface->released_buffers);
  wl_list_init(&host_surface->busy_buffers);
  host_surface->resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_surface->resource,
                                 &sl_surface_implementation, host_surface,
                                 sl_destroy_host_surface);
  host_surface->proxy = wl_compositor_create_surface(host->proxy);
  wl_surface_set_user_data(host_surface->proxy, host_surface);
  wl_surface_add_listener(host_surface->proxy, &sl_surface_listener,
                          host_surface);
  host_surface->viewport = NULL;
  if (host_surface->ctx->viewporter) {
    host_surface->viewport = wp_viewporter_get_viewport(
        host_surface->ctx->viewporter->internal, host_surface->proxy);
  }

  wl_list_for_each(window, &host->compositor->ctx->unpaired_windows, link) {
    if (window->host_surface_id == id) {
      unpaired_window = window;
      break;
    }
  }

  if (unpaired_window)
    sl_window_update(window);
}

static void sl_compositor_create_host_region(struct wl_client* client,
                                             struct wl_resource* resource,
                                             uint32_t id) {
  struct sl_host_compositor* host = wl_resource_get_user_data(resource);
  struct sl_host_region* host_region;

  host_region = malloc(sizeof(*host_region));
  assert(host_region);

  host_region->ctx = host->compositor->ctx;
  host_region->resource = wl_resource_create(
      client, &wl_region_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_region->resource,
                                 &sl_region_implementation, host_region,
                                 sl_destroy_host_region);
  host_region->proxy = wl_compositor_create_region(host->proxy);
  wl_region_set_user_data(host_region->proxy, host_region);
}

static const struct wl_compositor_interface sl_compositor_implementation = {
    sl_compositor_create_host_surface, sl_compositor_create_host_region};

static void sl_destroy_host_compositor(struct wl_resource* resource) {
  struct sl_host_compositor* host = wl_resource_get_user_data(resource);

  wl_compositor_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_bind_host_compositor(struct wl_client* client,
                                    void* data,
                                    uint32_t version,
                                    uint32_t id) {
  struct sl_compositor* compositor = (struct sl_compositor*)data;
  struct sl_host_compositor* host;

  host = malloc(sizeof(*host));
  assert(host);
  host->compositor = compositor;
  host->resource = wl_resource_create(client, &wl_compositor_interface,
                                      MIN(version, compositor->version), id);
  wl_resource_set_implementation(host->resource, &sl_compositor_implementation,
                                 host, sl_destroy_host_compositor);
  host->proxy = wl_registry_bind(
      wl_display_get_registry(compositor->ctx->display), compositor->id,
      &wl_compositor_interface, compositor->version);
  wl_compositor_set_user_data(host->proxy, host);
}

static void sl_host_buffer_destroy(struct wl_client* client,
                                   struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static const struct wl_buffer_interface sl_buffer_implementation = {
    sl_host_buffer_destroy};

static void sl_buffer_release(void* data, struct wl_buffer* buffer) {
  struct sl_host_buffer* host = wl_buffer_get_user_data(buffer);

  wl_buffer_send_release(host->resource);
}

static const struct wl_buffer_listener sl_buffer_listener = {sl_buffer_release};

static void sl_destroy_host_buffer(struct wl_resource* resource) {
  struct sl_host_buffer* host = wl_resource_get_user_data(resource);

  if (host->proxy)
    wl_buffer_destroy(host->proxy);
  if (host->shm_mmap) {
    host->shm_mmap->buffer_resource = NULL;
    sl_mmap_unref(host->shm_mmap);
  }
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

struct sl_host_buffer* sl_create_host_buffer(struct wl_client* client,
                                             uint32_t id,
                                             struct wl_buffer* proxy,
                                             int32_t width,
                                             int32_t height) {
  struct sl_host_buffer* host_buffer;

  host_buffer = malloc(sizeof(*host_buffer));
  assert(host_buffer);

  host_buffer->width = width;
  host_buffer->height = height;
  host_buffer->resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);
  wl_resource_set_implementation(host_buffer->resource,
                                 &sl_buffer_implementation, host_buffer,
                                 sl_destroy_host_buffer);
  host_buffer->shm_mmap = NULL;
  host_buffer->shm_format = 0;
  host_buffer->proxy = proxy;
  if (host_buffer->proxy) {
    wl_buffer_set_user_data(host_buffer->proxy, host_buffer);
    wl_buffer_add_listener(host_buffer->proxy, &sl_buffer_listener,
                           host_buffer);
  }

  return host_buffer;
}

static void sl_host_shm_pool_create_host_buffer(struct wl_client* client,
                                                struct wl_resource* resource,
                                                uint32_t id,
                                                int32_t offset,
                                                int32_t width,
                                                int32_t height,
                                                int32_t stride,
                                                uint32_t format) {
  struct sl_host_shm_pool* host = wl_resource_get_user_data(resource);

  if (host->shm->ctx->shm_driver == SHM_DRIVER_NOOP) {
    assert(host->proxy);
    sl_create_host_buffer(client, id,
                          wl_shm_pool_create_buffer(host->proxy, offset, width,
                                                    height, stride, format),
                          width, height);
  } else {
    struct sl_host_buffer* host_buffer =
        sl_create_host_buffer(client, id, NULL, width, height);

    host_buffer->shm_format = format;
    host_buffer->shm_mmap =
        sl_mmap_create(dup(host->fd), height * stride, offset, stride,
                       sl_bpp_for_shm_format(format));
    host_buffer->shm_mmap->buffer_resource = host_buffer->resource;
  }
}

static void sl_host_shm_pool_destroy(struct wl_client* client,
                                     struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_host_shm_pool_resize(struct wl_client* client,
                                    struct wl_resource* resource,
                                    int32_t size) {
  struct sl_host_shm_pool* host = wl_resource_get_user_data(resource);

  if (host->proxy)
    wl_shm_pool_resize(host->proxy, size);
}

static const struct wl_shm_pool_interface sl_shm_pool_implementation = {
    sl_host_shm_pool_create_host_buffer, sl_host_shm_pool_destroy,
    sl_host_shm_pool_resize};

static void sl_destroy_host_shm_pool(struct wl_resource* resource) {
  struct sl_host_shm_pool* host = wl_resource_get_user_data(resource);

  if (host->fd >= 0)
    close(host->fd);
  if (host->proxy)
    wl_shm_pool_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_shm_create_host_pool(struct wl_client* client,
                                    struct wl_resource* resource,
                                    uint32_t id,
                                    int fd,
                                    int32_t size) {
  struct sl_host_shm* host = wl_resource_get_user_data(resource);
  struct sl_host_shm_pool* host_shm_pool;

  host_shm_pool = malloc(sizeof(*host_shm_pool));
  assert(host_shm_pool);

  host_shm_pool->shm = host->shm;
  host_shm_pool->fd = -1;
  host_shm_pool->proxy = NULL;
  host_shm_pool->resource =
      wl_resource_create(client, &wl_shm_pool_interface, 1, id);
  wl_resource_set_implementation(host_shm_pool->resource,
                                 &sl_shm_pool_implementation, host_shm_pool,
                                 sl_destroy_host_shm_pool);

  switch (host->shm->ctx->shm_driver) {
    case SHM_DRIVER_NOOP:
      host_shm_pool->proxy = wl_shm_create_pool(host->shm_proxy, fd, size);
      wl_shm_pool_set_user_data(host_shm_pool->proxy, host_shm_pool);
      close(fd);
      break;
    case SHM_DRIVER_DMABUF:
    case SHM_DRIVER_VIRTWL:
    case SHM_DRIVER_VIRTWL_DMABUF:
      host_shm_pool->fd = fd;
      break;
  }
}

static const struct wl_shm_interface sl_shm_implementation = {
    sl_shm_create_host_pool};

static void sl_shm_format(void* data, struct wl_shm* shm, uint32_t format) {
  struct sl_host_shm* host = wl_shm_get_user_data(shm);

  switch (format) {
    case WL_SHM_FORMAT_RGB565:
    case WL_SHM_FORMAT_ARGB8888:
    case WL_SHM_FORMAT_ABGR8888:
    case WL_SHM_FORMAT_XRGB8888:
    case WL_SHM_FORMAT_XBGR8888:
      wl_shm_send_format(host->resource, format);
    default:
      break;
  }
}

static const struct wl_shm_listener sl_shm_listener = {sl_shm_format};

static void sl_drm_format(void* data,
                          struct zwp_linux_dmabuf_v1* linux_dmabuf,
                          uint32_t format) {
  struct sl_host_shm* host = zwp_linux_dmabuf_v1_get_user_data(linux_dmabuf);

  // Forward SHM versions of supported formats.
  switch (format) {
    case WL_DRM_FORMAT_RGB565:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_RGB565);
      break;
    case WL_DRM_FORMAT_ARGB8888:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_ARGB8888);
      break;
    case WL_DRM_FORMAT_ABGR8888:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_ABGR8888);
      break;
    case WL_DRM_FORMAT_XRGB8888:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_XRGB8888);
      break;
    case WL_DRM_FORMAT_XBGR8888:
      wl_shm_send_format(host->resource, WL_SHM_FORMAT_XBGR8888);
      break;
  }
}

static void sl_drm_modifier(void* data,
                            struct zwp_linux_dmabuf_v1* linux_dmabuf,
                            uint32_t format,
                            uint32_t modifier_hi,
                            uint32_t modifier_lo) {}

static const struct zwp_linux_dmabuf_v1_listener sl_linux_dmabuf_listener = {
    sl_drm_format, sl_drm_modifier};

static void sl_destroy_host_shm(struct wl_resource* resource) {
  struct sl_host_shm* host = wl_resource_get_user_data(resource);

  if (host->shm_proxy)
    wl_shm_destroy(host->shm_proxy);
  if (host->linux_dmabuf_proxy)
    zwp_linux_dmabuf_v1_destroy(host->linux_dmabuf_proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_bind_host_shm(struct wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  struct sl_shm* shm = (struct sl_shm*)data;
  struct sl_host_shm* host;

  host = malloc(sizeof(*host));
  assert(host);
  host->shm = shm;
  host->shm_proxy = NULL;
  host->linux_dmabuf_proxy = NULL;
  host->resource = wl_resource_create(client, &wl_shm_interface, 1, id);
  wl_resource_set_implementation(host->resource, &sl_shm_implementation, host,
                                 sl_destroy_host_shm);

  switch (shm->ctx->shm_driver) {
    case SHM_DRIVER_NOOP:
    case SHM_DRIVER_VIRTWL:
      host->shm_proxy = wl_registry_bind(
          wl_display_get_registry(shm->ctx->display), shm->id,
          &wl_shm_interface, wl_resource_get_version(host->resource));
      wl_shm_set_user_data(host->shm_proxy, host);
      wl_shm_add_listener(host->shm_proxy, &sl_shm_listener, host);
      break;
    case SHM_DRIVER_VIRTWL_DMABUF:
    case SHM_DRIVER_DMABUF:
      assert(shm->ctx->linux_dmabuf);
      host->linux_dmabuf_proxy = wl_registry_bind(
          wl_display_get_registry(shm->ctx->display),
          shm->ctx->linux_dmabuf->id, &zwp_linux_dmabuf_v1_interface,
          wl_resource_get_version(host->resource));
      zwp_linux_dmabuf_v1_set_user_data(host->linux_dmabuf_proxy, host);
      zwp_linux_dmabuf_v1_add_listener(host->linux_dmabuf_proxy,
                                       &sl_linux_dmabuf_listener, host);
      break;
  }
}

static void sl_shell_surface_pong(struct wl_client* client,
                                  struct wl_resource* resource,
                                  uint32_t serial) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);

  wl_shell_surface_pong(host->proxy, serial);
}

static void sl_shell_surface_move(struct wl_client* client,
                                  struct wl_resource* resource,
                                  struct wl_resource* seat_resource,
                                  uint32_t serial) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_seat* host_seat = wl_resource_get_user_data(seat_resource);

  wl_shell_surface_move(host->proxy, host_seat->proxy, serial);
}

static void sl_shell_surface_resize(struct wl_client* client,
                                    struct wl_resource* resource,
                                    struct wl_resource* seat_resource,
                                    uint32_t serial,
                                    uint32_t edges) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_seat* host_seat = wl_resource_get_user_data(seat_resource);

  wl_shell_surface_resize(host->proxy, host_seat->proxy, serial, edges);
}

static void sl_shell_surface_set_toplevel(struct wl_client* client,
                                          struct wl_resource* resource) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);

  wl_shell_surface_set_toplevel(host->proxy);
}

static void sl_shell_surface_set_transient(struct wl_client* client,
                                           struct wl_resource* resource,
                                           struct wl_resource* parent_resource,
                                           int32_t x,
                                           int32_t y,
                                           uint32_t flags) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_surface* host_parent =
      wl_resource_get_user_data(parent_resource);

  wl_shell_surface_set_transient(host->proxy, host_parent->proxy, x, y, flags);
}

static void sl_shell_surface_set_fullscreen(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t method,
    uint32_t framerate,
    struct wl_resource* output_resource) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_output* host_output =
      output_resource ? wl_resource_get_user_data(output_resource) : NULL;

  wl_shell_surface_set_fullscreen(host->proxy, method, framerate,
                                  host_output ? host_output->proxy : NULL);
}

static void sl_shell_surface_set_popup(struct wl_client* client,
                                       struct wl_resource* resource,
                                       struct wl_resource* seat_resource,
                                       uint32_t serial,
                                       struct wl_resource* parent_resource,
                                       int32_t x,
                                       int32_t y,
                                       uint32_t flags) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_seat* host_seat = wl_resource_get_user_data(seat_resource);
  struct sl_host_surface* host_parent =
      wl_resource_get_user_data(parent_resource);

  wl_shell_surface_set_popup(host->proxy, host_seat->proxy, serial,
                             host_parent->proxy, x, y, flags);
}

static void sl_shell_surface_set_maximized(
    struct wl_client* client,
    struct wl_resource* resource,
    struct wl_resource* output_resource) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);
  struct sl_host_output* host_output =
      output_resource ? wl_resource_get_user_data(output_resource) : NULL;

  wl_shell_surface_set_maximized(host->proxy,
                                 host_output ? host_output->proxy : NULL);
}

static void sl_shell_surface_set_title(struct wl_client* client,
                                       struct wl_resource* resource,
                                       const char* title) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);

  wl_shell_surface_set_title(host->proxy, title);
}

static void sl_shell_surface_set_class(struct wl_client* client,
                                       struct wl_resource* resource,
                                       const char* clazz) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);

  wl_shell_surface_set_class(host->proxy, clazz);
}

static const struct wl_shell_surface_interface sl_shell_surface_implementation =
    {sl_shell_surface_pong,          sl_shell_surface_move,
     sl_shell_surface_resize,        sl_shell_surface_set_toplevel,
     sl_shell_surface_set_transient, sl_shell_surface_set_fullscreen,
     sl_shell_surface_set_popup,     sl_shell_surface_set_maximized,
     sl_shell_surface_set_title,     sl_shell_surface_set_class};

static void sl_shell_surface_ping(void* data,
                                  struct wl_shell_surface* shell_surface,
                                  uint32_t serial) {
  struct sl_host_shell_surface* host =
      wl_shell_surface_get_user_data(shell_surface);

  wl_shell_surface_send_ping(host->resource, serial);
}

static void sl_shell_surface_configure(void* data,
                                       struct wl_shell_surface* shell_surface,
                                       uint32_t edges,
                                       int32_t width,
                                       int32_t height) {
  struct sl_host_shell_surface* host =
      wl_shell_surface_get_user_data(shell_surface);

  wl_shell_surface_send_configure(host->resource, edges, width, height);
}

static void sl_shell_surface_popup_done(
    void* data, struct wl_shell_surface* shell_surface) {
  struct sl_host_shell_surface* host =
      wl_shell_surface_get_user_data(shell_surface);

  wl_shell_surface_send_popup_done(host->resource);
}

static const struct wl_shell_surface_listener sl_shell_surface_listener = {
    sl_shell_surface_ping, sl_shell_surface_configure,
    sl_shell_surface_popup_done};

static void sl_destroy_host_shell_surface(struct wl_resource* resource) {
  struct sl_host_shell_surface* host = wl_resource_get_user_data(resource);

  wl_shell_surface_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_host_shell_get_shell_surface(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* surface_resource) {
  struct sl_host_shell* host = wl_resource_get_user_data(resource);
  struct sl_host_surface* host_surface =
      wl_resource_get_user_data(surface_resource);
  struct sl_host_shell_surface* host_shell_surface;

  host_shell_surface = malloc(sizeof(*host_shell_surface));
  assert(host_shell_surface);
  host_shell_surface->resource =
      wl_resource_create(client, &wl_shell_surface_interface, 1, id);
  wl_resource_set_implementation(
      host_shell_surface->resource, &sl_shell_surface_implementation,
      host_shell_surface, sl_destroy_host_shell_surface);
  host_shell_surface->proxy =
      wl_shell_get_shell_surface(host->proxy, host_surface->proxy);
  wl_shell_surface_set_user_data(host_shell_surface->proxy, host_shell_surface);
  wl_shell_surface_add_listener(host_shell_surface->proxy,
                                &sl_shell_surface_listener, host_shell_surface);
  host_surface->has_role = 1;
}

static const struct wl_shell_interface sl_shell_implementation = {
    sl_host_shell_get_shell_surface};

static void sl_destroy_host_shell(struct wl_resource* resource) {
  struct sl_host_shell* host = wl_resource_get_user_data(resource);

  wl_shell_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_bind_host_shell(struct wl_client* client,
                               void* data,
                               uint32_t version,
                               uint32_t id) {
  struct sl_shell* shell = (struct sl_shell*)data;
  struct sl_host_shell* host;

  host = malloc(sizeof(*host));
  assert(host);
  host->shell = shell;
  host->resource = wl_resource_create(client, &wl_shell_interface, 1, id);
  wl_resource_set_implementation(host->resource, &sl_shell_implementation, host,
                                 sl_destroy_host_shell);
  host->proxy = wl_registry_bind(wl_display_get_registry(shell->ctx->display),
                                 shell->id, &wl_shell_interface,
                                 wl_resource_get_version(host->resource));
  wl_shell_set_user_data(host->proxy, host);
}

static void sl_output_geometry(void* data,
                               struct wl_output* output,
                               int x,
                               int y,
                               int physical_width,
                               int physical_height,
                               int subpixel,
                               const char* make,
                               const char* model,
                               int transform) {
  struct sl_host_output* host = wl_output_get_user_data(output);

  host->x = x;
  host->y = y;
  host->physical_width = physical_width;
  host->physical_height = physical_height;
  host->subpixel = subpixel;
  free(host->model);
  host->model = strdup(model);
  free(host->make);
  host->make = strdup(make);
  host->transform = transform;
}

static void sl_output_mode(void* data,
                           struct wl_output* output,
                           uint32_t flags,
                           int width,
                           int height,
                           int refresh) {
  struct sl_host_output* host = wl_output_get_user_data(output);

  host->flags = flags;
  host->width = width;
  host->height = height;
  host->refresh = refresh;
}

static double sl_aura_scale_factor_to_double(int scale_factor) {
  // Aura scale factor is an enum that for all currently know values
  // is a scale value multipled by 1000. For example, enum value for
  // 1.25 scale factor is 1250.
  return scale_factor / 1000.0;
}

static void sl_send_host_output_state(struct sl_host_output* host) {
  double preferred_scale =
      sl_aura_scale_factor_to_double(host->preferred_scale);
  double current_scale = sl_aura_scale_factor_to_double(host->current_scale);
  double ideal_scale_factor = 1.0;
  double scale_factor = host->scale_factor;
  int scale;
  int physical_width;
  int physical_height;
  int x;
  int y;
  int width;
  int height;

  // Use the scale factor we received from aura shell protocol when available.
  if (host->output->ctx->aura_shell) {
    double device_scale_factor =
        sl_aura_scale_factor_to_double(host->device_scale_factor);

    ideal_scale_factor = device_scale_factor * preferred_scale;
    scale_factor = device_scale_factor * current_scale;
  }

  // Always use scale=1 and adjust geometry and mode based on ideal
  // scale factor for Xwayland client. For other clients, pick an optimal
  // scale and adjust geometry and mode based on it.
  if (host->output->ctx->xwayland) {
    scale = 1;
    physical_width = host->physical_width * ideal_scale_factor / scale_factor;
    physical_height = host->physical_height * ideal_scale_factor / scale_factor;
    // X/Y are best left at origin as managed X windows are kept centered on
    // the root window. The result is that all outputs are overlapping and
    // pointer events can always be dispatched to the visible region of the
    // window.
    x = y = 0;
    width = host->width * host->output->ctx->scale / scale_factor;
    height = host->height * host->output->ctx->scale / scale_factor;
  } else {
    scale =
        MIN(ceil(scale_factor / host->output->ctx->scale), MAX_OUTPUT_SCALE);
    physical_width = host->physical_width;
    physical_height = host->physical_height;
    // Should x/y be affected by scale?
    x = host->x;
    y = host->y;
    width = host->width * host->output->ctx->scale * scale / scale_factor;
    height = host->height * host->output->ctx->scale * scale / scale_factor;
  }

  if (host->output->ctx->dpi.size) {
    int dpi = (width * INCH_IN_MM) / physical_width;
    int adjusted_dpi = *((int*)host->output->ctx->dpi.data);
    double mmpd;
    int* p;

    wl_array_for_each(p, &host->output->ctx->dpi) {
      if (*p > dpi)
        break;

      adjusted_dpi = *p;
    }

    mmpd = INCH_IN_MM / adjusted_dpi;
    physical_width = width * mmpd + 0.5;
    physical_height = height * mmpd + 0.5;
  }

  wl_output_send_geometry(host->resource, x, y, physical_width, physical_height,
                          host->subpixel, host->make, host->model,
                          host->transform);
  wl_output_send_mode(host->resource, host->flags | WL_OUTPUT_MODE_CURRENT,
                      width, height, host->refresh);
  if (wl_resource_get_version(host->resource) >= WL_OUTPUT_SCALE_SINCE_VERSION)
    wl_output_send_scale(host->resource, scale);
  if (wl_resource_get_version(host->resource) >= WL_OUTPUT_DONE_SINCE_VERSION)
    wl_output_send_done(host->resource);
}

static void sl_output_done(void* data, struct wl_output* output) {
  struct sl_host_output* host = wl_output_get_user_data(output);

  // Early out if scale is expected but not yet know.
  if (host->expecting_scale)
    return;

  sl_send_host_output_state(host);

  // Expect scale if aura output exists.
  if (host->aura_output)
    host->expecting_scale = 1;
}

static void sl_output_scale(void* data,
                            struct wl_output* output,
                            int32_t scale_factor) {
  struct sl_host_output* host = wl_output_get_user_data(output);

  host->scale_factor = scale_factor;
}

static const struct wl_output_listener sl_output_listener = {
    sl_output_geometry, sl_output_mode, sl_output_done, sl_output_scale};

static void sl_aura_output_scale(void* data,
                                 struct zaura_output* output,
                                 uint32_t flags,
                                 uint32_t scale) {
  struct sl_host_output* host = zaura_output_get_user_data(output);

  switch (scale) {
    case ZAURA_OUTPUT_SCALE_FACTOR_0400:
    case ZAURA_OUTPUT_SCALE_FACTOR_0500:
    case ZAURA_OUTPUT_SCALE_FACTOR_0550:
    case ZAURA_OUTPUT_SCALE_FACTOR_0600:
    case ZAURA_OUTPUT_SCALE_FACTOR_0625:
    case ZAURA_OUTPUT_SCALE_FACTOR_0650:
    case ZAURA_OUTPUT_SCALE_FACTOR_0700:
    case ZAURA_OUTPUT_SCALE_FACTOR_0750:
    case ZAURA_OUTPUT_SCALE_FACTOR_0800:
    case ZAURA_OUTPUT_SCALE_FACTOR_0850:
    case ZAURA_OUTPUT_SCALE_FACTOR_0900:
    case ZAURA_OUTPUT_SCALE_FACTOR_0950:
    case ZAURA_OUTPUT_SCALE_FACTOR_1000:
    case ZAURA_OUTPUT_SCALE_FACTOR_1050:
    case ZAURA_OUTPUT_SCALE_FACTOR_1100:
    case ZAURA_OUTPUT_SCALE_FACTOR_1150:
    case ZAURA_OUTPUT_SCALE_FACTOR_1125:
    case ZAURA_OUTPUT_SCALE_FACTOR_1200:
    case ZAURA_OUTPUT_SCALE_FACTOR_1250:
    case ZAURA_OUTPUT_SCALE_FACTOR_1300:
    case ZAURA_OUTPUT_SCALE_FACTOR_1400:
    case ZAURA_OUTPUT_SCALE_FACTOR_1450:
    case ZAURA_OUTPUT_SCALE_FACTOR_1500:
    case ZAURA_OUTPUT_SCALE_FACTOR_1600:
    case ZAURA_OUTPUT_SCALE_FACTOR_1750:
    case ZAURA_OUTPUT_SCALE_FACTOR_1800:
    case ZAURA_OUTPUT_SCALE_FACTOR_2000:
    case ZAURA_OUTPUT_SCALE_FACTOR_2200:
    case ZAURA_OUTPUT_SCALE_FACTOR_2250:
    case ZAURA_OUTPUT_SCALE_FACTOR_2500:
    case ZAURA_OUTPUT_SCALE_FACTOR_2750:
    case ZAURA_OUTPUT_SCALE_FACTOR_3000:
    case ZAURA_OUTPUT_SCALE_FACTOR_3500:
    case ZAURA_OUTPUT_SCALE_FACTOR_4000:
    case ZAURA_OUTPUT_SCALE_FACTOR_4500:
    case ZAURA_OUTPUT_SCALE_FACTOR_5000:
      break;
    default:
      fprintf(stderr, "warning: unknown scale factor: %d\n", scale);
      break;
  }

  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT)
    host->current_scale = scale;
  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_PREFERRED)
    host->preferred_scale = scale;

  host->expecting_scale = 0;
}

static void sl_aura_output_connection(void* data,
                                      struct zaura_output* output,
                                      uint32_t connection) {
  struct sl_host_output* host = zaura_output_get_user_data(output);

  host->internal = connection == ZAURA_OUTPUT_CONNECTION_TYPE_INTERNAL;
}

static void sl_aura_output_device_scale_factor(void* data,
                                               struct zaura_output* output,
                                               uint32_t device_scale_factor) {
  struct sl_host_output* host = zaura_output_get_user_data(output);

  host->device_scale_factor = device_scale_factor;
}

static const struct zaura_output_listener sl_aura_output_listener = {
    sl_aura_output_scale, sl_aura_output_connection,
    sl_aura_output_device_scale_factor};

static void sl_destroy_host_output(struct wl_resource* resource) {
  struct sl_host_output* host = wl_resource_get_user_data(resource);

  if (host->aura_output)
    zaura_output_destroy(host->aura_output);
  if (wl_output_get_version(host->proxy) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
    wl_output_release(host->proxy);
  } else {
    wl_output_destroy(host->proxy);
  }
  wl_resource_set_user_data(resource, NULL);
  wl_list_remove(&host->link);
  free(host->make);
  free(host->model);
  free(host);
}

static void sl_bind_host_output(struct wl_client* client,
                                void* data,
                                uint32_t version,
                                uint32_t id) {
  struct sl_output* output = (struct sl_output*)data;
  struct sl_context* ctx = output->ctx;
  struct sl_host_output* host;

  host = malloc(sizeof(*host));
  assert(host);
  host->output = output;
  host->resource = wl_resource_create(client, &wl_output_interface,
                                      MIN(version, output->version), id);
  wl_resource_set_implementation(host->resource, NULL, host,
                                 sl_destroy_host_output);
  host->proxy = wl_registry_bind(wl_display_get_registry(ctx->display),
                                 output->id, &wl_output_interface,
                                 wl_resource_get_version(host->resource));
  wl_output_set_user_data(host->proxy, host);
  wl_output_add_listener(host->proxy, &sl_output_listener, host);
  host->aura_output = NULL;
  // We assume that first output is internal by default.
  host->internal = wl_list_empty(&ctx->host_outputs);
  host->x = 0;
  host->y = 0;
  host->physical_width = 0;
  host->physical_height = 0;
  host->subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
  host->make = strdup("unknown");
  host->model = strdup("unknown");
  host->transform = WL_OUTPUT_TRANSFORM_NORMAL;
  host->flags = 0;
  host->width = 1024;
  host->height = 768;
  host->refresh = 60000;
  host->scale_factor = 1;
  host->current_scale = 1000;
  host->preferred_scale = 1000;
  host->device_scale_factor = 1000;
  host->expecting_scale = 0;
  wl_list_insert(ctx->host_outputs.prev, &host->link);
  if (ctx->aura_shell) {
    host->expecting_scale = 1;
    host->internal = 0;
    host->aura_output =
        zaura_shell_get_aura_output(ctx->aura_shell->internal, host->proxy);
    zaura_output_set_user_data(host->aura_output, host);
    zaura_output_add_listener(host->aura_output, &sl_aura_output_listener,
                              host);
  }
}

static void sl_internal_data_offer_destroy(struct sl_data_offer* host) {
  wl_data_offer_destroy(host->internal);
  free(host);
}

static void sl_set_selection(struct sl_context* ctx,
                             struct sl_data_offer* data_offer) {
  if (ctx->selection_data_offer) {
    sl_internal_data_offer_destroy(ctx->selection_data_offer);
    ctx->selection_data_offer = NULL;
  }

  if (ctx->clipboard_manager) {
    if (!data_offer) {
      if (ctx->selection_owner == ctx->selection_window)
        xcb_set_selection_owner(ctx->connection, XCB_ATOM_NONE,
                                ctx->atoms[ATOM_CLIPBOARD].value,
                                ctx->selection_timestamp);
      return;
    }

    xcb_set_selection_owner(ctx->connection, ctx->selection_window,
                            ctx->atoms[ATOM_CLIPBOARD].value, XCB_CURRENT_TIME);
  }

  ctx->selection_data_offer = data_offer;
}

static const char* sl_utf8_mime_type = "text/plain;charset=utf-8";

static void sl_internal_data_offer_offer(void* data,
                                         struct wl_data_offer* data_offer,
                                         const char* type) {
  struct sl_data_offer* host = data;

  if (strcmp(type, sl_utf8_mime_type) == 0)
    host->utf8_text = 1;
}

static void sl_internal_data_offer_source_actions(
    void* data, struct wl_data_offer* data_offer, uint32_t source_actions) {}

static void sl_internal_data_offer_action(void* data,
                                          struct wl_data_offer* data_offer,
                                          uint32_t dnd_action) {}

static const struct wl_data_offer_listener sl_internal_data_offer_listener = {
    sl_internal_data_offer_offer, sl_internal_data_offer_source_actions,
    sl_internal_data_offer_action};

static void sl_internal_data_device_data_offer(
    void* data,
    struct wl_data_device* data_device,
    struct wl_data_offer* data_offer) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_data_offer* host_data_offer;

  host_data_offer = malloc(sizeof(*host_data_offer));
  assert(host_data_offer);

  host_data_offer->ctx = ctx;
  host_data_offer->internal = data_offer;
  host_data_offer->utf8_text = 0;

  wl_data_offer_add_listener(host_data_offer->internal,
                             &sl_internal_data_offer_listener, host_data_offer);
}

static void sl_internal_data_device_enter(void* data,
                                          struct wl_data_device* data_device,
                                          uint32_t serial,
                                          struct wl_surface* surface,
                                          wl_fixed_t x,
                                          wl_fixed_t y,
                                          struct wl_data_offer* data_offer) {}

static void sl_internal_data_device_leave(void* data,
                                          struct wl_data_device* data_device) {}

static void sl_internal_data_device_motion(void* data,
                                           struct wl_data_device* data_device,
                                           uint32_t time,
                                           wl_fixed_t x,
                                           wl_fixed_t y) {}

static void sl_internal_data_device_drop(void* data,
                                         struct wl_data_device* data_device) {}

static void sl_internal_data_device_selection(
    void* data,
    struct wl_data_device* data_device,
    struct wl_data_offer* data_offer) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_data_offer* host_data_offer =
      data_offer ? wl_data_offer_get_user_data(data_offer) : NULL;

  sl_set_selection(ctx, host_data_offer);
}

static const struct wl_data_device_listener sl_internal_data_device_listener = {
    sl_internal_data_device_data_offer, sl_internal_data_device_enter,
    sl_internal_data_device_leave,      sl_internal_data_device_motion,
    sl_internal_data_device_drop,       sl_internal_data_device_selection};

void sl_host_seat_added(struct sl_host_seat* host) {
  struct sl_context* ctx = host->seat->ctx;

  if (ctx->default_seat)
    return;

  ctx->default_seat = host;

  // Get data device for selections.
  if (ctx->data_device_manager && ctx->data_device_manager->internal) {
    ctx->selection_data_device = wl_data_device_manager_get_data_device(
        ctx->data_device_manager->internal, host->proxy);
    wl_data_device_add_listener(ctx->selection_data_device,
                                &sl_internal_data_device_listener, ctx);
  }
}

void sl_host_seat_removed(struct sl_host_seat* host) {
  if (host->seat->ctx->default_seat == host)
    host->seat->ctx->default_seat = NULL;
}

static void sl_subsurface_destroy(struct wl_client* client,
                                  struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_subsurface_set_position(struct wl_client* client,
                                       struct wl_resource* resource,
                                       int32_t x,
                                       int32_t y) {
  struct sl_host_subsurface* host = wl_resource_get_user_data(resource);
  double scale = host->ctx->scale;

  wl_subsurface_set_position(host->proxy, x / scale, y / scale);
}

static void sl_subsurface_place_above(struct wl_client* client,
                                      struct wl_resource* resource,
                                      struct wl_resource* sibling_resource) {
  struct sl_host_subsurface* host = wl_resource_get_user_data(resource);
  struct sl_host_surface* host_sibling =
      wl_resource_get_user_data(sibling_resource);

  wl_subsurface_place_above(host->proxy, host_sibling->proxy);
}

static void sl_subsurface_place_below(struct wl_client* client,
                                      struct wl_resource* resource,
                                      struct wl_resource* sibling_resource) {
  struct sl_host_subsurface* host = wl_resource_get_user_data(resource);
  struct sl_host_surface* host_sibling =
      wl_resource_get_user_data(sibling_resource);

  wl_subsurface_place_below(host->proxy, host_sibling->proxy);
}

static void sl_subsurface_set_sync(struct wl_client* client,
                                   struct wl_resource* resource) {
  struct sl_host_subsurface* host = wl_resource_get_user_data(resource);

  wl_subsurface_set_sync(host->proxy);
}

static void sl_subsurface_set_desync(struct wl_client* client,
                                     struct wl_resource* resource) {
  struct sl_host_subsurface* host = wl_resource_get_user_data(resource);

  wl_subsurface_set_desync(host->proxy);
}

static const struct wl_subsurface_interface sl_subsurface_implementation = {
    sl_subsurface_destroy,     sl_subsurface_set_position,
    sl_subsurface_place_above, sl_subsurface_place_below,
    sl_subsurface_set_sync,    sl_subsurface_set_desync};

static void sl_destroy_host_subsurface(struct wl_resource* resource) {
  struct sl_host_subsurface* host = wl_resource_get_user_data(resource);

  wl_subsurface_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_subcompositor_destroy(struct wl_client* client,
                                     struct wl_resource* resource) {
  wl_resource_destroy(resource);
}

static void sl_subcompositor_get_subsurface(
    struct wl_client* client,
    struct wl_resource* resource,
    uint32_t id,
    struct wl_resource* surface_resource,
    struct wl_resource* parent_resource) {
  struct sl_host_subcompositor* host = wl_resource_get_user_data(resource);
  struct sl_host_surface* host_surface =
      wl_resource_get_user_data(surface_resource);
  struct sl_host_surface* host_parent =
      wl_resource_get_user_data(parent_resource);
  struct sl_host_subsurface* host_subsurface;

  host_subsurface = malloc(sizeof(*host_subsurface));
  assert(host_subsurface);

  host_subsurface->ctx = host->ctx;
  host_subsurface->resource =
      wl_resource_create(client, &wl_subsurface_interface, 1, id);
  wl_resource_set_implementation(host_subsurface->resource,
                                 &sl_subsurface_implementation, host_subsurface,
                                 sl_destroy_host_subsurface);
  host_subsurface->proxy = wl_subcompositor_get_subsurface(
      host->proxy, host_surface->proxy, host_parent->proxy);
  wl_subsurface_set_user_data(host_subsurface->proxy, host_subsurface);
  host_surface->has_role = 1;
}

static const struct wl_subcompositor_interface sl_subcompositor_implementation =
    {sl_subcompositor_destroy, sl_subcompositor_get_subsurface};

static void sl_destroy_host_subcompositor(struct wl_resource* resource) {
  struct sl_host_subcompositor* host = wl_resource_get_user_data(resource);

  wl_subcompositor_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_bind_host_subcompositor(struct wl_client* client,
                                       void* data,
                                       uint32_t version,
                                       uint32_t id) {
  struct sl_subcompositor* subcompositor = (struct sl_subcompositor*)data;
  struct sl_host_subcompositor* host;

  host = malloc(sizeof(*host));
  assert(host);
  host->ctx = subcompositor->ctx;
  host->resource =
      wl_resource_create(client, &wl_subcompositor_interface, 1, id);
  wl_resource_set_implementation(host->resource,
                                 &sl_subcompositor_implementation, host,
                                 sl_destroy_host_subcompositor);
  host->proxy =
      wl_registry_bind(wl_display_get_registry(subcompositor->ctx->display),
                       subcompositor->id, &wl_subcompositor_interface, 1);
  wl_subcompositor_set_user_data(host->proxy, host);
}

struct sl_global* sl_global_create(struct sl_context* ctx,
                                   const struct wl_interface* interface,
                                   int version,
                                   void* data,
                                   wl_global_bind_func_t bind) {
  struct sl_host_registry* registry;
  struct sl_global* global;

  assert(version > 0);
  assert(version <= interface->version);

  global = malloc(sizeof *global);
  assert(global);

  global->ctx = ctx;
  global->name = ctx->next_global_id++;
  global->interface = interface;
  global->version = version;
  global->data = data;
  global->bind = bind;
  wl_list_insert(ctx->globals.prev, &global->link);

  wl_list_for_each(registry, &ctx->registries, link) {
    wl_resource_post_event(registry->resource, WL_REGISTRY_GLOBAL, global->name,
                           global->interface->name, global->version);
  }

  return global;
}

static void sl_global_destroy(struct sl_global* global) {
  struct sl_host_registry* registry;

  wl_list_for_each(registry, &global->ctx->registries, link)
      wl_resource_post_event(registry->resource, WL_REGISTRY_GLOBAL_REMOVE,
                             global->name);
  wl_list_remove(&global->link);
  free(global);
}

static void sl_registry_handler(void* data,
                                struct wl_registry* registry,
                                uint32_t id,
                                const char* interface,
                                uint32_t version) {
  struct sl_context* ctx = (struct sl_context*)data;

  if (strcmp(interface, "wl_compositor") == 0) {
    struct sl_compositor* compositor = malloc(sizeof(struct sl_compositor));
    assert(compositor);
    compositor->ctx = ctx;
    compositor->id = id;
    assert(version >= 3);
    compositor->version = 3;
    compositor->host_global =
        sl_global_create(ctx, &wl_compositor_interface, compositor->version,
                         compositor, sl_bind_host_compositor);
    compositor->internal = wl_registry_bind(
        registry, id, &wl_compositor_interface, compositor->version);
    assert(!ctx->compositor);
    ctx->compositor = compositor;
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    struct sl_subcompositor* subcompositor =
        malloc(sizeof(struct sl_subcompositor));
    assert(subcompositor);
    subcompositor->ctx = ctx;
    subcompositor->id = id;
    subcompositor->host_global =
        sl_global_create(ctx, &wl_subcompositor_interface, 1, subcompositor,
                         sl_bind_host_subcompositor);
    ctx->subcompositor = subcompositor;
  } else if (strcmp(interface, "wl_shm") == 0) {
    struct sl_shm* shm = malloc(sizeof(struct sl_shm));
    assert(shm);
    shm->ctx = ctx;
    shm->id = id;
    shm->host_global =
        sl_global_create(ctx, &wl_shm_interface, 1, shm, sl_bind_host_shm);
    shm->internal = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    assert(!ctx->shm);
    ctx->shm = shm;
  } else if (strcmp(interface, "wl_shell") == 0) {
    struct sl_shell* shell = malloc(sizeof(struct sl_shell));
    assert(shell);
    shell->ctx = ctx;
    shell->id = id;
    shell->host_global = sl_global_create(ctx, &wl_shell_interface, 1, shell,
                                          sl_bind_host_shell);
    assert(!ctx->shell);
    ctx->shell = shell;
  } else if (strcmp(interface, "wl_output") == 0) {
    struct sl_output* output = malloc(sizeof(struct sl_output));
    assert(output);
    output->ctx = ctx;
    output->id = id;
    output->version = MIN(3, version);
    output->host_global =
        sl_global_create(ctx, &wl_output_interface, output->version, output,
                         sl_bind_host_output);
    wl_list_insert(&ctx->outputs, &output->link);
  } else if (strcmp(interface, "wl_seat") == 0) {
    struct sl_seat* seat = malloc(sizeof(struct sl_seat));
    assert(seat);
    seat->ctx = ctx;
    seat->id = id;
    seat->version = MIN(5, version);
    seat->last_serial = 0;
    seat->host_global = sl_seat_global_create(seat);
    wl_list_insert(&ctx->seats, &seat->link);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
    struct sl_data_device_manager* data_device_manager =
        malloc(sizeof(struct sl_data_device_manager));
    assert(data_device_manager);
    data_device_manager->ctx = ctx;
    data_device_manager->id = id;
    data_device_manager->version = MIN(3, version);
    data_device_manager->internal = NULL;
    data_device_manager->host_global = NULL;
    assert(!ctx->data_device_manager);
    ctx->data_device_manager = data_device_manager;
    if (ctx->xwayland) {
      data_device_manager->internal =
          wl_registry_bind(registry, id, &wl_data_device_manager_interface,
                           data_device_manager->version);
    } else {
      data_device_manager->host_global =
          sl_data_device_manager_global_create(ctx);
    }
  } else if (strcmp(interface, "zxdg_shell_v6") == 0) {
    struct sl_xdg_shell* xdg_shell = malloc(sizeof(struct sl_xdg_shell));
    assert(xdg_shell);
    xdg_shell->ctx = ctx;
    xdg_shell->id = id;
    xdg_shell->internal = NULL;
    xdg_shell->host_global = NULL;
    assert(!ctx->xdg_shell);
    ctx->xdg_shell = xdg_shell;
    if (ctx->xwayland) {
      xdg_shell->internal =
          wl_registry_bind(registry, id, &zxdg_shell_v6_interface, 1);
      zxdg_shell_v6_add_listener(xdg_shell->internal,
                                 &sl_internal_xdg_shell_listener, NULL);
    } else {
      xdg_shell->host_global = sl_xdg_shell_global_create(ctx);
    }
  } else if (strcmp(interface, "zaura_shell") == 0) {
    if (version >= MIN_AURA_SHELL_VERSION) {
      struct sl_aura_shell* aura_shell = malloc(sizeof(struct sl_aura_shell));
      assert(aura_shell);
      aura_shell->ctx = ctx;
      aura_shell->id = id;
      aura_shell->version = MIN(6, version);
      aura_shell->host_gtk_shell_global = NULL;
      aura_shell->internal = wl_registry_bind(
          registry, id, &zaura_shell_interface, aura_shell->version);
      assert(!ctx->aura_shell);
      ctx->aura_shell = aura_shell;
      aura_shell->host_gtk_shell_global = sl_gtk_shell_global_create(ctx);
    }
  } else if (strcmp(interface, "wp_viewporter") == 0) {
    struct sl_viewporter* viewporter = malloc(sizeof(struct sl_viewporter));
    assert(viewporter);
    viewporter->ctx = ctx;
    viewporter->id = id;
    viewporter->host_viewporter_global = NULL;
    viewporter->internal =
        wl_registry_bind(registry, id, &wp_viewporter_interface, 1);
    assert(!ctx->viewporter);
    ctx->viewporter = viewporter;
    viewporter->host_viewporter_global = sl_viewporter_global_create(ctx);
    // Allow non-integer scale.
    ctx->scale = MIN(MAX_SCALE, MAX(MIN_SCALE, ctx->desired_scale));
  } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
    struct sl_linux_dmabuf* linux_dmabuf =
        malloc(sizeof(struct sl_linux_dmabuf));
    assert(linux_dmabuf);
    linux_dmabuf->ctx = ctx;
    linux_dmabuf->id = id;
    linux_dmabuf->version = MIN(2, version);
    linux_dmabuf->internal = wl_registry_bind(
        registry, id, &zwp_linux_dmabuf_v1_interface, linux_dmabuf->version);
    assert(!ctx->linux_dmabuf);
    ctx->linux_dmabuf = linux_dmabuf;
    linux_dmabuf->host_drm_global = sl_drm_global_create(ctx);
  } else if (strcmp(interface, "zcr_keyboard_extension_v1") == 0) {
    struct sl_keyboard_extension* keyboard_extension =
        malloc(sizeof(struct sl_keyboard_extension));
    assert(keyboard_extension);
    keyboard_extension->ctx = ctx;
    keyboard_extension->id = id;
    keyboard_extension->internal =
        wl_registry_bind(registry, id, &zcr_keyboard_extension_v1_interface, 1);
    assert(!ctx->keyboard_extension);
    ctx->keyboard_extension = keyboard_extension;
  }
}

static void sl_registry_remover(void* data,
                                struct wl_registry* registry,
                                uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_output* output;
  struct sl_seat* seat;

  if (ctx->compositor && ctx->compositor->id == id) {
    sl_global_destroy(ctx->compositor->host_global);
    wl_compositor_destroy(ctx->compositor->internal);
    free(ctx->compositor);
    ctx->compositor = NULL;
    return;
  }
  if (ctx->subcompositor && ctx->subcompositor->id == id) {
    sl_global_destroy(ctx->subcompositor->host_global);
    wl_shm_destroy(ctx->shm->internal);
    free(ctx->subcompositor);
    ctx->subcompositor = NULL;
    return;
  }
  if (ctx->shm && ctx->shm->id == id) {
    sl_global_destroy(ctx->shm->host_global);
    free(ctx->shm);
    ctx->shm = NULL;
    return;
  }
  if (ctx->shell && ctx->shell->id == id) {
    sl_global_destroy(ctx->shell->host_global);
    free(ctx->shell);
    ctx->shell = NULL;
    return;
  }
  if (ctx->data_device_manager && ctx->data_device_manager->id == id) {
    if (ctx->data_device_manager->host_global)
      sl_global_destroy(ctx->data_device_manager->host_global);
    if (ctx->data_device_manager->internal)
      wl_data_device_manager_destroy(ctx->data_device_manager->internal);
    free(ctx->data_device_manager);
    ctx->data_device_manager = NULL;
    return;
  }
  if (ctx->xdg_shell && ctx->xdg_shell->id == id) {
    if (ctx->xdg_shell->host_global)
      sl_global_destroy(ctx->xdg_shell->host_global);
    if (ctx->xdg_shell->internal)
      zxdg_shell_v6_destroy(ctx->xdg_shell->internal);
    free(ctx->xdg_shell);
    ctx->xdg_shell = NULL;
    return;
  }
  if (ctx->aura_shell && ctx->aura_shell->id == id) {
    if (ctx->aura_shell->host_gtk_shell_global)
      sl_global_destroy(ctx->aura_shell->host_gtk_shell_global);
    zaura_shell_destroy(ctx->aura_shell->internal);
    free(ctx->aura_shell);
    ctx->aura_shell = NULL;
    return;
  }
  if (ctx->viewporter && ctx->viewporter->id == id) {
    wp_viewporter_destroy(ctx->viewporter->internal);
    free(ctx->viewporter);
    ctx->viewporter = NULL;
    return;
  }
  if (ctx->linux_dmabuf && ctx->linux_dmabuf->id == id) {
    if (ctx->linux_dmabuf->host_drm_global)
      sl_global_destroy(ctx->linux_dmabuf->host_drm_global);
    zwp_linux_dmabuf_v1_destroy(ctx->linux_dmabuf->internal);
    free(ctx->linux_dmabuf);
    ctx->linux_dmabuf = NULL;
    return;
  }
  if (ctx->keyboard_extension && ctx->keyboard_extension->id == id) {
    zcr_keyboard_extension_v1_destroy(ctx->keyboard_extension->internal);
    free(ctx->keyboard_extension);
    ctx->keyboard_extension = NULL;
    return;
  }
  wl_list_for_each(output, &ctx->outputs, link) {
    if (output->id == id) {
      sl_global_destroy(output->host_global);
      wl_list_remove(&output->link);
      free(output);
      return;
    }
  }
  wl_list_for_each(seat, &ctx->seats, link) {
    if (seat->id == id) {
      sl_global_destroy(seat->host_global);
      wl_list_remove(&seat->link);
      free(seat);
      return;
    }
  }

  // Not reached.
  assert(0);
}

static const struct wl_registry_listener sl_registry_listener = {
    sl_registry_handler, sl_registry_remover};

static int sl_handle_event(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  int count = 0;

  if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
    wl_client_flush(ctx->client);
    exit(EXIT_SUCCESS);
  }

  if (mask & WL_EVENT_READABLE)
    count = wl_display_dispatch(ctx->display);
  if (mask & WL_EVENT_WRITABLE)
    wl_display_flush(ctx->display);

  if (mask == 0) {
    count = wl_display_dispatch_pending(ctx->display);
    wl_display_flush(ctx->display);
  }

  return count;
}

static void sl_create_window(struct sl_context* ctx,
                             xcb_window_t id,
                             int x,
                             int y,
                             int width,
                             int height,
                             int border_width) {
  struct sl_window* window = malloc(sizeof(struct sl_window));
  uint32_t values[1];
  assert(window);
  window->ctx = ctx;
  window->id = id;
  window->frame_id = XCB_WINDOW_NONE;
  window->host_surface_id = 0;
  window->unpaired = 1;
  window->x = x;
  window->y = y;
  window->width = width;
  window->height = height;
  window->border_width = border_width;
  window->depth = 0;
  window->managed = 0;
  window->realized = 0;
  window->activated = 0;
  window->allow_resize = 1;
  window->transient_for = XCB_WINDOW_NONE;
  window->client_leader = XCB_WINDOW_NONE;
  window->decorated = 0;
  window->name = NULL;
  window->clazz = NULL;
  window->startup_id = NULL;
  window->size_flags = P_POSITION;
  window->min_width = 0;
  window->min_height = 0;
  window->max_width = 0;
  window->max_height = 0;
  window->xdg_surface = NULL;
  window->xdg_toplevel = NULL;
  window->xdg_popup = NULL;
  window->aura_surface = NULL;
  window->next_config.serial = 0;
  window->next_config.mask = 0;
  window->next_config.states_length = 0;
  window->pending_config.serial = 0;
  window->pending_config.mask = 0;
  window->pending_config.states_length = 0;
  wl_list_insert(&ctx->unpaired_windows, &window->link);
  values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE;
  xcb_change_window_attributes(ctx->connection, window->id, XCB_CW_EVENT_MASK,
                               values);
}

static void sl_destroy_window(struct sl_window* window) {
  if (window->frame_id != XCB_WINDOW_NONE)
    xcb_destroy_window(window->ctx->connection, window->frame_id);

  if (window->ctx->host_focus_window == window) {
    window->ctx->host_focus_window = NULL;
    window->ctx->needs_set_input_focus = 1;
  }

  if (window->xdg_popup)
    zxdg_popup_v6_destroy(window->xdg_popup);
  if (window->xdg_toplevel)
    zxdg_toplevel_v6_destroy(window->xdg_toplevel);
  if (window->xdg_surface)
    zxdg_surface_v6_destroy(window->xdg_surface);
  if (window->aura_surface)
    zaura_surface_destroy(window->aura_surface);

  if (window->name)
    free(window->name);
  if (window->clazz)
    free(window->clazz);
  if (window->startup_id)
    free(window->startup_id);

  wl_list_remove(&window->link);
  free(window);
}

static int sl_is_window(struct sl_window* window, xcb_window_t id) {
  if (window->id == id)
    return 1;

  if (window->frame_id != XCB_WINDOW_NONE) {
    if (window->frame_id == id)
      return 1;
  }

  return 0;
}

static struct sl_window* sl_lookup_window(struct sl_context* ctx,
                                          xcb_window_t id) {
  struct sl_window* window;

  wl_list_for_each(window, &ctx->windows, link) {
    if (sl_is_window(window, id))
      return window;
  }
  wl_list_for_each(window, &ctx->unpaired_windows, link) {
    if (sl_is_window(window, id))
      return window;
  }
  return NULL;
}

static int sl_is_our_window(struct sl_context* ctx, xcb_window_t id) {
  const xcb_setup_t* setup = xcb_get_setup(ctx->connection);

  return (id & ~setup->resource_id_mask) == setup->resource_id_base;
}

static void sl_handle_create_notify(struct sl_context* ctx,
                                    xcb_create_notify_event_t* event) {
  if (sl_is_our_window(ctx, event->window))
    return;

  sl_create_window(ctx, event->window, event->x, event->y, event->width,
                   event->height, event->border_width);
}

static void sl_handle_destroy_notify(struct sl_context* ctx,
                                     xcb_destroy_notify_event_t* event) {
  struct sl_window* window;

  if (sl_is_our_window(ctx, event->window))
    return;

  window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;

  sl_destroy_window(window);
}

static void sl_handle_reparent_notify(struct sl_context* ctx,
                                      xcb_reparent_notify_event_t* event) {
  struct sl_window* window;

  if (event->parent == ctx->screen->root) {
    int width = 1;
    int height = 1;
    int border_width = 0;

    // Return early if window is already tracked. This happens when we
    // reparent an unampped window back to the root window.
    window = sl_lookup_window(ctx, event->window);
    if (window)
      return;

    xcb_get_geometry_reply_t* geometry_reply = xcb_get_geometry_reply(
        ctx->connection, xcb_get_geometry(ctx->connection, event->window),
        NULL);

    if (geometry_reply) {
      width = geometry_reply->width;
      height = geometry_reply->height;
      border_width = geometry_reply->border_width;
      free(geometry_reply);
    }
    sl_create_window(ctx, event->window, event->x, event->y, width, height,
                     border_width);
    return;
  }

  if (sl_is_our_window(ctx, event->parent))
    return;

  window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;

  sl_destroy_window(window);
}

static void sl_handle_map_request(struct sl_context* ctx,
                                  xcb_map_request_event_t* event) {
  struct sl_window* window = sl_lookup_window(ctx, event->window);
  struct {
    int type;
    xcb_atom_t atom;
  } properties[] = {
      {PROPERTY_WM_NAME, XCB_ATOM_WM_NAME},
      {PROPERTY_WM_CLASS, XCB_ATOM_WM_CLASS},
      {PROPERTY_WM_TRANSIENT_FOR, XCB_ATOM_WM_TRANSIENT_FOR},
      {PROPERTY_WM_NORMAL_HINTS, XCB_ATOM_WM_NORMAL_HINTS},
      {PROPERTY_WM_CLIENT_LEADER, ctx->atoms[ATOM_WM_CLIENT_LEADER].value},
      {PROPERTY_MOTIF_WM_HINTS, ctx->atoms[ATOM_MOTIF_WM_HINTS].value},
      {PROPERTY_NET_STARTUP_ID, ctx->atoms[ATOM_NET_STARTUP_ID].value},
  };
  xcb_get_geometry_cookie_t geometry_cookie;
  xcb_get_property_cookie_t property_cookies[ARRAY_SIZE(properties)];
  struct sl_wm_size_hints size_hints = {0};
  struct sl_mwm_hints mwm_hints = {0};
  uint32_t values[5];
  int i;

  if (!window)
    return;

  assert(!sl_is_our_window(ctx, event->window));

  window->managed = 1;
  if (window->frame_id == XCB_WINDOW_NONE)
    geometry_cookie = xcb_get_geometry(ctx->connection, window->id);

  for (i = 0; i < ARRAY_SIZE(properties); ++i) {
    property_cookies[i] =
        xcb_get_property(ctx->connection, 0, window->id, properties[i].atom,
                         XCB_ATOM_ANY, 0, 2048);
  }

  if (window->frame_id == XCB_WINDOW_NONE) {
    xcb_get_geometry_reply_t* geometry_reply =
        xcb_get_geometry_reply(ctx->connection, geometry_cookie, NULL);
    if (geometry_reply) {
      window->x = geometry_reply->x;
      window->y = geometry_reply->y;
      window->width = geometry_reply->width;
      window->height = geometry_reply->height;
      window->depth = geometry_reply->depth;
      free(geometry_reply);
    }
  }

  free(window->name);
  window->name = NULL;
  free(window->clazz);
  window->clazz = NULL;
  free(window->startup_id);
  window->startup_id = NULL;
  window->transient_for = XCB_WINDOW_NONE;
  window->client_leader = XCB_WINDOW_NONE;
  window->decorated = 1;
  window->size_flags = 0;

  for (i = 0; i < ARRAY_SIZE(properties); ++i) {
    xcb_get_property_reply_t* reply =
        xcb_get_property_reply(ctx->connection, property_cookies[i], NULL);

    if (!reply)
      continue;

    if (reply->type == XCB_ATOM_NONE) {
      free(reply);
      continue;
    }

    switch (properties[i].type) {
    case PROPERTY_WM_NAME:
      window->name = strndup(xcb_get_property_value(reply),
                             xcb_get_property_value_length(reply));
      break;
    case PROPERTY_WM_CLASS: {
      // WM_CLASS property contains two consecutive null-terminated strings.
      // These specify the Instance and Class names. If a global app ID is
      // not set then use Class name for app ID.
      const char *value = xcb_get_property_value(reply);
      int value_length = xcb_get_property_value_length(reply);
      int instance_length = strnlen(value, value_length);
      if (value_length > instance_length) {
        window->clazz = strndup(value + instance_length + 1,
                                value_length - instance_length - 1);
      }
    } break;
    case PROPERTY_WM_TRANSIENT_FOR:
      if (xcb_get_property_value_length(reply) >= 4)
        window->transient_for = *((uint32_t *)xcb_get_property_value(reply));
      break;
    case PROPERTY_WM_NORMAL_HINTS:
      if (xcb_get_property_value_length(reply) >= sizeof(size_hints))
        memcpy(&size_hints, xcb_get_property_value(reply), sizeof(size_hints));
      break;
    case PROPERTY_WM_CLIENT_LEADER:
      if (xcb_get_property_value_length(reply) >= 4)
        window->client_leader = *((uint32_t *)xcb_get_property_value(reply));
      break;
    case PROPERTY_MOTIF_WM_HINTS:
      if (xcb_get_property_value_length(reply) >= sizeof(mwm_hints))
        memcpy(&mwm_hints, xcb_get_property_value(reply), sizeof(mwm_hints));
      break;
    case PROPERTY_NET_STARTUP_ID:
      window->startup_id = strndup(xcb_get_property_value(reply),
                                   xcb_get_property_value_length(reply));
      break;
    default:
      break;
    }
    free(reply);
  }

  if (mwm_hints.flags & MWM_HINTS_DECORATIONS) {
    if (mwm_hints.decorations & MWM_DECOR_ALL)
      window->decorated = ~mwm_hints.decorations & MWM_DECOR_TITLE;
    else
      window->decorated = mwm_hints.decorations & MWM_DECOR_TITLE;
  }

  // Allow user/program controlled position for transients.
  if (window->transient_for)
    window->size_flags |= size_hints.flags & (US_POSITION | P_POSITION);

  // If startup ID is not set, then try the client leader window.
  if (!window->startup_id && window->client_leader) {
    xcb_get_property_reply_t* reply = xcb_get_property_reply(
        ctx->connection,
        xcb_get_property(ctx->connection, 0, window->client_leader,
                         ctx->atoms[ATOM_NET_STARTUP_ID].value, XCB_ATOM_ANY, 0,
                         2048),
        NULL);
    if (reply) {
      if (reply->type != XCB_ATOM_NONE) {
        window->startup_id = strndup(xcb_get_property_value(reply),
                                     xcb_get_property_value_length(reply));
      }
      free(reply);
    }
  }

  window->size_flags |= size_hints.flags & (P_MIN_SIZE | P_MAX_SIZE);
  if (window->size_flags & P_MIN_SIZE) {
    window->min_width = size_hints.min_width;
    window->min_height = size_hints.min_height;
  }
  if (window->size_flags & P_MAX_SIZE) {
    window->max_width = size_hints.max_width;
    window->max_height = size_hints.max_height;
  }

  window->border_width = 0;
  sl_adjust_window_size_for_screen_size(window);
  if (!(window->size_flags & (US_POSITION | P_POSITION)))
    sl_adjust_window_position_for_screen_size(window);

  values[0] = window->width;
  values[1] = window->height;
  values[2] = 0;
  xcb_configure_window(ctx->connection, window->id,
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                           XCB_CONFIG_WINDOW_BORDER_WIDTH,
                       values);
  // This needs to match the frame extents of the X11 frame window used
  // for reparenting or applications tend to be confused. The actual window
  // frame size used by the host compositor can be different.
  values[0] = 0;
  values[1] = 0;
  values[2] = 0;
  values[3] = 0;
  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, window->id,
                      ctx->atoms[ATOM_NET_FRAME_EXTENTS].value,
                      XCB_ATOM_CARDINAL, 32, 4, values);

  // Remove weird gravities.
  values[0] = XCB_GRAVITY_NORTH_WEST;
  xcb_change_window_attributes(ctx->connection, window->id, XCB_CW_WIN_GRAVITY,
                               values);

  if (window->frame_id == XCB_WINDOW_NONE) {
    int depth = window->depth ? window->depth : ctx->screen->root_depth;

    values[0] = ctx->screen->black_pixel;
    values[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
    values[2] = ctx->colormaps[depth];

    window->frame_id = xcb_generate_id(ctx->connection);
    xcb_create_window(
        ctx->connection, depth, window->frame_id, ctx->screen->root, window->x,
        window->y, window->width, window->height, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, ctx->visual_ids[depth],
        XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP, values);
    values[0] = XCB_STACK_MODE_BELOW;
    xcb_configure_window(ctx->connection, window->frame_id,
                         XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_reparent_window(ctx->connection, window->id, window->frame_id, 0, 0);
  } else {
    values[0] = window->x;
    values[1] = window->y;
    values[2] = window->width;
    values[3] = window->height;
    values[4] = XCB_STACK_MODE_BELOW;
    xcb_configure_window(
        ctx->connection, window->frame_id,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_STACK_MODE,
        values);
  }

  sl_window_set_wm_state(window, WM_STATE_NORMAL);
  sl_send_configure_notify(window);

  xcb_map_window(ctx->connection, window->id);
  xcb_map_window(ctx->connection, window->frame_id);
}

static void sl_handle_map_notify(struct sl_context* ctx,
                                 xcb_map_notify_event_t* event) {}

static void sl_handle_unmap_notify(struct sl_context* ctx,
                                   xcb_unmap_notify_event_t* event) {
  struct sl_window* window;

  if (sl_is_our_window(ctx, event->window))
    return;

  if (event->response_type & SEND_EVENT_MASK)
    return;

  window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;

  if (ctx->host_focus_window == window) {
    ctx->host_focus_window = NULL;
    ctx->needs_set_input_focus = 1;
  }

  if (window->host_surface_id) {
    window->host_surface_id = 0;
    sl_window_update(window);
  }

  sl_window_set_wm_state(window, WM_STATE_WITHDRAWN);

  // Reparent window and destroy frame if it exists.
  if (window->frame_id != XCB_WINDOW_NONE) {
    xcb_reparent_window(ctx->connection, window->id, ctx->screen->root,
                        window->x, window->y);
    xcb_destroy_window(ctx->connection, window->frame_id);
    window->frame_id = XCB_WINDOW_NONE;
  }

  // Reset properties to unmanaged state in case the window transitions to
  // an override-redirect window.
  window->managed = 0;
  window->decorated = 0;
  window->size_flags = P_POSITION;
}

static void sl_handle_configure_request(struct sl_context* ctx,
                                        xcb_configure_request_event_t* event) {
  struct sl_window* window = sl_lookup_window(ctx, event->window);
  int width = window->width;
  int height = window->height;
  uint32_t values[7];

  assert(!sl_is_our_window(ctx, event->window));

  if (!window->managed) {
    int i = 0;

    if (event->value_mask & XCB_CONFIG_WINDOW_X)
      values[i++] = event->x;
    if (event->value_mask & XCB_CONFIG_WINDOW_Y)
      values[i++] = event->y;
    if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH)
      values[i++] = event->width;
    if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
      values[i++] = event->height;
    if (event->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
      values[i++] = event->border_width;
    if (event->value_mask & XCB_CONFIG_WINDOW_SIBLING)
      values[i++] = event->sibling;
    if (event->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
      values[i++] = event->stack_mode;

    xcb_configure_window(ctx->connection, window->id, event->value_mask,
                         values);
    return;
  }

  // Ack configure events as satisfying request removes the guarantee
  // that matching contents will arrive.
  if (window->xdg_toplevel) {
    if (window->pending_config.serial) {
      zxdg_surface_v6_ack_configure(window->xdg_surface,
                                    window->pending_config.serial);
      window->pending_config.serial = 0;
      window->pending_config.mask = 0;
      window->pending_config.states_length = 0;
    }
    if (window->next_config.serial) {
      zxdg_surface_v6_ack_configure(window->xdg_surface,
                                    window->next_config.serial);
      window->next_config.serial = 0;
      window->next_config.mask = 0;
      window->next_config.states_length = 0;
    }
  }

  if (event->value_mask & XCB_CONFIG_WINDOW_X)
    window->x = event->x;
  if (event->value_mask & XCB_CONFIG_WINDOW_Y)
    window->y = event->y;

  if (window->allow_resize) {
    if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH)
      window->width = event->width;
    if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
      window->height = event->height;
  }

  sl_adjust_window_size_for_screen_size(window);
  if (window->size_flags & (US_POSITION | P_POSITION))
    sl_window_update(window);
  else
    sl_adjust_window_position_for_screen_size(window);

  values[0] = window->x;
  values[1] = window->y;
  values[2] = window->width;
  values[3] = window->height;
  values[4] = 0;
  xcb_configure_window(ctx->connection, window->frame_id,
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                           XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                       values);

  // We need to send a synthetic configure notify if:
  // - Not changing the size, location, border width.
  // - Moving the window without resizing it or changing its border width.
  if (width != window->width || height != window->height ||
      window->border_width) {
    xcb_configure_window(ctx->connection, window->id,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         &values[2]);
    window->border_width = 0;
  } else {
    sl_send_configure_notify(window);
  }
}

static void sl_handle_configure_notify(struct sl_context* ctx,
                                       xcb_configure_notify_event_t* event) {
  struct sl_window* window;

  if (sl_is_our_window(ctx, event->window))
    return;

  if (event->window == ctx->screen->root) {
    xcb_get_geometry_reply_t* geometry_reply = xcb_get_geometry_reply(
        ctx->connection, xcb_get_geometry(ctx->connection, event->window),
        NULL);
    int width = ctx->screen->width_in_pixels;
    int height = ctx->screen->height_in_pixels;

    if (geometry_reply) {
      width = geometry_reply->width;
      height = geometry_reply->height;
      free(geometry_reply);
    }

    if (width == ctx->screen->width_in_pixels ||
        height == ctx->screen->height_in_pixels) {
      return;
    }

    ctx->screen->width_in_pixels = width;
    ctx->screen->height_in_pixels = height;

    // Re-center managed windows.
    wl_list_for_each(window, &ctx->windows, link) {
      int x, y;

      if (window->size_flags & (US_POSITION | P_POSITION))
        continue;

      x = window->x;
      y = window->y;
      sl_adjust_window_position_for_screen_size(window);
      if (window->x != x || window->y != y) {
        uint32_t values[2];

        values[0] = window->x;
        values[1] = window->y;
        xcb_configure_window(ctx->connection, window->frame_id,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
        sl_send_configure_notify(window);
      }
    }
    return;
  }

  window = sl_lookup_window(ctx, event->window);
  if (!window)
    return;

  if (window->managed)
    return;

  window->width = event->width;
  window->height = event->height;
  window->border_width = event->border_width;
  if (event->x != window->x || event->y != window->y) {
    window->x = event->x;
    window->y = event->y;
    sl_window_update(window);
  }
}

static uint32_t sl_resize_edge(int net_wm_moveresize_size) {
  switch (net_wm_moveresize_size) {
  case NET_WM_MOVERESIZE_SIZE_TOPLEFT:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT;
  case NET_WM_MOVERESIZE_SIZE_TOP:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP;
  case NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT;
  case NET_WM_MOVERESIZE_SIZE_RIGHT:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT;
  case NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT;
  case NET_WM_MOVERESIZE_SIZE_BOTTOM:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM;
  case NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT;
  case NET_WM_MOVERESIZE_SIZE_LEFT:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT;
  default:
    return ZXDG_TOPLEVEL_V6_RESIZE_EDGE_NONE;
  }
}

static void sl_handle_client_message(struct sl_context* ctx,
                                     xcb_client_message_event_t* event) {
  if (event->type == ctx->atoms[ATOM_WL_SURFACE_ID].value) {
    struct sl_window *window, *unpaired_window = NULL;

    wl_list_for_each(window, &ctx->unpaired_windows, link) {
      if (sl_is_window(window, event->window)) {
        unpaired_window = window;
        break;
      }
    }

    if (unpaired_window) {
      unpaired_window->host_surface_id = event->data.data32[0];
      sl_window_update(unpaired_window);
    }
  } else if (event->type == ctx->atoms[ATOM_NET_WM_MOVERESIZE].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);

    if (window && window->xdg_toplevel) {
      struct sl_host_seat* seat = window->ctx->default_seat;

      if (!seat)
        return;

      if (event->data.data32[2] == NET_WM_MOVERESIZE_MOVE) {
        zxdg_toplevel_v6_move(window->xdg_toplevel, seat->proxy,
                              seat->seat->last_serial);
      } else {
        uint32_t edge = sl_resize_edge(event->data.data32[2]);

        if (edge == ZXDG_TOPLEVEL_V6_RESIZE_EDGE_NONE)
          return;

        zxdg_toplevel_v6_resize(window->xdg_toplevel, seat->proxy,
                                seat->seat->last_serial, edge);
      }
    }
  } else if (event->type == ctx->atoms[ATOM_NET_WM_STATE].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);

    if (window && window->xdg_toplevel) {
      int changed[ATOM_LAST + 1];
      uint32_t action = event->data.data32[0];
      int i;

      for (i = 0; i < ARRAY_SIZE(ctx->atoms); ++i) {
        changed[i] = event->data.data32[1] == ctx->atoms[i].value ||
                     event->data.data32[2] == ctx->atoms[i].value;
      }

      if (changed[ATOM_NET_WM_STATE_FULLSCREEN]) {
        if (action == NET_WM_STATE_ADD)
          zxdg_toplevel_v6_set_fullscreen(window->xdg_toplevel, NULL);
        else if (action == NET_WM_STATE_REMOVE)
          zxdg_toplevel_v6_unset_fullscreen(window->xdg_toplevel);
      }

      if (changed[ATOM_NET_WM_STATE_MAXIMIZED_VERT] &&
          changed[ATOM_NET_WM_STATE_MAXIMIZED_HORZ]) {
        if (action == NET_WM_STATE_ADD)
          zxdg_toplevel_v6_set_maximized(window->xdg_toplevel);
        else if (action == NET_WM_STATE_REMOVE)
          zxdg_toplevel_v6_unset_maximized(window->xdg_toplevel);
      }
    }
  }
}

static void sl_handle_focus_in(struct sl_context* ctx,
                               xcb_focus_in_event_t* event) {}

static void sl_handle_focus_out(struct sl_context* ctx,
                                xcb_focus_out_event_t* event) {}

static int sl_handle_selection_fd_writable(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = data;
  uint8_t *value;
  int bytes, bytes_left;

  value = xcb_get_property_value(ctx->selection_property_reply);
  bytes_left = xcb_get_property_value_length(ctx->selection_property_reply) -
               ctx->selection_property_offset;

  bytes = write(fd, value + ctx->selection_property_offset, bytes_left);
  if (bytes == -1) {
    fprintf(stderr, "write error to target fd: %m\n");
    close(fd);
  } else if (bytes == bytes_left) {
    if (ctx->selection_incremental_transfer) {
      xcb_delete_property(ctx->connection, ctx->selection_window,
                          ctx->atoms[ATOM_WL_SELECTION].value);
    } else {
      close(fd);
    }
  } else {
    ctx->selection_property_offset += bytes;
    return 1;
  }

  free(ctx->selection_property_reply);
  ctx->selection_property_reply = NULL;
  if (ctx->selection_send_event_source) {
    wl_event_source_remove(ctx->selection_send_event_source);
    ctx->selection_send_event_source = NULL;
  }
  return 1;
}

static void sl_write_selection_property(struct sl_context* ctx,
                                        xcb_get_property_reply_t* reply) {
  ctx->selection_property_offset = 0;
  ctx->selection_property_reply = reply;
  sl_handle_selection_fd_writable(ctx->selection_data_source_send_fd,
                                  WL_EVENT_WRITABLE, ctx);

  if (!ctx->selection_property_reply)
    return;

  assert(!ctx->selection_send_event_source);
  ctx->selection_send_event_source = wl_event_loop_add_fd(
      wl_display_get_event_loop(ctx->host_display),
      ctx->selection_data_source_send_fd, WL_EVENT_WRITABLE,
      sl_handle_selection_fd_writable, ctx);
}

static void sl_send_selection_notify(struct sl_context* ctx,
                                     xcb_atom_t property) {
  xcb_selection_notify_event_t event = {
      .response_type = XCB_SELECTION_NOTIFY,
      .sequence = 0,
      .time = ctx->selection_request.time,
      .requestor = ctx->selection_request.requestor,
      .selection = ctx->selection_request.selection,
      .target = ctx->selection_request.target,
      .property = property,
      .pad0 = 0};

  xcb_send_event(ctx->connection, 0, ctx->selection_request.requestor,
                 XCB_EVENT_MASK_NO_EVENT, (char*)&event);
}

static void sl_send_selection_data(struct sl_context* ctx) {
  assert(!ctx->selection_data_ack_pending);
  xcb_change_property(
      ctx->connection, XCB_PROP_MODE_REPLACE, ctx->selection_request.requestor,
      ctx->selection_request.property, ctx->atoms[ATOM_UTF8_STRING].value, 8,
      ctx->selection_data.size, ctx->selection_data.data);
  ctx->selection_data_ack_pending = 1;
  ctx->selection_data.size = 0;
}

static const uint32_t sl_incr_chunk_size = 64 * 1024;

static int sl_handle_selection_fd_readable(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = data;
  int bytes, offset, bytes_left;
  void *p;

  offset = ctx->selection_data.size;
  if (ctx->selection_data.size < sl_incr_chunk_size)
    p = wl_array_add(&ctx->selection_data, sl_incr_chunk_size);
  else
    p = (char*)ctx->selection_data.data + ctx->selection_data.size;
  bytes_left = ctx->selection_data.alloc - offset;

  bytes = read(fd, p, bytes_left);
  if (bytes == -1) {
    fprintf(stderr, "read error from data source: %m\n");
    sl_send_selection_notify(ctx, XCB_ATOM_NONE);
    ctx->selection_data_offer_receive_fd = -1;
    close(fd);
  } else {
    ctx->selection_data.size = offset + bytes;
    if (ctx->selection_data.size >= sl_incr_chunk_size) {
      if (!ctx->selection_incremental_transfer) {
        ctx->selection_incremental_transfer = 1;
        xcb_change_property(
            ctx->connection, XCB_PROP_MODE_REPLACE,
            ctx->selection_request.requestor, ctx->selection_request.property,
            ctx->atoms[ATOM_INCR].value, 32, 1, &sl_incr_chunk_size);
        ctx->selection_data_ack_pending = 1;
        sl_send_selection_notify(ctx, ctx->selection_request.property);
      } else if (!ctx->selection_data_ack_pending) {
        sl_send_selection_data(ctx);
      }
    } else if (bytes == 0) {
      if (!ctx->selection_data_ack_pending)
        sl_send_selection_data(ctx);
      if (!ctx->selection_incremental_transfer) {
        sl_send_selection_notify(ctx, ctx->selection_request.property);
        ctx->selection_request.requestor = XCB_NONE;
        wl_array_release(&ctx->selection_data);
      }
      xcb_flush(ctx->connection);
      ctx->selection_data_offer_receive_fd = -1;
      close(fd);
    } else {
      ctx->selection_data.size = offset + bytes;
      return 1;
    }
  }

  wl_event_source_remove(ctx->selection_event_source);
  ctx->selection_event_source = NULL;
  return 1;
}

static void sl_handle_property_notify(struct sl_context* ctx,
                                      xcb_property_notify_event_t* event) {
  if (event->atom == XCB_ATOM_WM_NAME) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window)
      return;

    if (window->name) {
      free(window->name);
      window->name = NULL;
    }

    if (event->state != XCB_PROPERTY_DELETE) {
      xcb_get_property_reply_t* reply = xcb_get_property_reply(
          ctx->connection,
          xcb_get_property(ctx->connection, 0, window->id, XCB_ATOM_WM_NAME,
                           XCB_ATOM_ANY, 0, 2048),
          NULL);
      if (reply) {
        window->name = strndup(xcb_get_property_value(reply),
                               xcb_get_property_value_length(reply));
        free(reply);
      }
    }

    if (!window->xdg_toplevel)
      return;

    if (window->name) {
      zxdg_toplevel_v6_set_title(window->xdg_toplevel, window->name);
    } else {
      zxdg_toplevel_v6_set_title(window->xdg_toplevel, "");
    }
  } else if (event->atom == XCB_ATOM_WM_NORMAL_HINTS) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window)
      return;

    window->size_flags &= ~(P_MIN_SIZE | P_MAX_SIZE);

    if (event->state != XCB_PROPERTY_DELETE) {
      struct sl_wm_size_hints size_hints = {0};
      xcb_get_property_reply_t* reply = xcb_get_property_reply(
          ctx->connection,
          xcb_get_property(ctx->connection, 0, window->id,
                           XCB_ATOM_WM_NORMAL_HINTS, XCB_ATOM_ANY, 0,
                           sizeof(size_hints)),
          NULL);
      if (reply) {
        memcpy(&size_hints, xcb_get_property_value(reply), sizeof(size_hints));
        free(reply);
      }

      window->size_flags |= size_hints.flags & (P_MIN_SIZE | P_MAX_SIZE);
      if (window->size_flags & P_MIN_SIZE) {
        window->min_width = size_hints.min_width;
        window->min_height = size_hints.min_height;
      }
      if (window->size_flags & P_MAX_SIZE) {
        window->max_width = size_hints.max_width;
        window->max_height = size_hints.max_height;
      }
    }

    if (!window->xdg_toplevel)
      return;

    if (window->size_flags & P_MIN_SIZE) {
      zxdg_toplevel_v6_set_min_size(window->xdg_toplevel,
                                    window->min_width / ctx->scale,
                                    window->min_height / ctx->scale);
    } else {
      zxdg_toplevel_v6_set_min_size(window->xdg_toplevel, 0, 0);
    }

    if (window->size_flags & P_MAX_SIZE) {
      zxdg_toplevel_v6_set_max_size(window->xdg_toplevel,
                                    window->max_width / ctx->scale,
                                    window->max_height / ctx->scale);
    } else {
      zxdg_toplevel_v6_set_max_size(window->xdg_toplevel, 0, 0);
    }
  } else if (event->atom == ctx->atoms[ATOM_MOTIF_WM_HINTS].value) {
    struct sl_window* window = sl_lookup_window(ctx, event->window);
    if (!window)
      return;

    // Managed windows are decorated by default.
    window->decorated = window->managed;

    if (event->state != XCB_PROPERTY_DELETE) {
      struct sl_mwm_hints mwm_hints = {0};
      xcb_get_property_reply_t* reply = xcb_get_property_reply(
          ctx->connection,
          xcb_get_property(ctx->connection, 0, window->id,
                           ctx->atoms[ATOM_MOTIF_WM_HINTS].value, XCB_ATOM_ANY,
                           0, sizeof(mwm_hints)),
          NULL);
      if (reply) {
        if (mwm_hints.flags & MWM_HINTS_DECORATIONS) {
          if (mwm_hints.decorations & MWM_DECOR_ALL)
            window->decorated = ~mwm_hints.decorations & MWM_DECOR_TITLE;
          else
            window->decorated = mwm_hints.decorations & MWM_DECOR_TITLE;
        }
      }
    }

    if (!window->aura_surface)
      return;

    zaura_surface_set_frame(window->aura_surface,
                            window->decorated
                                ? ZAURA_SURFACE_FRAME_TYPE_NORMAL
                                : window->depth == 32
                                      ? ZAURA_SURFACE_FRAME_TYPE_NONE
                                      : ZAURA_SURFACE_FRAME_TYPE_SHADOW);
  } else if (event->atom == ctx->atoms[ATOM_WL_SELECTION].value) {
    if (event->window == ctx->selection_window &&
        event->state == XCB_PROPERTY_NEW_VALUE &&
        ctx->selection_incremental_transfer) {
      xcb_get_property_reply_t* reply = xcb_get_property_reply(
          ctx->connection,
          xcb_get_property(ctx->connection, 0, ctx->selection_window,
                           ctx->atoms[ATOM_WL_SELECTION].value,
                           XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff),
          NULL);

      if (!reply)
        return;

      if (xcb_get_property_value_length(reply) > 0) {
        sl_write_selection_property(ctx, reply);
      } else {
        assert(!ctx->selection_send_event_source);
        close(ctx->selection_data_source_send_fd);
        free(reply);
      }
    }
  } else if (event->atom == ctx->selection_request.property) {
    if (event->window == ctx->selection_request.requestor &&
        event->state == XCB_PROPERTY_DELETE &&
        ctx->selection_incremental_transfer) {
      int data_size = ctx->selection_data.size;

      ctx->selection_data_ack_pending = 0;

      // Handle the case when there's more data to be received.
      if (ctx->selection_data_offer_receive_fd >= 0) {
        // Avoid sending empty data until transfer is complete.
        if (data_size)
          sl_send_selection_data(ctx);

        if (!ctx->selection_event_source) {
          ctx->selection_event_source = wl_event_loop_add_fd(
              wl_display_get_event_loop(ctx->host_display),
              ctx->selection_data_offer_receive_fd, WL_EVENT_READABLE,
              sl_handle_selection_fd_readable, ctx);
        }
        return;
      }

      sl_send_selection_data(ctx);

      // Release data if transfer is complete.
      if (!data_size) {
        ctx->selection_request.requestor = XCB_NONE;
        wl_array_release(&ctx->selection_data);
      }
    }
  }
}

static void sl_internal_data_source_target(void* data,
                                           struct wl_data_source* data_source,
                                           const char* mime_type) {}

static void sl_internal_data_source_send(void* data,
                                         struct wl_data_source* data_source,
                                         const char* mime_type,
                                         int32_t fd) {
  struct sl_data_source* host = data;
  struct sl_context* ctx = host->ctx;

  if (strcmp(mime_type, sl_utf8_mime_type) == 0) {
    int flags;
    int rv;

    xcb_convert_selection(
        ctx->connection, ctx->selection_window,
        ctx->atoms[ATOM_CLIPBOARD].value, ctx->atoms[ATOM_UTF8_STRING].value,
        ctx->atoms[ATOM_WL_SELECTION].value, XCB_CURRENT_TIME);

    flags = fcntl(fd, F_GETFL, 0);
    rv = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    assert(!rv);
    UNUSED(rv);

    ctx->selection_data_source_send_fd = fd;
  } else {
    close(fd);
  }
}

static void sl_internal_data_source_cancelled(
    void* data, struct wl_data_source* data_source) {
  struct sl_data_source* host = data;

  if (host->ctx->selection_data_source == host)
    host->ctx->selection_data_source = NULL;

  wl_data_source_destroy(data_source);
}

static const struct wl_data_source_listener sl_internal_data_source_listener = {
    sl_internal_data_source_target, sl_internal_data_source_send,
    sl_internal_data_source_cancelled};

static void sl_get_selection_targets(struct sl_context* ctx) {
  struct sl_data_source* data_source = NULL;
  xcb_get_property_reply_t *reply;
  xcb_atom_t *value;
  uint32_t i;

  reply = xcb_get_property_reply(
      ctx->connection,
      xcb_get_property(ctx->connection, 1, ctx->selection_window,
                       ctx->atoms[ATOM_WL_SELECTION].value,
                       XCB_GET_PROPERTY_TYPE_ANY, 0, 4096),
      NULL);
  if (!reply)
    return;

  if (reply->type != XCB_ATOM_ATOM) {
    free(reply);
    return;
  }

  if (ctx->data_device_manager) {
    data_source = malloc(sizeof(*data_source));
    assert(data_source);

    data_source->ctx = ctx;
    data_source->internal = wl_data_device_manager_create_data_source(
        ctx->data_device_manager->internal);
    wl_data_source_add_listener(data_source->internal,
                                &sl_internal_data_source_listener, data_source);

    value = xcb_get_property_value(reply);
    for (i = 0; i < reply->value_len; i++) {
      if (value[i] == ctx->atoms[ATOM_UTF8_STRING].value)
        wl_data_source_offer(data_source->internal, sl_utf8_mime_type);
    }

    if (ctx->selection_data_device && ctx->default_seat) {
      wl_data_device_set_selection(ctx->selection_data_device,
                                   data_source->internal,
                                   ctx->default_seat->seat->last_serial);
    }

    if (ctx->selection_data_source) {
      wl_data_source_destroy(ctx->selection_data_source->internal);
      free(ctx->selection_data_source);
    }
    ctx->selection_data_source = data_source;
  }

  free(reply);
}

static void sl_get_selection_data(struct sl_context* ctx) {
  xcb_get_property_reply_t* reply = xcb_get_property_reply(
      ctx->connection,
      xcb_get_property(ctx->connection, 1, ctx->selection_window,
                       ctx->atoms[ATOM_WL_SELECTION].value,
                       XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff),
      NULL);
  if (!reply)
    return;

  if (reply->type == ctx->atoms[ATOM_INCR].value) {
    ctx->selection_incremental_transfer = 1;
    free(reply);
  } else {
    ctx->selection_incremental_transfer = 0;
    sl_write_selection_property(ctx, reply);
  }
}

static void sl_handle_selection_notify(struct sl_context* ctx,
                                       xcb_selection_notify_event_t* event) {
  if (event->property == XCB_ATOM_NONE)
    return;

  if (event->target == ctx->atoms[ATOM_TARGETS].value)
    sl_get_selection_targets(ctx);
  else
    sl_get_selection_data(ctx);
}

static void sl_send_targets(struct sl_context* ctx) {
  xcb_atom_t targets[] = {
      ctx->atoms[ATOM_TIMESTAMP].value, ctx->atoms[ATOM_TARGETS].value,
      ctx->atoms[ATOM_UTF8_STRING].value, ctx->atoms[ATOM_TEXT].value,
  };

  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE,
                      ctx->selection_request.requestor,
                      ctx->selection_request.property, XCB_ATOM_ATOM, 32,
                      ARRAY_SIZE(targets), targets);

  sl_send_selection_notify(ctx, ctx->selection_request.property);
}

static void sl_send_timestamp(struct sl_context* ctx) {
  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE,
                      ctx->selection_request.requestor,
                      ctx->selection_request.property, XCB_ATOM_INTEGER, 32, 1,
                      &ctx->selection_timestamp);

  sl_send_selection_notify(ctx, ctx->selection_request.property);
}

static void sl_send_data(struct sl_context* ctx) {
  int rv;

  if (!ctx->selection_data_offer || !ctx->selection_data_offer->utf8_text) {
    sl_send_selection_notify(ctx, XCB_ATOM_NONE);
    return;
  }

  if (ctx->selection_event_source) {
    fprintf(stderr, "error: selection transfer already pending\n");
    sl_send_selection_notify(ctx, XCB_ATOM_NONE);
    return;
  }

  wl_array_init(&ctx->selection_data);
  ctx->selection_data_ack_pending = 0;

  switch (ctx->data_driver) {
    case DATA_DRIVER_VIRTWL: {
      struct virtwl_ioctl_new new_pipe = {
          .type = VIRTWL_IOCTL_NEW_PIPE_READ, .fd = -1, .flags = 0, .size = 0,
      };

      rv = ioctl(ctx->virtwl_fd, VIRTWL_IOCTL_NEW, &new_pipe);
      if (rv) {
        fprintf(stderr, "error: failed to create virtwl pipe: %s\n",
                strerror(errno));
        sl_send_selection_notify(ctx, XCB_ATOM_NONE);
        return;
      }

      ctx->selection_data_offer_receive_fd = new_pipe.fd;
      wl_data_offer_receive(ctx->selection_data_offer->internal,
                            sl_utf8_mime_type, new_pipe.fd);
    } break;
    case DATA_DRIVER_NOOP: {
      int p[2];

      rv = pipe2(p, O_CLOEXEC | O_NONBLOCK);
      assert(!rv);

      ctx->selection_data_offer_receive_fd = p[0];
      wl_data_offer_receive(ctx->selection_data_offer->internal,
                            sl_utf8_mime_type, p[1]);
      close(p[1]);
    } break;
  }

  ctx->selection_event_source = wl_event_loop_add_fd(
      wl_display_get_event_loop(ctx->host_display),
      ctx->selection_data_offer_receive_fd, WL_EVENT_READABLE,
      sl_handle_selection_fd_readable, ctx);
}

static void sl_handle_selection_request(struct sl_context* ctx,
                                        xcb_selection_request_event_t* event) {
  ctx->selection_request = *event;
  ctx->selection_incremental_transfer = 0;

  if (event->selection == ctx->atoms[ATOM_CLIPBOARD_MANAGER].value) {
    sl_send_selection_notify(ctx, ctx->selection_request.property);
    return;
  }

  if (event->target == ctx->atoms[ATOM_TARGETS].value) {
    sl_send_targets(ctx);
  } else if (event->target == ctx->atoms[ATOM_TIMESTAMP].value) {
    sl_send_timestamp(ctx);
  } else if (event->target == ctx->atoms[ATOM_UTF8_STRING].value ||
             event->target == ctx->atoms[ATOM_TEXT].value) {
    sl_send_data(ctx);
  } else {
    sl_send_selection_notify(ctx, XCB_ATOM_NONE);
  }
}

static void sl_handle_xfixes_selection_notify(
    struct sl_context* ctx, xcb_xfixes_selection_notify_event_t* event) {
  if (event->selection != ctx->atoms[ATOM_CLIPBOARD].value)
    return;

  if (event->owner == XCB_WINDOW_NONE) {
    // If client selection is gone. Set NULL selection for each seat.
    if (ctx->selection_owner != ctx->selection_window) {
      if (ctx->selection_data_device && ctx->default_seat) {
        wl_data_device_set_selection(ctx->selection_data_device, NULL,
                                     ctx->default_seat->seat->last_serial);
      }
    }
    ctx->selection_owner = XCB_WINDOW_NONE;
    return;
  }

  ctx->selection_owner = event->owner;

  // Save timestamp if it's our selection.
  if (event->owner == ctx->selection_window) {
    ctx->selection_timestamp = event->timestamp;
    return;
  }

  ctx->selection_incremental_transfer = 0;
  xcb_convert_selection(ctx->connection, ctx->selection_window,
                        ctx->atoms[ATOM_CLIPBOARD].value,
                        ctx->atoms[ATOM_TARGETS].value,
                        ctx->atoms[ATOM_WL_SELECTION].value, event->timestamp);
}

static int sl_handle_x_connection_event(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  xcb_generic_event_t *event;
  uint32_t count = 0;

  if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR))
    return 0;

  while ((event = xcb_poll_for_event(ctx->connection))) {
    switch (event->response_type & ~SEND_EVENT_MASK) {
    case XCB_CREATE_NOTIFY:
      sl_handle_create_notify(ctx, (xcb_create_notify_event_t*)event);
      break;
    case XCB_DESTROY_NOTIFY:
      sl_handle_destroy_notify(ctx, (xcb_destroy_notify_event_t*)event);
      break;
    case XCB_REPARENT_NOTIFY:
      sl_handle_reparent_notify(ctx, (xcb_reparent_notify_event_t*)event);
      break;
    case XCB_MAP_REQUEST:
      sl_handle_map_request(ctx, (xcb_map_request_event_t*)event);
      break;
    case XCB_MAP_NOTIFY:
      sl_handle_map_notify(ctx, (xcb_map_notify_event_t*)event);
      break;
    case XCB_UNMAP_NOTIFY:
      sl_handle_unmap_notify(ctx, (xcb_unmap_notify_event_t*)event);
      break;
    case XCB_CONFIGURE_REQUEST:
      sl_handle_configure_request(ctx, (xcb_configure_request_event_t*)event);
      break;
    case XCB_CONFIGURE_NOTIFY:
      sl_handle_configure_notify(ctx, (xcb_configure_notify_event_t*)event);
      break;
    case XCB_CLIENT_MESSAGE:
      sl_handle_client_message(ctx, (xcb_client_message_event_t*)event);
      break;
    case XCB_FOCUS_IN:
      sl_handle_focus_in(ctx, (xcb_focus_in_event_t*)event);
      break;
    case XCB_FOCUS_OUT:
      sl_handle_focus_out(ctx, (xcb_focus_out_event_t*)event);
      break;
    case XCB_PROPERTY_NOTIFY:
      sl_handle_property_notify(ctx, (xcb_property_notify_event_t*)event);
      break;
    case XCB_SELECTION_NOTIFY:
      sl_handle_selection_notify(ctx, (xcb_selection_notify_event_t*)event);
      break;
    case XCB_SELECTION_REQUEST:
      sl_handle_selection_request(ctx, (xcb_selection_request_event_t*)event);
      break;
    }

    switch (event->response_type - ctx->xfixes_extension->first_event) {
      case XCB_XFIXES_SELECTION_NOTIFY:
        sl_handle_xfixes_selection_notify(
            ctx, (xcb_xfixes_selection_notify_event_t*)event);
        break;
    }

    free(event);
    ++count;
  }

  if ((mask & ~WL_EVENT_WRITABLE) == 0)
    xcb_flush(ctx->connection);

  return count;
}

static void sl_connect(struct sl_context* ctx) {
  const char wm_name[] = "Sommelier";
  const xcb_setup_t *setup;
  xcb_screen_iterator_t screen_iterator;
  uint32_t values[1];
  xcb_void_cookie_t change_attributes_cookie, redirect_subwindows_cookie;
  xcb_generic_error_t *error;
  xcb_intern_atom_reply_t *atom_reply;
  xcb_depth_iterator_t depth_iterator;
  xcb_xfixes_query_version_reply_t *xfixes_query_version_reply;
  const xcb_query_extension_reply_t *composite_extension;
  unsigned i;

  ctx->connection = xcb_connect_to_fd(ctx->wm_fd, NULL);
  assert(!xcb_connection_has_error(ctx->connection));

  xcb_prefetch_extension_data(ctx->connection, &xcb_xfixes_id);
  xcb_prefetch_extension_data(ctx->connection, &xcb_composite_id);

  for (i = 0; i < ARRAY_SIZE(ctx->atoms); ++i) {
    const char* name = ctx->atoms[i].name;
    ctx->atoms[i].cookie =
        xcb_intern_atom(ctx->connection, 0, strlen(name), name);
  }

  setup = xcb_get_setup(ctx->connection);
  screen_iterator = xcb_setup_roots_iterator(setup);
  ctx->screen = screen_iterator.data;

  // Select for substructure redirect.
  values[0] = XCB_EVENT_MASK_STRUCTURE_NOTIFY |
              XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
              XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
  change_attributes_cookie = xcb_change_window_attributes(
      ctx->connection, ctx->screen->root, XCB_CW_EVENT_MASK, values);

  ctx->connection_event_source = wl_event_loop_add_fd(
      wl_display_get_event_loop(ctx->host_display),
      xcb_get_file_descriptor(ctx->connection), WL_EVENT_READABLE,
      &sl_handle_x_connection_event, ctx);

  ctx->xfixes_extension =
      xcb_get_extension_data(ctx->connection, &xcb_xfixes_id);
  assert(ctx->xfixes_extension->present);

  xfixes_query_version_reply = xcb_xfixes_query_version_reply(
      ctx->connection,
      xcb_xfixes_query_version(ctx->connection, XCB_XFIXES_MAJOR_VERSION,
                               XCB_XFIXES_MINOR_VERSION),
      NULL);
  assert(xfixes_query_version_reply);
  assert(xfixes_query_version_reply->major_version >= 5);
  free(xfixes_query_version_reply);

  composite_extension =
      xcb_get_extension_data(ctx->connection, &xcb_composite_id);
  assert(composite_extension->present);
  UNUSED(composite_extension);

  redirect_subwindows_cookie = xcb_composite_redirect_subwindows_checked(
      ctx->connection, ctx->screen->root, XCB_COMPOSITE_REDIRECT_MANUAL);

  // Another window manager should not be running.
  error = xcb_request_check(ctx->connection, change_attributes_cookie);
  assert(!error);

  // Redirecting subwindows of root for compositing should have succeeded.
  error = xcb_request_check(ctx->connection, redirect_subwindows_cookie);
  assert(!error);

  ctx->window = xcb_generate_id(ctx->connection);
  xcb_create_window(ctx->connection, 0, ctx->window, ctx->screen->root, 0, 0, 1,
                    1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0,
                    NULL);

  for (i = 0; i < ARRAY_SIZE(ctx->atoms); ++i) {
    atom_reply =
        xcb_intern_atom_reply(ctx->connection, ctx->atoms[i].cookie, &error);
    assert(!error);
    ctx->atoms[i].value = atom_reply->atom;
    free(atom_reply);
  }

  depth_iterator = xcb_screen_allowed_depths_iterator(ctx->screen);
  while (depth_iterator.rem > 0) {
    int depth = depth_iterator.data->depth;
    if (depth == ctx->screen->root_depth) {
      ctx->visual_ids[depth] = ctx->screen->root_visual;
      ctx->colormaps[depth] = ctx->screen->default_colormap;
    } else {
      xcb_visualtype_iterator_t visualtype_iterator =
          xcb_depth_visuals_iterator(depth_iterator.data);

      ctx->visual_ids[depth] = visualtype_iterator.data->visual_id;
      ctx->colormaps[depth] = xcb_generate_id(ctx->connection);
      xcb_create_colormap(ctx->connection, XCB_COLORMAP_ALLOC_NONE,
                          ctx->colormaps[depth], ctx->screen->root,
                          ctx->visual_ids[depth]);
    }
    xcb_depth_next(&depth_iterator);
  }
  assert(ctx->visual_ids[ctx->screen->root_depth]);

  if (ctx->clipboard_manager) {
    values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
    ctx->selection_window = xcb_generate_id(ctx->connection);
    xcb_create_window(ctx->connection, XCB_COPY_FROM_PARENT,
                      ctx->selection_window, ctx->screen->root, 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, ctx->screen->root_visual,
                      XCB_CW_EVENT_MASK, values);
    xcb_set_selection_owner(ctx->connection, ctx->selection_window,
                            ctx->atoms[ATOM_CLIPBOARD_MANAGER].value,
                            XCB_CURRENT_TIME);
    xcb_xfixes_select_selection_input(
        ctx->connection, ctx->selection_window,
        ctx->atoms[ATOM_CLIPBOARD].value,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE);
    sl_set_selection(ctx, NULL);
  }

  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, ctx->window,
                      ctx->atoms[ATOM_NET_SUPPORTING_WM_CHECK].value,
                      XCB_ATOM_WINDOW, 32, 1, &ctx->window);
  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, ctx->window,
                      ctx->atoms[ATOM_NET_WM_NAME].value,
                      ctx->atoms[ATOM_UTF8_STRING].value, 8, strlen(wm_name),
                      wm_name);
  xcb_change_property(ctx->connection, XCB_PROP_MODE_REPLACE, ctx->screen->root,
                      ctx->atoms[ATOM_NET_SUPPORTING_WM_CHECK].value,
                      XCB_ATOM_WINDOW, 32, 1, &ctx->window);
  xcb_set_selection_owner(ctx->connection, ctx->window,
                          ctx->atoms[ATOM_WM_S0].value, XCB_CURRENT_TIME);

  xcb_set_input_focus(ctx->connection, XCB_INPUT_FOCUS_NONE, XCB_NONE,
                      XCB_CURRENT_TIME);
  xcb_flush(ctx->connection);
}

static void sl_sd_notify(const char* state) {
  const char* socket_name;
  struct msghdr msghdr;
  struct iovec iovec;
  struct sockaddr_un addr;
  int fd;
  int rv;

  socket_name = getenv("NOTIFY_SOCKET");
  assert(socket_name);

  fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  assert(fd >= 0);

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_name, sizeof(addr.sun_path));

  memset(&iovec, 0, sizeof(iovec));
  iovec.iov_base = (char*)state;
  iovec.iov_len = strlen(state);

  memset(&msghdr, 0, sizeof(msghdr));
  msghdr.msg_name = &addr;
  msghdr.msg_namelen =
      offsetof(struct sockaddr_un, sun_path) + strlen(socket_name);
  msghdr.msg_iov = &iovec;
  msghdr.msg_iovlen = 1;

  rv = sendmsg(fd, &msghdr, MSG_NOSIGNAL);
  assert(rv != -1);
  UNUSED(rv);
}

static int sl_handle_sigchld(int signal_number, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  int status;
  pid_t pid;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (pid == ctx->child_pid) {
      ctx->child_pid = -1;
      if (WIFEXITED(status) && WEXITSTATUS(status)) {
        fprintf(stderr, "Child exited with status: %d\n", WEXITSTATUS(status));
      }
      if (ctx->exit_with_child) {
        if (ctx->xwayland_pid >= 0)
          kill(ctx->xwayland_pid, SIGTERM);
      } else {
        // Notify systemd that we are ready to accept connections now that
        // child process has finished running and all environment is ready.
        if (ctx->sd_notify)
          sl_sd_notify(ctx->sd_notify);
      }
    } else if (pid == ctx->xwayland_pid) {
      ctx->xwayland_pid = -1;
      if (WIFEXITED(status) && WEXITSTATUS(status)) {
        fprintf(stderr, "Xwayland exited with status: %d\n",
                WEXITSTATUS(status));
        exit(WEXITSTATUS(status));
      }
    }
  }

  return 1;
}

static void sl_execvp(const char* file,
                      char* const argv[],
                      int wayland_socked_fd) {
  if (wayland_socked_fd >= 0) {
    char fd_str[8];
    int fd;

    fd = dup(wayland_socked_fd);
    snprintf(fd_str, sizeof(fd_str), "%d", fd);
    setenv("WAYLAND_SOCKET", fd_str, 1);
  }

  setenv("SOMMELIER_VERSION", SOMMELIER_VERSION, 1);

  execvp(file, argv);
  perror(file);
}

static void sl_calculate_scale_for_xwayland(struct sl_context* ctx) {
  struct sl_host_output* output;
  double default_scale_factor = 1.0;
  double scale;

  // Find internal output and determine preferred scale factor.
  wl_list_for_each(output, &ctx->host_outputs, link) {
    if (output->internal) {
      double preferred_scale =
          sl_aura_scale_factor_to_double(output->preferred_scale);

      if (ctx->aura_shell) {
        double device_scale_factor =
            sl_aura_scale_factor_to_double(output->device_scale_factor);

        default_scale_factor = device_scale_factor * preferred_scale;
      }
      break;
    }
  }

  // We use the default scale factor multipled by desired scale set by the
  // user. This gives us HiDPI support by default but the user can still
  // adjust it if higher or lower density is preferred.
  scale = ctx->desired_scale * default_scale_factor;

  // Round to integer scale if wp_viewporter interface is not present.
  if (!ctx->viewporter)
    scale = round(scale);

  // Clamp and set scale.
  ctx->scale = MIN(MAX_SCALE, MAX(MIN_SCALE, scale));

  // Scale affects output state. Send updated output state to xwayland.
  wl_list_for_each(output, &ctx->host_outputs, link)
      sl_send_host_output_state(output);
}

static int sl_handle_display_ready_event(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  char display_name[9];
  char xcursor_size_str[8];
  int bytes_read = 0;
  pid_t pid;

  if (!(mask & WL_EVENT_READABLE))
    return 0;

  display_name[0] = ':';
  do {
    int bytes_left = sizeof(display_name) - bytes_read - 1;
    int bytes;

    if (!bytes_left)
      break;

    bytes = read(fd, &display_name[bytes_read + 1], bytes_left);
    if (!bytes)
      break;

    bytes_read += bytes;
  } while (display_name[bytes_read] != '\n');

  display_name[bytes_read] = '\0';
  setenv("DISPLAY", display_name, 1);

  sl_connect(ctx);

  wl_event_source_remove(ctx->display_ready_event_source);
  ctx->display_ready_event_source = NULL;
  close(fd);

  // Calculate scale now that the default scale factor is known. This also
  // happens to workaround an issue in Xwayland where an output update is
  // needed for DPI to be set correctly.
  sl_calculate_scale_for_xwayland(ctx);
  wl_display_flush_clients(ctx->host_display);

  snprintf(xcursor_size_str, sizeof(xcursor_size_str), "%d",
           (int)(XCURSOR_SIZE_BASE * ctx->scale + 0.5));
  setenv("XCURSOR_SIZE", xcursor_size_str, 1);

  pid = fork();
  assert(pid >= 0);
  if (pid == 0) {
    sl_execvp(ctx->runprog[0], ctx->runprog, -1);
    _exit(EXIT_FAILURE);
  }

  ctx->child_pid = pid;

  return 1;
}

static void sl_sigchld_handler(int signal) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    continue;
}

static void sl_client_destroy_notify(struct wl_listener* listener, void* data) {
  exit(0);
}

static void sl_registry_bind(struct wl_client* client,
                             struct wl_resource* resource,
                             uint32_t name,
                             const char* interface,
                             uint32_t version,
                             uint32_t id) {
  struct sl_host_registry* host = wl_resource_get_user_data(resource);
  struct sl_global* global;

  wl_list_for_each(global, &host->ctx->globals, link) {
    if (global->name == name)
      break;
  }

  assert(&global->link != &host->ctx->globals);
  assert(version != 0);
  assert(global->version >= version);

  global->bind(client, global->data, version, id);
}

static const struct wl_registry_interface sl_registry_implementation = {
    sl_registry_bind};

static void sl_sync_callback_done(void* data,
                                  struct wl_callback* callback,
                                  uint32_t serial) {
  struct sl_host_callback* host = wl_callback_get_user_data(callback);

  wl_callback_send_done(host->resource, serial);
  wl_resource_destroy(host->resource);
}

static const struct wl_callback_listener sl_sync_callback_listener = {
    sl_sync_callback_done};

static void sl_display_sync(struct wl_client* client,
                            struct wl_resource* resource,
                            uint32_t id) {
  struct sl_context* ctx = wl_resource_get_user_data(resource);
  struct sl_host_callback* host_callback;

  host_callback = malloc(sizeof(*host_callback));
  assert(host_callback);

  host_callback->resource =
      wl_resource_create(client, &wl_callback_interface, 1, id);
  wl_resource_set_implementation(host_callback->resource, NULL, host_callback,
                                 sl_host_callback_destroy);
  host_callback->proxy = wl_display_sync(ctx->display);
  wl_callback_set_user_data(host_callback->proxy, host_callback);
  wl_callback_add_listener(host_callback->proxy, &sl_sync_callback_listener,
                           host_callback);
}

static void sl_destroy_host_registry(struct wl_resource* resource) {
  struct sl_host_registry* host = wl_resource_get_user_data(resource);

  wl_list_remove(&host->link);
  free(host);
}

static void sl_display_get_registry(struct wl_client* client,
                                    struct wl_resource* resource,
                                    uint32_t id) {
  struct sl_context* ctx = wl_resource_get_user_data(resource);
  struct sl_host_registry* host_registry;
  struct sl_global* global;

  host_registry = malloc(sizeof(*host_registry));
  assert(host_registry);

  host_registry->ctx = ctx;
  host_registry->resource =
      wl_resource_create(client, &wl_registry_interface, 1, id);
  wl_list_insert(&ctx->registries, &host_registry->link);
  wl_resource_set_implementation(host_registry->resource,
                                 &sl_registry_implementation, host_registry,
                                 sl_destroy_host_registry);

  wl_list_for_each(global, &ctx->globals, link) {
    wl_resource_post_event(host_registry->resource, WL_REGISTRY_GLOBAL,
                           global->name, global->interface->name,
                           global->version);
  }
}

static const struct wl_display_interface sl_display_implementation = {
    sl_display_sync, sl_display_get_registry};

static enum wl_iterator_result sl_set_display_implementation(
    struct wl_resource* resource, void* user_data) {
  struct sl_context* ctx = (struct sl_context*)user_data;

  if (strcmp(wl_resource_get_class(resource), "wl_display") == 0) {
    wl_resource_set_implementation(resource, &sl_display_implementation, ctx,
                                   NULL);
    return WL_ITERATOR_STOP;
  }

  return WL_ITERATOR_CONTINUE;
}

static int sl_handle_virtwl_ctx_event(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  uint8_t ioctl_buffer[4096];
  struct virtwl_ioctl_txn *ioctl_recv = (struct virtwl_ioctl_txn *)ioctl_buffer;
  void *recv_data = ioctl_buffer + sizeof(struct virtwl_ioctl_txn);
  size_t max_recv_size = sizeof(ioctl_buffer) - sizeof(struct virtwl_ioctl_txn);
  char fd_buffer[CMSG_LEN(sizeof(int) * VIRTWL_SEND_MAX_ALLOCS)];
  struct msghdr msg = {0};
  struct iovec buffer_iov;
  ssize_t bytes;
  int fd_count;
  int rv;

  ioctl_recv->len = max_recv_size;
  rv = ioctl(fd, VIRTWL_IOCTL_RECV, ioctl_recv);
  if (rv) {
    close(ctx->virtwl_socket_fd);
    ctx->virtwl_socket_fd = -1;
    return 0;
  }

  buffer_iov.iov_base = recv_data;
  buffer_iov.iov_len = ioctl_recv->len;

  msg.msg_iov = &buffer_iov;
  msg.msg_iovlen = 1;
  msg.msg_control = fd_buffer;

  // Count how many FDs the kernel gave us.
  for (fd_count = 0; fd_count < VIRTWL_SEND_MAX_ALLOCS; fd_count++) {
    if (ioctl_recv->fds[fd_count] < 0)
      break;
  }
  if (fd_count) {
    struct cmsghdr *cmsg;

    // Need to set msg_controllen so CMSG_FIRSTHDR will return the first
    // cmsghdr. We copy every fd we just received from the ioctl into this
    // cmsghdr.
    msg.msg_controllen = sizeof(fd_buffer);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(fd_count * sizeof(int));
    memcpy(CMSG_DATA(cmsg), ioctl_recv->fds, fd_count * sizeof(int));
    msg.msg_controllen = cmsg->cmsg_len;
  }

  bytes = sendmsg(ctx->virtwl_socket_fd, &msg, MSG_NOSIGNAL);
  assert(bytes == ioctl_recv->len);
  UNUSED(bytes);

  while (fd_count--)
    close(ioctl_recv->fds[fd_count]);

  return 1;
}

static int sl_handle_virtwl_socket_event(int fd, uint32_t mask, void* data) {
  struct sl_context* ctx = (struct sl_context*)data;
  uint8_t ioctl_buffer[4096];
  struct virtwl_ioctl_txn *ioctl_send = (struct virtwl_ioctl_txn *)ioctl_buffer;
  void *send_data = ioctl_buffer + sizeof(struct virtwl_ioctl_txn);
  size_t max_send_size = sizeof(ioctl_buffer) - sizeof(struct virtwl_ioctl_txn);
  char fd_buffer[CMSG_LEN(sizeof(int) * VIRTWL_SEND_MAX_ALLOCS)];
  struct iovec buffer_iov;
  struct msghdr msg = {0};
  struct cmsghdr *cmsg;
  ssize_t bytes;
  int fd_count = 0;
  int rv;
  int i;

  buffer_iov.iov_base = send_data;
  buffer_iov.iov_len = max_send_size;

  msg.msg_iov = &buffer_iov;
  msg.msg_iovlen = 1;
  msg.msg_control = fd_buffer;
  msg.msg_controllen = sizeof(fd_buffer);

  bytes = recvmsg(ctx->virtwl_socket_fd, &msg, 0);
  assert(bytes > 0);

  // If there were any FDs recv'd by recvmsg, there will be some data in the
  // msg_control buffer. To get the FDs out we iterate all cmsghdr's within and
  // unpack the FDs if the cmsghdr type is SCM_RIGHTS.
  for (cmsg = msg.msg_controllen != 0 ? CMSG_FIRSTHDR(&msg) : NULL; cmsg;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    size_t cmsg_fd_count;

    if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
      continue;

    cmsg_fd_count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);

    // fd_count will never exceed VIRTWL_SEND_MAX_ALLOCS because the
    // control message buffer only allocates enough space for that many FDs.
    memcpy(&ioctl_send->fds[fd_count], CMSG_DATA(cmsg),
           cmsg_fd_count * sizeof(int));
    fd_count += cmsg_fd_count;
  }

  for (i = fd_count; i < VIRTWL_SEND_MAX_ALLOCS; ++i)
    ioctl_send->fds[i] = -1;

  // The FDs and data were extracted from the recvmsg call into the ioctl_send
  // structure which we now pass along to the kernel.
  ioctl_send->len = bytes;
  rv = ioctl(ctx->virtwl_ctx_fd, VIRTWL_IOCTL_SEND, ioctl_send);
  assert(!rv);
  UNUSED(rv);

  while (fd_count--)
    close(ioctl_send->fds[fd_count]);

  return 1;
}

// Break |str| into a sequence of zero or more nonempty arguments. No more
// than |argc| arguments will be added to |argv|. Returns the total number of
// argments found in |str|.
static int sl_parse_cmd_prefix(char* str, int argc, char** argv) {
  char *s = str;
  int n = 0;
  int delim = 0;

  do {
    // Look for ending delimiter if |delim| is set.
    if (delim) {
      if (*s == delim) {
        delim = 0;
        *s = '\0';
      }
      ++s;
    } else {
      // Skip forward to first non-space character.
      while (*s == ' ' && *s != '\0')
        ++s;

      // Check for quote delimiter.
      if (*s == '"') {
        delim = '"';
        ++s;
      } else {
        delim = ' ';
      }

      // Add string to arguments if there's room.
      if (n < argc)
        argv[n] = s;

      ++n;
    }
  } while (*s != '\0');

  return n;
}

static void sl_print_usage() {
  printf(
      "usage: sommelier [options] [program] [args...]\n\n"
      "options:\n"
      "  -h, --help\t\t\tPrint this help\n"
      "  -X\t\t\t\tEnable X11 forwarding\n"
      "  --master\t\t\tRun as master and spawn child processes\n"
      "  --socket=SOCKET\t\tName of socket to listen on\n"
      "  --display=DISPLAY\t\tWayland display to connect to\n"
      "  --shm-driver=DRIVER\t\tSHM driver to use (noop, dmabuf, virtwl)\n"
      "  --data-driver=DRIVER\t\tData driver to use (noop, virtwl)\n"
      "  --scale=SCALE\t\t\tScale factor for contents\n"
      "  --dpi=[DPI[,DPI...]]\t\tDPI buckets\n"
      "  --peer-cmd-prefix=PREFIX\tPeer process command line prefix\n"
      "  --accelerators=ACCELERATORS\tList of keyboard accelerators\n"
      "  --application-id=ID\t\tForced application ID for X11 clients\n"
      "  --x-display=DISPLAY\t\tX11 display to listen on\n"
      "  --xwayland-path=PATH\t\tPath to Xwayland executable\n"
      "  --xwayland-cmd-prefix=PREFIX\tXwayland command line prefix\n"
      "  --no-exit-with-child\t\tKeep process alive after child exists\n"
      "  --no-clipboard-manager\tDisable X11 clipboard manager\n"
      "  --frame-color=COLOR\t\tWindow frame color for X11 clients\n"
      "  --virtwl-device=DEVICE\tVirtWL device to use\n"
      "  --drm-device=DEVICE\t\tDRM device to use\n"
      "  --glamor\t\t\tUse glamor to accelerate X11 clients\n");
}

int main(int argc, char **argv) {
  struct sl_context ctx = {
      .runprog = NULL,
      .display = NULL,
      .host_display = NULL,
      .client = NULL,
      .compositor = NULL,
      .subcompositor = NULL,
      .shm = NULL,
      .shell = NULL,
      .data_device_manager = NULL,
      .xdg_shell = NULL,
      .aura_shell = NULL,
      .viewporter = NULL,
      .linux_dmabuf = NULL,
      .keyboard_extension = NULL,
      .display_event_source = NULL,
      .display_ready_event_source = NULL,
      .sigchld_event_source = NULL,
      .shm_driver = SHM_DRIVER_NOOP,
      .data_driver = DATA_DRIVER_NOOP,
      .wm_fd = -1,
      .virtwl_fd = -1,
      .virtwl_ctx_fd = -1,
      .virtwl_socket_fd = -1,
      .virtwl_ctx_event_source = NULL,
      .virtwl_socket_event_source = NULL,
      .drm_device = NULL,
      .gbm = NULL,
      .xwayland = 0,
      .xwayland_pid = -1,
      .child_pid = -1,
      .peer_pid = -1,
      .xkb_context = NULL,
      .next_global_id = 1,
      .connection = NULL,
      .connection_event_source = NULL,
      .xfixes_extension = NULL,
      .screen = NULL,
      .window = 0,
      .host_focus_window = NULL,
      .needs_set_input_focus = 0,
      .desired_scale = 1.0,
      .scale = 1.0,
      .application_id = NULL,
      .exit_with_child = 1,
      .sd_notify = NULL,
      .clipboard_manager = 0,
      .frame_color = 0,
      .has_frame_color = 0,
      .default_seat = NULL,
      .selection_window = XCB_WINDOW_NONE,
      .selection_owner = XCB_WINDOW_NONE,
      .selection_incremental_transfer = 0,
      .selection_request = {.requestor = XCB_NONE, .property = XCB_ATOM_NONE},
      .selection_timestamp = XCB_CURRENT_TIME,
      .selection_data_device = NULL,
      .selection_data_offer = NULL,
      .selection_data_source = NULL,
      .selection_data_source_send_fd = -1,
      .selection_send_event_source = NULL,
      .selection_property_reply = NULL,
      .selection_property_offset = 0,
      .selection_event_source = NULL,
      .selection_data_offer_receive_fd = -1,
      .selection_data_ack_pending = 0,
      .atoms =
          {
                  [ATOM_WM_S0] = {"WM_S0"},
                  [ATOM_WM_PROTOCOLS] = {"WM_PROTOCOLS"},
                  [ATOM_WM_STATE] = {"WM_STATE"},
                  [ATOM_WM_DELETE_WINDOW] = {"WM_DELETE_WINDOW"},
                  [ATOM_WM_TAKE_FOCUS] = {"WM_TAKE_FOCUS"},
                  [ATOM_WM_CLIENT_LEADER] = {"WM_CLIENT_LEADER"},
                  [ATOM_WL_SURFACE_ID] = {"WL_SURFACE_ID"},
                  [ATOM_UTF8_STRING] = {"UTF8_STRING"},
                  [ATOM_MOTIF_WM_HINTS] = {"_MOTIF_WM_HINTS"},
                  [ATOM_NET_FRAME_EXTENTS] = {"_NET_FRAME_EXTENTS"},
                  [ATOM_NET_STARTUP_ID] = {"_NET_STARTUP_ID"},
                  [ATOM_NET_SUPPORTING_WM_CHECK] = {"_NET_SUPPORTING_WM_CHECK"},
                  [ATOM_NET_WM_NAME] = {"_NET_WM_NAME"},
                  [ATOM_NET_WM_MOVERESIZE] = {"_NET_WM_MOVERESIZE"},
                  [ATOM_NET_WM_STATE] = {"_NET_WM_STATE"},
                  [ATOM_NET_WM_STATE_FULLSCREEN] = {"_NET_WM_STATE_FULLSCREEN"},
                  [ATOM_NET_WM_STATE_MAXIMIZED_VERT] =
                      {"_NET_WM_STATE_MAXIMIZED_VERT"},
                  [ATOM_NET_WM_STATE_MAXIMIZED_HORZ] =
                      {"_NET_WM_STATE_MAXIMIZED_HORZ"},
                  [ATOM_CLIPBOARD] = {"CLIPBOARD"},
                  [ATOM_CLIPBOARD_MANAGER] = {"CLIPBOARD_MANAGER"},
                  [ATOM_TARGETS] = {"TARGETS"},
                  [ATOM_TIMESTAMP] = {"TIMESTAMP"},
                  [ATOM_TEXT] = {"TEXT"},
                  [ATOM_INCR] = {"INCR"},
                  [ATOM_WL_SELECTION] = {"_WL_SELECTION"},
          },
      .visual_ids = {0},
      .colormaps = {0}};
  const char *display = getenv("SOMMELIER_DISPLAY");
  const char *scale = getenv("SOMMELIER_SCALE");
  const char* dpi = getenv("SOMMELIER_DPI");
  const char *clipboard_manager = getenv("SOMMELIER_CLIPBOARD_MANAGER");
  const char *frame_color = getenv("SOMMELIER_FRAME_COLOR");
  const char *virtwl_device = getenv("SOMMELIER_VIRTWL_DEVICE");
  const char *drm_device = getenv("SOMMELIER_DRM_DEVICE");
  const char *glamor = getenv("SOMMELIER_GLAMOR");
  const char *shm_driver = getenv("SOMMELIER_SHM_DRIVER");
  const char *data_driver = getenv("SOMMELIER_DATA_DRIVER");
  const char *peer_cmd_prefix = getenv("SOMMELIER_PEER_CMD_PREFIX");
  const char *xwayland_cmd_prefix = getenv("SOMMELIER_XWAYLAND_CMD_PREFIX");
  const char *accelerators = getenv("SOMMELIER_ACCELERATORS");
  const char *xwayland_path = getenv("SOMMELIER_XWAYLAND_PATH");
  const char *socket_name = "wayland-0";
  const char *runtime_dir;
  struct wl_event_loop *event_loop;
  struct wl_listener client_destroy_listener = {.notify =
                                                    sl_client_destroy_notify};
  int sv[2];
  pid_t pid;
  int virtwl_display_fd = -1;
  int xdisplay = -1;
  int master = 0;
  int client_fd = -1;
  int rv;
  int i;

  for (i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0 ||
        strcmp(arg, "-?") == 0) {
      sl_print_usage();
      return EXIT_SUCCESS;
    }
    if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("Version: %s\n", SOMMELIER_VERSION);
      return EXIT_SUCCESS;
    }
    if (strstr(arg, "--master") == arg) {
      master = 1;
    } else if (strstr(arg, "--socket") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      socket_name = s;
    } else if (strstr(arg, "--display") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      display = s;
    } else if (strstr(arg, "--shm-driver") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      shm_driver = s;
    } else if (strstr(arg, "--data-driver") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      data_driver = s;
    } else if (strstr(arg, "--peer-pid") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      ctx.peer_pid = atoi(s);
    } else if (strstr(arg, "--peer-cmd-prefix") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      peer_cmd_prefix = s;
    } else if (strstr(arg, "--xwayland-cmd-prefix") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      xwayland_cmd_prefix = s;
    } else if (strstr(arg, "--client-fd") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      client_fd = atoi(s);
    } else if (strstr(arg, "--scale") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      scale = s;
    } else if (strstr(arg, "--dpi") == arg) {
      const char* s = strchr(arg, '=');
      ++s;
      dpi = s;
    } else if (strstr(arg, "--accelerators") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      accelerators = s;
    } else if (strstr(arg, "--application-id") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      ctx.application_id = s;
    } else if (strstr(arg, "-X") == arg) {
      ctx.xwayland = 1;
    } else if (strstr(arg, "--x-display") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      xdisplay = atoi(s);
      // Automatically enable X forwarding if X display is specified.
      ctx.xwayland = 1;
    } else if (strstr(arg, "--xwayland-path") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      xwayland_path = s;
    } else if (strstr(arg, "--no-exit-with-child") == arg) {
      ctx.exit_with_child = 0;
    } else if (strstr(arg, "--sd-notify") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      ctx.sd_notify = s;
    } else if (strstr(arg, "--no-clipboard-manager") == arg) {
      clipboard_manager = "0";
    } else if (strstr(arg, "--frame-color") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      frame_color = s;
    } else if (strstr(arg, "--virtwl-device") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      virtwl_device = s;
    } else if (strstr(arg, "--drm-device") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      drm_device = s;
    } else if (strstr(arg, "--glamor") == arg) {
      glamor = "1";
    } else if (arg[0] == '-') {
      if (strcmp(arg, "--") != 0) {
        fprintf(stderr, "Option `%s' is unknown.\n", arg);
        return EXIT_FAILURE;
      }
      ctx.runprog = &argv[i + 1];
      break;
    } else {
      ctx.runprog = &argv[i];
      break;
    }
  }

  runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir) {
    fprintf(stderr, "error: XDG_RUNTIME_DIR not set in the environment\n");
    return EXIT_FAILURE;
  }

  if (master) {
    char lock_addr[UNIX_PATH_MAX + LOCK_SUFFIXLEN];
    struct sockaddr_un addr;
    struct sigaction sa;
    struct stat sock_stat;
    int lock_fd;
    int sock_fd;

    addr.sun_family = AF_LOCAL;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%s", runtime_dir,
             socket_name);

    snprintf(lock_addr, sizeof(lock_addr), "%s%s", addr.sun_path, LOCK_SUFFIX);

    lock_fd = open(lock_addr, O_CREAT | O_CLOEXEC,
                   (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP));
    assert(lock_fd >= 0);

    rv = flock(lock_fd, LOCK_EX | LOCK_NB);
    if (rv < 0) {
      fprintf(stderr,
              "error: unable to lock %s, is another compositor running?\n",
              lock_addr);
      return EXIT_FAILURE;
    }

    rv = stat(addr.sun_path, &sock_stat);
    if (rv >= 0) {
      if (sock_stat.st_mode & (S_IWUSR | S_IWGRP))
        unlink(addr.sun_path);
    } else {
      assert(errno == ENOENT);
    }

    sock_fd = socket(PF_LOCAL, SOCK_STREAM, 0);
    assert(sock_fd >= 0);

    rv = bind(sock_fd, (struct sockaddr *)&addr,
              offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path));
    assert(rv >= 0);

    rv = listen(sock_fd, 128);
    assert(rv >= 0);

    sa.sa_handler = sl_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    rv = sigaction(SIGCHLD, &sa, NULL);
    assert(rv >= 0);

    if (ctx.sd_notify)
      sl_sd_notify(ctx.sd_notify);

    do {
      struct ucred ucred;
      socklen_t length = sizeof(addr);

      client_fd = accept(sock_fd, (struct sockaddr *)&addr, &length);
      if (client_fd < 0) {
        fprintf(stderr, "error: failed to accept: %m\n");
        continue;
      }

      ucred.pid = -1;
      length = sizeof(ucred);
      rv = getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &length);

      pid = fork();
      assert(pid != -1);
      if (pid == 0) {
        char client_fd_str[64], peer_pid_str[64];
        char peer_cmd_prefix_str[1024];
        char *args[64];
        int i = 0, j;

        close(sock_fd);
        close(lock_fd);

        if (!peer_cmd_prefix)
          peer_cmd_prefix = PEER_CMD_PREFIX;

        if (peer_cmd_prefix) {
          snprintf(peer_cmd_prefix_str, sizeof(peer_cmd_prefix_str), "%s",
                   peer_cmd_prefix);

          i = sl_parse_cmd_prefix(peer_cmd_prefix_str, 32, args);
          if (i > 32) {
            fprintf(stderr, "error: too many arguments in cmd prefix: %d\n", i);
            i = 0;
          }
        }

        args[i++] = argv[0];
        snprintf(peer_pid_str, sizeof(peer_pid_str), "--peer-pid=%d",
                 ucred.pid);
        args[i++] = peer_pid_str;
        snprintf(client_fd_str, sizeof(client_fd_str), "--client-fd=%d",
                 client_fd);
        args[i++] = client_fd_str;

        // forward some flags.
        for (j = 1; j < argc; ++j) {
          char *arg = argv[j];
          if (strstr(arg, "--display") == arg ||
              strstr(arg, "--scale") == arg ||
              strstr(arg, "--accelerators") == arg ||
              strstr(arg, "--virtwl-device") == arg ||
              strstr(arg, "--drm-device") == arg ||
              strstr(arg, "--shm-driver") == arg ||
              strstr(arg, "--data-driver") == arg) {
            args[i++] = arg;
          }
        }

        args[i++] = NULL;

        execvp(args[0], args);
        _exit(EXIT_FAILURE);
      }
      close(client_fd);
    } while (1);

    _exit(EXIT_FAILURE);
  }

  if (client_fd == -1) {
    if (!ctx.runprog || !ctx.runprog[0]) {
      sl_print_usage();
      return EXIT_FAILURE;
    }
  }

  if (ctx.xwayland) {
    assert(client_fd == -1);

    ctx.clipboard_manager = 1;
    if (clipboard_manager)
      ctx.clipboard_manager = !!strcmp(clipboard_manager, "0");
  }

  if (scale) {
    ctx.desired_scale = atof(scale);
    // Round to integer scale until we detect wp_viewporter support.
    ctx.scale = MIN(MAX_SCALE, MAX(MIN_SCALE, round(ctx.desired_scale)));
  }

  if (frame_color) {
    int r, g, b;
    if (sscanf(frame_color, "#%02x%02x%02x", &r, &g, &b) == 3) {
      ctx.frame_color = 0xff000000 | (r << 16) | (g << 8) | (b << 0);
      ctx.has_frame_color = 1;
    }
  }

  // Handle broken pipes without signals that kill the entire process.
  signal(SIGPIPE, SIG_IGN);

  ctx.host_display = wl_display_create();
  assert(ctx.host_display);

  event_loop = wl_display_get_event_loop(ctx.host_display);

  if (!virtwl_device)
    virtwl_device = VIRTWL_DEVICE;

  if (virtwl_device) {
    struct virtwl_ioctl_new new_ctx = {
        .type = VIRTWL_IOCTL_NEW_CTX, .fd = -1, .flags = 0, .size = 0,
    };

    ctx.virtwl_fd = open(virtwl_device, O_RDWR);
    if (ctx.virtwl_fd == -1) {
      fprintf(stderr, "error: could not open %s (%s)\n", virtwl_device,
              strerror(errno));
      return EXIT_FAILURE;
    }

    // We use a virtwl context unless display was explicitly specified.
    // WARNING: It's critical that we never call wl_display_roundtrip
    // as we're not spawning a new thread to handle forwarding. Calling
    // wl_display_roundtrip will cause a deadlock.
    if (!display) {
      int vws[2];

      // Connection to virtwl channel.
      rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, vws);
      assert(!rv);

      ctx.virtwl_socket_fd = vws[0];
      virtwl_display_fd = vws[1];

      rv = ioctl(ctx.virtwl_fd, VIRTWL_IOCTL_NEW, &new_ctx);
      if (rv) {
        fprintf(stderr, "error: failed to create virtwl context: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
      }

      ctx.virtwl_ctx_fd = new_ctx.fd;

      ctx.virtwl_socket_event_source = wl_event_loop_add_fd(
          event_loop, ctx.virtwl_socket_fd, WL_EVENT_READABLE,
          sl_handle_virtwl_socket_event, &ctx);
      ctx.virtwl_ctx_event_source =
          wl_event_loop_add_fd(event_loop, ctx.virtwl_ctx_fd, WL_EVENT_READABLE,
                               sl_handle_virtwl_ctx_event, &ctx);
    }
  }

  if (drm_device) {
    int drm_fd = open(drm_device, O_RDWR | O_CLOEXEC);
    if (drm_fd == -1) {
      fprintf(stderr, "error: could not open %s (%s)\n", drm_device,
              strerror(errno));
      return EXIT_FAILURE;
    }

    ctx.gbm = gbm_create_device(drm_fd);
    if (!ctx.gbm) {
      fprintf(stderr, "error: couldn't get display device\n");
      return EXIT_FAILURE;
    }

    ctx.drm_device = drm_device;
  }

  if (!shm_driver)
    shm_driver = ctx.xwayland ? XWAYLAND_SHM_DRIVER : SHM_DRIVER;

  if (shm_driver) {
    if (strcmp(shm_driver, "dmabuf") == 0) {
      if (!ctx.drm_device) {
        fprintf(stderr, "error: need drm device for dmabuf driver\n");
        return EXIT_FAILURE;
      }
      ctx.shm_driver = SHM_DRIVER_DMABUF;
    } else if (strcmp(shm_driver, "virtwl") == 0 ||
               strcmp(shm_driver, "virtwl-dmabuf") == 0) {
      if (ctx.virtwl_fd == -1) {
        fprintf(stderr, "error: need device for virtwl driver\n");
        return EXIT_FAILURE;
      }
      ctx.shm_driver = strcmp(shm_driver, "virtwl") ? SHM_DRIVER_VIRTWL_DMABUF
                                                    : SHM_DRIVER_VIRTWL;
    }
  } else if (ctx.drm_device) {
    ctx.shm_driver = SHM_DRIVER_DMABUF;
  } else if (ctx.virtwl_fd != -1) {
    ctx.shm_driver = SHM_DRIVER_VIRTWL_DMABUF;
  }

  if (data_driver) {
    if (strcmp(data_driver, "virtwl") == 0) {
      if (ctx.virtwl_fd == -1) {
        fprintf(stderr, "error: need device for virtwl driver\n");
        return EXIT_FAILURE;
      }
      ctx.data_driver = DATA_DRIVER_VIRTWL;
    }
  } else if (ctx.virtwl_fd != -1) {
    ctx.data_driver = DATA_DRIVER_VIRTWL;
  }

  // Use well known values for DPI by default with Xwayland.
  if (!dpi && ctx.xwayland)
    dpi = "72,96,160,240,320,480";

  wl_array_init(&ctx.dpi);
  if (dpi) {
    char* str = strdup(dpi);
    char* token = strtok(str, ",");
    int* p;

    while (token) {
      p = wl_array_add(&ctx.dpi, sizeof *p);
      assert(p);
      *p = MAX(MIN_DPI, MIN(atoi(token), MAX_DPI));
      token = strtok(NULL, ",");
    }
    free(str);
  }

  if (ctx.runprog || ctx.xwayland) {
    // Wayland connection from client.
    rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
    assert(!rv);

    client_fd = sv[0];
  }

  // The success of this depends on xkb-data being installed.
  ctx.xkb_context = xkb_context_new(0);
  if (!ctx.xkb_context) {
    fprintf(stderr, "error: xkb_context_new failed. xkb-data missing?\n");
    return EXIT_FAILURE;
  }

  if (virtwl_display_fd != -1) {
    ctx.display = wl_display_connect_to_fd(virtwl_display_fd);
  } else {
    if (display == NULL)
      display = getenv("WAYLAND_DISPLAY");
    if (display == NULL)
      display = "wayland-0";

    ctx.display = wl_display_connect(display);
  }

  if (!ctx.display) {
    fprintf(stderr, "error: failed to connect to %s\n", display);
    return EXIT_FAILURE;
  }

  wl_list_init(&ctx.accelerators);
  wl_list_init(&ctx.registries);
  wl_list_init(&ctx.globals);
  wl_list_init(&ctx.outputs);
  wl_list_init(&ctx.seats);
  wl_list_init(&ctx.windows);
  wl_list_init(&ctx.unpaired_windows);
  wl_list_init(&ctx.host_outputs);

  // Parse the list of accelerators that should be reserved by the
  // compositor. Format is "|MODIFIERS|KEYSYM", where MODIFIERS is a
  // list of modifier names (E.g. <Control><Alt>) and KEYSYM is an
  // XKB key symbol name (E.g Delete).
  if (accelerators) {
    uint32_t modifiers = 0;

    while (*accelerators) {
      if (*accelerators == ',') {
        accelerators++;
      } else if (*accelerators == '<') {
        if (strncmp(accelerators, "<Control>", 9) == 0) {
          modifiers |= CONTROL_MASK;
          accelerators += 9;
        } else if (strncmp(accelerators, "<Alt>", 5) == 0) {
          modifiers |= ALT_MASK;
          accelerators += 5;
        } else if (strncmp(accelerators, "<Shift>", 7) == 0) {
          modifiers |= SHIFT_MASK;
          accelerators += 7;
        } else {
          fprintf(stderr, "error: invalid modifier\n");
          return EXIT_FAILURE;
        }
      } else {
        struct sl_accelerator* accelerator;
        const char *end = strchrnul(accelerators, ',');
        char *name = strndup(accelerators, end - accelerators);

        accelerator = malloc(sizeof(*accelerator));
        accelerator->modifiers = modifiers;
        accelerator->symbol =
            xkb_keysym_from_name(name, XKB_KEYSYM_CASE_INSENSITIVE);
        if (accelerator->symbol == XKB_KEY_NoSymbol) {
          fprintf(stderr, "error: invalid key symbol\n");
          return EXIT_FAILURE;
        }

        wl_list_insert(&ctx.accelerators, &accelerator->link);

        modifiers = 0;
        accelerators = end;
        free(name);
      }
    }
  }

  ctx.display_event_source =
      wl_event_loop_add_fd(event_loop, wl_display_get_fd(ctx.display),
                           WL_EVENT_READABLE, sl_handle_event, &ctx);

  wl_registry_add_listener(wl_display_get_registry(ctx.display),
                           &sl_registry_listener, &ctx);

  ctx.client = wl_client_create(ctx.host_display, client_fd);

  // Replace the core display implementation. This is needed in order to
  // implement sync handler properly.
  wl_client_for_each_resource(ctx.client, sl_set_display_implementation, &ctx);

  if (ctx.runprog || ctx.xwayland) {
    ctx.sigchld_event_source =
        wl_event_loop_add_signal(event_loop, SIGCHLD, sl_handle_sigchld, &ctx);

    if (ctx.xwayland) {
      int ds[2], wm[2];

      // Xwayland display ready socket.
      rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ds);
      assert(!rv);

      ctx.display_ready_event_source =
          wl_event_loop_add_fd(event_loop, ds[0], WL_EVENT_READABLE,
                               sl_handle_display_ready_event, &ctx);

      // X connection to Xwayland.
      rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, wm);
      assert(!rv);

      ctx.wm_fd = wm[0];

      pid = fork();
      assert(pid != -1);
      if (pid == 0) {
        char display_str[8], display_fd_str[8], wm_fd_str[8];
        char xwayland_path_str[1024];
        char xwayland_cmd_prefix_str[1024];
        char *args[64];
        int i = 0;
        int fd;

        if (xwayland_cmd_prefix) {
          snprintf(xwayland_cmd_prefix_str, sizeof(xwayland_cmd_prefix_str),
                   "%s", xwayland_cmd_prefix);

          i = sl_parse_cmd_prefix(xwayland_cmd_prefix_str, 32, args);
          if (i > 32) {
            fprintf(stderr, "error: too many arguments in cmd prefix: %d\n", i);
            i = 0;
          }
        }

        snprintf(xwayland_path_str, sizeof(xwayland_path_str), "%s",
                 xwayland_path ? xwayland_path : XWAYLAND_PATH);
        args[i++] = xwayland_path_str;

        fd = dup(ds[1]);
        snprintf(display_fd_str, sizeof(display_fd_str), "%d", fd);
        fd = dup(wm[1]);
        snprintf(wm_fd_str, sizeof(wm_fd_str), "%d", fd);

        if (xdisplay > 0) {
          snprintf(display_str, sizeof(display_str), ":%d", xdisplay);
          args[i++] = display_str;
        }
        args[i++] = "-nolisten";
        args[i++] = "tcp";
        args[i++] = "-rootless";
        if (ctx.drm_device) {
          // Use DRM and software rendering unless glamor is enabled.
          if (!glamor || !strcmp(glamor, "0"))
            args[i++] = "-drm";
        } else {
          args[i++] = "-shm";
        }
        args[i++] = "-displayfd";
        args[i++] = display_fd_str;
        args[i++] = "-wm";
        args[i++] = wm_fd_str;
        args[i++] = NULL;

        sl_execvp(args[0], args, sv[1]);
        _exit(EXIT_FAILURE);
      }
      close(wm[1]);
      ctx.xwayland_pid = pid;
    } else {
      pid = fork();
      assert(pid != -1);
      if (pid == 0) {
        sl_execvp(ctx.runprog[0], ctx.runprog, sv[1]);
        _exit(EXIT_FAILURE);
      }
      ctx.child_pid = pid;
    }
    close(sv[1]);
  }

  wl_client_add_destroy_listener(ctx.client, &client_destroy_listener);

  do {
    wl_display_flush_clients(ctx.host_display);
    if (ctx.connection) {
      if (ctx.needs_set_input_focus) {
        sl_set_input_focus(&ctx, ctx.host_focus_window);
        ctx.needs_set_input_focus = 0;
      }
      xcb_flush(ctx.connection);
    }
    if (wl_display_flush(ctx.display) < 0)
      return EXIT_FAILURE;
  } while (wl_event_loop_dispatch(event_loop, -1) != -1);

  return EXIT_SUCCESS;
}
