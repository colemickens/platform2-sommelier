// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/strings/string_number_conversions.h"
#include "brillo/syslog_logging.h"

template <typename... Args>
void do_nothing(Args... args) {}

struct wl_display* display;

struct wayland_globals {
  struct wl_seat* seat;
  struct wl_data_device_manager* data_device_manager;
  struct wl_shm* shm;
  struct wl_compositor* compositor;
  struct wl_shell* shell;
  struct wl_output* output;
};

struct data_device_data {
  const char* mime_type;
  bool done = false;
};

struct data_offer_data {
  struct data_device_data* data;
};

// Flushes all queued events in the wl_display struct, blocking until
// completion. wl_display_flush alone isn't enough, because it never blocks and
// so may fail to send requests that were made prior to calling it.
void blocking_display_flush(struct wl_display* display) {
  int fd = wl_display_get_fd(display);
  struct pollfd poll_struct {
    fd, POLLOUT | POLLWRBAND, 0
  };
  while (wl_display_flush(display) == -1) {
    if (errno != EAGAIN) {
      const char* error = strerror(errno);
      LOG(ERROR) << "Encountered error while flushing display socket: "
                 << error;
    }
    poll(&poll_struct, 1, -1);
  }
}

void offer(void* user_data,
           struct wl_data_offer* data_offer,
           const char* mime_type) {
  struct data_offer_data* data =
      reinterpret_cast<struct data_offer_data*>(user_data);

  if (strcmp(data->data->mime_type, mime_type) != 0)
    return;

  int pipefd[2];
  int rv = pipe(pipefd);
  if (rv) {
    LOG(ERROR) << "Failed to call pipe: " << errno;
    exit(1);
  }
  wl_data_offer_receive(data_offer, mime_type, pipefd[1]);

  // We must flush all queued requests before listening on the read end of the
  // pipe to avoid deadlocking.
  blocking_display_flush(display);

  close(pipefd[1]);
  char buffer[4096];
  int bytes;
  while ((bytes = HANDLE_EINTR(read(pipefd[0], buffer, sizeof(buffer) - 1))) !=
         0) {
    if (bytes < 0) {
      LOG(ERROR) << "Read from pipe failed: " << errno;
      exit(1);
    }
    // Null terminate the partial read so that printf will handle it correctly
    buffer[bytes] = '\0';
    printf("%s", buffer);
  }
  close(pipefd[0]);
  data->data->done = true;
}

struct wl_data_offer_listener data_offer_listener {
  offer, do_nothing /* source_actions */, do_nothing /* action */,
};

void new_data_offer(void* user_data,
                    struct wl_data_device* data_device,
                    struct wl_data_offer* data_offer) {
  struct data_offer_data* data_offer_data = new struct data_offer_data;
  data_offer_data->data = reinterpret_cast<struct data_device_data*>(user_data);
  wl_data_offer_add_listener(data_offer, &data_offer_listener, data_offer_data);
}

struct wl_data_device_listener data_device_listener {
  new_data_offer, do_nothing /* enter */, do_nothing /* leave */,
      do_nothing /* motion */, do_nothing /* drop */,
      do_nothing /* selection */,
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
  LOG(INFO) << "Starting wayland_paste_demo application";

  if (argc != 2) {
    LOG(ERROR) << "Usage: wayland_paste_demo [mime type]";
    return 1;
  }

  display = wl_display_connect(nullptr);

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
  wl_shell_surface_set_title(shell_surface, "Wayland Paste Demo");
  wl_shell_surface_set_toplevel(shell_surface);
  wl_surface_attach(surface, buffer, 0,
                    0);  // Must come after creating shell surface.
  wl_surface_set_buffer_scale(surface, output_data.scale);
  wl_surface_damage(surface, 0, 0, width / output_data.scale,
                    height / output_data.scale);
  wl_surface_commit(surface);

  // Listen for data offers.
  struct wl_data_device* data_device = wl_data_device_manager_get_data_device(
      globals.data_device_manager, globals.seat);
  struct data_device_data data_device_data;
  data_device_data.mime_type = argv[1];
  wl_data_device_add_listener(data_device, &data_device_listener,
                              &data_device_data);

  while (!data_device_data.done) {
    wl_display_dispatch(display);
  }

  // Tear down all the protocol objects.
  wl_display_disconnect(display);
  return 0;
}
