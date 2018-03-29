// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include <wayland-server.h>
#include <wayland-util.h>
#include <xcb/composite.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <xkbcommon/xkbcommon.h>

#include "aura-shell-client-protocol.h"
#include "drm-server-protocol.h"
#include "gtk-shell-server-protocol.h"
#include "keyboard-extension-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "version.h"
#include "viewporter-client-protocol.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "xdg-shell-unstable-v6-server-protocol.h"

struct xwl;

struct xwl_global {
  struct xwl *xwl;
  const struct wl_interface *interface;
  uint32_t name;
  uint32_t version;
  void *data;
  wl_global_bind_func_t bind;
  struct wl_list link;
};

struct xwl_host_registry {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct wl_list link;
};

struct xwl_host_callback {
  struct wl_resource *resource;
  struct wl_callback *proxy;
};

struct xwl_compositor {
  struct xwl *xwl;
  uint32_t id;
  uint32_t version;
  struct xwl_global *host_global;
  struct wl_compositor *internal;
};

typedef void (*xwl_begin_end_access_func_t)(int fd);

struct xwl_mmap {
  int refcount;
  int fd;
  void *addr;
  size_t size;
  size_t offset;
  size_t stride;
  size_t bpp;
  xwl_begin_end_access_func_t begin_access;
  xwl_begin_end_access_func_t end_access;
  struct wl_resource *buffer_resource;
};

struct xwl_output_buffer;

struct xwl_host_surface {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct wl_surface *proxy;
  struct wp_viewport *viewport;
  uint32_t contents_width;
  uint32_t contents_height;
  int32_t contents_scale;
  struct xwl_mmap *contents_shm_mmap;
  int is_cursor;
  uint32_t last_event_serial;
  struct xwl_output_buffer *current_buffer;
  struct wl_list released_buffers;
  struct wl_list busy_buffers;
};

struct xwl_output_buffer {
  struct wl_list link;
  uint32_t width;
  uint32_t height;
  uint32_t format;
  struct wl_buffer *internal;
  struct xwl_mmap *mmap;
  struct pixman_region32 damage;
  struct xwl_host_surface *surface;
};

struct xwl_host_region {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct wl_region *proxy;
};

struct xwl_host_compositor {
  struct xwl_compositor *compositor;
  struct wl_resource *resource;
  struct wl_compositor *proxy;
};

struct xwl_host_buffer {
  struct wl_resource *resource;
  struct wl_buffer *proxy;
  uint32_t width;
  uint32_t height;
  struct xwl_mmap *shm_mmap;
  uint32_t shm_format;
};

struct xwl_host_shm_pool {
  struct xwl_shm *shm;
  struct wl_resource *resource;
  struct wl_shm_pool *proxy;
  int fd;
};

struct xwl_host_shm {
  struct xwl_shm *shm;
  struct wl_resource *resource;
  struct wl_shm *proxy;
};

struct xwl_shm {
  struct xwl *xwl;
  uint32_t id;
  struct xwl_global *host_global;
  struct wl_shm *internal;
};

struct xwl_host_shell_surface {
  struct wl_resource *resource;
  struct wl_shell_surface *proxy;
};

struct xwl_host_shell {
  struct xwl_shell *shell;
  struct wl_resource *resource;
  struct wl_shell *proxy;
};

struct xwl_shell {
  struct xwl *xwl;
  uint32_t id;
  struct xwl_global *host_global;
};

struct xwl_host_output {
  struct xwl_output *output;
  struct wl_resource *resource;
  struct wl_output *proxy;
  struct zaura_output *aura_output;
  int x;
  int y;
  int physical_width;
  int physical_height;
  int subpixel;
  char *make;
  char *model;
  int transform;
  uint32_t flags;
  int width;
  int height;
  int refresh;
  int scale_factor;
  int current_scale;
  int max_scale;
};

struct xwl_output {
  struct xwl *xwl;
  uint32_t id;
  uint32_t version;
  struct xwl_global *host_global;
  struct wl_list link;
};

struct xwl_seat {
  struct xwl *xwl;
  uint32_t id;
  uint32_t version;
  struct xwl_global *host_global;
  uint32_t last_serial;
  struct wl_list link;
};

struct xwl_host_pointer {
  struct xwl_seat *seat;
  struct wl_resource *resource;
  struct wl_pointer *proxy;
  struct wl_resource *focus_resource;
  struct wl_listener focus_resource_listener;
  uint32_t focus_serial;
};

struct xwl_host_keyboard {
  struct xwl_seat *seat;
  struct wl_resource *resource;
  struct wl_keyboard *proxy;
  struct zcr_extended_keyboard_v1 *extended_keyboard_proxy;
  struct wl_resource *focus_resource;
  struct wl_listener focus_resource_listener;
  uint32_t focus_serial;
  struct xkb_keymap *keymap;
  struct xkb_state *state;
  xkb_mod_mask_t control_mask;
  xkb_mod_mask_t alt_mask;
  xkb_mod_mask_t shift_mask;
  uint32_t modifiers;
};

struct xwl_host_touch {
  struct xwl_seat *seat;
  struct wl_resource *resource;
  struct wl_touch *proxy;
  struct wl_resource *focus_resource;
  struct wl_listener focus_resource_listener;
};

struct xwl_host_seat {
  struct xwl_seat *seat;
  struct wl_resource *resource;
  struct wl_seat *proxy;
};

struct xwl_data_device_manager {
  struct xwl *xwl;
  uint32_t id;
  uint32_t version;
  struct xwl_global *host_global;
  struct wl_data_device_manager *internal;
};

struct xwl_host_data_device_manager {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct wl_data_device_manager *proxy;
};

struct xwl_host_data_device {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct wl_data_device *proxy;
};

struct xwl_host_data_source {
  struct wl_resource *resource;
  struct wl_data_source *proxy;
};

struct xwl_host_data_offer {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct wl_data_offer *proxy;
};

struct xwl_data_offer {
  struct xwl *xwl;
  struct wl_data_offer *internal;
  int utf8_text;
};

struct xwl_data_source {
  struct xwl *xwl;
  struct wl_data_source *internal;
};

struct xwl_xdg_shell {
  struct xwl *xwl;
  uint32_t id;
  struct xwl_global *host_global;
  struct zxdg_shell_v6 *internal;
};

struct xwl_host_xdg_shell {
  struct xwl_xdg_shell *xdg_shell;
  struct wl_resource *resource;
  struct zxdg_shell_v6 *proxy;
};

struct xwl_host_xdg_surface {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct zxdg_surface_v6 *proxy;
};

struct xwl_host_xdg_toplevel {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct zxdg_toplevel_v6 *proxy;
};

struct xwl_host_xdg_popup {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct zxdg_popup_v6 *proxy;
};

struct xwl_host_xdg_positioner {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct zxdg_positioner_v6 *proxy;
};

struct xwl_subcompositor {
  struct xwl *xwl;
  uint32_t id;
  struct xwl_global *host_global;
};

struct xwl_host_subcompositor {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct wl_subcompositor *proxy;
};

struct xwl_host_subsurface {
  struct xwl *xwl;
  struct wl_resource *resource;
  struct wl_subsurface *proxy;
};

struct xwl_aura_shell {
  struct xwl *xwl;
  uint32_t id;
  uint32_t version;
  struct xwl_global *host_gtk_shell_global;
  struct zaura_shell *internal;
};

struct xwl_host_gtk_shell {
  struct xwl_aura_shell *aura_shell;
  struct wl_resource *resource;
  struct zaura_shell *proxy;
  struct wl_callback *callback;
  char *startup_id;
  struct wl_list surfaces;
};

struct xwl_host_gtk_surface {
  struct wl_resource *resource;
  struct zaura_surface *proxy;
  struct wl_list link;
};

struct xwl_viewporter {
  struct xwl *xwl;
  uint32_t id;
  struct wp_viewporter *internal;
};

struct xwl_linux_dmabuf {
  struct xwl *xwl;
  uint32_t id;
  uint32_t version;
  struct xwl_global *host_drm_global;
  struct zwp_linux_dmabuf_v1 *internal;
};

struct xwl_host_drm {
  struct xwl_linux_dmabuf *linux_dmabuf;
  uint32_t version;
  struct wl_resource *resource;
  struct wl_callback *callback;
};

struct xwl_keyboard_extension {
  struct xwl *xwl;
  uint32_t id;
  struct zcr_keyboard_extension_v1 *internal;
};

struct xwl_config {
  uint32_t serial;
  uint32_t mask;
  uint32_t values[5];
  uint32_t states_length;
  uint32_t states[3];
};

struct xwl_window {
  struct xwl *xwl;
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
  struct xwl_config next_config;
  struct xwl_config pending_config;
  struct zxdg_surface_v6 *xdg_surface;
  struct zxdg_toplevel_v6 *xdg_toplevel;
  struct zxdg_popup_v6 *xdg_popup;
  struct zaura_surface *aura_surface;
  struct wl_list link;
};

enum {
  ATOM_WM_S0,
  ATOM_WM_PROTOCOLS,
  ATOM_WM_STATE,
  ATOM_WM_DELETE_WINDOW,
  ATOM_WM_TAKE_FOCUS,
  ATOM_WM_CLIENT_LEADER,
  ATOM_WL_SURFACE_ID,
  ATOM_UTF8_STRING,
  ATOM_MOTIF_WM_HINTS,
  ATOM_NET_FRAME_EXTENTS,
  ATOM_NET_STARTUP_ID,
  ATOM_NET_SUPPORTING_WM_CHECK,
  ATOM_NET_WM_NAME,
  ATOM_NET_WM_MOVERESIZE,
  ATOM_NET_WM_STATE,
  ATOM_NET_WM_STATE_FULLSCREEN,
  ATOM_NET_WM_STATE_MAXIMIZED_VERT,
  ATOM_NET_WM_STATE_MAXIMIZED_HORZ,
  ATOM_CLIPBOARD,
  ATOM_CLIPBOARD_MANAGER,
  ATOM_TARGETS,
  ATOM_TIMESTAMP,
  ATOM_TEXT,
  ATOM_INCR,
  ATOM_WL_SELECTION,
  ATOM_LAST = ATOM_WL_SELECTION,
};

struct xwl_accelerator {
  struct wl_list link;
  uint32_t modifiers;
  xkb_keysym_t symbol;
};

struct xwl_data_transfer {
  int read_fd;
  int write_fd;
  size_t offset;
  size_t bytes_left;
  uint8_t data[4096];
  struct wl_event_source *read_event_source;
  struct wl_event_source *write_event_source;
};

struct xwl {
  char **runprog;
  struct wl_display *display;
  struct wl_display *host_display;
  struct wl_client *client;
  struct xwl_compositor *compositor;
  struct xwl_subcompositor *subcompositor;
  struct xwl_shm *shm;
  struct xwl_shell *shell;
  struct xwl_data_device_manager *data_device_manager;
  struct xwl_xdg_shell *xdg_shell;
  struct xwl_aura_shell *aura_shell;
  struct xwl_viewporter *viewporter;
  struct xwl_linux_dmabuf *linux_dmabuf;
  struct xwl_keyboard_extension *keyboard_extension;
  struct wl_list outputs;
  struct wl_list seats;
  struct wl_event_source *display_event_source;
  struct wl_event_source *display_ready_event_source;
  struct wl_event_source *sigchld_event_source;
  int shm_driver;
  int data_driver;
  int wm_fd;
  int virtwl_fd;
  int virtwl_ctx_fd;
  int virtwl_socket_fd;
  struct wl_event_source *virtwl_ctx_event_source;
  struct wl_event_source *virtwl_socket_event_source;
  const char *drm_device;
  struct gbm_device *gbm;
  int xwayland;
  pid_t xwayland_pid;
  pid_t child_pid;
  pid_t peer_pid;
  struct xkb_context *xkb_context;
  struct wl_list accelerators;
  struct wl_list registries;
  struct wl_list globals;
  int next_global_id;
  xcb_connection_t *connection;
  struct wl_event_source *connection_event_source;
  const xcb_query_extension_reply_t *xfixes_extension;
  xcb_screen_t *screen;
  xcb_window_t window;
  struct wl_list windows, unpaired_windows;
  struct xwl_window *host_focus_window;
  int needs_set_input_focus;
  double desired_scale;
  double scale;
  const char *app_id;
  int exit_with_child;
  const char *sd_notify;
  int clipboard_manager;
  uint32_t frame_color;
  int has_frame_color;
  int show_window_title;
  struct xwl_host_seat *default_seat;
  xcb_window_t selection_window;
  xcb_window_t selection_owner;
  int selection_incremental_transfer;
  xcb_selection_request_event_t selection_request;
  xcb_timestamp_t selection_timestamp;
  struct wl_data_device *selection_data_device;
  struct xwl_data_offer *selection_data_offer;
  struct xwl_data_source *selection_data_source;
  int selection_data_source_send_fd;
  struct wl_event_source *selection_send_event_source;
  xcb_get_property_reply_t *selection_property_reply;
  int selection_property_offset;
  struct wl_event_source *selection_event_source;
  struct wl_array selection_data;
  int selection_data_offer_receive_fd;
  int selection_data_ack_pending;
  union {
    const char *name;
    xcb_intern_atom_cookie_t cookie;
    xcb_atom_t value;
  } atoms[ATOM_LAST + 1];
  xcb_visualid_t visual_ids[256];
  xcb_colormap_t colormaps[256];
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
};

enum {
  DATA_DRIVER_NOOP,
  DATA_DRIVER_VIRTWL,
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

#define CAPTION_HEIGHT 32

#define MIN_SCALE 0.1
#define MAX_SCALE 10.0

#define MIN_SIZE (INT_MIN / 10)
#define MAX_SIZE (INT_MAX / 10)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#define LOCK_SUFFIX ".lock"
#define LOCK_SUFFIXLEN 5

#define CONTROL_MASK (1 << 0)
#define ALT_MASK (1 << 1)
#define SHIFT_MASK (1 << 2)

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

static void xwl_dmabuf_sync(int fd, __u64 flags) {
  struct dma_buf_sync sync = {0};
  int rv;

  sync.flags = flags;
  do {
    rv = ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
  } while (rv == -1 && errno == EINTR);
}

static void xwl_dmabuf_begin_access(int fd) {
  xwl_dmabuf_sync(fd, DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW);
}

static void xwl_dmabuf_end_access(int fd) {
  xwl_dmabuf_sync(fd, DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW);
}

static struct xwl_mmap *xwl_mmap_create(int fd, size_t size, size_t offset,
                                        size_t stride, size_t bpp) {
  struct xwl_mmap *map;

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

static struct xwl_mmap *xwl_mmap_ref(struct xwl_mmap *map) {
  map->refcount++;
  return map;
}

static void xwl_mmap_unref(struct xwl_mmap *map) {
  if (map->refcount-- == 1) {
    munmap(map->addr, map->size + map->offset);
    close(map->fd);
    free(map);
  }
}

static void xwl_output_buffer_destroy(struct xwl_output_buffer *buffer) {
  wl_buffer_destroy(buffer->internal);
  xwl_mmap_unref(buffer->mmap);
  pixman_region32_fini(&buffer->damage);
  wl_list_remove(&buffer->link);
}

static void xwl_output_buffer_release(void *data, struct wl_buffer *buffer) {
  struct xwl_output_buffer *output_buffer = wl_buffer_get_user_data(buffer);
  struct xwl_output_buffer *item, *next;
  struct xwl_host_surface *host_surface = output_buffer->surface;

  wl_list_remove(&output_buffer->link);
  wl_list_insert(&host_surface->released_buffers, &output_buffer->link);

  // Remove unused buffers.
  wl_list_for_each_safe(item, next, &host_surface->released_buffers, link) {
    if (item != output_buffer && item != host_surface->current_buffer)
      xwl_output_buffer_destroy(item);
  }
}

static const struct wl_buffer_listener xwl_output_buffer_listener = {
    xwl_output_buffer_release};

static void xwl_internal_xdg_shell_ping(void *data,
                                        struct zxdg_shell_v6 *xdg_shell,
                                        uint32_t serial) {
  zxdg_shell_v6_pong(xdg_shell, serial);
}

static const struct zxdg_shell_v6_listener xwl_internal_xdg_shell_listener = {
    xwl_internal_xdg_shell_ping};

static void xwl_send_configure_notify(struct xwl_window *window) {
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

  xcb_send_event(window->xwl->connection, 0, window->id,
                 XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char *)&event);
}

static void xwl_adjust_window_size_for_screen_size(struct xwl_window *window) {
  struct xwl *xwl = window->xwl;

  // Clamp size to screen.
  window->width = MIN(window->width, xwl->screen->width_in_pixels);
  window->height = MIN(window->height, xwl->screen->height_in_pixels);
}

static void
xwl_adjust_window_position_for_screen_size(struct xwl_window *window) {
  struct xwl *xwl = window->xwl;

  // Center horizontally/vertically.
  window->x = xwl->screen->width_in_pixels / 2 - window->width / 2;
  window->y = xwl->screen->height_in_pixels / 2 - window->height / 2;
}

static void xwl_configure_window(struct xwl_window *window) {
  assert(!window->pending_config.serial);

  if (window->next_config.mask) {
    int x = window->x;
    int y = window->y;
    int i = 0;

    xcb_configure_window(window->xwl->connection, window->frame_id,
                         window->next_config.mask, window->next_config.values);

    if (window->next_config.mask & XCB_CONFIG_WINDOW_X)
      x = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_Y)
      y = window->next_config.values[i++];

    assert(window->managed);
    xcb_configure_window(window->xwl->connection, window->id,
                         window->next_config.mask &
                             ~(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y),
                         &window->next_config.values[i]);

    if (window->next_config.mask & XCB_CONFIG_WINDOW_WIDTH)
      window->width = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_HEIGHT)
      window->height = window->next_config.values[i++];
    if (window->next_config.mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
      window->border_width = window->next_config.values[i++];

    if (x != window->x || y != window->y) {
      window->x = x;
      window->y = y;
      xwl_send_configure_notify(window);
    }
  }

  if (window->managed) {
    xcb_change_property(window->xwl->connection, XCB_PROP_MODE_REPLACE,
                        window->id, window->xwl->atoms[ATOM_NET_WM_STATE].value,
                        XCB_ATOM_ATOM, 32, window->next_config.states_length,
                        window->next_config.states);
  }

  window->pending_config = window->next_config;
  window->next_config.serial = 0;
  window->next_config.mask = 0;
  window->next_config.states_length = 0;
}

static void xwl_set_input_focus(struct xwl *xwl, struct xwl_window *window) {
  if (window) {
    xcb_client_message_event_t event = {
        .response_type = XCB_CLIENT_MESSAGE,
        .format = 32,
        .window = window->id,
        .type = xwl->atoms[ATOM_WM_PROTOCOLS].value,
        .data.data32 =
            {
                xwl->atoms[ATOM_WM_TAKE_FOCUS].value, XCB_CURRENT_TIME,
            },
    };

    if (!window->managed)
      return;

    xcb_send_event(xwl->connection, 0, window->id,
                   XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (char *)&event);

    xcb_set_input_focus(xwl->connection, XCB_INPUT_FOCUS_NONE, window->id,
                        XCB_CURRENT_TIME);
  } else {
    xcb_set_input_focus(xwl->connection, XCB_INPUT_FOCUS_NONE, XCB_NONE,
                        XCB_CURRENT_TIME);
  }
}

static void xwl_restack_windows(struct xwl *xwl, uint32_t focus_resource_id) {
  struct xwl_window *sibling;
  uint32_t values[1];

  wl_list_for_each(sibling, &xwl->windows, link) {
    if (!sibling->managed)
      continue;

    // Move focus window to the top and all other windows to the bottom.
    values[0] = sibling->host_surface_id == focus_resource_id
                    ? XCB_STACK_MODE_ABOVE
                    : XCB_STACK_MODE_BELOW;
    xcb_configure_window(xwl->connection, sibling->frame_id,
                         XCB_CONFIG_WINDOW_STACK_MODE, values);
  }
}

static void xwl_roundtrip(struct xwl *xwl) {
  free(xcb_get_input_focus_reply(xwl->connection,
                                 xcb_get_input_focus(xwl->connection), NULL));
}

static int
xwl_process_pending_configure_acks(struct xwl_window *window,
                                   struct xwl_host_surface *host_surface) {
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
    xwl_configure_window(window);

  return 1;
}

static void xwl_internal_xdg_surface_configure(
    void *data, struct zxdg_surface_v6 *xdg_surface, uint32_t serial) {
  struct xwl_window *window = zxdg_surface_v6_get_user_data(xdg_surface);

  window->next_config.serial = serial;
  if (!window->pending_config.serial) {
    struct wl_resource *host_resource;
    struct xwl_host_surface *host_surface = NULL;

    host_resource =
        wl_client_get_object(window->xwl->client, window->host_surface_id);
    if (host_resource)
      host_surface = wl_resource_get_user_data(host_resource);

    xwl_configure_window(window);

    if (xwl_process_pending_configure_acks(window, host_surface)) {
      if (host_surface)
        wl_surface_commit(host_surface->proxy);
    }
  }
}

static const struct zxdg_surface_v6_listener xwl_internal_xdg_surface_listener =
    {xwl_internal_xdg_surface_configure};

static void xwl_internal_xdg_toplevel_configure(
    void *data, struct zxdg_toplevel_v6 *xdg_toplevel, int32_t width,
    int32_t height, struct wl_array *states) {
  struct xwl_window *window = zxdg_toplevel_v6_get_user_data(xdg_toplevel);
  int activated = 0;
  uint32_t *state;
  int i = 0;

  if (!window->managed)
    return;

  if (width && height) {
    int32_t width_in_pixels = width * window->xwl->scale;
    int32_t height_in_pixels = height * window->xwl->scale;
    int i = 0;

    window->next_config.mask = XCB_CONFIG_WINDOW_WIDTH |
                               XCB_CONFIG_WINDOW_HEIGHT |
                               XCB_CONFIG_WINDOW_BORDER_WIDTH;
    if (!(window->size_flags & (US_POSITION | P_POSITION))) {
      window->next_config.mask |= XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
      window->next_config.values[i++] =
          window->xwl->screen->width_in_pixels / 2 - width_in_pixels / 2;
      window->next_config.values[i++] =
          window->xwl->screen->height_in_pixels / 2 - height_in_pixels / 2;
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
          window->xwl->atoms[ATOM_NET_WM_STATE_FULLSCREEN].value;
    }
    if (*state == ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED) {
      window->allow_resize = 0;
      window->next_config.states[i++] =
          window->xwl->atoms[ATOM_NET_WM_STATE_MAXIMIZED_VERT].value;
      window->next_config.states[i++] =
          window->xwl->atoms[ATOM_NET_WM_STATE_MAXIMIZED_HORZ].value;
    }
    if (*state == ZXDG_TOPLEVEL_V6_STATE_ACTIVATED)
      activated = 1;
    if (*state == ZXDG_TOPLEVEL_V6_STATE_RESIZING)
      window->allow_resize = 0;
  }

  if (activated != window->activated) {
    if (activated != (window->xwl->host_focus_window == window)) {
      window->xwl->host_focus_window = activated ? window : NULL;
      window->xwl->needs_set_input_focus = 1;
    }
    window->activated = activated;
  }

  window->next_config.states_length = i;
}

static void
xwl_internal_xdg_toplevel_close(void *data,
                                struct zxdg_toplevel_v6 *xdg_toplevel) {
  struct xwl_window *window = zxdg_toplevel_v6_get_user_data(xdg_toplevel);
  xcb_client_message_event_t event = {
      .response_type = XCB_CLIENT_MESSAGE,
      .format = 32,
      .window = window->id,
      .type = window->xwl->atoms[ATOM_WM_PROTOCOLS].value,
      .data.data32 =
          {
              window->xwl->atoms[ATOM_WM_DELETE_WINDOW].value, XCB_CURRENT_TIME,
          },
  };

  xcb_send_event(window->xwl->connection, 0, window->id,
                 XCB_EVENT_MASK_NO_EVENT, (const char *)&event);
}

static const struct zxdg_toplevel_v6_listener
    xwl_internal_xdg_toplevel_listener = {xwl_internal_xdg_toplevel_configure,
                                          xwl_internal_xdg_toplevel_close};

static void xwl_internal_xdg_popup_configure(void *data,
                                             struct zxdg_popup_v6 *xdg_popup,
                                             int32_t x, int32_t y,
                                             int32_t width, int32_t height) {}

static void xwl_internal_xdg_popup_done(void *data,
                                        struct zxdg_popup_v6 *zxdg_popup_v6) {}

static const struct zxdg_popup_v6_listener xwl_internal_xdg_popup_listener = {
    xwl_internal_xdg_popup_configure, xwl_internal_xdg_popup_done};

static void xwl_window_set_wm_state(struct xwl_window *window, int state) {
  struct xwl *xwl = window->xwl;
  uint32_t values[2];

  values[0] = state;
  values[1] = XCB_WINDOW_NONE;

  xcb_change_property(xwl->connection, XCB_PROP_MODE_REPLACE, window->id,
                      xwl->atoms[ATOM_WM_STATE].value,
                      xwl->atoms[ATOM_WM_STATE].value, 32, 2, values);
}

