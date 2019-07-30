// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/strings/string_number_conversions.h"
#include "brillo/syslog_logging.h"

template <typename... Args>
void do_nothing(Args... args) {}

struct wayland_globals {
  struct wl_seat* seat;
  struct wl_data_device_manager* data_device_manager;
  struct wl_shm* shm;
  struct wl_compositor* compositor;
  struct wl_shell* shell;
  struct wl_output* output;
};

struct output_data {
  int32_t width;
  int32_t height;
  uint32_t scale;
  bool done = false;
};

void output_mode(void* user_data,
                 struct wl_output* output,
                 uint32_t flags,
                 int32_t width,
                 int32_t height,
                 int32_t refresh) {
  struct output_data* data = reinterpret_cast<struct output_data*>(user_data);
  data->width = width;
  data->height = height;
}

void output_done(void* user_data, struct wl_output* output) {
  struct output_data* data = reinterpret_cast<struct output_data*>(user_data);
  data->done = true;
}

void output_scale(void* user_data, struct wl_output* output, int32_t scale) {
  struct output_data* data = reinterpret_cast<struct output_data*>(user_data);
  data->scale = scale;
}

struct wl_output_listener output_listener {
  do_nothing /* geometry */, output_mode, output_done, output_scale,
};

void shell_surface_ping(void* data,
                        struct wl_shell_surface* shell_surface,
                        uint32_t serial) {
  wl_shell_surface_pong(shell_surface, serial);
}

struct wl_shell_surface_listener shell_surface_listener {
  shell_surface_ping, do_nothing /* configure */, do_nothing /* popup_done */,
};

struct keyboard_data {
  struct wl_data_device* data_device;
  struct wl_data_source* data_source;
  bool set_selection;
};

void keyboard_key(void* user_data,
                  struct wl_keyboard* keyboard,
                  uint32_t serial,
                  uint32_t time,
                  uint32_t key,
                  uint32_t state) {
  struct keyboard_data* data =
      reinterpret_cast<struct keyboard_data*>(user_data);
  if (!data->set_selection) {
    wl_data_device_set_selection(data->data_device, data->data_source, serial);
    data->set_selection = true;
  }
}

struct wl_keyboard_listener keyboard_listener {
  do_nothing /* keymap */, do_nothing /* enter */, do_nothing /* leave */,
      keyboard_key, do_nothing /* modifiers */, do_nothing /*repeat_info */,
};

struct data_source_data {
  const char* data;
  size_t data_len;
  bool done;
};

void data_source_send(void* user_data,
                      struct wl_data_source* data_source,
                      const char* mime_type,
                      int32_t fd) {
  struct data_source_data* data =
      reinterpret_cast<struct data_source_data*>(user_data);
  base::WriteFileDescriptor(fd, data->data, data->data_len);
  close(fd);
  data->done = true;
}

struct wl_data_source_listener data_source_listener {
  do_nothing /* target */, data_source_send, do_nothing /* cancelled */,
      do_nothing /* dnd_drop_performed */, do_nothing /* dnd_finished */,
      do_nothing /* action */,
};

void registry_global(void* data,
                     struct wl_registry* registry,
                     uint32_t id,
                     const char* interface,
                     uint32_t version) {
  auto* globals = reinterpret_cast<struct wayland_globals*>(data);
  if (strcmp("wl_seat", interface) == 0) {
    globals->seat = reinterpret_cast<struct wl_seat*>(
        wl_registry_bind(registry, id, &wl_seat_interface, version));
  } else if (strcmp("wl_data_device_manager", interface) == 0) {
    globals->data_device_manager =
        reinterpret_cast<struct wl_data_device_manager*>(wl_registry_bind(
            registry, id, &wl_data_device_manager_interface, version));
  } else if (strcmp("wl_shm", interface) == 0) {
    globals->shm = reinterpret_cast<struct wl_shm*>(
        wl_registry_bind(registry, id, &wl_shm_interface, version));
  } else if (strcmp("wl_compositor", interface) == 0) {
    globals->compositor = reinterpret_cast<struct wl_compositor*>(
        wl_registry_bind(registry, id, &wl_compositor_interface, version));
  } else if (strcmp("wl_shell", interface) == 0) {
    globals->shell = reinterpret_cast<struct wl_shell*>(
        wl_registry_bind(registry, id, &wl_shell_interface, version));
  } else if (strcmp("wl_output", interface) == 0) {
    globals->output = reinterpret_cast<struct wl_output*>(
        wl_registry_bind(registry, id, &wl_output_interface, version));
  }
}