static void xwl_window_update(struct xwl_window *window) {
  struct wl_resource *host_resource = NULL;
  struct xwl_host_surface *host_surface;
  struct xwl *xwl = window->xwl;
  struct xwl_window *parent = NULL;
  const char *app_id = NULL;

  if (window->host_surface_id) {
    host_resource = wl_client_get_object(xwl->client, window->host_surface_id);
    if (host_resource && window->unpaired) {
      wl_list_remove(&window->link);
      wl_list_insert(&xwl->windows, &window->link);
      window->unpaired = 0;
    }
  } else if (!window->unpaired) {
    wl_list_remove(&window->link);
    wl_list_insert(&xwl->unpaired_windows, &window->link);
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
    return;
  }

  host_surface = wl_resource_get_user_data(host_resource);
  assert(host_surface);
  assert(!host_surface->is_cursor);

  assert(xwl->xdg_shell);
  assert(xwl->xdg_shell->internal);

  if (window->managed) {
    app_id = xwl->app_id ? xwl->app_id : window->clazz;

    if (window->transient_for != XCB_WINDOW_NONE) {
      struct xwl_window *sibling;

      wl_list_for_each(sibling, &xwl->windows, link) {
        if (sibling->id == window->transient_for) {
          if (sibling->xdg_toplevel)
            parent = sibling;
          break;
        }
      }
    }
  } else {
    struct xwl_window *sibling;
    uint32_t parent_last_event_serial = 0;

    wl_list_for_each(sibling, &xwl->windows, link) {
      struct wl_resource *sibling_host_resource;
      struct xwl_host_surface *sibling_host_surface;

      if (!sibling->realized)
        continue;

      sibling_host_resource =
          wl_client_get_object(xwl->client, sibling->host_surface_id);
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
    xcb_get_geometry_reply_t *geometry_reply = xcb_get_geometry_reply(
        xwl->connection, xcb_get_geometry(xwl->connection, window->id), NULL);
    if (geometry_reply) {
      window->depth = geometry_reply->depth;
      free(geometry_reply);
    }
  }

  if (!window->xdg_surface) {
    window->xdg_surface = zxdg_shell_v6_get_xdg_surface(
        xwl->xdg_shell->internal, host_surface->proxy);
    zxdg_surface_v6_set_user_data(window->xdg_surface, window);
    zxdg_surface_v6_add_listener(window->xdg_surface,
                                 &xwl_internal_xdg_surface_listener, window);
  }

  if (xwl->aura_shell) {
    if (!window->aura_surface) {
      window->aura_surface = zaura_shell_get_aura_surface(
          xwl->aura_shell->internal, host_surface->proxy);
    }
    zaura_surface_set_frame(window->aura_surface,
                            window->decorated
                                ? ZAURA_SURFACE_FRAME_TYPE_NORMAL
                                : window->depth == 32
                                      ? ZAURA_SURFACE_FRAME_TYPE_NONE
                                      : ZAURA_SURFACE_FRAME_TYPE_SHADOW);

    if (xwl->has_frame_color &&
        xwl->aura_shell->version >=
            ZAURA_SURFACE_SET_FRAME_COLORS_SINCE_VERSION) {
      zaura_surface_set_frame_colors(window->aura_surface, xwl->frame_color,
                                     xwl->frame_color);
    }

    if (xwl->aura_shell->version >= ZAURA_SURFACE_SET_STARTUP_ID_SINCE_VERSION)
      zaura_surface_set_startup_id(window->aura_surface, window->startup_id);
  }

  if (window->managed || !parent) {
    if (!window->xdg_toplevel) {
      window->xdg_toplevel = zxdg_surface_v6_get_toplevel(window->xdg_surface);
      zxdg_toplevel_v6_set_user_data(window->xdg_toplevel, window);
      zxdg_toplevel_v6_add_listener(
          window->xdg_toplevel, &xwl_internal_xdg_toplevel_listener, window);
    }
    if (parent)
      zxdg_toplevel_v6_set_parent(window->xdg_toplevel, parent->xdg_toplevel);
    if (window->name && xwl->show_window_title)
      zxdg_toplevel_v6_set_title(window->xdg_toplevel, window->name);
    if (app_id)
      zxdg_toplevel_v6_set_app_id(window->xdg_toplevel, app_id);
  } else if (!window->xdg_popup) {
    struct zxdg_positioner_v6 *positioner;

    positioner = zxdg_shell_v6_create_positioner(xwl->xdg_shell->internal);
    assert(positioner);
    zxdg_positioner_v6_set_anchor(positioner,
                                  ZXDG_POSITIONER_V6_ANCHOR_TOP |
                                      ZXDG_POSITIONER_V6_ANCHOR_LEFT);
    zxdg_positioner_v6_set_gravity(positioner,
                                   ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                                       ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
    zxdg_positioner_v6_set_anchor_rect(
        positioner, (window->x - parent->x) / xwl->scale,
        (window->y - parent->y) / xwl->scale, 1, 1);

    window->xdg_popup = zxdg_surface_v6_get_popup(
        window->xdg_surface, parent->xdg_surface, positioner);
    zxdg_popup_v6_set_user_data(window->xdg_popup, window);
    zxdg_popup_v6_add_listener(window->xdg_popup,
                               &xwl_internal_xdg_popup_listener, window);

    zxdg_positioner_v6_destroy(positioner);
  }

  if ((window->size_flags & (US_POSITION | P_POSITION)) && parent &&
      xwl->aura_shell &&
      xwl->aura_shell->version >= ZAURA_SURFACE_SET_PARENT_SINCE_VERSION) {
    zaura_surface_set_parent(window->aura_surface, parent->aura_surface,
                             (window->x - parent->x) / xwl->scale,
                             (window->y - parent->y) / xwl->scale);
  }

  wl_surface_commit(host_surface->proxy);
  if (host_surface->contents_width && host_surface->contents_height)
    window->realized = 1;
}

static int xwl_supported_shm_format(uint32_t format) {
  switch (format) {
  case WL_SHM_FORMAT_RGB565:
  case WL_SHM_FORMAT_ARGB8888:
  case WL_SHM_FORMAT_ABGR8888:
  case WL_SHM_FORMAT_XRGB8888:
  case WL_SHM_FORMAT_XBGR8888:
    return 1;
  }
  return 0;
}

static size_t xwl_bpp_for_shm_format(uint32_t format) {
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

static uint32_t xwl_gbm_format_for_shm_format(uint32_t format) {
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

static uint32_t xwl_drm_format_for_shm_format(int format) {
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

static void xwl_data_transfer_destroy(struct xwl_data_transfer *transfer) {
  if (transfer->read_event_source)
    wl_event_source_remove(transfer->read_event_source);
  assert(transfer->write_event_source);
  wl_event_source_remove(transfer->write_event_source);
  close(transfer->read_fd);
  close(transfer->write_fd);
  free(transfer);
}

static int xwl_handle_data_transfer_read(int fd, uint32_t mask, void *data) {
  struct xwl_data_transfer *transfer = (struct xwl_data_transfer *)data;

  if ((mask & WL_EVENT_READABLE) == 0) {
    assert(mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR));
    wl_event_source_remove(transfer->read_event_source);
    transfer->read_event_source = NULL;
    return 0;
  }

  assert(!transfer->bytes_left);

  transfer->bytes_left =
      read(transfer->read_fd, transfer->data, sizeof(transfer->data));
  if (transfer->bytes_left) {
    transfer->offset = 0;
    wl_event_source_fd_update(transfer->read_event_source, 0);
    wl_event_source_fd_update(transfer->write_event_source, WL_EVENT_WRITABLE);
  } else {
    xwl_data_transfer_destroy(transfer);
  }

  return 0;
}

static int xwl_handle_data_transfer_write(int fd, uint32_t mask, void *data) {
  struct xwl_data_transfer *transfer = (struct xwl_data_transfer *)data;
  int rv;

  if ((mask & WL_EVENT_WRITABLE) == 0) {
    assert(mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR));
    xwl_data_transfer_destroy(transfer);
    return 0;
  }

  assert(transfer->bytes_left);

  rv = write(transfer->write_fd, transfer->data + transfer->offset,
             transfer->bytes_left);

  if (rv < 0) {
    assert(errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE);
  } else {
    assert(rv <= transfer->bytes_left);
    transfer->bytes_left -= rv;
    transfer->offset += rv;
  }

  if (!transfer->bytes_left) {
    wl_event_source_fd_update(transfer->write_event_source, 0);
    if (transfer->read_event_source) {
      wl_event_source_fd_update(transfer->read_event_source, WL_EVENT_READABLE);
    } else {
      xwl_data_transfer_destroy(transfer);
    }
    return 0;
  }

  return 1;
}

static void xwl_data_transfer_create(struct wl_event_loop *event_loop,
                                     int read_fd, int write_fd) {
  struct xwl_data_transfer *transfer;
  int flags;
  int rv;

  flags = fcntl(write_fd, F_GETFL, 0);
  rv = fcntl(write_fd, F_SETFL, flags | O_NONBLOCK);
  assert(!rv);

  transfer = malloc(sizeof(*transfer));
  assert(transfer);
  transfer->read_fd = read_fd;
  transfer->write_fd = write_fd;
  transfer->offset = 0;
  transfer->bytes_left = 0;
  transfer->read_event_source =
      wl_event_loop_add_fd(event_loop, read_fd, WL_EVENT_READABLE,
                           xwl_handle_data_transfer_read, transfer);
  transfer->write_event_source = wl_event_loop_add_fd(
      event_loop, write_fd, 0, xwl_handle_data_transfer_write, transfer);
}

static void xwl_host_surface_destroy(struct wl_client *client,
                                     struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_host_surface_attach(struct wl_client *client,
                                    struct wl_resource *resource,
                                    struct wl_resource *buffer_resource,
                                    int32_t x, int32_t y) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_buffer *host_buffer =
      buffer_resource ? wl_resource_get_user_data(buffer_resource) : NULL;
  struct wl_buffer *buffer_proxy = NULL;
  struct xwl_window *window;
  double scale = host->xwl->scale;

  host->current_buffer = NULL;
  if (host->contents_shm_mmap) {
    xwl_mmap_unref(host->contents_shm_mmap);
    host->contents_shm_mmap = NULL;
  }

  if (host_buffer) {
    host->contents_width = host_buffer->width;
    host->contents_height = host_buffer->height;
    buffer_proxy = host_buffer->proxy;
    if (host_buffer->shm_mmap)
      host->contents_shm_mmap = xwl_mmap_ref(host_buffer->shm_mmap);
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

      xwl_output_buffer_destroy(host->current_buffer);
      host->current_buffer = NULL;
    }

    // Allocate new output buffer.
    if (!host->current_buffer) {
      size_t width = host_buffer->width;
      size_t height = host_buffer->height;
      size_t size = host_buffer->shm_mmap->size;
      uint32_t shm_format = host_buffer->shm_format;
      size_t bpp = xwl_bpp_for_shm_format(shm_format);

      host->current_buffer = malloc(sizeof(struct xwl_output_buffer));
      assert(host->current_buffer);
      wl_list_insert(&host->released_buffers, &host->current_buffer->link);
      host->current_buffer->width = width;
      host->current_buffer->height = height;
      host->current_buffer->format = shm_format;
      host->current_buffer->surface = host;
      pixman_region32_init_rect(&host->current_buffer->damage, 0, 0, MAX_SIZE,
                                MAX_SIZE);

      switch (host->xwl->shm_driver) {
      case SHM_DRIVER_DMABUF: {
        struct zwp_linux_buffer_params_v1 *buffer_params;
        struct gbm_bo *bo;
        int stride0;
        int fd;

        bo = gbm_bo_create(host->xwl->gbm, width, height,
                           xwl_gbm_format_for_shm_format(shm_format),
                           GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
        stride0 = gbm_bo_get_stride(bo);
        fd = gbm_bo_get_fd(bo);

        buffer_params = zwp_linux_dmabuf_v1_create_params(
            host->xwl->linux_dmabuf->internal);
        zwp_linux_buffer_params_v1_add(buffer_params, fd, 0, 0, stride0, 0, 0);
        host->current_buffer->internal =
            zwp_linux_buffer_params_v1_create_immed(
                buffer_params, width, height,
                xwl_drm_format_for_shm_format(shm_format), 0);
        zwp_linux_buffer_params_v1_destroy(buffer_params);

        host->current_buffer->mmap =
            xwl_mmap_create(fd, height * stride0, 0, stride0, bpp);
        host->current_buffer->mmap->begin_access = xwl_dmabuf_begin_access;
        host->current_buffer->mmap->end_access = xwl_dmabuf_end_access;

        gbm_bo_destroy(bo);
      } break;
      case SHM_DRIVER_VIRTWL: {
        struct virtwl_ioctl_new new_alloc = {
            .type = VIRTWL_IOCTL_NEW_ALLOC, .fd = -1, .flags = 0, .size = size};
        struct wl_shm_pool *pool;
        int rv;

        rv = ioctl(host->xwl->virtwl_fd, VIRTWL_IOCTL_NEW, &new_alloc);
        assert(rv == 0);

        pool = wl_shm_create_pool(host->xwl->shm->internal, new_alloc.fd, size);
        host->current_buffer->internal = wl_shm_pool_create_buffer(
            pool, 0, width, height, host_buffer->shm_mmap->stride, shm_format);

        host->current_buffer->mmap = xwl_mmap_create(
            new_alloc.fd, size, 0, host_buffer->shm_mmap->stride, bpp);

        wl_shm_pool_destroy(pool);
      } break;
      }

      assert(host->current_buffer->internal);
      assert(host->current_buffer->mmap);

      wl_buffer_set_user_data(host->current_buffer->internal,
                              host->current_buffer);
      wl_buffer_add_listener(host->current_buffer->internal,
                             &xwl_output_buffer_listener, host->current_buffer);
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

  wl_list_for_each(window, &host->xwl->windows, link) {
    if (window->host_surface_id == wl_resource_get_id(resource)) {
      while (xwl_process_pending_configure_acks(window, host))
        continue;

      break;
    }
  }
}

static void xwl_host_surface_damage(struct wl_client *client,
                                    struct wl_resource *resource, int32_t x,
                                    int32_t y, int32_t width, int32_t height) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);
  double scale = host->xwl->scale;
  struct xwl_output_buffer *buffer;
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

static void xwl_frame_callback_done(void *data, struct wl_callback *callback,
                                    uint32_t time) {
  struct xwl_host_callback *host = wl_callback_get_user_data(callback);

  wl_callback_send_done(host->resource, time);
  wl_resource_destroy(host->resource);
}

static const struct wl_callback_listener xwl_frame_callback_listener = {
    xwl_frame_callback_done};

static void xwl_host_callback_destroy(struct wl_resource *resource) {
  struct xwl_host_callback *host = wl_resource_get_user_data(resource);

  wl_callback_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_host_surface_frame(struct wl_client *client,
                                   struct wl_resource *resource,
                                   uint32_t callback) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_callback *host_callback;

  host_callback = malloc(sizeof(*host_callback));
  assert(host_callback);

  host_callback->resource =
      wl_resource_create(client, &wl_callback_interface, 1, callback);
  wl_resource_set_implementation(host_callback->resource, NULL, host_callback,
                                 xwl_host_callback_destroy);
  host_callback->proxy = wl_surface_frame(host->proxy);
  wl_callback_set_user_data(host_callback->proxy, host_callback);
  wl_callback_add_listener(host_callback->proxy, &xwl_frame_callback_listener,
                           host_callback);
}

static void
xwl_host_surface_set_opaque_region(struct wl_client *client,
                                   struct wl_resource *resource,
                                   struct wl_resource *region_resource) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_region *host_region =
      region_resource ? wl_resource_get_user_data(region_resource) : NULL;

  wl_surface_set_opaque_region(host->proxy,
                               host_region ? host_region->proxy : NULL);
}

static void
xwl_host_surface_set_input_region(struct wl_client *client,
                                  struct wl_resource *resource,
                                  struct wl_resource *region_resource) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_region *host_region =
      region_resource ? wl_resource_get_user_data(region_resource) : NULL;

  wl_surface_set_input_region(host->proxy,
                              host_region ? host_region->proxy : NULL);
}

static void xwl_host_surface_commit(struct wl_client *client,
                                    struct wl_resource *resource) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);
  struct xwl_window *window;

  if (host->contents_shm_mmap) {
    uint8_t *src_base =
        host->contents_shm_mmap->addr + host->contents_shm_mmap->offset;
    uint8_t *dst_base =
        host->current_buffer->mmap->addr + host->current_buffer->mmap->offset;
    size_t src_stride = host->contents_shm_mmap->stride;
    size_t dst_stride = host->current_buffer->mmap->stride;
    size_t bpp = host->contents_shm_mmap->bpp;
    pixman_box32_t *rect;
    int n;

    if (host->current_buffer->mmap->begin_access)
      host->current_buffer->mmap->begin_access(host->current_buffer->mmap->fd);

    rect = pixman_region32_rectangles(&host->current_buffer->damage, &n);
    while (n--) {
      int32_t x1, y1, x2, y2;

      x1 = rect->x1 * host->contents_scale;
      y1 = rect->y1 * host->contents_scale;
      x2 = rect->x2 * host->contents_scale;
      y2 = rect->y2 * host->contents_scale;

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
    double scale = host->xwl->scale * host->contents_scale;

    if (host->viewport) {
      wp_viewport_set_destination(host->viewport,
                                  ceil(host->contents_width / scale),
                                  ceil(host->contents_height / scale));
    } else {
      wl_surface_set_buffer_scale(host->proxy, scale);
    }
  }

  // No need to defer cursor or non-xwayland client commits.
  if (host->is_cursor || !host->xwl->xwayland) {
    wl_surface_commit(host->proxy);
  } else {
    // Commit if surface is associated with a window. Otherwise, defer
    // commit until window is created.
    wl_list_for_each(window, &host->xwl->windows, link) {
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
    xwl_mmap_unref(host->contents_shm_mmap);
    host->contents_shm_mmap = NULL;
  }
}

static void xwl_host_surface_set_buffer_transform(struct wl_client *client,
                                                  struct wl_resource *resource,
                                                  int32_t transform) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);

  wl_surface_set_buffer_transform(host->proxy, transform);
}

static void xwl_host_surface_set_buffer_scale(struct wl_client *client,
                                              struct wl_resource *resource,
                                              int32_t scale) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);

  host->contents_scale = scale;
}

static void xwl_host_surface_damage_buffer(struct wl_client *client,
                                           struct wl_resource *resource,
                                           int32_t x, int32_t y, int32_t width,
                                           int32_t height) {
  assert(0);
}

static const struct wl_surface_interface xwl_surface_implementation = {
    xwl_host_surface_destroy,
    xwl_host_surface_attach,
    xwl_host_surface_damage,
    xwl_host_surface_frame,
    xwl_host_surface_set_opaque_region,
    xwl_host_surface_set_input_region,
    xwl_host_surface_commit,
    xwl_host_surface_set_buffer_transform,
    xwl_host_surface_set_buffer_scale,
    xwl_host_surface_damage_buffer};

static void xwl_destroy_host_surface(struct wl_resource *resource) {
  struct xwl_host_surface *host = wl_resource_get_user_data(resource);
  struct xwl_window *window, *surface_window = NULL;
  struct xwl_output_buffer *buffer;

  wl_list_for_each(window, &host->xwl->windows, link) {
    if (window->host_surface_id == wl_resource_get_id(resource)) {
      surface_window = window;
      break;
    }
  }

  if (surface_window) {
    surface_window->host_surface_id = 0;
    xwl_window_update(surface_window);
  }

  if (host->contents_shm_mmap)
    xwl_mmap_unref(host->contents_shm_mmap);

  while (!wl_list_empty(&host->released_buffers)) {
    buffer = wl_container_of(host->released_buffers.next, buffer, link);
    xwl_output_buffer_destroy(buffer);
  }
  while (!wl_list_empty(&host->busy_buffers)) {
    buffer = wl_container_of(host->busy_buffers.next, buffer, link);
    xwl_output_buffer_destroy(buffer);
  }

  if (host->viewport)
    wp_viewport_destroy(host->viewport);
  wl_surface_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_region_destroy(struct wl_client *client,
                               struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_region_add(struct wl_client *client,
                           struct wl_resource *resource, int32_t x, int32_t y,
                           int32_t width, int32_t height) {
  struct xwl_host_region *host = wl_resource_get_user_data(resource);
  double scale = host->xwl->scale;
  int32_t x1, y1, x2, y2;

  x1 = x / scale;
  y1 = y / scale;
  x2 = (x + width) / scale;
  y2 = (y + height) / scale;

  wl_region_add(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void xwl_region_subtract(struct wl_client *client,
                                struct wl_resource *resource, int32_t x,
                                int32_t y, int32_t width, int32_t height) {
  struct xwl_host_region *host = wl_resource_get_user_data(resource);
  double scale = host->xwl->scale;
  int32_t x1, y1, x2, y2;

  x1 = x / scale;
  y1 = y / scale;
  x2 = (x + width) / scale;
  y2 = (y + height) / scale;

  wl_region_subtract(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static const struct wl_region_interface xwl_region_implementation = {
    xwl_region_destroy, xwl_region_add, xwl_region_subtract};

static void xwl_destroy_host_region(struct wl_resource *resource) {
  struct xwl_host_region *host = wl_resource_get_user_data(resource);

  wl_region_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_compositor_create_host_surface(struct wl_client *client,
                                               struct wl_resource *resource,
                                               uint32_t id) {
  struct xwl_host_compositor *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_surface;
  struct xwl_window *window, *unpaired_window = NULL;

  host_surface = malloc(sizeof(*host_surface));
  assert(host_surface);

  host_surface->xwl = host->compositor->xwl;
  host_surface->contents_width = 0;
  host_surface->contents_height = 0;
  host_surface->contents_scale = 1;
  host_surface->contents_shm_mmap = NULL;
  host_surface->is_cursor = 0;
  host_surface->last_event_serial = 0;
  host_surface->current_buffer = NULL;
  wl_list_init(&host_surface->released_buffers);
  wl_list_init(&host_surface->busy_buffers);
  host_surface->resource = wl_resource_create(
      client, &wl_surface_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_surface->resource,
                                 &xwl_surface_implementation, host_surface,
                                 xwl_destroy_host_surface);
  host_surface->proxy = wl_compositor_create_surface(host->proxy);
  wl_surface_set_user_data(host_surface->proxy, host_surface);
  host_surface->viewport = NULL;
  if (host_surface->xwl->viewporter) {
    host_surface->viewport = wp_viewporter_get_viewport(
        host_surface->xwl->viewporter->internal, host_surface->proxy);
  }

  wl_list_for_each(window, &host->compositor->xwl->unpaired_windows, link) {
    if (window->host_surface_id == id) {
      unpaired_window = window;
      break;
    }
  }

  if (unpaired_window)
    xwl_window_update(window);
}

static void xwl_compositor_create_host_region(struct wl_client *client,
                                              struct wl_resource *resource,
                                              uint32_t id) {
  struct xwl_host_compositor *host = wl_resource_get_user_data(resource);
  struct xwl_host_region *host_region;

  host_region = malloc(sizeof(*host_region));
  assert(host_region);

  host_region->xwl = host->compositor->xwl;
  host_region->resource = wl_resource_create(
      client, &wl_region_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_region->resource,
                                 &xwl_region_implementation, host_region,
                                 xwl_destroy_host_region);
  host_region->proxy = wl_compositor_create_region(host->proxy);
  wl_region_set_user_data(host_region->proxy, host_region);
}

static const struct wl_compositor_interface xwl_compositor_implementation = {
    xwl_compositor_create_host_surface, xwl_compositor_create_host_region};

static void xwl_destroy_host_compositor(struct wl_resource *resource) {
  struct xwl_host_compositor *host = wl_resource_get_user_data(resource);

  wl_compositor_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_bind_host_compositor(struct wl_client *client, void *data,
                                     uint32_t version, uint32_t id) {
  struct xwl_compositor *compositor = (struct xwl_compositor *)data;
  struct xwl_host_compositor *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->compositor = compositor;
  host->resource = wl_resource_create(client, &wl_compositor_interface,
                                      MIN(version, compositor->version), id);
  wl_resource_set_implementation(host->resource, &xwl_compositor_implementation,
                                 host, xwl_destroy_host_compositor);
  host->proxy = wl_registry_bind(
      wl_display_get_registry(compositor->xwl->display), compositor->id,
      &wl_compositor_interface, compositor->version);
  wl_compositor_set_user_data(host->proxy, host);
}

static void xwl_host_buffer_destroy(struct wl_client *client,
                                    struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_buffer_interface xwl_buffer_implementation = {
    xwl_host_buffer_destroy};

static void xwl_buffer_release(void *data, struct wl_buffer *buffer) {
  struct xwl_host_buffer *host = wl_buffer_get_user_data(buffer);

  wl_buffer_send_release(host->resource);
}

static const struct wl_buffer_listener xwl_buffer_listener = {
    xwl_buffer_release};

static void xwl_destroy_host_buffer(struct wl_resource *resource) {
  struct xwl_host_buffer *host = wl_resource_get_user_data(resource);

  if (host->proxy)
    wl_buffer_destroy(host->proxy);
  if (host->shm_mmap) {
    host->shm_mmap->buffer_resource = NULL;
    xwl_mmap_unref(host->shm_mmap);
  }
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_host_shm_pool_create_host_buffer(struct wl_client *client,
                                                 struct wl_resource *resource,
                                                 uint32_t id, int32_t offset,
                                                 int32_t width, int32_t height,
                                                 int32_t stride,
                                                 uint32_t format) {
  struct xwl_host_shm_pool *host = wl_resource_get_user_data(resource);
  struct xwl_host_buffer *host_buffer;

  host_buffer = malloc(sizeof(*host_buffer));
  assert(host_buffer);

  host_buffer->width = width;
  host_buffer->height = height;
  host_buffer->resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);
  wl_resource_set_implementation(host_buffer->resource,
                                 &xwl_buffer_implementation, host_buffer,
                                 xwl_destroy_host_buffer);

  if (host->shm->xwl->shm_driver == SHM_DRIVER_NOOP) {
    assert(host->proxy);
    host_buffer->shm_mmap = NULL;
    host_buffer->shm_format = 0;
    host_buffer->proxy = wl_shm_pool_create_buffer(host->proxy, offset, width,
                                                   height, stride, format);
    wl_buffer_set_user_data(host_buffer->proxy, host_buffer);
    wl_buffer_add_listener(host_buffer->proxy, &xwl_buffer_listener,
                           host_buffer);
  } else {
    host_buffer->proxy = NULL;
    host_buffer->shm_format = format;
    host_buffer->shm_mmap =
        xwl_mmap_create(dup(host->fd), height * stride, offset, stride,
                        xwl_bpp_for_shm_format(format));
    host_buffer->shm_mmap->buffer_resource = host_buffer->resource;
  }
}

static void xwl_host_shm_pool_destroy(struct wl_client *client,
                                      struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_host_shm_pool_resize(struct wl_client *client,
                                     struct wl_resource *resource,
                                     int32_t size) {
  struct xwl_host_shm_pool *host = wl_resource_get_user_data(resource);

  if (host->proxy)
    wl_shm_pool_resize(host->proxy, size);
}

static const struct wl_shm_pool_interface xwl_shm_pool_implementation = {
    xwl_host_shm_pool_create_host_buffer, xwl_host_shm_pool_destroy,
    xwl_host_shm_pool_resize};

static void xwl_destroy_host_shm_pool(struct wl_resource *resource) {
  struct xwl_host_shm_pool *host = wl_resource_get_user_data(resource);

  if (host->fd >= 0)
    close(host->fd);
  if (host->proxy)
    wl_shm_pool_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_shm_create_host_pool(struct wl_client *client,
                                     struct wl_resource *resource, uint32_t id,
                                     int fd, int32_t size) {
  struct xwl_host_shm *host = wl_resource_get_user_data(resource);
  struct xwl_host_shm_pool *host_shm_pool;

  host_shm_pool = malloc(sizeof(*host_shm_pool));
  assert(host_shm_pool);

  host_shm_pool->shm = host->shm;
  host_shm_pool->fd = -1;
  host_shm_pool->proxy = NULL;
  host_shm_pool->resource =
      wl_resource_create(client, &wl_shm_pool_interface, 1, id);
  wl_resource_set_implementation(host_shm_pool->resource,
                                 &xwl_shm_pool_implementation, host_shm_pool,
                                 xwl_destroy_host_shm_pool);

  switch (host->shm->xwl->shm_driver) {
  case SHM_DRIVER_NOOP:
    host_shm_pool->proxy = wl_shm_create_pool(host->proxy, fd, size);
    wl_shm_pool_set_user_data(host_shm_pool->proxy, host_shm_pool);
    close(fd);
    break;
  case SHM_DRIVER_DMABUF:
  case SHM_DRIVER_VIRTWL:
    host_shm_pool->fd = fd;
    break;
  }
}

static const struct wl_shm_interface xwl_shm_implementation = {
    xwl_shm_create_host_pool};

static void xwl_shm_format(void *data, struct wl_shm *shm, uint32_t format) {
  struct xwl_host_shm *host = wl_shm_get_user_data(shm);

  if (xwl_supported_shm_format(format))
    wl_shm_send_format(host->resource, format);
}

static const struct wl_shm_listener xwl_shm_listener = {xwl_shm_format};

static void xwl_destroy_host_shm(struct wl_resource *resource) {
  struct xwl_host_shm *host = wl_resource_get_user_data(resource);

  wl_shm_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_bind_host_shm(struct wl_client *client, void *data,
                              uint32_t version, uint32_t id) {
  struct xwl_shm *shm = (struct xwl_shm *)data;
  struct xwl_host_shm *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->shm = shm;
  host->resource = wl_resource_create(client, &wl_shm_interface, 1, id);
  wl_resource_set_implementation(host->resource, &xwl_shm_implementation, host,
                                 xwl_destroy_host_shm);
  host->proxy = wl_registry_bind(wl_display_get_registry(shm->xwl->display),
                                 shm->id, &wl_shm_interface,
                                 wl_resource_get_version(host->resource));
  wl_shm_set_user_data(host->proxy, host);
  wl_shm_add_listener(host->proxy, &xwl_shm_listener, host);
}

static void xwl_shell_surface_pong(struct wl_client *client,
                                   struct wl_resource *resource,
                                   uint32_t serial) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);

  wl_shell_surface_pong(host->proxy, serial);
}

static void xwl_shell_surface_move(struct wl_client *client,
                                   struct wl_resource *resource,
                                   struct wl_resource *seat_resource,
                                   uint32_t serial) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_seat *host_seat = wl_resource_get_user_data(seat_resource);

  wl_shell_surface_move(host->proxy, host_seat->proxy, serial);
}

static void xwl_shell_surface_resize(struct wl_client *client,
                                     struct wl_resource *resource,
                                     struct wl_resource *seat_resource,
                                     uint32_t serial, uint32_t edges) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_seat *host_seat = wl_resource_get_user_data(seat_resource);

  wl_shell_surface_resize(host->proxy, host_seat->proxy, serial, edges);
}

static void xwl_shell_surface_set_toplevel(struct wl_client *client,
                                           struct wl_resource *resource) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);

  wl_shell_surface_set_toplevel(host->proxy);
}

static void xwl_shell_surface_set_transient(struct wl_client *client,
                                            struct wl_resource *resource,
                                            struct wl_resource *parent_resource,
                                            int32_t x, int32_t y,
                                            uint32_t flags) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_parent =
      wl_resource_get_user_data(parent_resource);

  wl_shell_surface_set_transient(host->proxy, host_parent->proxy, x, y, flags);
}

static void xwl_shell_surface_set_fullscreen(
    struct wl_client *client, struct wl_resource *resource, uint32_t method,
    uint32_t framerate, struct wl_resource *output_resource) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_output *host_output =
      output_resource ? wl_resource_get_user_data(output_resource) : NULL;

  wl_shell_surface_set_fullscreen(host->proxy, method, framerate,
                                  host_output ? host_output->proxy : NULL);
}

static void xwl_shell_surface_set_popup(struct wl_client *client,
                                        struct wl_resource *resource,
                                        struct wl_resource *seat_resource,
                                        uint32_t serial,
                                        struct wl_resource *parent_resource,
                                        int32_t x, int32_t y, uint32_t flags) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_seat *host_seat = wl_resource_get_user_data(seat_resource);
  struct xwl_host_surface *host_parent =
      wl_resource_get_user_data(parent_resource);

  wl_shell_surface_set_popup(host->proxy, host_seat->proxy, serial,
                             host_parent->proxy, x, y, flags);
}

static void
xwl_shell_surface_set_maximized(struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *output_resource) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_output *host_output =
      output_resource ? wl_resource_get_user_data(output_resource) : NULL;

  wl_shell_surface_set_maximized(host->proxy,
                                 host_output ? host_output->proxy : NULL);
}

static void xwl_shell_surface_set_title(struct wl_client *client,
                                        struct wl_resource *resource,
                                        const char *title) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);

  wl_shell_surface_set_title(host->proxy, title);
}

static void xwl_shell_surface_set_class(struct wl_client *client,
                                        struct wl_resource *resource,
                                        const char *clazz) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);

  wl_shell_surface_set_class(host->proxy, clazz);
}

static const struct wl_shell_surface_interface
    xwl_shell_surface_implementation = {
        xwl_shell_surface_pong,          xwl_shell_surface_move,
        xwl_shell_surface_resize,        xwl_shell_surface_set_toplevel,
        xwl_shell_surface_set_transient, xwl_shell_surface_set_fullscreen,
        xwl_shell_surface_set_popup,     xwl_shell_surface_set_maximized,
        xwl_shell_surface_set_title,     xwl_shell_surface_set_class};

static void xwl_shell_surface_ping(void *data,
                                   struct wl_shell_surface *shell_surface,
                                   uint32_t serial) {
  struct xwl_host_shell_surface *host =
      wl_shell_surface_get_user_data(shell_surface);

  wl_shell_surface_send_ping(host->resource, serial);
}

static void xwl_shell_surface_configure(void *data,
                                        struct wl_shell_surface *shell_surface,
                                        uint32_t edges, int32_t width,
                                        int32_t height) {
  struct xwl_host_shell_surface *host =
      wl_shell_surface_get_user_data(shell_surface);

  wl_shell_surface_send_configure(host->resource, edges, width, height);
}

static void
xwl_shell_surface_popup_done(void *data,
                             struct wl_shell_surface *shell_surface) {
  struct xwl_host_shell_surface *host =
      wl_shell_surface_get_user_data(shell_surface);

  wl_shell_surface_send_popup_done(host->resource);
}

static const struct wl_shell_surface_listener xwl_shell_surface_listener = {
    xwl_shell_surface_ping, xwl_shell_surface_configure,
    xwl_shell_surface_popup_done};

static void xwl_destroy_host_shell_surface(struct wl_resource *resource) {
  struct xwl_host_shell_surface *host = wl_resource_get_user_data(resource);

  wl_shell_surface_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void
xwl_host_shell_get_shell_surface(struct wl_client *client,
                                 struct wl_resource *resource, uint32_t id,
                                 struct wl_resource *surface_resource) {
  struct xwl_host_shell *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_surface =
      wl_resource_get_user_data(surface_resource);
  struct xwl_host_shell_surface *host_shell_surface;

  host_shell_surface = malloc(sizeof(*host_shell_surface));
  assert(host_shell_surface);
  host_shell_surface->resource =
      wl_resource_create(client, &wl_shell_surface_interface, 1, id);
  wl_resource_set_implementation(
      host_shell_surface->resource, &xwl_shell_surface_implementation,
      host_shell_surface, xwl_destroy_host_shell_surface);
  host_shell_surface->proxy =
      wl_shell_get_shell_surface(host->proxy, host_surface->proxy);
  wl_shell_surface_set_user_data(host_shell_surface->proxy, host_shell_surface);
  wl_shell_surface_add_listener(host_shell_surface->proxy,
                                &xwl_shell_surface_listener,
                                host_shell_surface);
}

static const struct wl_shell_interface xwl_shell_implementation = {
    xwl_host_shell_get_shell_surface};

static void xwl_destroy_host_shell(struct wl_resource *resource) {
  struct xwl_host_shell *host = wl_resource_get_user_data(resource);

  wl_shell_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_bind_host_shell(struct wl_client *client, void *data,
                                uint32_t version, uint32_t id) {
  struct xwl_shell *shell = (struct xwl_shell *)data;
  struct xwl_host_shell *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->shell = shell;
  host->resource = wl_resource_create(client, &wl_shell_interface, 1, id);
  wl_resource_set_implementation(host->resource, &xwl_shell_implementation,
                                 host, xwl_destroy_host_shell);
  host->proxy = wl_registry_bind(wl_display_get_registry(shell->xwl->display),
                                 shell->id, &wl_shell_interface,
                                 wl_resource_get_version(host->resource));
  wl_shell_set_user_data(host->proxy, host);
}

static void xwl_output_geometry(void *data, struct wl_output *output, int x,
                                int y, int physical_width, int physical_height,
                                int subpixel, const char *make,
                                const char *model, int transform) {
  struct xwl_host_output *host = wl_output_get_user_data(output);

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

static void xwl_output_mode(void *data, struct wl_output *output,
                            uint32_t flags, int width, int height,
                            int refresh) {
  struct xwl_host_output *host = wl_output_get_user_data(output);

  host->flags = flags;
  host->width = width;
  host->height = height;
  host->refresh = refresh;
}

static void xwl_output_done(void *data, struct wl_output *output) {
  struct xwl_host_output *host = wl_output_get_user_data(output);
  int scale_factor;
  double scale;

  // Early out if current scale is expected but not yet know.
  if (!host->current_scale)
    return;

  // Always use 1 for scale factor and adjust geometry and mode based on max
  // scale factor for Xwayland client. Otherwise, pick an optimal scale factor
  // and adjust geometry and mode for it.
  if (host->output->xwl->xwayland) {
    double current_scale = host->current_scale / 1000.0;
    int max_scale_factor = host->max_scale / 1000.0;

    scale_factor = 1;
    scale = (host->output->xwl->scale * current_scale) / max_scale_factor;
  } else {
    scale_factor = ceil(host->scale_factor / host->output->xwl->scale);
    scale = (host->output->xwl->scale * scale_factor) / host->scale_factor;
  }

  wl_output_send_geometry(host->resource, host->x, host->y,
                          host->physical_width * scale,
                          host->physical_height * scale, host->subpixel,
                          host->make, host->model, host->transform);
  wl_output_send_mode(host->resource, host->flags | WL_OUTPUT_MODE_CURRENT,
                      host->width * scale, host->height * scale, host->refresh);
  wl_output_send_scale(host->resource, scale_factor);
  wl_output_send_done(host->resource);

  // Reset current scale.
  host->current_scale = 1000;

  // Expect current scale if aura output exists.
  if (host->aura_output)
    host->current_scale = 0;
}

static void xwl_output_scale(void *data, struct wl_output *output,
                             int32_t scale_factor) {
  struct xwl_host_output *host = wl_output_get_user_data(output);

  host->scale_factor = scale_factor;
}

static const struct wl_output_listener xwl_output_listener = {
    xwl_output_geometry, xwl_output_mode, xwl_output_done, xwl_output_scale};

static void xwl_aura_output_scale(void *data, struct zaura_output *output,
                                  uint32_t flags, uint32_t scale) {
  struct xwl_host_output *host = zaura_output_get_user_data(output);

  switch (scale) {
  case ZAURA_OUTPUT_SCALE_FACTOR_0500:
  case ZAURA_OUTPUT_SCALE_FACTOR_0600:
  case ZAURA_OUTPUT_SCALE_FACTOR_0625:
  case ZAURA_OUTPUT_SCALE_FACTOR_0750:
  case ZAURA_OUTPUT_SCALE_FACTOR_0800:
  case ZAURA_OUTPUT_SCALE_FACTOR_1000:
  case ZAURA_OUTPUT_SCALE_FACTOR_1125:
  case ZAURA_OUTPUT_SCALE_FACTOR_1200:
  case ZAURA_OUTPUT_SCALE_FACTOR_1250:
  case ZAURA_OUTPUT_SCALE_FACTOR_1500:
  case ZAURA_OUTPUT_SCALE_FACTOR_1600:
  case ZAURA_OUTPUT_SCALE_FACTOR_2000:
    break;
  default:
    fprintf(stderr, "Warning: Unknown scale factor: %d\n", scale);
    break;
  }

  if (flags & ZAURA_OUTPUT_SCALE_PROPERTY_CURRENT)
    host->current_scale = scale;

  host->max_scale = MAX(host->max_scale, scale);
}

static const struct zaura_output_listener xwl_aura_output_listener = {
    xwl_aura_output_scale};

static void xwl_destroy_host_output(struct wl_resource *resource) {
  struct xwl_host_output *host = wl_resource_get_user_data(resource);

  if (host->aura_output)
    zaura_output_destroy(host->aura_output);
  if (wl_output_get_version(host->proxy) >= WL_OUTPUT_RELEASE_SINCE_VERSION) {
    wl_output_release(host->proxy);
  } else {
    wl_output_destroy(host->proxy);
  }
  wl_resource_set_user_data(resource, NULL);
  free(host->make);
  free(host->model);
  free(host);
}

static void xwl_bind_host_output(struct wl_client *client, void *data,
                                 uint32_t version, uint32_t id) {
  struct xwl_output *output = (struct xwl_output *)data;
  struct xwl *xwl = output->xwl;
  struct xwl_host_output *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->output = output;
  host->resource = wl_resource_create(client, &wl_output_interface,
                                      MIN(version, output->version), id);
  wl_resource_set_implementation(host->resource, NULL, host,
                                 xwl_destroy_host_output);
  host->proxy = wl_registry_bind(wl_display_get_registry(xwl->display),
                                 output->id, &wl_output_interface,
                                 wl_resource_get_version(host->resource));
  wl_output_set_user_data(host->proxy, host);
  wl_output_add_listener(host->proxy, &xwl_output_listener, host);
  host->aura_output = NULL;
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
  host->max_scale = 1000;
  if (xwl->aura_shell &&
      (xwl->aura_shell->version >= ZAURA_SHELL_GET_AURA_OUTPUT_SINCE_VERSION)) {
    host->current_scale = 0;
    host->aura_output =
        zaura_shell_get_aura_output(xwl->aura_shell->internal, host->proxy);
    zaura_output_set_user_data(host->aura_output, host);
    zaura_output_add_listener(host->aura_output, &xwl_aura_output_listener,
                              host);
  }
}

static void xwl_internal_data_offer_destroy(struct xwl_data_offer *host) {
  wl_data_offer_destroy(host->internal);
  free(host);
}

static void xwl_set_selection(struct xwl *xwl,
                              struct xwl_data_offer *data_offer) {

  if (xwl->selection_data_offer) {
    xwl_internal_data_offer_destroy(xwl->selection_data_offer);
    xwl->selection_data_offer = NULL;
  }

  if (xwl->clipboard_manager) {
    if (!data_offer) {
      if (xwl->selection_owner == xwl->selection_window)
        xcb_set_selection_owner(xwl->connection, XCB_ATOM_NONE,
                                xwl->atoms[ATOM_CLIPBOARD].value,
                                xwl->selection_timestamp);
      return;
    }

    xcb_set_selection_owner(xwl->connection, xwl->selection_window,
                            xwl->atoms[ATOM_CLIPBOARD].value, XCB_CURRENT_TIME);
  }

  xwl->selection_data_offer = data_offer;
}

static const char *xwl_utf8_mime_type = "text/plain;charset=utf-8";

static void xwl_internal_data_offer_offer(void *data,
                                          struct wl_data_offer *data_offer,
                                          const char *type) {
  struct xwl_data_offer *host = data;

  if (strcmp(type, xwl_utf8_mime_type) == 0)
    host->utf8_text = 1;
}

static void xwl_internal_data_offer_source_actions(
    void *data, struct wl_data_offer *data_offer, uint32_t source_actions) {}

static void xwl_internal_data_offer_action(void *data,
                                           struct wl_data_offer *data_offer,
                                           uint32_t dnd_action) {}

static const struct wl_data_offer_listener xwl_internal_data_offer_listener = {
    xwl_internal_data_offer_offer, xwl_internal_data_offer_source_actions,
    xwl_internal_data_offer_action};

static void
xwl_internal_data_device_data_offer(void *data,
                                    struct wl_data_device *data_device,
                                    struct wl_data_offer *data_offer) {
  struct xwl *xwl = (struct xwl *)data;
  struct xwl_data_offer *host_data_offer;

  host_data_offer = malloc(sizeof(*host_data_offer));
  assert(host_data_offer);

  host_data_offer->xwl = xwl;
  host_data_offer->internal = data_offer;
  host_data_offer->utf8_text = 0;

  wl_data_offer_add_listener(host_data_offer->internal,
                             &xwl_internal_data_offer_listener,
                             host_data_offer);
}

static void xwl_internal_data_device_enter(void *data,
                                           struct wl_data_device *data_device,
                                           uint32_t serial,
                                           struct wl_surface *surface,
                                           wl_fixed_t x, wl_fixed_t y,
                                           struct wl_data_offer *data_offer) {}

static void xwl_internal_data_device_leave(void *data,
                                           struct wl_data_device *data_device) {
}

static void xwl_internal_data_device_motion(void *data,
                                            struct wl_data_device *data_device,
                                            uint32_t time, wl_fixed_t x,
                                            wl_fixed_t y) {}

static void xwl_internal_data_device_drop(void *data,
                                          struct wl_data_device *data_device) {}

static void
xwl_internal_data_device_selection(void *data,
                                   struct wl_data_device *data_device,
                                   struct wl_data_offer *data_offer) {
  struct xwl *xwl = (struct xwl *)data;
  struct xwl_data_offer *host_data_offer =
      data_offer ? wl_data_offer_get_user_data(data_offer) : NULL;

  xwl_set_selection(xwl, host_data_offer);
}

static const struct wl_data_device_listener xwl_internal_data_device_listener =
    {xwl_internal_data_device_data_offer, xwl_internal_data_device_enter,
     xwl_internal_data_device_leave,      xwl_internal_data_device_motion,
     xwl_internal_data_device_drop,       xwl_internal_data_device_selection};

static void xwl_host_pointer_set_cursor(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t serial,
                                        struct wl_resource *surface_resource,
                                        int32_t hotspot_x, int32_t hotspot_y) {
  struct xwl_host_pointer *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_surface = NULL;
  double scale = host->seat->xwl->scale;

  if (surface_resource) {
    host_surface = wl_resource_get_user_data(surface_resource);
    host_surface->is_cursor = 1;
    if (host_surface->contents_width && host_surface->contents_height)
      wl_surface_commit(host_surface->proxy);
  }

  wl_pointer_set_cursor(host->proxy, serial,
                        host_surface ? host_surface->proxy : NULL,
                        hotspot_x / scale, hotspot_y / scale);
}

static void xwl_host_pointer_release(struct wl_client *client,
                                     struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_pointer_interface xwl_pointer_implementation = {
    xwl_host_pointer_set_cursor, xwl_host_pointer_release};

static void xwl_set_last_event_serial(struct wl_resource *surface_resource,
                                      uint32_t serial) {
  struct xwl_host_surface *host_surface =
      wl_resource_get_user_data(surface_resource);

  host_surface->last_event_serial = serial;
}

static void xwl_pointer_set_focus(struct xwl_host_pointer *host,
                                  uint32_t serial,
                                  struct xwl_host_surface *host_surface,
                                  wl_fixed_t x, wl_fixed_t y) {
  struct wl_resource *surface_resource =
      host_surface ? host_surface->resource : NULL;

  if (surface_resource == host->focus_resource)
    return;

  if (host->focus_resource)
    wl_pointer_send_leave(host->resource, serial, host->focus_resource);

  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = surface_resource;
  host->focus_serial = serial;

  if (surface_resource) {
    double scale = host->seat->xwl->scale;

    if (host->seat->xwl->xwayland) {
      // Make sure focus surface is on top before sending enter event.
      xwl_restack_windows(host->seat->xwl,
                          wl_resource_get_id(surface_resource));
      xwl_roundtrip(host->seat->xwl);
    }

    wl_resource_add_destroy_listener(surface_resource,
                                     &host->focus_resource_listener);

    wl_pointer_send_enter(host->resource, serial, surface_resource, x * scale,
                          y * scale);
  }
}

static void xwl_pointer_enter(void *data, struct wl_pointer *pointer,
                              uint32_t serial, struct wl_surface *surface,
                              wl_fixed_t x, wl_fixed_t y) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);
  struct xwl_host_surface *host_surface =
      surface ? wl_surface_get_user_data(surface) : NULL;

  if (!host_surface)
    return;

  xwl_pointer_set_focus(host, serial, host_surface, x, y);

  if (host->focus_resource)
    xwl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;
}

static void xwl_pointer_leave(void *data, struct wl_pointer *pointer,
                              uint32_t serial, struct wl_surface *surface) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);

  xwl_pointer_set_focus(host, serial, NULL, 0, 0);
}

static void xwl_pointer_motion(void *data, struct wl_pointer *pointer,
                               uint32_t time, wl_fixed_t x, wl_fixed_t y) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);
  double scale = host->seat->xwl->scale;

  wl_pointer_send_motion(host->resource, time, x * scale, y * scale);
}

static void xwl_pointer_button(void *data, struct wl_pointer *pointer,
                               uint32_t serial, uint32_t time, uint32_t button,
                               uint32_t state) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);

  wl_pointer_send_button(host->resource, serial, time, button, state);

  if (host->focus_resource)
    xwl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;
}

static void xwl_pointer_axis(void *data, struct wl_pointer *pointer,
                             uint32_t time, uint32_t axis, wl_fixed_t value) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);
  double scale = host->seat->xwl->scale;

  wl_pointer_send_axis(host->resource, time, axis, value * scale);
}

static void xwl_pointer_frame(void *data, struct wl_pointer *pointer) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);

  wl_pointer_send_frame(host->resource);
}

void xwl_pointer_axis_source(void *data, struct wl_pointer *pointer,
                             uint32_t axis_source) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);

  wl_pointer_send_axis_source(host->resource, axis_source);
}

static void xwl_pointer_axis_stop(void *data, struct wl_pointer *pointer,
                                  uint32_t time, uint32_t axis) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);

  wl_pointer_send_axis_stop(host->resource, time, axis);
}

static void xwl_pointer_axis_discrete(void *data, struct wl_pointer *pointer,
                                      uint32_t axis, int32_t discrete) {
  struct xwl_host_pointer *host = wl_pointer_get_user_data(pointer);

  wl_pointer_send_axis_discrete(host->resource, axis, discrete);
}

static const struct wl_pointer_listener xwl_pointer_listener = {
    xwl_pointer_enter,       xwl_pointer_leave,     xwl_pointer_motion,
    xwl_pointer_button,      xwl_pointer_axis,      xwl_pointer_frame,
    xwl_pointer_axis_source, xwl_pointer_axis_stop, xwl_pointer_axis_discrete};

static void xwl_host_keyboard_release(struct wl_client *client,
                                      struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_keyboard_interface xwl_keyboard_implementation = {
    xwl_host_keyboard_release};

static void xwl_keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                                uint32_t format, int32_t fd, uint32_t size) {
  struct xwl_host_keyboard *host = wl_keyboard_get_user_data(keyboard);

  wl_keyboard_send_keymap(host->resource, format, fd, size);

  if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    void *data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

    assert(data != MAP_FAILED);

    if (host->keymap)
      xkb_keymap_unref(host->keymap);

    host->keymap = xkb_keymap_new_from_string(
        host->seat->xwl->xkb_context, data, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
    assert(host->keymap);

    munmap(data, size);

    if (host->state)
      xkb_state_unref(host->state);
    host->state = xkb_state_new(host->keymap);
    assert(host->state);

    host->control_mask = 1 << xkb_keymap_mod_get_index(host->keymap, "Control");
    host->alt_mask = 1 << xkb_keymap_mod_get_index(host->keymap, "Mod1");
    host->shift_mask = 1 << xkb_keymap_mod_get_index(host->keymap, "Shift");
  }

  close(fd);
}

static void xwl_keyboard_set_focus(struct xwl_host_keyboard *host,
                                   uint32_t serial,
                                   struct xwl_host_surface *host_surface,
                                   struct wl_array *keys) {
  struct wl_resource *surface_resource =
      host_surface ? host_surface->resource : NULL;

  if (surface_resource == host->focus_resource)
    return;

  if (host->focus_resource)
    wl_keyboard_send_leave(host->resource, serial, host->focus_resource);

  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = surface_resource;
  host->focus_serial = serial;

  if (surface_resource) {
    wl_resource_add_destroy_listener(surface_resource,
                                     &host->focus_resource_listener);
    wl_keyboard_send_enter(host->resource, serial, surface_resource, keys);
  }

  host->seat->last_serial = serial;
}

static void xwl_keyboard_enter(void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, struct wl_surface *surface,
                               struct wl_array *keys) {
  struct xwl_host_keyboard *host = wl_keyboard_get_user_data(keyboard);
  struct xwl_host_surface *host_surface =
      surface ? wl_surface_get_user_data(surface) : NULL;

  if (!host_surface)
    return;

  xwl_keyboard_set_focus(host, serial, host_surface, keys);

  host->seat->last_serial = serial;
}

static void xwl_keyboard_leave(void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, struct wl_surface *surface) {
  struct xwl_host_keyboard *host = wl_keyboard_get_user_data(keyboard);

  xwl_keyboard_set_focus(host, serial, NULL, NULL);
}