struct wl_registry_listener registry_listener = {
    registry_global,
    do_nothing /* global_remove */,
};

int main(int argc, char* argv[]) {
  brillo::InitLog(brillo::kLogToStderrIfTty);
  LOG(INFO) << "Starting wayland_copy_demo application";

  if (argc != 3) {
    LOG(ERROR) << "Usage: wayland_copy_demo [mime type] [data-to-copy]";
    return 1;
  }

  struct wl_display* display = wl_display_connect(nullptr);

  // Get globals objects from registry.
  struct wayland_globals globals;
  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, &globals);
  wl_display_roundtrip(display);

  // Get size of output.
  struct output_data output_data;
  wl_output_add_listener(globals.output, &output_listener, &output_data);
  while (!output_data.done) {
    wl_display_dispatch(display);
  }
  int width = output_data.width;
  int height = output_data.height;
  int stride = width * 4 /* 32 bpp */;
  int memory_size = stride * height;

  // Create a shared memory buffer.
  base::SharedMemory shared_mem;
  shared_mem.CreateAndMapAnonymous(memory_size);
  struct wl_shm_pool* pool =
      wl_shm_create_pool(globals.shm, shared_mem.handle().fd, memory_size);
  struct wl_buffer* buffer = wl_shm_pool_create_buffer(
      pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);

  // Create a surface.
  struct wl_surface* surface = wl_compositor_create_surface(globals.compositor);
  struct wl_shell_surface* shell_surface =
      wl_shell_get_shell_surface(globals.shell, surface);
  wl_shell_surface_add_listener(shell_surface, &shell_surface_listener,
                                nullptr);
  wl_shell_surface_set_title(shell_surface, "Wayland Copy Demo");
  wl_shell_surface_set_toplevel(shell_surface);
  wl_surface_attach(surface, buffer, 0,
                    0);  // Must come after creating shell surface.
  wl_surface_set_buffer_scale(surface, output_data.scale);
  wl_surface_damage(surface, 0, 0, width / output_data.scale,
                    height / output_data.scale);
  wl_surface_commit(surface);

  // Make a data source.
  struct wl_data_device* data_device = wl_data_device_manager_get_data_device(
      globals.data_device_manager, globals.seat);
  struct wl_data_source* data_source =
      wl_data_device_manager_create_data_source(globals.data_device_manager);
  wl_data_source_offer(data_source, argv[1]);

  // Set up the data source listener.
  struct data_source_data data_source_data;
  data_source_data.data = argv[2];
  data_source_data.data_len = strlen(argv[2]);
  data_source_data.done = false;
  wl_data_source_add_listener(data_source, &data_source_listener,
                              &data_source_data);

  // Set up the keyboard listener.
  struct wl_keyboard* keyboard = wl_seat_get_keyboard(globals.seat);
  struct keyboard_data keyboard_data;
  keyboard_data.data_device = data_device;
  keyboard_data.data_source = data_source;
  keyboard_data.set_selection = false;
  wl_keyboard_add_listener(keyboard, &keyboard_listener, &keyboard_data);

  while (!data_source_data.done) {
    wl_display_dispatch(display);
  }

  // Tear down all the protocol objects.
  wl_display_disconnect(display);
  return 0;
}