static void xwl_keyboard_key(void *data, struct wl_keyboard *keyboard,
                             uint32_t serial, uint32_t time, uint32_t key,
                             uint32_t state) {
  struct xwl_host_keyboard *host = wl_keyboard_get_user_data(keyboard);

  if (host->state) {
    const xkb_keysym_t *symbols;
    uint32_t num_symbols;
    xkb_keysym_t symbol = XKB_KEY_NoSymbol;
    uint32_t code = key + 8;
    struct xwl_accelerator *accelerator;

    num_symbols = xkb_state_key_get_syms(host->state, code, &symbols);
    if (num_symbols == 1)
      symbol = symbols[0];

    wl_list_for_each(accelerator, &host->seat->xwl->accelerators, link) {
      if (host->modifiers != accelerator->modifiers)
        continue;
      if (symbol != accelerator->symbol)
        continue;

      assert(host->extended_keyboard_proxy);
      zcr_extended_keyboard_v1_ack_key(
          host->extended_keyboard_proxy, serial,
          ZCR_EXTENDED_KEYBOARD_V1_HANDLED_STATE_NOT_HANDLED);
      return;
    }
  }

  wl_keyboard_send_key(host->resource, serial, time, key, state);

  if (host->focus_resource)
    xwl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;

  if (host->extended_keyboard_proxy) {
    zcr_extended_keyboard_v1_ack_key(
        host->extended_keyboard_proxy, serial,
        ZCR_EXTENDED_KEYBOARD_V1_HANDLED_STATE_HANDLED);
  }
}

static void xwl_keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                                   uint32_t serial, uint32_t mods_depressed,
                                   uint32_t mods_latched, uint32_t mods_locked,
                                   uint32_t group) {
  struct xwl_host_keyboard *host = wl_keyboard_get_user_data(keyboard);
  xkb_mod_mask_t mask;

  wl_keyboard_send_modifiers(host->resource, serial, mods_depressed,
                             mods_latched, mods_locked, group);

  if (host->focus_resource)
    xwl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;

  if (!host->keymap)
    return;

  xkb_state_update_mask(host->state, mods_depressed, mods_latched, mods_locked,
                        0, 0, group);
  mask = xkb_state_serialize_mods(host->state, XKB_STATE_MODS_DEPRESSED |
                                                   XKB_STATE_MODS_LATCHED);
  host->modifiers = 0;
  if (mask & host->control_mask)
    host->modifiers |= CONTROL_MASK;
  if (mask & host->alt_mask)
    host->modifiers |= ALT_MASK;
  if (mask & host->shift_mask)
    host->modifiers |= SHIFT_MASK;
}

static void xwl_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
                                     int32_t rate, int32_t delay) {
  struct xwl_host_keyboard *host = wl_keyboard_get_user_data(keyboard);

  wl_keyboard_send_repeat_info(host->resource, rate, delay);
}

static const struct wl_keyboard_listener xwl_keyboard_listener = {
    xwl_keyboard_keymap, xwl_keyboard_enter,     xwl_keyboard_leave,
    xwl_keyboard_key,    xwl_keyboard_modifiers, xwl_keyboard_repeat_info};

static void xwl_host_touch_release(struct wl_client *client,
                                   struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_touch_interface xwl_touch_implementation = {
    xwl_host_touch_release};

static void xwl_host_touch_down(void *data, struct wl_touch *touch,
                                uint32_t serial, uint32_t time,
                                struct wl_surface *surface, int32_t id,
                                wl_fixed_t x, wl_fixed_t y) {
  struct xwl_host_touch *host = wl_touch_get_user_data(touch);
  struct xwl_host_surface *host_surface =
      surface ? wl_surface_get_user_data(surface) : NULL;
  double scale = host->seat->xwl->scale;

  if (!host_surface)
    return;

  if (host_surface->resource != host->focus_resource) {
    wl_list_remove(&host->focus_resource_listener.link);
    wl_list_init(&host->focus_resource_listener.link);
    host->focus_resource = host_surface->resource;
    wl_resource_add_destroy_listener(host_surface->resource,
                                     &host->focus_resource_listener);
  }

  if (host->seat->xwl->xwayland) {
    // Make sure focus surface is on top before sending down event.
    xwl_restack_windows(host->seat->xwl,
                        wl_resource_get_id(host_surface->resource));
    xwl_roundtrip(host->seat->xwl);
  }

  wl_touch_send_down(host->resource, serial, time, host_surface->resource, id,
                     x * scale, y * scale);

  if (host->focus_resource)
    xwl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;
}

static void xwl_host_touch_up(void *data, struct wl_touch *touch,
                              uint32_t serial, uint32_t time, int32_t id) {
  struct xwl_host_touch *host = wl_touch_get_user_data(touch);

  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = NULL;

  wl_touch_send_up(host->resource, serial, time, id);

  if (host->focus_resource)
    xwl_set_last_event_serial(host->focus_resource, serial);
  host->seat->last_serial = serial;
}

static void xwl_host_touch_motion(void *data, struct wl_touch *touch,
                                  uint32_t time, int32_t id, wl_fixed_t x,
                                  wl_fixed_t y) {
  struct xwl_host_touch *host = wl_touch_get_user_data(touch);
  double scale = host->seat->xwl->scale;

  wl_touch_send_motion(host->resource, time, id, x * scale, y * scale);
}

static void xwl_host_touch_frame(void *data, struct wl_touch *touch) {
  struct xwl_host_touch *host = wl_touch_get_user_data(touch);

  wl_touch_send_frame(host->resource);
}

static void xwl_host_touch_cancel(void *data, struct wl_touch *touch) {
  struct xwl_host_touch *host = wl_touch_get_user_data(touch);

  wl_touch_send_cancel(host->resource);
}

static const struct wl_touch_listener xwl_touch_listener = {
    xwl_host_touch_down, xwl_host_touch_up, xwl_host_touch_motion,
    xwl_host_touch_frame, xwl_host_touch_cancel};

static void xwl_destroy_host_pointer(struct wl_resource *resource) {
  struct xwl_host_pointer *host = wl_resource_get_user_data(resource);

  if (wl_pointer_get_version(host->proxy) >= WL_POINTER_RELEASE_SINCE_VERSION) {
    wl_pointer_release(host->proxy);
  } else {
    wl_pointer_destroy(host->proxy);
  }
  wl_list_remove(&host->focus_resource_listener.link);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_pointer_focus_resource_destroyed(struct wl_listener *listener,
                                                 void *data) {
  struct xwl_host_pointer *host;

  host = wl_container_of(listener, host, focus_resource_listener);
  xwl_pointer_set_focus(host, host->focus_serial, NULL, 0, 0);
}

static void xwl_host_seat_get_host_pointer(struct wl_client *client,
                                           struct wl_resource *resource,
                                           uint32_t id) {
  struct xwl_host_seat *host = wl_resource_get_user_data(resource);
  struct xwl_host_pointer *host_pointer;

  host_pointer = malloc(sizeof(*host_pointer));
  assert(host_pointer);

  host_pointer->seat = host->seat;
  host_pointer->resource = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_pointer->resource,
                                 &xwl_pointer_implementation, host_pointer,
                                 xwl_destroy_host_pointer);
  host_pointer->proxy = wl_seat_get_pointer(host->proxy);
  wl_pointer_set_user_data(host_pointer->proxy, host_pointer);
  wl_pointer_add_listener(host_pointer->proxy, &xwl_pointer_listener,
                          host_pointer);
  wl_list_init(&host_pointer->focus_resource_listener.link);
  host_pointer->focus_resource_listener.notify =
      xwl_pointer_focus_resource_destroyed;
  host_pointer->focus_resource = NULL;
  host_pointer->focus_serial = 0;
}

static void xwl_destroy_host_keyboard(struct wl_resource *resource) {
  struct xwl_host_keyboard *host = wl_resource_get_user_data(resource);

  if (host->extended_keyboard_proxy)
    zcr_extended_keyboard_v1_destroy(host->extended_keyboard_proxy);

  if (host->keymap)
    xkb_keymap_unref(host->keymap);
  if (host->state)
    xkb_state_unref(host->state);

  if (wl_keyboard_get_version(host->proxy) >=
      WL_KEYBOARD_RELEASE_SINCE_VERSION) {
    wl_keyboard_release(host->proxy);
  } else {
    wl_keyboard_destroy(host->proxy);
  }
  wl_list_remove(&host->focus_resource_listener.link);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_keyboard_focus_resource_destroyed(struct wl_listener *listener,
                                                  void *data) {
  struct xwl_host_keyboard *host;

  host = wl_container_of(listener, host, focus_resource_listener);
  xwl_keyboard_set_focus(host, host->focus_serial, NULL, NULL);
}

static void xwl_host_seat_get_host_keyboard(struct wl_client *client,
                                            struct wl_resource *resource,
                                            uint32_t id) {
  struct xwl_host_seat *host = wl_resource_get_user_data(resource);
  struct xwl_host_keyboard *host_keyboard;

  host_keyboard = malloc(sizeof(*host_keyboard));
  assert(host_keyboard);

  host_keyboard->seat = host->seat;
  host_keyboard->resource = wl_resource_create(
      client, &wl_keyboard_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_keyboard->resource,
                                 &xwl_keyboard_implementation, host_keyboard,
                                 xwl_destroy_host_keyboard);
  host_keyboard->proxy = wl_seat_get_keyboard(host->proxy);
  wl_keyboard_set_user_data(host_keyboard->proxy, host_keyboard);
  wl_keyboard_add_listener(host_keyboard->proxy, &xwl_keyboard_listener,
                           host_keyboard);
  wl_list_init(&host_keyboard->focus_resource_listener.link);
  host_keyboard->focus_resource_listener.notify =
      xwl_keyboard_focus_resource_destroyed;
  host_keyboard->focus_resource = NULL;
  host_keyboard->focus_serial = 0;
  host_keyboard->keymap = NULL;
  host_keyboard->state = NULL;
  host_keyboard->control_mask = 0;
  host_keyboard->alt_mask = 0;
  host_keyboard->shift_mask = 0;
  host_keyboard->modifiers = 0;

  if (host->seat->xwl->keyboard_extension) {
    host_keyboard->extended_keyboard_proxy =
        zcr_keyboard_extension_v1_get_extended_keyboard(
            host->seat->xwl->keyboard_extension->internal,
            host_keyboard->proxy);
  } else {
    host_keyboard->extended_keyboard_proxy = NULL;
  }
}

static void xwl_destroy_host_touch(struct wl_resource *resource) {
  struct xwl_host_touch *host = wl_resource_get_user_data(resource);

  if (wl_touch_get_version(host->proxy) >= WL_TOUCH_RELEASE_SINCE_VERSION) {
    wl_touch_release(host->proxy);
  } else {
    wl_touch_destroy(host->proxy);
  }
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_touch_focus_resource_destroyed(struct wl_listener *listener,
                                               void *data) {
  struct xwl_host_touch *host;

  host = wl_container_of(listener, host, focus_resource_listener);
  wl_list_remove(&host->focus_resource_listener.link);
  wl_list_init(&host->focus_resource_listener.link);
  host->focus_resource = NULL;
}

static void xwl_host_seat_get_host_touch(struct wl_client *client,
                                         struct wl_resource *resource,
                                         uint32_t id) {
  struct xwl_host_seat *host = wl_resource_get_user_data(resource);
  struct xwl_host_touch *host_touch;

  host_touch = malloc(sizeof(*host_touch));
  assert(host_touch);

  host_touch->seat = host->seat;
  host_touch->resource = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(host_touch->resource,
                                 &xwl_touch_implementation, host_touch,
                                 xwl_destroy_host_touch);
  host_touch->proxy = wl_seat_get_touch(host->proxy);
  wl_touch_set_user_data(host_touch->proxy, host_touch);
  wl_touch_add_listener(host_touch->proxy, &xwl_touch_listener, host_touch);
  wl_list_init(&host_touch->focus_resource_listener.link);
  host_touch->focus_resource_listener.notify =
      xwl_touch_focus_resource_destroyed;
  host_touch->focus_resource = NULL;
}

static void xwl_host_seat_release(struct wl_client *client,
                                  struct wl_resource *resource) {
  struct xwl_host_seat *host = wl_resource_get_user_data(resource);

  wl_seat_release(host->proxy);
}

static const struct wl_seat_interface xwl_seat_implementation = {
    xwl_host_seat_get_host_pointer, xwl_host_seat_get_host_keyboard,
    xwl_host_seat_get_host_touch, xwl_host_seat_release};

static void xwl_seat_capabilities(void *data, struct wl_seat *seat,
                                  uint32_t capabilities) {
  struct xwl_host_seat *host = wl_seat_get_user_data(seat);

  wl_seat_send_capabilities(host->resource, capabilities);
}

static void xwl_seat_name(void *data, struct wl_seat *seat, const char *name) {
  struct xwl_host_seat *host = wl_seat_get_user_data(seat);

  if (wl_resource_get_version(host->resource) >= WL_SEAT_NAME_SINCE_VERSION)
    wl_seat_send_name(host->resource, name);
}

static const struct wl_seat_listener xwl_seat_listener = {xwl_seat_capabilities,
                                                          xwl_seat_name};

static void xwl_destroy_host_seat(struct wl_resource *resource) {
  struct xwl_host_seat *host = wl_resource_get_user_data(resource);

  if (host->seat->xwl->default_seat == host)
    host->seat->xwl->default_seat = NULL;

  if (wl_seat_get_version(host->proxy) >= WL_SEAT_RELEASE_SINCE_VERSION)
    wl_seat_release(host->proxy);
  else
    wl_seat_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_bind_host_seat(struct wl_client *client, void *data,
                               uint32_t version, uint32_t id) {
  struct xwl_seat *seat = (struct xwl_seat *)data;
  struct xwl_host_seat *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->seat = seat;
  host->resource = wl_resource_create(client, &wl_seat_interface,
                                      MIN(version, seat->version), id);
  wl_resource_set_implementation(host->resource, &xwl_seat_implementation, host,
                                 xwl_destroy_host_seat);
  host->proxy = wl_registry_bind(wl_display_get_registry(seat->xwl->display),
                                 seat->id, &wl_seat_interface,
                                 wl_resource_get_version(host->resource));
  wl_seat_set_user_data(host->proxy, host);
  wl_seat_add_listener(host->proxy, &xwl_seat_listener, host);

  if (!seat->xwl->default_seat) {
    seat->xwl->default_seat = host;
    if (seat->xwl->data_device_manager &&
        seat->xwl->data_device_manager->internal) {
      seat->xwl->selection_data_device = wl_data_device_manager_get_data_device(
          seat->xwl->data_device_manager->internal, host->proxy);
      wl_data_device_add_listener(seat->xwl->selection_data_device,
                                  &xwl_internal_data_device_listener,
                                  seat->xwl);
    }
  }
}

static void xwl_drm_authenticate(struct wl_client *client,
                                 struct wl_resource *resource, uint32_t id) {}

static void xwl_drm_create_buffer(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t id,
                                  uint32_t name, int32_t width, int32_t height,
                                  uint32_t stride, uint32_t format) {
  assert(0);
}

static void xwl_drm_create_planar_buffer(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    uint32_t name, int32_t width, int32_t height, uint32_t format,
    int32_t offset0, int32_t stride0, int32_t offset1, int32_t stride1,
    int32_t offset2, int32_t stride2) {
  assert(0);
}

static void xwl_drm_create_prime_buffer(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    int32_t name, int32_t width, int32_t height, uint32_t format,
    int32_t offset0, int32_t stride0, int32_t offset1, int32_t stride1,
    int32_t offset2, int32_t stride2) {
  struct xwl_host_drm *host = wl_resource_get_user_data(resource);
  struct xwl_host_buffer *host_buffer;
  struct zwp_linux_buffer_params_v1 *buffer_params;

  assert(name >= 0);
  assert(!offset1);
  assert(!stride1);
  assert(!offset2);
  assert(!stride2);

  host_buffer = malloc(sizeof(*host_buffer));
  assert(host_buffer);

  host_buffer->width = width;
  host_buffer->height = height;
  host_buffer->shm_mmap = NULL;
  host_buffer->shm_format = 0;
  host_buffer->resource =
      wl_resource_create(client, &wl_buffer_interface, 1, id);
  wl_resource_set_implementation(host_buffer->resource,
                                 &xwl_buffer_implementation, host_buffer,
                                 xwl_destroy_host_buffer);
  buffer_params =
      zwp_linux_dmabuf_v1_create_params(host->linux_dmabuf->internal);
  zwp_linux_buffer_params_v1_add(buffer_params, name, 0, offset0, stride0, 0,
                                 0);
  host_buffer->proxy = zwp_linux_buffer_params_v1_create_immed(
      buffer_params, width, height, format, 0);
  zwp_linux_buffer_params_v1_destroy(buffer_params);
  close(name);
  wl_buffer_set_user_data(host_buffer->proxy, host_buffer);
  wl_buffer_add_listener(host_buffer->proxy, &xwl_buffer_listener, host_buffer);
}

static const struct wl_drm_interface xwl_drm_implementation = {
    xwl_drm_authenticate, xwl_drm_create_buffer, xwl_drm_create_planar_buffer,
    xwl_drm_create_prime_buffer};

static void xwl_destroy_host_drm(struct wl_resource *resource) {
  struct xwl_host_drm *host = wl_resource_get_user_data(resource);

  wl_callback_destroy(host->callback);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_drm_callback_done(void *data, struct wl_callback *callback,
                                  uint32_t serial) {
  struct xwl_host_drm *host = wl_callback_get_user_data(callback);

  wl_drm_send_device(host->resource, host->linux_dmabuf->xwl->drm_device);
  wl_drm_send_format(host->resource, WL_DRM_FORMAT_ARGB8888);
  wl_drm_send_format(host->resource, WL_DRM_FORMAT_XRGB8888);
  wl_drm_send_format(host->resource, WL_DRM_FORMAT_RGB565);
  if (host->version >= WL_DRM_CREATE_PRIME_BUFFER_SINCE_VERSION)
    wl_drm_send_capabilities(host->resource, WL_DRM_CAPABILITY_PRIME);
}

static const struct wl_callback_listener xwl_drm_callback_listener = {
    xwl_drm_callback_done};

static void xwl_bind_host_drm(struct wl_client *client, void *data,
                              uint32_t version, uint32_t id) {
  struct xwl_linux_dmabuf *linux_dmabuf = (struct xwl_linux_dmabuf *)data;
  struct xwl_host_drm *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->linux_dmabuf = linux_dmabuf;
  host->version = MIN(version, 2);
  host->resource =
      wl_resource_create(client, &wl_drm_interface, host->version, id);
  wl_resource_set_implementation(host->resource, &xwl_drm_implementation, host,
                                 xwl_destroy_host_drm);

  host->callback = wl_display_sync(linux_dmabuf->xwl->display);
  wl_callback_set_user_data(host->callback, host);
  wl_callback_add_listener(host->callback, &xwl_drm_callback_listener, host);
}

static void xwl_xdg_positioner_destroy(struct wl_client *client,
                                       struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_xdg_positioner_set_size(struct wl_client *client,
                                        struct wl_resource *resource,
                                        int32_t width, int32_t height) {
  struct xwl_host_xdg_positioner *host = wl_resource_get_user_data(resource);
  double scale = host->xwl->scale;

  zxdg_positioner_v6_set_size(host->proxy, width / scale, height / scale);
}

static void xwl_xdg_positioner_set_anchor_rect(struct wl_client *client,
                                               struct wl_resource *resource,
                                               int32_t x, int32_t y,
                                               int32_t width, int32_t height) {
  struct xwl_host_xdg_positioner *host = wl_resource_get_user_data(resource);
  double scale = host->xwl->scale;
  int32_t x1, y1, x2, y2;

  x1 = x / scale;
  y1 = y / scale;
  x2 = (x + width) / scale;
  y2 = (y + height) / scale;

  zxdg_positioner_v6_set_anchor_rect(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void xwl_xdg_positioner_set_anchor(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t anchor) {
  struct xwl_host_xdg_positioner *host = wl_resource_get_user_data(resource);

  zxdg_positioner_v6_set_anchor(host->proxy, anchor);
}

static void xwl_xdg_positioner_set_gravity(struct wl_client *client,
                                           struct wl_resource *resource,
                                           uint32_t gravity) {
  struct xwl_host_xdg_positioner *host = wl_resource_get_user_data(resource);

  zxdg_positioner_v6_set_gravity(host->proxy, gravity);
}

static void
xwl_xdg_positioner_set_constraint_adjustment(struct wl_client *client,
                                             struct wl_resource *resource,
                                             uint32_t constraint_adjustment) {
  struct xwl_host_xdg_positioner *host = wl_resource_get_user_data(resource);

  zxdg_positioner_v6_set_constraint_adjustment(host->proxy,
                                               constraint_adjustment);
}

static void xwl_xdg_positioner_set_offset(struct wl_client *client,
                                          struct wl_resource *resource,
                                          int32_t x, int32_t y) {
  struct xwl_host_xdg_positioner *host = wl_resource_get_user_data(resource);
  double scale = host->xwl->scale;

  zxdg_positioner_v6_set_offset(host->proxy, x / scale, y / scale);
}

static const struct zxdg_positioner_v6_interface
    xwl_xdg_positioner_implementation = {
        xwl_xdg_positioner_destroy,
        xwl_xdg_positioner_set_size,
        xwl_xdg_positioner_set_anchor_rect,
        xwl_xdg_positioner_set_anchor,
        xwl_xdg_positioner_set_gravity,
        xwl_xdg_positioner_set_constraint_adjustment,
        xwl_xdg_positioner_set_offset};

static void xwl_destroy_host_xdg_positioner(struct wl_resource *resource) {
  struct xwl_host_xdg_positioner *host = wl_resource_get_user_data(resource);

  zxdg_positioner_v6_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_xdg_popup_destroy(struct wl_client *client,
                                  struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_xdg_popup_grab(struct wl_client *client,
                               struct wl_resource *resource,
                               struct wl_resource *seat_resource,
                               uint32_t serial) {
  struct xwl_host_xdg_popup *host = wl_resource_get_user_data(resource);
  struct xwl_host_seat *host_seat = wl_resource_get_user_data(seat_resource);

  zxdg_popup_v6_grab(host->proxy, host_seat->proxy, serial);
}

static const struct zxdg_popup_v6_interface xwl_xdg_popup_implementation = {
    xwl_xdg_popup_destroy, xwl_xdg_popup_grab};

static void xwl_xdg_popup_configure(void *data, struct zxdg_popup_v6 *xdg_popup,
                                    int32_t x, int32_t y, int32_t width,
                                    int32_t height) {
  struct xwl_host_xdg_popup *host = zxdg_popup_v6_get_user_data(xdg_popup);
  double scale = host->xwl->scale;
  int32_t x1, y1, x2, y2;

  x1 = x * scale;
  y1 = y * scale;
  x2 = (x + width) * scale;
  y2 = (y + height) * scale;

  zxdg_popup_v6_send_configure(host->resource, x1, y1, x2 - x1, y2 - y1);
}

static void xwl_xdg_popup_popup_done(void *data,
                                     struct zxdg_popup_v6 *xdg_popup) {
  struct xwl_host_xdg_popup *host = zxdg_popup_v6_get_user_data(xdg_popup);

  zxdg_popup_v6_send_popup_done(host->resource);
}

static const struct zxdg_popup_v6_listener xwl_xdg_popup_listener = {
    xwl_xdg_popup_configure, xwl_xdg_popup_popup_done};

static void xwl_destroy_host_xdg_popup(struct wl_resource *resource) {
  struct xwl_host_xdg_popup *host = wl_resource_get_user_data(resource);

  zxdg_popup_v6_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_xdg_toplevel_destroy(struct wl_client *client,
                                     struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_xdg_toplevel_set_parent(struct wl_client *client,
                                        struct wl_resource *resource,
                                        struct wl_resource *parent_resource) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);
  struct xwl_host_xdg_toplevel *host_parent =
      parent_resource ? wl_resource_get_user_data(parent_resource) : NULL;

  zxdg_toplevel_v6_set_parent(host->proxy,
                              host_parent ? host_parent->proxy : NULL);
}

static void xwl_xdg_toplevel_set_title(struct wl_client *client,
                                       struct wl_resource *resource,
                                       const char *title) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_set_title(host->proxy, title);
}

static void xwl_xdg_toplevel_set_app_id(struct wl_client *client,
                                        struct wl_resource *resource,
                                        const char *app_id) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_set_app_id(host->proxy, app_id);
}

static void xwl_xdg_toplevel_show_window_menu(struct wl_client *client,
                                              struct wl_resource *resource,
                                              struct wl_resource *seat_resource,
                                              uint32_t serial, int32_t x,
                                              int32_t y) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);
  struct xwl_host_seat *host_seat =
      seat_resource ? wl_resource_get_user_data(seat_resource) : NULL;

  zxdg_toplevel_v6_show_window_menu(
      host->proxy, host_seat ? host_seat->proxy : NULL, serial, x, y);
}

static void xwl_xdg_toplevel_move(struct wl_client *client,
                                  struct wl_resource *resource,
                                  struct wl_resource *seat_resource,
                                  uint32_t serial) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);
  struct xwl_host_seat *host_seat =
      seat_resource ? wl_resource_get_user_data(seat_resource) : NULL;

  zxdg_toplevel_v6_move(host->proxy, host_seat ? host_seat->proxy : NULL,
                        serial);
}

static void xwl_xdg_toplevel_resize(struct wl_client *client,
                                    struct wl_resource *resource,
                                    struct wl_resource *seat_resource,
                                    uint32_t serial, uint32_t edges) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);
  struct xwl_host_seat *host_seat =
      seat_resource ? wl_resource_get_user_data(seat_resource) : NULL;

  zxdg_toplevel_v6_resize(host->proxy, host_seat ? host_seat->proxy : NULL,
                          serial, edges);
}

static void xwl_xdg_toplevel_set_max_size(struct wl_client *client,
                                          struct wl_resource *resource,
                                          int32_t width, int32_t height) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_set_max_size(host->proxy, width, height);
}

static void xwl_xdg_toplevel_set_min_size(struct wl_client *client,
                                          struct wl_resource *resource,
                                          int32_t width, int32_t height) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_set_min_size(host->proxy, width, height);
}

static void xwl_xdg_toplevel_set_maximized(struct wl_client *client,
                                           struct wl_resource *resource) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_set_maximized(host->proxy);
}

static void xwl_xdg_toplevel_unset_maximized(struct wl_client *client,
                                             struct wl_resource *resource) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_unset_maximized(host->proxy);
}

static void
xwl_xdg_toplevel_set_fullscreen(struct wl_client *client,
                                struct wl_resource *resource,
                                struct wl_resource *output_resource) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);
  struct xwl_host_output *host_output =
      output_resource ? wl_resource_get_user_data(resource) : NULL;

  zxdg_toplevel_v6_set_fullscreen(host->proxy,
                                  host_output ? host_output->proxy : NULL);
}

static void xwl_xdg_toplevel_unset_fullscreen(struct wl_client *client,
                                              struct wl_resource *resource) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_unset_fullscreen(host->proxy);
}

static void xwl_xdg_toplevel_set_minimized(struct wl_client *client,
                                           struct wl_resource *resource) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_set_minimized(host->proxy);
}

static const struct zxdg_toplevel_v6_interface xwl_xdg_toplevel_implementation =
    {xwl_xdg_toplevel_destroy,          xwl_xdg_toplevel_set_parent,
     xwl_xdg_toplevel_set_title,        xwl_xdg_toplevel_set_app_id,
     xwl_xdg_toplevel_show_window_menu, xwl_xdg_toplevel_move,
     xwl_xdg_toplevel_resize,           xwl_xdg_toplevel_set_max_size,
     xwl_xdg_toplevel_set_min_size,     xwl_xdg_toplevel_set_maximized,
     xwl_xdg_toplevel_unset_maximized,  xwl_xdg_toplevel_set_fullscreen,
     xwl_xdg_toplevel_unset_fullscreen, xwl_xdg_toplevel_set_minimized};

static void xwl_xdg_toplevel_configure(void *data,
                                       struct zxdg_toplevel_v6 *xdg_toplevel,
                                       int32_t width, int32_t height,
                                       struct wl_array *states) {
  struct xwl_host_xdg_toplevel *host =
      zxdg_toplevel_v6_get_user_data(xdg_toplevel);
  double scale = host->xwl->scale;

  zxdg_toplevel_v6_send_configure(host->resource, width * scale, height * scale,
                                  states);
}

static void xwl_xdg_toplevel_close(void *data,
                                   struct zxdg_toplevel_v6 *xdg_toplevel) {
  struct xwl_host_xdg_toplevel *host =
      zxdg_toplevel_v6_get_user_data(xdg_toplevel);

  zxdg_toplevel_v6_send_close(host->resource);
}

static const struct zxdg_toplevel_v6_listener xwl_xdg_toplevel_listener = {
    xwl_xdg_toplevel_configure, xwl_xdg_toplevel_close};

static void xwl_destroy_host_xdg_toplevel(struct wl_resource *resource) {
  struct xwl_host_xdg_toplevel *host = wl_resource_get_user_data(resource);

  zxdg_toplevel_v6_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_xdg_surface_destroy(struct wl_client *client,
                                    struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_xdg_surface_get_toplevel(struct wl_client *client,
                                         struct wl_resource *resource,
                                         uint32_t id) {
  struct xwl_host_xdg_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_xdg_toplevel *host_xdg_toplevel;

  host_xdg_toplevel = malloc(sizeof(*host_xdg_toplevel));
  assert(host_xdg_toplevel);

  host_xdg_toplevel->xwl = host->xwl;
  host_xdg_toplevel->resource =
      wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);
  wl_resource_set_implementation(
      host_xdg_toplevel->resource, &xwl_xdg_toplevel_implementation,
      host_xdg_toplevel, xwl_destroy_host_xdg_toplevel);
  host_xdg_toplevel->proxy = zxdg_surface_v6_get_toplevel(host->proxy);
  zxdg_toplevel_v6_set_user_data(host_xdg_toplevel->proxy, host_xdg_toplevel);
  zxdg_toplevel_v6_add_listener(host_xdg_toplevel->proxy,
                                &xwl_xdg_toplevel_listener, host_xdg_toplevel);
}

static void xwl_xdg_surface_get_popup(struct wl_client *client,
                                      struct wl_resource *resource, uint32_t id,
                                      struct wl_resource *parent_resource,
                                      struct wl_resource *positioner_resource) {
  struct xwl_host_xdg_surface *host = wl_resource_get_user_data(resource);
  struct xwl_host_xdg_surface *host_parent =
      wl_resource_get_user_data(parent_resource);
  struct xwl_host_xdg_positioner *host_positioner =
      wl_resource_get_user_data(positioner_resource);
  struct xwl_host_xdg_popup *host_xdg_popup;

  host_xdg_popup = malloc(sizeof(*host_xdg_popup));
  assert(host_xdg_popup);

  host_xdg_popup->xwl = host->xwl;
  host_xdg_popup->resource =
      wl_resource_create(client, &zxdg_popup_v6_interface, 1, id);
  wl_resource_set_implementation(host_xdg_popup->resource,
                                 &xwl_xdg_popup_implementation, host_xdg_popup,
                                 xwl_destroy_host_xdg_popup);
  host_xdg_popup->proxy = zxdg_surface_v6_get_popup(
      host->proxy, host_parent->proxy, host_positioner->proxy);
  zxdg_popup_v6_set_user_data(host_xdg_popup->proxy, host_xdg_popup);
  zxdg_popup_v6_add_listener(host_xdg_popup->proxy, &xwl_xdg_popup_listener,
                             host_xdg_popup);
}

static void xwl_xdg_surface_set_window_geometry(struct wl_client *client,
                                                struct wl_resource *resource,
                                                int32_t x, int32_t y,
                                                int32_t width, int32_t height) {
  struct xwl_host_xdg_surface *host = wl_resource_get_user_data(resource);
  double scale = host->xwl->scale;
  int32_t x1, y1, x2, y2;

  x1 = x / scale;
  y1 = y / scale;
  x2 = (x + width) / scale;
  y2 = (y + height) / scale;

  zxdg_surface_v6_set_window_geometry(host->proxy, x1, y1, x2 - x1, y2 - y1);
}

static void xwl_xdg_surface_ack_configure(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t serial) {
  struct xwl_host_xdg_surface *host = wl_resource_get_user_data(resource);

  zxdg_surface_v6_ack_configure(host->proxy, serial);
}

static const struct zxdg_surface_v6_interface xwl_xdg_surface_implementation = {
    xwl_xdg_surface_destroy, xwl_xdg_surface_get_toplevel,
    xwl_xdg_surface_get_popup, xwl_xdg_surface_set_window_geometry,
    xwl_xdg_surface_ack_configure};

static void xwl_xdg_surface_configure(void *data,
                                      struct zxdg_surface_v6 *xdg_surface,
                                      uint32_t serial) {
  struct xwl_host_xdg_surface *host =
      zxdg_surface_v6_get_user_data(xdg_surface);

  zxdg_surface_v6_send_configure(host->resource, serial);
}

static const struct zxdg_surface_v6_listener xwl_xdg_surface_listener = {
    xwl_xdg_surface_configure};

static void xwl_destroy_host_xdg_surface(struct wl_resource *resource) {
  struct xwl_host_xdg_surface *host = wl_resource_get_user_data(resource);

  zxdg_surface_v6_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_xdg_shell_destroy(struct wl_client *client,
                                  struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_xdg_shell_create_positioner(struct wl_client *client,
                                            struct wl_resource *resource,
                                            uint32_t id) {
  struct xwl_host_xdg_shell *host = wl_resource_get_user_data(resource);
  struct xwl_host_xdg_positioner *host_xdg_positioner;

  host_xdg_positioner = malloc(sizeof(*host_xdg_positioner));
  assert(host_xdg_positioner);

  host_xdg_positioner->xwl = host->xdg_shell->xwl;
  host_xdg_positioner->resource =
      wl_resource_create(client, &zxdg_positioner_v6_interface, 1, id);
  wl_resource_set_implementation(
      host_xdg_positioner->resource, &xwl_xdg_positioner_implementation,
      host_xdg_positioner, xwl_destroy_host_xdg_positioner);
  host_xdg_positioner->proxy = zxdg_shell_v6_create_positioner(host->proxy);
  zxdg_positioner_v6_set_user_data(host_xdg_positioner->proxy,
                                   host_xdg_positioner);
}

static void
xwl_xdg_shell_get_xdg_surface(struct wl_client *client,
                              struct wl_resource *resource, uint32_t id,
                              struct wl_resource *surface_resource) {
  struct xwl_host_xdg_shell *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_surface =
      wl_resource_get_user_data(surface_resource);
  struct xwl_host_xdg_surface *host_xdg_surface;

  host_xdg_surface = malloc(sizeof(*host_xdg_surface));
  assert(host_xdg_surface);

  host_xdg_surface->xwl = host->xdg_shell->xwl;
  host_xdg_surface->resource =
      wl_resource_create(client, &zxdg_surface_v6_interface, 1, id);
  wl_resource_set_implementation(
      host_xdg_surface->resource, &xwl_xdg_surface_implementation,
      host_xdg_surface, xwl_destroy_host_xdg_surface);
  host_xdg_surface->proxy =
      zxdg_shell_v6_get_xdg_surface(host->proxy, host_surface->proxy);
  zxdg_surface_v6_set_user_data(host_xdg_surface->proxy, host_xdg_surface);
  zxdg_surface_v6_add_listener(host_xdg_surface->proxy,
                               &xwl_xdg_surface_listener, host_xdg_surface);
}

static void xwl_xdg_shell_pong(struct wl_client *client,
                               struct wl_resource *resource, uint32_t serial) {
  struct xwl_host_xdg_shell *host = wl_resource_get_user_data(resource);

  zxdg_shell_v6_pong(host->proxy, serial);
}

static const struct zxdg_shell_v6_interface xwl_xdg_shell_implementation = {
    xwl_xdg_shell_destroy, xwl_xdg_shell_create_positioner,
    xwl_xdg_shell_get_xdg_surface, xwl_xdg_shell_pong};

static void xwl_xdg_shell_ping(void *data, struct zxdg_shell_v6 *xdg_shell,
                               uint32_t serial) {
  struct xwl_host_xdg_shell *host = zxdg_shell_v6_get_user_data(xdg_shell);

  zxdg_shell_v6_send_ping(host->resource, serial);
}

static const struct zxdg_shell_v6_listener xwl_xdg_shell_listener = {
    xwl_xdg_shell_ping};

static void xwl_destroy_host_xdg_shell(struct wl_resource *resource) {
  struct xwl_host_xdg_shell *host = wl_resource_get_user_data(resource);

  zxdg_shell_v6_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_bind_host_xdg_shell(struct wl_client *client, void *data,
                                    uint32_t version, uint32_t id) {
  struct xwl_xdg_shell *xdg_shell = (struct xwl_xdg_shell *)data;
  struct xwl_host_xdg_shell *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->xdg_shell = xdg_shell;
  host->resource = wl_resource_create(client, &zxdg_shell_v6_interface, 1, id);
  wl_resource_set_implementation(host->resource, &xwl_xdg_shell_implementation,
                                 host, xwl_destroy_host_xdg_shell);
  host->proxy =
      wl_registry_bind(wl_display_get_registry(xdg_shell->xwl->display),
                       xdg_shell->id, &zxdg_shell_v6_interface, 1);
  zxdg_shell_v6_set_user_data(host->proxy, host);
  zxdg_shell_v6_add_listener(host->proxy, &xwl_xdg_shell_listener, host);
}

static void xwl_data_offer_accept(struct wl_client *client,
                                  struct wl_resource *resource, uint32_t serial,
                                  const char *mime_type) {
  struct xwl_host_data_offer *host = wl_resource_get_user_data(resource);

  wl_data_offer_accept(host->proxy, serial, mime_type);
}

static void xwl_data_offer_receive(struct wl_client *client,
                                   struct wl_resource *resource,
                                   const char *mime_type, int32_t fd) {
  struct xwl_host_data_offer *host = wl_resource_get_user_data(resource);

  switch (host->xwl->data_driver) {
  case DATA_DRIVER_VIRTWL: {
    struct virtwl_ioctl_new new_pipe = {
        .type = VIRTWL_IOCTL_NEW_PIPE_READ, .fd = -1, .flags = 0, .size = 0,
    };
    int rv;

    rv = ioctl(host->xwl->virtwl_fd, VIRTWL_IOCTL_NEW, &new_pipe);
    if (rv) {
      fprintf(stderr, "error: failed to create virtwl pipe: %s\n",
              strerror(errno));
      close(fd);
      return;
    }

    xwl_data_transfer_create(wl_display_get_event_loop(host->xwl->host_display),
                             new_pipe.fd, fd);

    wl_data_offer_receive(host->proxy, mime_type, new_pipe.fd);
  } break;
  case DATA_DRIVER_NOOP:
    wl_data_offer_receive(host->proxy, mime_type, fd);
    close(fd);
    break;
  }
}

static void xwl_data_offer_destroy(struct wl_client *client,
                                   struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_data_offer_finish(struct wl_client *client,
                                  struct wl_resource *resource) {
  struct xwl_host_data_offer *host = wl_resource_get_user_data(resource);

  wl_data_offer_finish(host->proxy);
}

static void xwl_data_offer_set_actions(struct wl_client *client,
                                       struct wl_resource *resource,
                                       uint32_t dnd_actions,
                                       uint32_t preferred_action) {
  struct xwl_host_data_offer *host = wl_resource_get_user_data(resource);

  wl_data_offer_set_actions(host->proxy, dnd_actions, preferred_action);
}

static const struct wl_data_offer_interface xwl_data_offer_implementation = {
    xwl_data_offer_accept, xwl_data_offer_receive, xwl_data_offer_destroy,
    xwl_data_offer_finish, xwl_data_offer_set_actions};

static void xwl_data_offer_offer(void *data, struct wl_data_offer *data_offer,
                                 const char *mime_type) {
  struct xwl_host_data_offer *host = wl_data_offer_get_user_data(data_offer);

  wl_data_offer_send_offer(host->resource, mime_type);
}

static void xwl_data_offer_source_actions(void *data,
                                          struct wl_data_offer *data_offer,
                                          uint32_t source_actions) {
  struct xwl_host_data_offer *host = wl_data_offer_get_user_data(data_offer);

  wl_data_offer_send_source_actions(host->resource, source_actions);
}

static void xwl_data_offer_action(void *data, struct wl_data_offer *data_offer,
                                  uint32_t dnd_action) {
  struct xwl_host_data_offer *host = wl_data_offer_get_user_data(data_offer);

  wl_data_offer_send_action(host->resource, dnd_action);
}

static const struct wl_data_offer_listener xwl_data_offer_listener = {
    xwl_data_offer_offer, xwl_data_offer_source_actions, xwl_data_offer_action};

static void xwl_destroy_host_data_offer(struct wl_resource *resource) {
  struct xwl_host_data_offer *host = wl_resource_get_user_data(resource);

  wl_data_offer_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_data_source_offer(struct wl_client *client,
                                  struct wl_resource *resource,
                                  const char *mime_type) {
  struct xwl_host_data_source *host = wl_resource_get_user_data(resource);

  wl_data_source_offer(host->proxy, mime_type);
}

static void xwl_data_source_destroy(struct wl_client *client,
                                    struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_data_source_set_actions(struct wl_client *client,
                                        struct wl_resource *resource,
                                        uint32_t dnd_actions) {
  struct xwl_host_data_source *host = wl_resource_get_user_data(resource);

  wl_data_source_set_actions(host->proxy, dnd_actions);
}

static const struct wl_data_source_interface xwl_data_source_implementation = {
    xwl_data_source_offer, xwl_data_source_destroy,
    xwl_data_source_set_actions};

static void xwl_data_source_target(void *data,
                                   struct wl_data_source *data_source,
                                   const char *mime_type) {
  struct xwl_host_data_source *host = wl_data_source_get_user_data(data_source);

  wl_data_source_send_target(host->resource, mime_type);
}

static void xwl_data_source_send(void *data, struct wl_data_source *data_source,
                                 const char *mime_type, int32_t fd) {
  struct xwl_host_data_source *host = wl_data_source_get_user_data(data_source);

  wl_data_source_send_send(host->resource, mime_type, fd);
  close(fd);
}

static void xwl_data_source_cancelled(void *data,
                                      struct wl_data_source *data_source) {
  struct xwl_host_data_source *host = wl_data_source_get_user_data(data_source);

  wl_data_source_send_cancelled(host->resource);
}

static const struct wl_data_source_listener xwl_data_source_listener = {
    xwl_data_source_target, xwl_data_source_send, xwl_data_source_cancelled};

static void xwl_destroy_host_data_source(struct wl_resource *resource) {
  struct xwl_host_data_source *host = wl_resource_get_user_data(resource);

  wl_data_source_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_data_device_start_drag(struct wl_client *client,
                                       struct wl_resource *resource,
                                       struct wl_resource *source_resource,
                                       struct wl_resource *origin_resource,
                                       struct wl_resource *icon_resource,
                                       uint32_t serial) {
  struct xwl_host_data_device *host = wl_resource_get_user_data(resource);
  struct xwl_host_data_source *host_source =
      source_resource ? wl_resource_get_user_data(source_resource) : NULL;
  struct xwl_host_surface *host_origin =
      origin_resource ? wl_resource_get_user_data(origin_resource) : NULL;
  struct xwl_host_surface *host_icon =
      icon_resource ? wl_resource_get_user_data(icon_resource) : NULL;

  wl_data_device_start_drag(host->proxy,
                            host_source ? host_source->proxy : NULL,
                            host_origin ? host_origin->proxy : NULL,
                            host_icon ? host_icon->proxy : NULL, serial);
}

static void xwl_data_device_set_selection(struct wl_client *client,
                                          struct wl_resource *resource,
                                          struct wl_resource *source_resource,
                                          uint32_t serial) {
  struct xwl_host_data_device *host = wl_resource_get_user_data(resource);
  struct xwl_host_data_source *host_source =
      source_resource ? wl_resource_get_user_data(source_resource) : NULL;

  wl_data_device_set_selection(host->proxy,
                               host_source ? host_source->proxy : NULL, serial);
}

static void xwl_data_device_release(struct wl_client *client,
                                    struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static const struct wl_data_device_interface xwl_data_device_implementation = {
    xwl_data_device_start_drag, xwl_data_device_set_selection,
    xwl_data_device_release};

static void xwl_data_device_data_offer(void *data,
                                       struct wl_data_device *data_device,
                                       struct wl_data_offer *data_offer) {
  struct xwl_host_data_device *host = wl_data_device_get_user_data(data_device);
  struct xwl_host_data_offer *host_data_offer;

  host_data_offer = malloc(sizeof(*host_data_offer));
  assert(host_data_offer);

  host_data_offer->xwl = host->xwl;
  host_data_offer->resource = wl_resource_create(
      wl_resource_get_client(host->resource), &wl_data_offer_interface,
      wl_resource_get_version(host->resource), 0);
  wl_resource_set_implementation(host_data_offer->resource,
                                 &xwl_data_offer_implementation,
                                 host_data_offer, xwl_destroy_host_data_offer);
  host_data_offer->proxy = data_offer;
  wl_data_offer_set_user_data(host_data_offer->proxy, host_data_offer);
  wl_data_offer_add_listener(host_data_offer->proxy, &xwl_data_offer_listener,
                             host_data_offer);

  wl_data_device_send_data_offer(host->resource, host_data_offer->resource);
}

static void xwl_data_device_enter(void *data,
                                  struct wl_data_device *data_device,
                                  uint32_t serial, struct wl_surface *surface,
                                  wl_fixed_t x, wl_fixed_t y,
                                  struct wl_data_offer *data_offer) {
  struct xwl_host_data_device *host = wl_data_device_get_user_data(data_device);
  struct xwl_host_surface *host_surface = wl_surface_get_user_data(surface);
  struct xwl_host_data_offer *host_data_offer =
      wl_data_offer_get_user_data(data_offer);
  double scale = host->xwl->scale;

  wl_data_device_send_enter(host->resource, serial, host_surface->resource,
                            wl_fixed_from_double(wl_fixed_to_double(x) * scale),
                            wl_fixed_from_double(wl_fixed_to_double(y) * scale),
                            host_data_offer->resource);
}

static void xwl_data_device_leave(void *data,
                                  struct wl_data_device *data_device) {
  struct xwl_host_data_device *host = wl_data_device_get_user_data(data_device);

  wl_data_device_send_leave(host->resource);
}

static void xwl_data_device_motion(void *data,
                                   struct wl_data_device *data_device,
                                   uint32_t time, wl_fixed_t x, wl_fixed_t y) {
  struct xwl_host_data_device *host = wl_data_device_get_user_data(data_device);
  double scale = host->xwl->scale;

  wl_data_device_send_motion(
      host->resource, time, wl_fixed_from_double(wl_fixed_to_double(x) * scale),
      wl_fixed_from_double(wl_fixed_to_double(y) * scale));
}

static void xwl_data_device_drop(void *data,
                                 struct wl_data_device *data_device) {
  struct xwl_host_data_device *host = wl_data_device_get_user_data(data_device);

  wl_data_device_send_drop(host->resource);
}

static void xwl_data_device_selection(void *data,
                                      struct wl_data_device *data_device,
                                      struct wl_data_offer *data_offer) {
  struct xwl_host_data_device *host = wl_data_device_get_user_data(data_device);
  struct xwl_host_data_offer *host_data_offer =
      wl_data_offer_get_user_data(data_offer);

  wl_data_device_send_selection(host->resource, host_data_offer->resource);
}

static const struct wl_data_device_listener xwl_data_device_listener = {
    xwl_data_device_data_offer, xwl_data_device_enter,
    xwl_data_device_leave,      xwl_data_device_motion,
    xwl_data_device_drop,       xwl_data_device_selection};

static void xwl_destroy_host_data_device(struct wl_resource *resource) {
  struct xwl_host_data_device *host = wl_resource_get_user_data(resource);

  if (wl_data_device_get_version(host->proxy) >=
      WL_DATA_DEVICE_RELEASE_SINCE_VERSION) {
    wl_data_device_release(host->proxy);
  } else {
    wl_data_device_destroy(host->proxy);
  }
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_data_device_manager_create_data_source(
    struct wl_client *client, struct wl_resource *resource, uint32_t id) {
  struct xwl_host_data_device_manager *host =
      wl_resource_get_user_data(resource);
  struct xwl_host_data_source *host_data_source;

  host_data_source = malloc(sizeof(*host_data_source));
  assert(host_data_source);

  host_data_source->resource = wl_resource_create(
      client, &wl_data_source_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(
      host_data_source->resource, &xwl_data_source_implementation,
      host_data_source, xwl_destroy_host_data_source);
  host_data_source->proxy =
      wl_data_device_manager_create_data_source(host->proxy);
  wl_data_source_set_user_data(host_data_source->proxy, host_data_source);
  wl_data_source_add_listener(host_data_source->proxy,
                              &xwl_data_source_listener, host_data_source);
}

static void xwl_data_device_manager_get_data_device(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    struct wl_resource *seat_resource) {
  struct xwl_host_data_device_manager *host =
      wl_resource_get_user_data(resource);
  struct xwl_host_seat *host_seat = wl_resource_get_user_data(seat_resource);
  struct xwl_host_data_device *host_data_device;

  host_data_device = malloc(sizeof(*host_data_device));
  assert(host_data_device);

  host_data_device->xwl = host->xwl;
  host_data_device->resource = wl_resource_create(
      client, &wl_data_device_interface, wl_resource_get_version(resource), id);
  wl_resource_set_implementation(
      host_data_device->resource, &xwl_data_device_implementation,
      host_data_device, xwl_destroy_host_data_device);
  host_data_device->proxy =
      wl_data_device_manager_get_data_device(host->proxy, host_seat->proxy);
  wl_data_device_set_user_data(host_data_device->proxy, host_data_device);
  wl_data_device_add_listener(host_data_device->proxy,
                              &xwl_data_device_listener, host_data_device);
}

static const struct wl_data_device_manager_interface
    xwl_data_device_manager_implementation = {
        xwl_data_device_manager_create_data_source,
        xwl_data_device_manager_get_data_device};

static void xwl_destroy_host_data_device_manager(struct wl_resource *resource) {
  struct xwl_host_data_device_manager *host =
      wl_resource_get_user_data(resource);

  wl_data_device_manager_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_bind_host_data_device_manager(struct wl_client *client,
                                              void *data, uint32_t version,
                                              uint32_t id) {
  struct xwl_data_device_manager *data_device_manager =
      (struct xwl_data_device_manager *)data;
  struct xwl_host_data_device_manager *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->xwl = data_device_manager->xwl;
  host->resource =
      wl_resource_create(client, &wl_data_device_manager_interface,
                         MIN(version, data_device_manager->version), id);
  wl_resource_set_implementation(host->resource,
                                 &xwl_data_device_manager_implementation, host,
                                 xwl_destroy_host_data_device_manager);
  host->proxy = wl_registry_bind(
      wl_display_get_registry(data_device_manager->xwl->display),
      data_device_manager->id, &wl_data_device_manager_interface,
      data_device_manager->version);
  wl_data_device_manager_set_user_data(host->proxy, host);
}

static void xwl_subsurface_destroy(struct wl_client *client,
                                   struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_subsurface_set_position(struct wl_client *client,
                                        struct wl_resource *resource, int32_t x,
                                        int32_t y) {
  struct xwl_host_subsurface *host = wl_resource_get_user_data(resource);
  double scale = host->xwl->scale;

  wl_subsurface_set_position(host->proxy, x / scale, y / scale);
}

static void xwl_subsurface_place_above(struct wl_client *client,
                                       struct wl_resource *resource,
                                       struct wl_resource *sibling_resource) {
  struct xwl_host_subsurface *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_sibling =
      wl_resource_get_user_data(sibling_resource);

  wl_subsurface_place_above(host->proxy, host_sibling->proxy);
}

static void xwl_subsurface_place_below(struct wl_client *client,
                                       struct wl_resource *resource,
                                       struct wl_resource *sibling_resource) {
  struct xwl_host_subsurface *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_sibling =
      wl_resource_get_user_data(sibling_resource);

  wl_subsurface_place_below(host->proxy, host_sibling->proxy);
}

static void xwl_subsurface_set_sync(struct wl_client *client,
                                    struct wl_resource *resource) {
  struct xwl_host_subsurface *host = wl_resource_get_user_data(resource);

  wl_subsurface_set_sync(host->proxy);
}

static void xwl_subsurface_set_desync(struct wl_client *client,
                                      struct wl_resource *resource) {
  struct xwl_host_subsurface *host = wl_resource_get_user_data(resource);

  wl_subsurface_set_desync(host->proxy);
}

static const struct wl_subsurface_interface xwl_subsurface_implementation = {
    xwl_subsurface_destroy,     xwl_subsurface_set_position,
    xwl_subsurface_place_above, xwl_subsurface_place_below,
    xwl_subsurface_set_sync,    xwl_subsurface_set_desync};

static void xwl_destroy_host_subsurface(struct wl_resource *resource) {
  struct xwl_host_subsurface *host = wl_resource_get_user_data(resource);

  wl_subsurface_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_subcompositor_destroy(struct wl_client *client,
                                      struct wl_resource *resource) {
  wl_resource_destroy(resource);
}

static void xwl_subcompositor_get_subsurface(
    struct wl_client *client, struct wl_resource *resource, uint32_t id,
    struct wl_resource *surface_resource, struct wl_resource *parent_resource) {
  struct xwl_host_subcompositor *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_surface =
      wl_resource_get_user_data(surface_resource);
  struct xwl_host_surface *host_parent =
      wl_resource_get_user_data(parent_resource);
  struct xwl_host_subsurface *host_subsurface;

  host_subsurface = malloc(sizeof(*host_subsurface));
  assert(host_subsurface);

  host_subsurface->xwl = host->xwl;
  host_subsurface->resource =
      wl_resource_create(client, &wl_subsurface_interface, 1, id);
  wl_resource_set_implementation(host_subsurface->resource,
                                 &xwl_subsurface_implementation,
                                 host_subsurface, xwl_destroy_host_subsurface);
  host_subsurface->proxy = wl_subcompositor_get_subsurface(
      host->proxy, host_surface->proxy, host_parent->proxy);
  wl_subsurface_set_user_data(host_subsurface->proxy, host_subsurface);
}

static const struct wl_subcompositor_interface
    xwl_subcompositor_implementation = {xwl_subcompositor_destroy,
                                        xwl_subcompositor_get_subsurface};

static void xwl_destroy_host_subcompositor(struct wl_resource *resource) {
  struct xwl_host_subcompositor *host = wl_resource_get_user_data(resource);

  wl_subcompositor_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_bind_host_subcompositor(struct wl_client *client, void *data,
                                        uint32_t version, uint32_t id) {
  struct xwl_subcompositor *subcompositor = (struct xwl_subcompositor *)data;
  struct xwl_host_subcompositor *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->xwl = subcompositor->xwl;
  host->resource =
      wl_resource_create(client, &wl_subcompositor_interface, 1, id);
  wl_resource_set_implementation(host->resource,
                                 &xwl_subcompositor_implementation, host,
                                 xwl_destroy_host_subcompositor);
  host->proxy =
      wl_registry_bind(wl_display_get_registry(subcompositor->xwl->display),
                       subcompositor->id, &wl_subcompositor_interface, 1);
  wl_subcompositor_set_user_data(host->proxy, host);
}

static void xwl_gtk_surface_set_dbus_properties(
    struct wl_client *client, struct wl_resource *resource,
    const char *application_id, const char *app_menu_path,
    const char *menubar_path, const char *window_object_path,
    const char *application_object_path, const char *unique_bus_name) {}

static void xwl_gtk_surface_set_modal(struct wl_client *client,
                                      struct wl_resource *resource) {}

static void xwl_gtk_surface_unset_modal(struct wl_client *client,
                                        struct wl_resource *resource) {}

static void xwl_gtk_surface_present(struct wl_client *client,
                                    struct wl_resource *resource,
                                    uint32_t time) {}

static const struct gtk_surface1_interface xwl_gtk_surface_implementation = {
    xwl_gtk_surface_set_dbus_properties, xwl_gtk_surface_set_modal,
    xwl_gtk_surface_unset_modal, xwl_gtk_surface_present};

static void xwl_destroy_host_gtk_surface(struct wl_resource *resource) {
  struct xwl_host_gtk_surface *host = wl_resource_get_user_data(resource);

  zaura_surface_destroy(host->proxy);
  wl_list_remove(&host->link);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void
xwl_gtk_shell_get_gtk_surface(struct wl_client *client,
                              struct wl_resource *resource, uint32_t id,
                              struct wl_resource *surface_resource) {
  struct xwl_host_gtk_shell *host = wl_resource_get_user_data(resource);
  struct xwl_host_surface *host_surface =
      wl_resource_get_user_data(surface_resource);
  struct xwl_host_gtk_surface *host_gtk_surface;

  host_gtk_surface = malloc(sizeof(*host_gtk_surface));
  assert(host_gtk_surface);

  wl_list_insert(&host->surfaces, &host_gtk_surface->link);
  host_gtk_surface->resource =
      wl_resource_create(client, &gtk_surface1_interface, 1, id);
  wl_resource_set_implementation(
      host_gtk_surface->resource, &xwl_gtk_surface_implementation,
      host_gtk_surface, xwl_destroy_host_gtk_surface);
  host_gtk_surface->proxy =
      zaura_shell_get_aura_surface(host->proxy, host_surface->proxy);

  if (host->aura_shell->version >= ZAURA_SURFACE_SET_STARTUP_ID_SINCE_VERSION)
    zaura_surface_set_startup_id(host_gtk_surface->proxy, host->startup_id);
}

static void xwl_gtk_shell_set_startup_id(struct wl_client *client,
                                         struct wl_resource *resource,
                                         const char *startup_id) {
  struct xwl_host_gtk_shell *host = wl_resource_get_user_data(resource);

  free(host->startup_id);
  host->startup_id = startup_id ? strdup(startup_id) : NULL;

  if (host->aura_shell->version >= ZAURA_SURFACE_SET_STARTUP_ID_SINCE_VERSION) {
    struct xwl_host_gtk_surface *surface;

    wl_list_for_each(surface, &host->surfaces, link)
        zaura_surface_set_startup_id(surface->proxy, host->startup_id);
  }
}

static void xwl_gtk_shell_system_bell(struct wl_client *client,
                                      struct wl_resource *resource,
                                      struct wl_resource *surface_resource) {}

static const struct gtk_shell1_interface xwl_gtk_shell_implementation = {
    xwl_gtk_shell_get_gtk_surface, xwl_gtk_shell_set_startup_id,
    xwl_gtk_shell_system_bell};

static void xwl_destroy_host_gtk_shell(struct wl_resource *resource) {
  struct xwl_host_gtk_shell *host = wl_resource_get_user_data(resource);

  free(host->startup_id);
  wl_callback_destroy(host->callback);
  zaura_shell_destroy(host->proxy);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void xwl_gtk_shell_callback_done(void *data,
                                        struct wl_callback *callback,
                                        uint32_t serial) {
  struct xwl_host_gtk_shell *host = wl_callback_get_user_data(callback);

  gtk_shell1_send_capabilities(host->resource, 0);
}

static const struct wl_callback_listener xwl_gtk_shell_callback_listener = {
    xwl_gtk_shell_callback_done};

static void xwl_bind_host_gtk_shell(struct wl_client *client, void *data,
                                    uint32_t version, uint32_t id) {
  struct xwl_aura_shell *aura_shell = (struct xwl_aura_shell *)data;
  struct xwl_host_gtk_shell *host;

  host = malloc(sizeof(*host));
  assert(host);
  host->aura_shell = aura_shell;
  host->startup_id = NULL;
  wl_list_init(&host->surfaces);
  host->resource = wl_resource_create(client, &gtk_shell1_interface, 1, id);
  wl_resource_set_implementation(host->resource, &xwl_gtk_shell_implementation,
                                 host, xwl_destroy_host_gtk_shell);
  host->proxy = wl_registry_bind(
      wl_display_get_registry(aura_shell->xwl->display), aura_shell->id,
      &zaura_shell_interface, aura_shell->version);
  zaura_shell_set_user_data(host->proxy, host);

  host->callback = wl_display_sync(aura_shell->xwl->display);
  wl_callback_set_user_data(host->callback, host);
  wl_callback_add_listener(host->callback, &xwl_gtk_shell_callback_listener,
                           host);
}

static struct xwl_global *
xwl_global_create(struct xwl *xwl, const struct wl_interface *interface,
                  int version, void *data, wl_global_bind_func_t bind) {
  struct xwl_host_registry *registry;
  struct xwl_global *global;

  assert(version > 0);
  assert(version <= interface->version);

  global = malloc(sizeof *global);
  assert(global);

  global->xwl = xwl;
  global->name = xwl->next_global_id++;
  global->interface = interface;
  global->version = version;
  global->data = data;
  global->bind = bind;
  wl_list_insert(xwl->globals.prev, &global->link);

  wl_list_for_each(registry, &xwl->registries, link) {
    wl_resource_post_event(registry->resource, WL_REGISTRY_GLOBAL, global->name,
                           global->interface->name, global->version);
  }

  return global;
}

static void xwl_global_destroy(struct xwl_global *global) {
  struct xwl_host_registry *registry;

  wl_list_for_each(registry, &global->xwl->registries, link)
      wl_resource_post_event(registry->resource, WL_REGISTRY_GLOBAL_REMOVE,
                             global->name);
  wl_list_remove(&global->link);
  free(global);
}

static void xwl_registry_handler(void *data, struct wl_registry *registry,
                                 uint32_t id, const char *interface,
                                 uint32_t version) {
  struct xwl *xwl = (struct xwl *)data;

  if (strcmp(interface, "wl_compositor") == 0) {
    struct xwl_compositor *compositor = malloc(sizeof(struct xwl_compositor));
    assert(compositor);
    compositor->xwl = xwl;
    compositor->id = id;
    assert(version >= 3);
    compositor->version = 3;
    compositor->host_global =
        xwl_global_create(xwl, &wl_compositor_interface, compositor->version,
                          compositor, xwl_bind_host_compositor);
    compositor->internal = wl_registry_bind(
        registry, id, &wl_compositor_interface, compositor->version);
    assert(!xwl->compositor);
    xwl->compositor = compositor;
  } else if (strcmp(interface, "wl_subcompositor") == 0) {
    struct xwl_subcompositor *subcompositor =
        malloc(sizeof(struct xwl_subcompositor));
    assert(subcompositor);
    subcompositor->xwl = xwl;
    subcompositor->id = id;
    subcompositor->host_global =
        xwl_global_create(xwl, &wl_subcompositor_interface, 1, subcompositor,
                          xwl_bind_host_subcompositor);
    xwl->subcompositor = subcompositor;
  } else if (strcmp(interface, "wl_shm") == 0) {
    struct xwl_shm *shm = malloc(sizeof(struct xwl_shm));
    assert(shm);
    shm->xwl = xwl;
    shm->id = id;
    shm->host_global =
        xwl_global_create(xwl, &wl_shm_interface, 1, shm, xwl_bind_host_shm);
    shm->internal = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    assert(!xwl->shm);
    xwl->shm = shm;
  } else if (strcmp(interface, "wl_shell") == 0) {
    struct xwl_shell *shell = malloc(sizeof(struct xwl_shell));
    assert(shell);
    shell->xwl = xwl;
    shell->id = id;
    shell->host_global = xwl_global_create(xwl, &wl_shell_interface, 1, shell,
                                           xwl_bind_host_shell);
    assert(!xwl->shell);
    xwl->shell = shell;
  } else if (strcmp(interface, "wl_output") == 0) {
    struct xwl_output *output = malloc(sizeof(struct xwl_output));
    assert(output);
    output->xwl = xwl;
    output->id = id;
    output->version = MIN(2, version);
    output->host_global =
        xwl_global_create(xwl, &wl_output_interface, output->version, output,
                          xwl_bind_host_output);
    wl_list_insert(&xwl->outputs, &output->link);
  } else if (strcmp(interface, "wl_seat") == 0) {
    struct xwl_seat *seat = malloc(sizeof(struct xwl_seat));
    assert(seat);
    seat->xwl = xwl;
    seat->id = id;
    seat->version = MIN(5, version);
    seat->host_global = xwl_global_create(
        xwl, &wl_seat_interface, seat->version, seat, xwl_bind_host_seat);
    seat->last_serial = 0;
    wl_list_insert(&xwl->seats, &seat->link);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
    struct xwl_data_device_manager *data_device_manager =
        malloc(sizeof(struct xwl_data_device_manager));
    assert(data_device_manager);
    data_device_manager->xwl = xwl;
    data_device_manager->id = id;
    data_device_manager->version = MIN(3, version);
    if (xwl->xwayland) {
      data_device_manager->host_global = NULL;
      data_device_manager->internal =
          wl_registry_bind(registry, id, &wl_data_device_manager_interface,
                           data_device_manager->version);
    } else {
      data_device_manager->internal = NULL;
      data_device_manager->host_global = xwl_global_create(
          xwl, &wl_data_device_manager_interface, data_device_manager->version,
          data_device_manager, xwl_bind_host_data_device_manager);
    }
    xwl->data_device_manager = data_device_manager;
  } else if (strcmp(interface, "zxdg_shell_v6") == 0) {
    struct xwl_xdg_shell *xdg_shell = malloc(sizeof(struct xwl_xdg_shell));
    assert(xdg_shell);
    xdg_shell->xwl = xwl;
    xdg_shell->id = id;
    if (xwl->xwayland) {
      xdg_shell->host_global = NULL;
      xdg_shell->internal =
          wl_registry_bind(registry, id, &zxdg_shell_v6_interface, 1);
      zxdg_shell_v6_add_listener(xdg_shell->internal,
                                 &xwl_internal_xdg_shell_listener, NULL);
    } else {
      xdg_shell->internal = NULL;
      xdg_shell->host_global = xwl_global_create(
          xwl, &zxdg_shell_v6_interface, 1, xdg_shell, xwl_bind_host_xdg_shell);
    }
    assert(!xwl->xdg_shell);
    xwl->xdg_shell = xdg_shell;
  } else if (strcmp(interface, "zaura_shell") == 0) {
    struct xwl_aura_shell *aura_shell = malloc(sizeof(struct xwl_aura_shell));
    assert(aura_shell);
    aura_shell->xwl = xwl;
    aura_shell->id = id;
    aura_shell->version = MIN(4, version);
    aura_shell->host_gtk_shell_global = NULL;
    aura_shell->internal = wl_registry_bind(
        registry, id, &zaura_shell_interface, aura_shell->version);
    assert(!xwl->aura_shell);
    xwl->aura_shell = aura_shell;
    aura_shell->host_gtk_shell_global = xwl_global_create(
        xwl, &gtk_shell1_interface, 1, aura_shell, xwl_bind_host_gtk_shell);
  } else if (strcmp(interface, "wp_viewporter") == 0) {
    struct xwl_viewporter *viewporter = malloc(sizeof(struct xwl_viewporter));
    assert(viewporter);
    viewporter->xwl = xwl;
    viewporter->id = id;
    viewporter->internal =
        wl_registry_bind(registry, id, &wp_viewporter_interface, 1);
    assert(!xwl->viewporter);
    xwl->viewporter = viewporter;
    // Allow non-integer scale.
    xwl->scale = MIN(MAX_SCALE, MAX(MIN_SCALE, xwl->desired_scale));
  } else if (strcmp(interface, "zwp_linux_dmabuf_v1") == 0) {
    struct xwl_linux_dmabuf *linux_dmabuf =
        malloc(sizeof(struct xwl_linux_dmabuf));
    assert(linux_dmabuf);
    linux_dmabuf->xwl = xwl;
    linux_dmabuf->id = id;
    linux_dmabuf->version = MIN(2, version);
    linux_dmabuf->host_drm_global = NULL;
    linux_dmabuf->internal = wl_registry_bind(
        registry, id, &zwp_linux_dmabuf_v1_interface, linux_dmabuf->version);
    assert(!xwl->linux_dmabuf);
    xwl->linux_dmabuf = linux_dmabuf;

    // Register wl_drm global if DRM device is available and DMABuf interface
    // version is sufficient.
    if (xwl->drm_device && linux_dmabuf->version >= 2) {
      linux_dmabuf->host_drm_global = xwl_global_create(
          xwl, &wl_drm_interface, 2, linux_dmabuf, xwl_bind_host_drm);
    }
  } else if (strcmp(interface, "zcr_keyboard_extension_v1") == 0) {
    struct xwl_keyboard_extension *keyboard_extension =
        malloc(sizeof(struct xwl_keyboard_extension));
    assert(keyboard_extension);
    keyboard_extension->xwl = xwl;
    keyboard_extension->id = id;
    keyboard_extension->internal =
        wl_registry_bind(registry, id, &zcr_keyboard_extension_v1_interface, 1);
    assert(!xwl->keyboard_extension);
    xwl->keyboard_extension = keyboard_extension;
  }
}

static void xwl_registry_remover(void *data, struct wl_registry *registry,
                                 uint32_t id) {
  struct xwl *xwl = (struct xwl *)data;
  struct xwl_output *output;
  struct xwl_seat *seat;

  if (xwl->compositor && xwl->compositor->id == id) {
    xwl_global_destroy(xwl->compositor->host_global);
    wl_compositor_destroy(xwl->compositor->internal);
    free(xwl->compositor);
    xwl->compositor = NULL;
    return;
  }
  if (xwl->subcompositor && xwl->subcompositor->id == id) {
    xwl_global_destroy(xwl->subcompositor->host_global);
    wl_shm_destroy(xwl->shm->internal);
    free(xwl->subcompositor);
    xwl->subcompositor = NULL;
    return;
  }
  if (xwl->shm && xwl->shm->id == id) {
    xwl_global_destroy(xwl->shm->host_global);
    free(xwl->shm);
    xwl->shm = NULL;
    return;
  }
  if (xwl->shell && xwl->shell->id == id) {
    xwl_global_destroy(xwl->shell->host_global);
    free(xwl->shell);
    xwl->shell = NULL;
    return;
  }
  if (xwl->data_device_manager && xwl->data_device_manager->id == id) {
    if (xwl->data_device_manager->host_global)
      xwl_global_destroy(xwl->data_device_manager->host_global);
    if (xwl->data_device_manager->internal)
      wl_data_device_manager_destroy(xwl->data_device_manager->internal);
    free(xwl->data_device_manager);
    xwl->data_device_manager = NULL;
    return;
  }
  if (xwl->xdg_shell && xwl->xdg_shell->id == id) {
    if (xwl->xdg_shell->host_global)
      xwl_global_destroy(xwl->xdg_shell->host_global);
    if (xwl->xdg_shell->internal)
      zxdg_shell_v6_destroy(xwl->xdg_shell->internal);
    free(xwl->xdg_shell);
    xwl->xdg_shell = NULL;
    return;
  }
  if (xwl->aura_shell && xwl->aura_shell->id == id) {
    if (xwl->aura_shell->host_gtk_shell_global)
      xwl_global_destroy(xwl->aura_shell->host_gtk_shell_global);
    zaura_shell_destroy(xwl->aura_shell->internal);
    free(xwl->aura_shell);
    xwl->aura_shell = NULL;
    return;
  }
  if (xwl->viewporter && xwl->viewporter->id == id) {
    wp_viewporter_destroy(xwl->viewporter->internal);
    free(xwl->viewporter);
    xwl->viewporter = NULL;
    return;
  }
  if (xwl->linux_dmabuf && xwl->linux_dmabuf->id == id) {
    if (xwl->linux_dmabuf->host_drm_global)
      xwl_global_destroy(xwl->linux_dmabuf->host_drm_global);
    zwp_linux_dmabuf_v1_destroy(xwl->linux_dmabuf->internal);
    free(xwl->linux_dmabuf);
    xwl->linux_dmabuf = NULL;
    return;
  }
  if (xwl->keyboard_extension && xwl->keyboard_extension->id == id) {
    zcr_keyboard_extension_v1_destroy(xwl->keyboard_extension->internal);
    free(xwl->keyboard_extension);
    xwl->keyboard_extension = NULL;
    return;
  }
  wl_list_for_each(output, &xwl->outputs, link) {
    if (output->id == id) {
      xwl_global_destroy(output->host_global);
      wl_list_remove(&output->link);
      free(output);
      return;
    }
  }
  wl_list_for_each(seat, &xwl->seats, link) {
    if (seat->id == id) {
      xwl_global_destroy(seat->host_global);
      wl_list_remove(&seat->link);
      free(seat);
      return;
    }
  }

  // Not reached.
  assert(0);
}

static const struct wl_registry_listener xwl_registry_listener = {
    xwl_registry_handler, xwl_registry_remover};

static int xwl_handle_event(int fd, uint32_t mask, void *data) {
  struct xwl *xwl = (struct xwl *)data;
  int count = 0;

  if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
    wl_client_flush(xwl->client);
    exit(EXIT_SUCCESS);
  }

  if (mask & WL_EVENT_READABLE)
    count = wl_display_dispatch(xwl->display);
  if (mask & WL_EVENT_WRITABLE)
    wl_display_flush(xwl->display);

  if (mask == 0) {
    count = wl_display_dispatch_pending(xwl->display);
    wl_display_flush(xwl->display);
  }

  return count;
}

static void xwl_create_window(struct xwl *xwl, xcb_window_t id, int x, int y,
                              int width, int height, int border_width) {
  struct xwl_window *window = malloc(sizeof(struct xwl_window));
  uint32_t values[1];
  assert(window);
  window->xwl = xwl;
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
  wl_list_insert(&xwl->unpaired_windows, &window->link);
  values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_FOCUS_CHANGE;
  xcb_change_window_attributes(xwl->connection, window->id, XCB_CW_EVENT_MASK,
                               values);
}

static void xwl_destroy_window(struct xwl_window *window) {
  if (window->frame_id != XCB_WINDOW_NONE)
    xcb_destroy_window(window->xwl->connection, window->frame_id);

  if (window->xwl->host_focus_window == window) {
    window->xwl->host_focus_window = NULL;
    window->xwl->needs_set_input_focus = 1;
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

static int xwl_is_window(struct xwl_window *window, xcb_window_t id) {
  if (window->id == id)
    return 1;

  if (window->frame_id != XCB_WINDOW_NONE) {
    if (window->frame_id == id)
      return 1;
  }

  return 0;
}

static struct xwl_window *xwl_lookup_window(struct xwl *xwl, xcb_window_t id) {
  struct xwl_window *window;

  wl_list_for_each(window, &xwl->windows, link) {
    if (xwl_is_window(window, id))
      return window;
  }
  wl_list_for_each(window, &xwl->unpaired_windows, link) {
    if (xwl_is_window(window, id))
      return window;
  }
  return NULL;
}

static int xwl_is_our_window(struct xwl *xwl, xcb_window_t id) {
  const xcb_setup_t *setup = xcb_get_setup(xwl->connection);

  return (id & ~setup->resource_id_mask) == setup->resource_id_base;
}

static void xwl_handle_create_notify(struct xwl *xwl,
                                     xcb_create_notify_event_t *event) {
  if (xwl_is_our_window(xwl, event->window))
    return;

  xwl_create_window(xwl, event->window, event->x, event->y, event->width,
                    event->height, event->border_width);
}

static void xwl_handle_destroy_notify(struct xwl *xwl,
                                      xcb_destroy_notify_event_t *event) {
  struct xwl_window *window;

  if (xwl_is_our_window(xwl, event->window))
    return;

  window = xwl_lookup_window(xwl, event->window);
  if (!window)
    return;

  xwl_destroy_window(window);
}

static void xwl_handle_reparent_notify(struct xwl *xwl,
                                       xcb_reparent_notify_event_t *event) {
  struct xwl_window *window;

  if (event->parent == xwl->screen->root) {
    int width = 1;
    int height = 1;
    int border_width = 0;
    xcb_get_geometry_reply_t *geometry_reply = xcb_get_geometry_reply(
        xwl->connection, xcb_get_geometry(xwl->connection, event->window),
        NULL);

    if (geometry_reply) {
      width = geometry_reply->width;
      height = geometry_reply->height;
      border_width = geometry_reply->border_width;
      free(geometry_reply);
    }
    xwl_create_window(xwl, event->window, event->x, event->y, width, height,
                      border_width);
    return;
  }

  if (xwl_is_our_window(xwl, event->parent))
    return;

  window = xwl_lookup_window(xwl, event->window);
  if (!window)
    return;

  xwl_destroy_window(window);
}

static void xwl_handle_map_request(struct xwl *xwl,
                                   xcb_map_request_event_t *event) {
  struct xwl_window *window = xwl_lookup_window(xwl, event->window);
  struct {
    int type;
    xcb_atom_t atom;
  } properties[] = {
      {PROPERTY_WM_NAME, XCB_ATOM_WM_NAME},
      {PROPERTY_WM_CLASS, XCB_ATOM_WM_CLASS},
      {PROPERTY_WM_TRANSIENT_FOR, XCB_ATOM_WM_TRANSIENT_FOR},
      {PROPERTY_WM_NORMAL_HINTS, XCB_ATOM_WM_NORMAL_HINTS},
      {PROPERTY_WM_CLIENT_LEADER, xwl->atoms[ATOM_WM_CLIENT_LEADER].value},
      {PROPERTY_MOTIF_WM_HINTS, xwl->atoms[ATOM_MOTIF_WM_HINTS].value},
      {PROPERTY_NET_STARTUP_ID, xwl->atoms[ATOM_NET_STARTUP_ID].value},
  };
  xcb_get_geometry_cookie_t geometry_cookie;
  xcb_get_property_cookie_t property_cookies[ARRAY_SIZE(properties)];
  struct xwl_wm_size_hints {
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
  } size_hints = {0};
  struct xwl_mwm_hints {
    uint32_t flags;
    uint32_t functions;
    uint32_t decorations;
    int32_t input_mode;
    uint32_t status;
  } mwm_hints = {0};
  uint32_t values[5];
  int i;

  if (!window)
    return;

  assert(!xwl_is_our_window(xwl, event->window));

  window->managed = 1;
  if (window->frame_id == XCB_WINDOW_NONE)
    geometry_cookie = xcb_get_geometry(xwl->connection, window->id);

  for (i = 0; i < ARRAY_SIZE(properties); ++i) {
    property_cookies[i] =
        xcb_get_property(xwl->connection, 0, window->id, properties[i].atom,
                         XCB_ATOM_ANY, 0, 2048);
  }

  if (window->frame_id == XCB_WINDOW_NONE) {
    xcb_get_geometry_reply_t *geometry_reply =
        xcb_get_geometry_reply(xwl->connection, geometry_cookie, NULL);
    if (geometry_reply) {
      window->x = geometry_reply->x;
      window->y = geometry_reply->y;
      window->width = geometry_reply->width;
      window->height = geometry_reply->height;
      window->depth = geometry_reply->depth;
      free(geometry_reply);
    }
  }

  if (window->name) {
    free(window->name);
    window->name = NULL;
  }
  if (window->clazz) {
    free(window->clazz);
    window->clazz = NULL;
  }
  if (window->startup_id) {
    free(window->startup_id);
    window->startup_id = NULL;
  }
  window->transient_for = XCB_WINDOW_NONE;
  window->client_leader = XCB_WINDOW_NONE;
  window->decorated = 1;
  window->size_flags = 0;

  for (i = 0; i < ARRAY_SIZE(properties); ++i) {
    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(xwl->connection, property_cookies[i], NULL);

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
    xcb_get_property_reply_t *reply = xcb_get_property_reply(
        xwl->connection,
        xcb_get_property(xwl->connection, 0, window->client_leader,
                         xwl->atoms[ATOM_NET_STARTUP_ID].value, XCB_ATOM_ANY, 0,
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

  window->border_width = 0;
  xwl_adjust_window_size_for_screen_size(window);
  if (window->size_flags & (US_POSITION | P_POSITION)) {
    // x/y fields are obsolete but some clients still expect them to be
    // honored so use them if greater than zero.
    if (size_hints.x > 0)
      window->x = size_hints.x;
    if (size_hints.y > 0)
      window->y = size_hints.y;
  } else {
    xwl_adjust_window_position_for_screen_size(window);
  }

  values[0] = window->width;
  values[1] = window->height;
  values[2] = 0;
  xcb_configure_window(xwl->connection, window->id,
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                           XCB_CONFIG_WINDOW_BORDER_WIDTH,
                       values);
  values[0] = 0;
  values[1] = 0;
  values[2] = window->decorated ? CAPTION_HEIGHT * xwl->scale : 0;
  values[3] = 0;
  xcb_change_property(xwl->connection, XCB_PROP_MODE_REPLACE, window->id,
                      xwl->atoms[ATOM_NET_FRAME_EXTENTS].value,
                      XCB_ATOM_CARDINAL, 32, 4, values);

  if (window->frame_id == XCB_WINDOW_NONE) {
    int depth = window->depth ? window->depth : xwl->screen->root_depth;

    values[0] = xwl->screen->black_pixel;
    values[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
                XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
    values[2] = xwl->colormaps[depth];

    window->frame_id = xcb_generate_id(xwl->connection);
    xcb_create_window(
        xwl->connection, depth, window->frame_id, xwl->screen->root, window->x,
        window->y, window->width, window->height, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, xwl->visual_ids[depth],
        XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP, values);
    values[0] = XCB_STACK_MODE_BELOW;
    xcb_configure_window(xwl->connection, window->frame_id,
                         XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_reparent_window(xwl->connection, window->id, window->frame_id, 0, 0);
  } else {
    values[0] = window->x;
    values[1] = window->y;
    values[2] = window->width;
    values[3] = window->height;
    values[4] = XCB_STACK_MODE_BELOW;
    xcb_configure_window(
        xwl->connection, window->frame_id,
        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
            XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_STACK_MODE,
        values);
  }

  xwl_window_set_wm_state(window, WM_STATE_NORMAL);
  xwl_send_configure_notify(window);

  xcb_map_window(xwl->connection, window->id);
  xcb_map_window(xwl->connection, window->frame_id);
}

static void xwl_handle_map_notify(struct xwl *xwl,
                                  xcb_map_notify_event_t *event) {}

static void xwl_handle_unmap_notify(struct xwl *xwl,
                                    xcb_unmap_notify_event_t *event) {
  struct xwl_window *window;

  if (xwl_is_our_window(xwl, event->window))
    return;

  if (event->response_type & SEND_EVENT_MASK)
    return;

  window = xwl_lookup_window(xwl, event->window);
  if (!window)
    return;

  if (xwl->host_focus_window == window) {
    xwl->host_focus_window = NULL;
    xwl->needs_set_input_focus = 1;
  }

  if (window->host_surface_id) {
    window->host_surface_id = 0;
    xwl_window_update(window);
  }

  xwl_window_set_wm_state(window, WM_STATE_WITHDRAWN);

  if (window->frame_id != XCB_WINDOW_NONE)
    xcb_unmap_window(xwl->connection, window->frame_id);
}

static void xwl_handle_configure_request(struct xwl *xwl,
                                         xcb_configure_request_event_t *event) {
  struct xwl_window *window = xwl_lookup_window(xwl, event->window);
  int width = window->width;
  int height = window->height;
  uint32_t values[7];

  assert(!xwl_is_our_window(xwl, event->window));

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

    xcb_configure_window(xwl->connection, window->id, event->value_mask,
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

  xwl_adjust_window_size_for_screen_size(window);
  if (window->size_flags & (US_POSITION | P_POSITION))
    xwl_window_update(window);
  else
    xwl_adjust_window_position_for_screen_size(window);

  values[0] = window->x;
  values[1] = window->y;
  values[2] = window->width;
  values[3] = window->height;
  values[4] = 0;
  xcb_configure_window(xwl->connection, window->frame_id,
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                           XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                       values);

  // We need to send a synthetic configure notify if:
  // - Not changing the size, location, border width.
  // - Moving the window without resizing it or changing its border width.
  if (width != window->width || height != window->height ||
      window->border_width) {
    xcb_configure_window(xwl->connection, window->id,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         &values[2]);
    window->border_width = 0;
  } else {
    xwl_send_configure_notify(window);
  }
}

static void xwl_handle_configure_notify(struct xwl *xwl,
                                        xcb_configure_notify_event_t *event) {
  struct xwl_window *window;

  if (xwl_is_our_window(xwl, event->window))
    return;

  if (event->window == xwl->screen->root) {
    xcb_get_geometry_reply_t *geometry_reply = xcb_get_geometry_reply(
        xwl->connection, xcb_get_geometry(xwl->connection, event->window),
        NULL);
    int width = xwl->screen->width_in_pixels;
    int height = xwl->screen->height_in_pixels;

    if (geometry_reply) {
      width = geometry_reply->width;
      height = geometry_reply->height;
      free(geometry_reply);
    }

    if (width == xwl->screen->width_in_pixels ||
        height == xwl->screen->height_in_pixels) {
      return;
    }

    xwl->screen->width_in_pixels = width;
    xwl->screen->height_in_pixels = height;

    // Re-center managed windows.
    wl_list_for_each(window, &xwl->windows, link) {
      int x, y;

      if (window->size_flags & (US_POSITION | P_POSITION))
        continue;

      x = window->x;
      y = window->y;
      xwl_adjust_window_position_for_screen_size(window);
      if (window->x != x || window->y != y) {
        uint32_t values[2];

        values[0] = window->x;
        values[1] = window->y;
        xcb_configure_window(xwl->connection, window->frame_id,
                             XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
        xwl_send_configure_notify(window);
      }
    }
    return;
  }

  window = xwl_lookup_window(xwl, event->window);
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
    xwl_window_update(window);
  }
}

static uint32_t xwl_resize_edge(int net_wm_moveresize_size) {
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

static void xwl_handle_client_message(struct xwl *xwl,
                                      xcb_client_message_event_t *event) {
  if (event->type == xwl->atoms[ATOM_WL_SURFACE_ID].value) {
    struct xwl_window *window, *unpaired_window = NULL;

    wl_list_for_each(window, &xwl->unpaired_windows, link) {
      if (xwl_is_window(window, event->window)) {
        unpaired_window = window;
        break;
      }
    }

    if (unpaired_window) {
      unpaired_window->host_surface_id = event->data.data32[0];
      xwl_window_update(unpaired_window);
    }
  } else if (event->type == xwl->atoms[ATOM_NET_WM_MOVERESIZE].value) {
    struct xwl_window *window = xwl_lookup_window(xwl, event->window);

    if (window && window->xdg_toplevel) {
      struct xwl_host_seat *seat = window->xwl->default_seat;

      if (!seat)
        return;

      if (event->data.data32[2] == NET_WM_MOVERESIZE_MOVE) {
        zxdg_toplevel_v6_move(window->xdg_toplevel, seat->proxy,
                              seat->seat->last_serial);
      } else {
        uint32_t edge = xwl_resize_edge(event->data.data32[2]);

        if (edge == ZXDG_TOPLEVEL_V6_RESIZE_EDGE_NONE)
          return;

        zxdg_toplevel_v6_resize(window->xdg_toplevel, seat->proxy,
                                seat->seat->last_serial, edge);
      }
    }
  } else if (event->type == xwl->atoms[ATOM_NET_WM_STATE].value) {
    struct xwl_window *window = xwl_lookup_window(xwl, event->window);

    if (window && window->xdg_toplevel) {
      int changed[ATOM_LAST + 1];
      uint32_t action = event->data.data32[0];
      int i;

      for (i = 0; i < ARRAY_SIZE(xwl->atoms); ++i) {
        changed[i] = event->data.data32[1] == xwl->atoms[i].value ||
                     event->data.data32[2] == xwl->atoms[i].value;
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

static void xwl_handle_focus_in(struct xwl *xwl, xcb_focus_in_event_t *event) {}

static void xwl_handle_focus_out(struct xwl *xwl,
                                 xcb_focus_out_event_t *event) {}

static int xwl_handle_selection_fd_writable(int fd, uint32_t mask, void *data) {
  struct xwl *xwl = data;
  uint8_t *value;
  int bytes, bytes_left;

  value = xcb_get_property_value(xwl->selection_property_reply);
  bytes_left = xcb_get_property_value_length(xwl->selection_property_reply) -
               xwl->selection_property_offset;

  bytes = write(fd, value + xwl->selection_property_offset, bytes_left);
  if (bytes == -1) {
    fprintf(stderr, "write error to target fd: %m\n");
    close(fd);
  } else if (bytes == bytes_left) {
    if (xwl->selection_incremental_transfer) {
      xcb_delete_property(xwl->connection, xwl->selection_window,
                          xwl->atoms[ATOM_WL_SELECTION].value);
    } else {
      close(fd);
    }
  } else {
    xwl->selection_property_offset += bytes;
    return 1;
  }

  free(xwl->selection_property_reply);
  xwl->selection_property_reply = NULL;
  if (xwl->selection_send_event_source) {
    wl_event_source_remove(xwl->selection_send_event_source);
    xwl->selection_send_event_source = NULL;
  }
  return 1;
}

static void xwl_write_selection_property(struct xwl *xwl,
                                         xcb_get_property_reply_t *reply) {
  xwl->selection_property_offset = 0;
  xwl->selection_property_reply = reply;
  xwl_handle_selection_fd_writable(xwl->selection_data_source_send_fd,
                                   WL_EVENT_WRITABLE, xwl);

  if (!xwl->selection_property_reply)
    return;

  assert(!xwl->selection_send_event_source);
  xwl->selection_send_event_source = wl_event_loop_add_fd(
      wl_display_get_event_loop(xwl->host_display),
      xwl->selection_data_source_send_fd, WL_EVENT_WRITABLE,
      xwl_handle_selection_fd_writable, xwl);
}

static void xwl_send_selection_notify(struct xwl *xwl, xcb_atom_t property) {
  xcb_selection_notify_event_t event = {
      .response_type = XCB_SELECTION_NOTIFY,
      .sequence = 0,
      .time = xwl->selection_request.time,
      .requestor = xwl->selection_request.requestor,
      .selection = xwl->selection_request.selection,
      .target = xwl->selection_request.target,
      .property = property,
      .pad0 = 0};

  xcb_send_event(xwl->connection, 0, xwl->selection_request.requestor,
                 XCB_EVENT_MASK_NO_EVENT, (char *)&event);
}

static void xwl_send_selection_data(struct xwl *xwl) {
  assert(!xwl->selection_data_ack_pending);
  xcb_change_property(
      xwl->connection, XCB_PROP_MODE_REPLACE, xwl->selection_request.requestor,
      xwl->selection_request.property, xwl->atoms[ATOM_UTF8_STRING].value, 8,
      xwl->selection_data.size, xwl->selection_data.data);
  xwl->selection_data_ack_pending = 1;
  xwl->selection_data.size = 0;
}

static const uint32_t xwl_incr_chunk_size = 64 * 1024;

static int xwl_handle_selection_fd_readable(int fd, uint32_t mask, void *data) {
  struct xwl *xwl = data;
  int bytes, offset, bytes_left;
  void *p;

  offset = xwl->selection_data.size;
  if (xwl->selection_data.size < xwl_incr_chunk_size)
    p = wl_array_add(&xwl->selection_data, xwl_incr_chunk_size);
  else
    p = (char *)xwl->selection_data.data + xwl->selection_data.size;
  bytes_left = xwl->selection_data.alloc - offset;

  bytes = read(fd, p, bytes_left);
  if (bytes == -1) {
    fprintf(stderr, "read error from data source: %m\n");
    xwl_send_selection_notify(xwl, XCB_ATOM_NONE);
    xwl->selection_data_offer_receive_fd = -1;
    close(fd);
  } else {
    xwl->selection_data.size = offset + bytes;
    if (xwl->selection_data.size >= xwl_incr_chunk_size) {
      if (!xwl->selection_incremental_transfer) {
        xwl->selection_incremental_transfer = 1;
        xcb_change_property(
            xwl->connection, XCB_PROP_MODE_REPLACE,
            xwl->selection_request.requestor, xwl->selection_request.property,
            xwl->atoms[ATOM_INCR].value, 32, 1, &xwl_incr_chunk_size);
        xwl->selection_data_ack_pending = 1;
        xwl_send_selection_notify(xwl, xwl->selection_request.property);
      } else if (!xwl->selection_data_ack_pending) {
        xwl_send_selection_data(xwl);
      }
    } else if (bytes == 0) {
      if (!xwl->selection_data_ack_pending)
        xwl_send_selection_data(xwl);
      if (!xwl->selection_incremental_transfer) {
        xwl_send_selection_notify(xwl, xwl->selection_request.property);
        xwl->selection_request.requestor = XCB_NONE;
        wl_array_release(&xwl->selection_data);
      }
      xcb_flush(xwl->connection);
      xwl->selection_data_offer_receive_fd = -1;
      close(fd);
    } else {
      xwl->selection_data.size = offset + bytes;
      return 1;
    }
  }

  wl_event_source_remove(xwl->selection_event_source);
  xwl->selection_event_source = NULL;
  return 1;
}

static void xwl_handle_property_notify(struct xwl *xwl,
                                       xcb_property_notify_event_t *event) {
  if (event->atom == XCB_ATOM_WM_NAME) {
    struct xwl_window *window = xwl_lookup_window(xwl, event->window);
    if (!window)
      return;

    if (window->name) {
      free(window->name);
      window->name = NULL;
    }

    if (event->state != XCB_PROPERTY_DELETE) {
      xcb_get_property_reply_t *reply = xcb_get_property_reply(
          xwl->connection,
          xcb_get_property(xwl->connection, 0, window->id, XCB_ATOM_WM_NAME,
                           XCB_ATOM_ANY, 0, 2048),
          NULL);
      if (reply) {
        window->name = strndup(xcb_get_property_value(reply),
                               xcb_get_property_value_length(reply));
        free(reply);
      }
    }

    if (!window->xdg_toplevel || !xwl->show_window_title)
      return;

    if (window->name) {
      zxdg_toplevel_v6_set_title(window->xdg_toplevel, window->name);
    } else {
      zxdg_toplevel_v6_set_title(window->xdg_toplevel, "");
    }
  } else if (event->atom == xwl->atoms[ATOM_WL_SELECTION].value) {
    if (event->window == xwl->selection_window &&
        event->state == XCB_PROPERTY_NEW_VALUE &&
        xwl->selection_incremental_transfer) {
      xcb_get_property_reply_t *reply = xcb_get_property_reply(
          xwl->connection,
          xcb_get_property(xwl->connection, 0, xwl->selection_window,
                           xwl->atoms[ATOM_WL_SELECTION].value,
                           XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff),
          NULL);

      if (!reply)
        return;

      if (xcb_get_property_value_length(reply) > 0) {
        xwl_write_selection_property(xwl, reply);
      } else {
        assert(!xwl->selection_send_event_source);
        close(xwl->selection_data_source_send_fd);
        free(reply);
      }
    }
  } else if (event->atom == xwl->selection_request.property) {
    if (event->window == xwl->selection_request.requestor &&
        event->state == XCB_PROPERTY_DELETE &&
        xwl->selection_incremental_transfer) {
      int data_size = xwl->selection_data.size;

      xwl->selection_data_ack_pending = 0;

      // Handle the case when there's more data to be received.
      if (xwl->selection_data_offer_receive_fd >= 0) {
        // Avoid sending empty data until transfer is complete.
        if (data_size)
          xwl_send_selection_data(xwl);

        if (!xwl->selection_event_source) {
          xwl->selection_event_source = wl_event_loop_add_fd(
              wl_display_get_event_loop(xwl->host_display),
              xwl->selection_data_offer_receive_fd, WL_EVENT_READABLE,
              xwl_handle_selection_fd_readable, xwl);
        }
        return;
      }

      xwl_send_selection_data(xwl);

      // Release data if transfer is complete.
      if (!data_size) {
        xwl->selection_request.requestor = XCB_NONE;
        wl_array_release(&xwl->selection_data);
      }
    }
  }
}

static void xwl_internal_data_source_target(void *data,
                                            struct wl_data_source *data_source,
                                            const char *mime_type) {}

static void xwl_internal_data_source_send(void *data,
                                          struct wl_data_source *data_source,
                                          const char *mime_type, int32_t fd) {
  struct xwl_data_source *host = data;
  struct xwl *xwl = host->xwl;

  if (strcmp(mime_type, xwl_utf8_mime_type) == 0) {
    int flags;
    int rv;

    xcb_convert_selection(
        xwl->connection, xwl->selection_window,
        xwl->atoms[ATOM_CLIPBOARD].value, xwl->atoms[ATOM_UTF8_STRING].value,
        xwl->atoms[ATOM_WL_SELECTION].value, XCB_CURRENT_TIME);

    flags = fcntl(fd, F_GETFL, 0);
    rv = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    assert(!rv);

    xwl->selection_data_source_send_fd = fd;
  } else {
    close(fd);
  }
}

static void
xwl_internal_data_source_cancelled(void *data,
                                   struct wl_data_source *data_source) {
  struct xwl_data_source *host = data;

  if (host->xwl->selection_data_source == host)
    host->xwl->selection_data_source = NULL;

  wl_data_source_destroy(data_source);
}

static const struct wl_data_source_listener xwl_internal_data_source_listener =
    {xwl_internal_data_source_target, xwl_internal_data_source_send,
     xwl_internal_data_source_cancelled};

static void xwl_get_selection_targets(struct xwl *xwl) {
  struct xwl_data_source *data_source = NULL;
  xcb_get_property_reply_t *reply;
  xcb_atom_t *value;
  uint32_t i;

  reply = xcb_get_property_reply(
      xwl->connection,
      xcb_get_property(xwl->connection, 1, xwl->selection_window,
                       xwl->atoms[ATOM_WL_SELECTION].value,
                       XCB_GET_PROPERTY_TYPE_ANY, 0, 4096),
      NULL);
  if (!reply)
    return;

  if (reply->type != XCB_ATOM_ATOM) {
    free(reply);
    return;
  }

  if (xwl->data_device_manager) {
    data_source = malloc(sizeof(*data_source));
    assert(data_source);

    data_source->xwl = xwl;
    data_source->internal = wl_data_device_manager_create_data_source(
        xwl->data_device_manager->internal);
    wl_data_source_add_listener(
        data_source->internal, &xwl_internal_data_source_listener, data_source);

    value = xcb_get_property_value(reply);
    for (i = 0; i < reply->value_len; i++) {
      if (value[i] == xwl->atoms[ATOM_UTF8_STRING].value)
        wl_data_source_offer(data_source->internal, xwl_utf8_mime_type);
    }

    if (xwl->selection_data_device && xwl->default_seat) {
      wl_data_device_set_selection(xwl->selection_data_device,
                                   data_source->internal,
                                   xwl->default_seat->seat->last_serial);
    }

    if (xwl->selection_data_source) {
      wl_data_source_destroy(xwl->selection_data_source->internal);
      free(xwl->selection_data_source);
    }
    xwl->selection_data_source = data_source;
  }

  free(reply);
}

static void xwl_get_selection_data(struct xwl *xwl) {
  xcb_get_property_reply_t *reply = xcb_get_property_reply(
      xwl->connection,
      xcb_get_property(xwl->connection, 1, xwl->selection_window,
                       xwl->atoms[ATOM_WL_SELECTION].value,
                       XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff),
      NULL);
  if (!reply)
    return;

  if (reply->type == xwl->atoms[ATOM_INCR].value) {
    xwl->selection_incremental_transfer = 1;
    free(reply);
  } else {
    xwl->selection_incremental_transfer = 0;
    xwl_write_selection_property(xwl, reply);
  }
}

static void xwl_handle_selection_notify(struct xwl *xwl,
                                        xcb_selection_notify_event_t *event) {
  if (event->property == XCB_ATOM_NONE)
    return;

  if (event->target == xwl->atoms[ATOM_TARGETS].value)
    xwl_get_selection_targets(xwl);
  else
    xwl_get_selection_data(xwl);
}

static void xwl_send_targets(struct xwl *xwl) {
  xcb_atom_t targets[] = {
      xwl->atoms[ATOM_TIMESTAMP].value, xwl->atoms[ATOM_TARGETS].value,
      xwl->atoms[ATOM_UTF8_STRING].value, xwl->atoms[ATOM_TEXT].value,
  };

  xcb_change_property(xwl->connection, XCB_PROP_MODE_REPLACE,
                      xwl->selection_request.requestor,
                      xwl->selection_request.property, XCB_ATOM_ATOM, 32,
                      ARRAY_SIZE(targets), targets);

  xwl_send_selection_notify(xwl, xwl->selection_request.property);
}

static void xwl_send_timestamp(struct xwl *xwl) {
  xcb_change_property(xwl->connection, XCB_PROP_MODE_REPLACE,
                      xwl->selection_request.requestor,
                      xwl->selection_request.property, XCB_ATOM_INTEGER, 32, 1,
                      &xwl->selection_timestamp);

  xwl_send_selection_notify(xwl, xwl->selection_request.property);
}

static void xwl_send_data(struct xwl *xwl) {
  int rv;

  if (!xwl->selection_data_offer || !xwl->selection_data_offer->utf8_text) {
    xwl_send_selection_notify(xwl, XCB_ATOM_NONE);
    return;
  }

  wl_array_init(&xwl->selection_data);
  xwl->selection_data_ack_pending = 0;

  switch (xwl->data_driver) {
  case DATA_DRIVER_VIRTWL: {
    struct virtwl_ioctl_new new_pipe = {
        .type = VIRTWL_IOCTL_NEW_PIPE_READ, .fd = -1, .flags = 0, .size = 0,
    };

    rv = ioctl(xwl->virtwl_fd, VIRTWL_IOCTL_NEW, &new_pipe);
    if (rv) {
      fprintf(stderr, "error: failed to create virtwl pipe: %s\n",
              strerror(errno));
      xwl_send_selection_notify(xwl, XCB_ATOM_NONE);
      return;
    }

    xwl->selection_data_offer_receive_fd = new_pipe.fd;
    wl_data_offer_receive(xwl->selection_data_offer->internal,
                          xwl_utf8_mime_type, new_pipe.fd);
  } break;
  case DATA_DRIVER_NOOP: {
    int p[2];

    rv = pipe2(p, O_CLOEXEC | O_NONBLOCK);
    assert(!rv);

    xwl->selection_data_offer_receive_fd = p[0];
    wl_data_offer_receive(xwl->selection_data_offer->internal,
                          xwl_utf8_mime_type, p[1]);
    close(p[1]);
  } break;
  }

  assert(!xwl->selection_event_source);
  xwl->selection_event_source = wl_event_loop_add_fd(
      wl_display_get_event_loop(xwl->host_display),
      xwl->selection_data_offer_receive_fd, WL_EVENT_READABLE,
      xwl_handle_selection_fd_readable, xwl);
}

static void xwl_handle_selection_request(struct xwl *xwl,
                                         xcb_selection_request_event_t *event) {
  xwl->selection_request = *event;
  xwl->selection_incremental_transfer = 0;

  if (event->selection == xwl->atoms[ATOM_CLIPBOARD_MANAGER].value) {
    xwl_send_selection_notify(xwl, xwl->selection_request.property);
    return;
  }

  if (event->target == xwl->atoms[ATOM_TARGETS].value) {
    xwl_send_targets(xwl);
  } else if (event->target == xwl->atoms[ATOM_TIMESTAMP].value) {
    xwl_send_timestamp(xwl);
  } else if (event->target == xwl->atoms[ATOM_UTF8_STRING].value ||
             event->target == xwl->atoms[ATOM_TEXT].value) {
    xwl_send_data(xwl);
  } else {
    xwl_send_selection_notify(xwl, XCB_ATOM_NONE);
  }
}

static void
xwl_handle_xfixes_selection_notify(struct xwl *xwl,
                                   xcb_xfixes_selection_notify_event_t *event) {
  if (event->selection != xwl->atoms[ATOM_CLIPBOARD].value)
    return;

  if (event->owner == XCB_WINDOW_NONE) {
    // If client selection is gone. Set NULL selection for each seat.
    if (xwl->selection_owner != xwl->selection_window) {
      if (xwl->selection_data_device && xwl->default_seat) {
        wl_data_device_set_selection(xwl->selection_data_device, NULL,
                                     xwl->default_seat->seat->last_serial);
      }
    }
    xwl->selection_owner = XCB_WINDOW_NONE;
    return;
  }

  xwl->selection_owner = event->owner;

  // Save timestamp if it's our selection.
  if (event->owner == xwl->selection_window) {
    xwl->selection_timestamp = event->timestamp;
    return;
  }

  xwl->selection_incremental_transfer = 0;
  xcb_convert_selection(xwl->connection, xwl->selection_window,
                        xwl->atoms[ATOM_CLIPBOARD].value,
                        xwl->atoms[ATOM_TARGETS].value,
                        xwl->atoms[ATOM_WL_SELECTION].value, event->timestamp);
}

static int xwl_handle_x_connection_event(int fd, uint32_t mask, void *data) {
  struct xwl *xwl = (struct xwl *)data;
  xcb_generic_event_t *event;
  uint32_t count = 0;

  if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR))
    return 0;

  while ((event = xcb_poll_for_event(xwl->connection))) {
    switch (event->response_type & ~SEND_EVENT_MASK) {
    case XCB_CREATE_NOTIFY:
      xwl_handle_create_notify(xwl, (xcb_create_notify_event_t *)event);
      break;
    case XCB_DESTROY_NOTIFY:
      xwl_handle_destroy_notify(xwl, (xcb_destroy_notify_event_t *)event);
      break;
    case XCB_REPARENT_NOTIFY:
      xwl_handle_reparent_notify(xwl, (xcb_reparent_notify_event_t *)event);
      break;
    case XCB_MAP_REQUEST:
      xwl_handle_map_request(xwl, (xcb_map_request_event_t *)event);
      break;
    case XCB_MAP_NOTIFY:
      xwl_handle_map_notify(xwl, (xcb_map_notify_event_t *)event);
      break;
    case XCB_UNMAP_NOTIFY:
      xwl_handle_unmap_notify(xwl, (xcb_unmap_notify_event_t *)event);
      break;
    case XCB_CONFIGURE_REQUEST:
      xwl_handle_configure_request(xwl, (xcb_configure_request_event_t *)event);
      break;
    case XCB_CONFIGURE_NOTIFY:
      xwl_handle_configure_notify(xwl, (xcb_configure_notify_event_t *)event);
      break;
    case XCB_CLIENT_MESSAGE:
      xwl_handle_client_message(xwl, (xcb_client_message_event_t *)event);
      break;
    case XCB_FOCUS_IN:
      xwl_handle_focus_in(xwl, (xcb_focus_in_event_t *)event);
      break;
    case XCB_FOCUS_OUT:
      xwl_handle_focus_out(xwl, (xcb_focus_out_event_t *)event);
      break;
    case XCB_PROPERTY_NOTIFY:
      xwl_handle_property_notify(xwl, (xcb_property_notify_event_t *)event);
      break;
    case XCB_SELECTION_NOTIFY:
      xwl_handle_selection_notify(xwl, (xcb_selection_notify_event_t *)event);
      break;
    case XCB_SELECTION_REQUEST:
      xwl_handle_selection_request(xwl, (xcb_selection_request_event_t *)event);
      break;
    }

    switch (event->response_type - xwl->xfixes_extension->first_event) {
    case XCB_XFIXES_SELECTION_NOTIFY:
      xwl_handle_xfixes_selection_notify(
          xwl, (xcb_xfixes_selection_notify_event_t *)event);
      break;
    }

    free(event);
    ++count;
  }

  if ((mask & ~WL_EVENT_WRITABLE) == 0)
    xcb_flush(xwl->connection);

  return count;
}

static void xwl_connect(struct xwl *xwl) {
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

  xwl->connection = xcb_connect_to_fd(xwl->wm_fd, NULL);
  assert(!xcb_connection_has_error(xwl->connection));

  xcb_prefetch_extension_data(xwl->connection, &xcb_xfixes_id);
  xcb_prefetch_extension_data(xwl->connection, &xcb_composite_id);

  for (i = 0; i < ARRAY_SIZE(xwl->atoms); ++i) {
    const char *name = xwl->atoms[i].name;
    xwl->atoms[i].cookie =
        xcb_intern_atom(xwl->connection, 0, strlen(name), name);
  }

  setup = xcb_get_setup(xwl->connection);
  screen_iterator = xcb_setup_roots_iterator(setup);
  xwl->screen = screen_iterator.data;

  // Select for substructure redirect.
  values[0] = XCB_EVENT_MASK_STRUCTURE_NOTIFY |
              XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
              XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
  change_attributes_cookie = xcb_change_window_attributes(
      xwl->connection, xwl->screen->root, XCB_CW_EVENT_MASK, values);

  xwl->connection_event_source = wl_event_loop_add_fd(
      wl_display_get_event_loop(xwl->host_display),
      xcb_get_file_descriptor(xwl->connection), WL_EVENT_READABLE,
      &xwl_handle_x_connection_event, xwl);

  xwl->xfixes_extension =
      xcb_get_extension_data(xwl->connection, &xcb_xfixes_id);
  assert(xwl->xfixes_extension->present);

  xfixes_query_version_reply = xcb_xfixes_query_version_reply(
      xwl->connection,
      xcb_xfixes_query_version(xwl->connection, XCB_XFIXES_MAJOR_VERSION,
                               XCB_XFIXES_MINOR_VERSION),
      NULL);
  assert(xfixes_query_version_reply);
  assert(xfixes_query_version_reply->major_version >= 5);
  free(xfixes_query_version_reply);

  composite_extension =
      xcb_get_extension_data(xwl->connection, &xcb_composite_id);
  assert(composite_extension->present);

  redirect_subwindows_cookie = xcb_composite_redirect_subwindows_checked(
      xwl->connection, xwl->screen->root, XCB_COMPOSITE_REDIRECT_MANUAL);

  // Another window manager should not be running.
  error = xcb_request_check(xwl->connection, change_attributes_cookie);
  assert(!error);

  // Redirecting subwindows of root for compositing should have succeeded.
  error = xcb_request_check(xwl->connection, redirect_subwindows_cookie);
  assert(!error);

  xwl->window = xcb_generate_id(xwl->connection);
  xcb_create_window(xwl->connection, 0, xwl->window, xwl->screen->root, 0, 0, 1,
                    1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0,
                    NULL);

  for (i = 0; i < ARRAY_SIZE(xwl->atoms); ++i) {
    atom_reply =
        xcb_intern_atom_reply(xwl->connection, xwl->atoms[i].cookie, &error);
    assert(!error);
    xwl->atoms[i].value = atom_reply->atom;
    free(atom_reply);
  }

  depth_iterator = xcb_screen_allowed_depths_iterator(xwl->screen);
  while (depth_iterator.rem > 0) {
    int depth = depth_iterator.data->depth;
    if (depth == xwl->screen->root_depth) {
      xwl->visual_ids[depth] = xwl->screen->root_visual;
      xwl->colormaps[depth] = xwl->screen->default_colormap;
    } else {
      xcb_visualtype_iterator_t visualtype_iterator =
          xcb_depth_visuals_iterator(depth_iterator.data);

      xwl->visual_ids[depth] = visualtype_iterator.data->visual_id;
      xwl->colormaps[depth] = xcb_generate_id(xwl->connection);
      xcb_create_colormap(xwl->connection, XCB_COLORMAP_ALLOC_NONE,
                          xwl->colormaps[depth], xwl->screen->root,
                          xwl->visual_ids[depth]);
    }
    xcb_depth_next(&depth_iterator);
  }
  assert(xwl->visual_ids[xwl->screen->root_depth]);

  if (xwl->clipboard_manager) {
    values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xwl->selection_window = xcb_generate_id(xwl->connection);
    xcb_create_window(xwl->connection, XCB_COPY_FROM_PARENT,
                      xwl->selection_window, xwl->screen->root, 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, xwl->screen->root_visual,
                      XCB_CW_EVENT_MASK, values);
    xcb_set_selection_owner(xwl->connection, xwl->selection_window,
                            xwl->atoms[ATOM_CLIPBOARD_MANAGER].value,
                            XCB_CURRENT_TIME);
    xcb_xfixes_select_selection_input(
        xwl->connection, xwl->selection_window,
        xwl->atoms[ATOM_CLIPBOARD].value,
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY |
            XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE);
    xwl_set_selection(xwl, NULL);
  }

  xcb_change_property(xwl->connection, XCB_PROP_MODE_REPLACE, xwl->window,
                      xwl->atoms[ATOM_NET_SUPPORTING_WM_CHECK].value,
                      XCB_ATOM_WINDOW, 32, 1, &xwl->window);
  xcb_change_property(xwl->connection, XCB_PROP_MODE_REPLACE, xwl->window,
                      xwl->atoms[ATOM_NET_WM_NAME].value,
                      xwl->atoms[ATOM_UTF8_STRING].value, 8, strlen(wm_name),
                      wm_name);
  xcb_change_property(xwl->connection, XCB_PROP_MODE_REPLACE, xwl->screen->root,
                      xwl->atoms[ATOM_NET_SUPPORTING_WM_CHECK].value,
                      XCB_ATOM_WINDOW, 32, 1, &xwl->window);
  xcb_set_selection_owner(xwl->connection, xwl->window,
                          xwl->atoms[ATOM_WM_S0].value, XCB_CURRENT_TIME);

  xcb_set_input_focus(xwl->connection, XCB_INPUT_FOCUS_NONE, XCB_NONE,
                      XCB_CURRENT_TIME);
  xcb_flush(xwl->connection);
}

static int xwl_handle_sigchld(int signal_number, void *data) {
  struct xwl *xwl = (struct xwl *)data;
  int status;
  pid_t pid;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (pid == xwl->child_pid) {
      xwl->child_pid = -1;
      if (WIFEXITED(status) && WEXITSTATUS(status)) {
        fprintf(stderr, "Child exited with status: %d\n", WEXITSTATUS(status));
      }
      if (xwl->exit_with_child && xwl->xwayland_pid >= 0)
        kill(xwl->xwayland_pid, SIGTERM);
    } else if (pid == xwl->xwayland_pid) {
      xwl->xwayland_pid = -1;
      if (WIFEXITED(status) && WEXITSTATUS(status)) {
        fprintf(stderr, "Xwayland exited with status: %d\n",
                WEXITSTATUS(status));
        exit(WEXITSTATUS(status));
      }
    }
  }

  return 1;
}

static void xwl_execvp(const char *file, char *const argv[],
                       int wayland_socked_fd) {
  if (wayland_socked_fd >= 0) {
    char fd_str[8];
    int fd;

    fd = dup(wayland_socked_fd);
    snprintf(fd_str, sizeof(fd_str), "%d", fd);
    setenv("WAYLAND_SOCKET", fd_str, 1);
  }

  setenv("SOMMELIER_VERSION", VERSION, 1);

  execvp(file, argv);
  perror(file);
}

static void xwl_sd_notify(const char *state) {
  const char *socket_name;
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
  iovec.iov_base = (char *)state;
  iovec.iov_len = strlen(state);

  memset(&msghdr, 0, sizeof(msghdr));
  msghdr.msg_name = &addr;
  msghdr.msg_namelen =
      offsetof(struct sockaddr_un, sun_path) + strlen(socket_name);
  msghdr.msg_iov = &iovec;
  msghdr.msg_iovlen = 1;

  rv = sendmsg(fd, &msghdr, MSG_NOSIGNAL);
  assert(rv != -1);
}

static int xwl_handle_display_ready_event(int fd, uint32_t mask, void *data) {
  struct xwl *xwl = (struct xwl *)data;
  char display_name[9];
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

  xwl_connect(xwl);

  wl_event_source_remove(xwl->display_ready_event_source);
  xwl->display_ready_event_source = NULL;
  close(fd);

  if (xwl->sd_notify)
    xwl_sd_notify(xwl->sd_notify);

  pid = fork();
  assert(pid >= 0);
  if (pid == 0) {
    xwl_execvp(xwl->runprog[0], xwl->runprog, -1);
    _exit(EXIT_FAILURE);
  }

  xwl->child_pid = pid;

  return 1;
}

static void xwl_sigchld_handler(int signal) {
  while (waitpid(-1, NULL, WNOHANG) > 0)
    continue;
}

static void xwl_client_destroy_notify(struct wl_listener *listener,
                                      void *data) {
  exit(0);
}

static void xwl_registry_bind(struct wl_client *client,
                              struct wl_resource *resource, uint32_t name,
                              const char *interface, uint32_t version,
                              uint32_t id) {
  struct xwl_host_registry *host = wl_resource_get_user_data(resource);
  struct xwl_global *global;

  wl_list_for_each(global, &host->xwl->globals, link) {
    if (global->name == name)
      break;
  }

  assert(&global->link != &host->xwl->globals);
  assert(version != 0);
  assert(global->version >= version);

  global->bind(client, global->data, version, id);
}

static const struct wl_registry_interface xwl_registry_implementation = {
    xwl_registry_bind};

static void xwl_sync_callback_done(void *data, struct wl_callback *callback,
                                   uint32_t serial) {
  struct xwl_host_callback *host = wl_callback_get_user_data(callback);

  wl_callback_send_done(host->resource, serial);
  wl_resource_destroy(host->resource);
}

static const struct wl_callback_listener xwl_sync_callback_listener = {
    xwl_sync_callback_done};

static void xwl_display_sync(struct wl_client *client,
                             struct wl_resource *resource, uint32_t id) {
  struct xwl *xwl = wl_resource_get_user_data(resource);
  struct xwl_host_callback *host_callback;

  host_callback = malloc(sizeof(*host_callback));
  assert(host_callback);

  host_callback->resource =
      wl_resource_create(client, &wl_callback_interface, 1, id);
  wl_resource_set_implementation(host_callback->resource, NULL, host_callback,
                                 xwl_host_callback_destroy);
  host_callback->proxy = wl_display_sync(xwl->display);
  wl_callback_set_user_data(host_callback->proxy, host_callback);
  wl_callback_add_listener(host_callback->proxy, &xwl_sync_callback_listener,
                           host_callback);
}

static void xwl_destroy_host_registry(struct wl_resource *resource) {
  struct xwl_host_registry *host = wl_resource_get_user_data(resource);

  wl_list_remove(&host->link);
  free(host);
}

static void xwl_display_get_registry(struct wl_client *client,
                                     struct wl_resource *resource,
                                     uint32_t id) {
  struct xwl *xwl = wl_resource_get_user_data(resource);
  struct xwl_host_registry *host_registry;
  struct xwl_global *global;

  host_registry = malloc(sizeof(*host_registry));
  assert(host_registry);

  host_registry->xwl = xwl;
  host_registry->resource =
      wl_resource_create(client, &wl_registry_interface, 1, id);
  wl_list_insert(&xwl->registries, &host_registry->link);
  wl_resource_set_implementation(host_registry->resource,
                                 &xwl_registry_implementation, host_registry,
                                 xwl_destroy_host_registry);

  wl_list_for_each(global, &xwl->globals, link) {
    wl_resource_post_event(host_registry->resource, WL_REGISTRY_GLOBAL,
                           global->name, global->interface->name,
                           global->version);
  }
}

static const struct wl_display_interface xwl_display_implementation = {
    xwl_display_sync, xwl_display_get_registry};

static enum wl_iterator_result
xwl_set_display_implementation(struct wl_resource *resource, void *user_data) {
  struct xwl *xwl = (struct xwl *)user_data;

  if (strcmp(wl_resource_get_class(resource), "wl_display") == 0) {
    wl_resource_set_implementation(resource, &xwl_display_implementation, xwl,
                                   NULL);
    return WL_ITERATOR_STOP;
  }

  return WL_ITERATOR_CONTINUE;
}

static int xwl_handle_virtwl_ctx_event(int fd, uint32_t mask, void *data) {
  struct xwl *xwl = (struct xwl *)data;
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
    close(xwl->virtwl_socket_fd);
    xwl->virtwl_socket_fd = -1;
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

  bytes = sendmsg(xwl->virtwl_socket_fd, &msg, MSG_NOSIGNAL);
  assert(bytes == ioctl_recv->len);

  while (fd_count--)
    close(ioctl_recv->fds[fd_count]);

  return 1;
}

static int xwl_handle_virtwl_socket_event(int fd, uint32_t mask, void *data) {
  struct xwl *xwl = (struct xwl *)data;
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

  bytes = recvmsg(xwl->virtwl_socket_fd, &msg, 0);
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
  rv = ioctl(xwl->virtwl_ctx_fd, VIRTWL_IOCTL_SEND, ioctl_send);
  assert(!rv);

  while (fd_count--)
    close(ioctl_send->fds[fd_count]);

  return 1;
}

// Break |str| into a sequence of zero or more nonempty arguments. No more
// than |argc| arguments will be added to |argv|. Returns the total number of
// argments found in |str|.
static int xwl_parse_cmd_prefix(char *str, int argc, char **argv) {
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

static void xwl_print_usage() {
  printf("usage: sommelier [options] [program] [args...]\n\n"
         "options:\n"
         "  -h, --help\t\t\tPrint this help\n"
         "  -X\t\t\t\tEnable X11 forwarding\n"
         "  --master\t\t\tRun as master and spawn child processes\n"
         "  --socket=SOCKET\t\tName of socket to listen on\n"
         "  --display=DISPLAY\t\tWayland display to connect to\n"
         "  --shm-driver=DRIVER\t\tSHM driver to use (noop, dmabuf, virtwl)\n"
         "  --data-driver=DRIVER\t\tData driver to use (noop, virtwl)\n"
         "  --scale=SCALE\t\t\tScale factor for contents\n"
         "  --peer-cmd-prefix=PREFIX\tPeer process command line prefix\n"
         "  --accelerators=ACCELERATORS\tList of keyboard accelerators\n"
         "  --app-id=ID\t\t\tForced application ID for X11 clients\n"
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
  struct xwl xwl = {
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
      .app_id = NULL,
      .exit_with_child = 1,
      .sd_notify = NULL,
      .clipboard_manager = 0,
      .frame_color = 0,
      .has_frame_color = 0,
      .show_window_title = 0,
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
                  [ATOM_TIMESTAMP] = {"TIMESTAMP"}, [ATOM_TEXT] = {"TEXT"},
                  [ATOM_INCR] = {"INCR"},
                  [ATOM_WL_SELECTION] = {"_WL_SELECTION"},
          },
      .visual_ids = {0},
      .colormaps = {0}};
  const char *display = getenv("SOMMELIER_DISPLAY");
  const char *scale = getenv("SOMMELIER_SCALE");
  const char *clipboard_manager = getenv("SOMMELIER_CLIPBOARD_MANAGER");
  const char *frame_color = getenv("SOMMELIER_FRAME_COLOR");
  const char *show_window_title = getenv("SOMMELIER_SHOW_WINDOW_TITLE");
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
                                                    xwl_client_destroy_notify};
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
      xwl_print_usage();
      return EXIT_SUCCESS;
    }
    if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("Version: %s\n", VERSION);
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
      xwl.peer_pid = atoi(s);
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
    } else if (strstr(arg, "--accelerators") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      accelerators = s;
    } else if (strstr(arg, "--app-id") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      xwl.app_id = s;
    } else if (strstr(arg, "-X") == arg) {
      xwl.xwayland = 1;
    } else if (strstr(arg, "--x-display") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      xdisplay = atoi(s);
      // Automatically enable X forwarding if X display is specified.
      xwl.xwayland = 1;
    } else if (strstr(arg, "--xwayland-path") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      xwayland_path = s;
    } else if (strstr(arg, "--no-exit-with-child") == arg) {
      xwl.exit_with_child = 0;
    } else if (strstr(arg, "--sd-notify") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      xwl.sd_notify = s;
    } else if (strstr(arg, "--no-clipboard-manager") == arg) {
      clipboard_manager = "0";
    } else if (strstr(arg, "--frame-color") == arg) {
      const char *s = strchr(arg, '=');
      ++s;
      frame_color = s;
    } else if (strstr(arg, "--show-window-title") == arg) {
      show_window_title = "1";
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
      xwl.runprog = &argv[i + 1];
      break;
    } else {
      xwl.runprog = &argv[i];
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

    sa.sa_handler = xwl_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    rv = sigaction(SIGCHLD, &sa, NULL);
    assert(rv >= 0);

    if (xwl.sd_notify)
      xwl_sd_notify(xwl.sd_notify);

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

        if (peer_cmd_prefix) {
          snprintf(peer_cmd_prefix_str, sizeof(peer_cmd_prefix_str), "%s",
                   peer_cmd_prefix);

          i = xwl_parse_cmd_prefix(peer_cmd_prefix_str, 32, args);
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
    if (!xwl.runprog || !xwl.runprog[0]) {
      xwl_print_usage();
      return EXIT_FAILURE;
    }
  }

  if (xwl.xwayland) {
    assert(client_fd == -1);

    xwl.clipboard_manager = 1;
    if (clipboard_manager)
      xwl.clipboard_manager = !!strcmp(clipboard_manager, "0");
  }

  if (scale) {
    xwl.desired_scale = atof(scale);
    // Round to integer scale until we detect wp_viewporter support.
    xwl.scale = MIN(MAX_SCALE, MAX(MIN_SCALE, round(xwl.desired_scale)));
  }

  if (frame_color) {
    int r, g, b;
    if (sscanf(frame_color, "#%02x%02x%02x", &r, &g, &b) == 3) {
      xwl.frame_color = 0xff000000 | (r << 16) | (g << 8) | (b << 0);
      xwl.has_frame_color = 1;
    }
  }

  if (show_window_title)
    xwl.show_window_title = !!strcmp(show_window_title, "0");

  // Handle broken pipes without signals that kill the entire process.
  signal(SIGPIPE, SIG_IGN);

  xwl.host_display = wl_display_create();
  assert(xwl.host_display);

  event_loop = wl_display_get_event_loop(xwl.host_display);

  if (virtwl_device) {
    struct virtwl_ioctl_new new_ctx = {
        .type = VIRTWL_IOCTL_NEW_CTX, .fd = -1, .flags = 0, .size = 0,
    };

    xwl.virtwl_fd = open(virtwl_device, O_RDWR);
    if (xwl.virtwl_fd == -1) {
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

      xwl.virtwl_socket_fd = vws[0];
      virtwl_display_fd = vws[1];

      rv = ioctl(xwl.virtwl_fd, VIRTWL_IOCTL_NEW, &new_ctx);
      assert(!rv);

      xwl.virtwl_ctx_fd = new_ctx.fd;

      xwl.virtwl_socket_event_source = wl_event_loop_add_fd(
          event_loop, xwl.virtwl_socket_fd, WL_EVENT_READABLE,
          xwl_handle_virtwl_socket_event, &xwl);
      xwl.virtwl_ctx_event_source =
          wl_event_loop_add_fd(event_loop, xwl.virtwl_ctx_fd, WL_EVENT_READABLE,
                               xwl_handle_virtwl_ctx_event, &xwl);
    }
  }

  if (drm_device) {
    int drm_fd = open(drm_device, O_RDWR | O_CLOEXEC);
    if (drm_fd == -1) {
      fprintf(stderr, "error: could not open %s (%s)\n", drm_device,
              strerror(errno));
      return EXIT_FAILURE;
    }

    xwl.gbm = gbm_create_device(drm_fd);
    if (!xwl.gbm) {
      fprintf(stderr, "error: couldn't get display device\n");
      return EXIT_FAILURE;
    }

    xwl.drm_device = drm_device;
  }

  if (shm_driver) {
    if (strcmp(shm_driver, "dmabuf") == 0) {
      if (!xwl.drm_device) {
        fprintf(stderr, "error: need drm device for dmabuf driver\n");
        return EXIT_FAILURE;
      }
      xwl.shm_driver = SHM_DRIVER_DMABUF;
    } else if (strcmp(shm_driver, "virtwl") == 0) {
      if (xwl.virtwl_fd == -1) {
        fprintf(stderr, "error: need device for virtwl driver\n");
        return EXIT_FAILURE;
      }
      xwl.shm_driver = SHM_DRIVER_VIRTWL;
    }
  } else if (xwl.drm_device) {
    xwl.shm_driver = SHM_DRIVER_DMABUF;
  } else if (xwl.virtwl_fd != -1) {
    xwl.shm_driver = SHM_DRIVER_VIRTWL;
  }

  if (data_driver) {
    if (strcmp(data_driver, "virtwl") == 0) {
      if (xwl.virtwl_fd == -1) {
        fprintf(stderr, "error: need device for virtwl driver\n");
        return EXIT_FAILURE;
      }
      xwl.data_driver = DATA_DRIVER_VIRTWL;
    }
  } else if (xwl.virtwl_fd != -1) {
    xwl.data_driver = DATA_DRIVER_VIRTWL;
  }

  if (xwl.runprog || xwl.xwayland) {
    // Wayland connection from client.
    rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv);
    assert(!rv);

    client_fd = sv[0];
  }

  xwl.xkb_context = xkb_context_new(0);
  assert(xwl.xkb_context);

  if (virtwl_display_fd != -1) {
    xwl.display = wl_display_connect_to_fd(virtwl_display_fd);
  } else {
    if (display == NULL)
      display = getenv("WAYLAND_DISPLAY");
    if (display == NULL)
      display = "wayland-0";

    xwl.display = wl_display_connect(display);
  }

  if (!xwl.display) {
    fprintf(stderr, "error: failed to connect to %s\n", display);
    return EXIT_FAILURE;
  }

  wl_list_init(&xwl.accelerators);
  wl_list_init(&xwl.registries);
  wl_list_init(&xwl.globals);
  wl_list_init(&xwl.outputs);
  wl_list_init(&xwl.seats);
  wl_list_init(&xwl.windows);
  wl_list_init(&xwl.unpaired_windows);

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
        struct xwl_accelerator *accelerator;
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

        wl_list_insert(&xwl.accelerators, &accelerator->link);

        modifiers = 0;
        accelerators = end;
        free(name);
      }
    }
  }

  xwl.display_event_source =
      wl_event_loop_add_fd(event_loop, wl_display_get_fd(xwl.display),
                           WL_EVENT_READABLE, xwl_handle_event, &xwl);

  wl_registry_add_listener(wl_display_get_registry(xwl.display),
                           &xwl_registry_listener, &xwl);

  xwl.client = wl_client_create(xwl.host_display, client_fd);

  // Replace the core display implementation. This is needed in order to
  // implement sync handler properly.
  wl_client_for_each_resource(xwl.client, xwl_set_display_implementation, &xwl);

  if (xwl.runprog || xwl.xwayland) {
    xwl.sigchld_event_source =
        wl_event_loop_add_signal(event_loop, SIGCHLD, xwl_handle_sigchld, &xwl);

    if (xwl.xwayland) {
      int ds[2], wm[2];

      // Xwayland display ready socket.
      rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, ds);
      assert(!rv);

      xwl.display_ready_event_source =
          wl_event_loop_add_fd(event_loop, ds[0], WL_EVENT_READABLE,
                               xwl_handle_display_ready_event, &xwl);

      // X connection to Xwayland.
      rv = socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, wm);
      assert(!rv);

      xwl.wm_fd = wm[0];

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

          i = xwl_parse_cmd_prefix(xwayland_cmd_prefix_str, 32, args);
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
        if (xwl.drm_device) {
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

        xwl_execvp(args[0], args, sv[1]);
        _exit(EXIT_FAILURE);
      }
      close(wm[1]);
      xwl.xwayland_pid = pid;
    } else {
      pid = fork();
      assert(pid != -1);
      if (pid == 0) {
        xwl_execvp(xwl.runprog[0], xwl.runprog, sv[1]);
        _exit(EXIT_FAILURE);
      }
      xwl.child_pid = pid;
    }
    close(sv[1]);
  }

  wl_client_add_destroy_listener(xwl.client, &client_destroy_listener);

  do {
    wl_display_flush_clients(xwl.host_display);
    if (xwl.connection) {
      if (xwl.needs_set_input_focus) {
        xwl_set_input_focus(&xwl, xwl.host_focus_window);
        xwl.needs_set_input_focus = 0;
      }
      xcb_flush(xwl.connection);
    }
    if (wl_display_flush(xwl.display) < 0)
      return EXIT_FAILURE;
  } while (wl_event_loop_dispatch(event_loop, -1) != -1);

  return EXIT_SUCCESS;
}
