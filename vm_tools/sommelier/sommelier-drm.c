// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sommelier.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include "drm-server-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"

struct sl_host_drm {
  struct sl_context* ctx;
  uint32_t version;
  struct wl_resource* resource;
  struct zwp_linux_dmabuf_v1* linux_dmabuf_proxy;
  struct wl_callback* callback;
};

static void sl_drm_authenticate(struct wl_client* client,
                                struct wl_resource* resource,
                                uint32_t id) {}

static void sl_drm_create_buffer(struct wl_client* client,
                                 struct wl_resource* resource,
                                 uint32_t id,
                                 uint32_t name,
                                 int32_t width,
                                 int32_t height,
                                 uint32_t stride,
                                 uint32_t format) {
  assert(0);
}

static void sl_drm_create_planar_buffer(struct wl_client* client,
                                        struct wl_resource* resource,
                                        uint32_t id,
                                        uint32_t name,
                                        int32_t width,
                                        int32_t height,
                                        uint32_t format,
                                        int32_t offset0,
                                        int32_t stride0,
                                        int32_t offset1,
                                        int32_t stride1,
                                        int32_t offset2,
                                        int32_t stride2) {
  assert(0);
}

static void sl_drm_create_prime_buffer(struct wl_client* client,
                                       struct wl_resource* resource,
                                       uint32_t id,
                                       int32_t name,
                                       int32_t width,
                                       int32_t height,
                                       uint32_t format,
                                       int32_t offset0,
                                       int32_t stride0,
                                       int32_t offset1,
                                       int32_t stride1,
                                       int32_t offset2,
                                       int32_t stride2) {
  struct sl_host_drm* host = wl_resource_get_user_data(resource);
  struct zwp_linux_buffer_params_v1* buffer_params;

  assert(name >= 0);
  assert(!offset1);
  assert(!stride1);
  assert(!offset2);
  assert(!stride2);

  buffer_params =
      zwp_linux_dmabuf_v1_create_params(host->ctx->linux_dmabuf->internal);
  zwp_linux_buffer_params_v1_add(buffer_params, name, 0, offset0, stride0, 0,
                                 0);
  sl_create_host_buffer(client, id,
                        zwp_linux_buffer_params_v1_create_immed(
                            buffer_params, width, height, format, 0),
                        width, height);
  zwp_linux_buffer_params_v1_destroy(buffer_params);
  close(name);
}

static const struct wl_drm_interface sl_drm_implementation = {
    sl_drm_authenticate, sl_drm_create_buffer, sl_drm_create_planar_buffer,
    sl_drm_create_prime_buffer};

static void sl_destroy_host_drm(struct wl_resource* resource) {
  struct sl_host_drm* host = wl_resource_get_user_data(resource);

  zwp_linux_dmabuf_v1_destroy(host->linux_dmabuf_proxy);
  wl_callback_destroy(host->callback);
  wl_resource_set_user_data(resource, NULL);
  free(host);
}

static void sl_drm_format(void* data,
                          struct zwp_linux_dmabuf_v1* linux_dmabuf,
                          uint32_t format) {
  struct sl_host_drm* host = zwp_linux_dmabuf_v1_get_user_data(linux_dmabuf);

  switch (format) {
    case WL_DRM_FORMAT_RGB565:
    case WL_DRM_FORMAT_ARGB8888:
    case WL_DRM_FORMAT_ABGR8888:
    case WL_DRM_FORMAT_XRGB8888:
    case WL_DRM_FORMAT_XBGR8888:
      wl_drm_send_format(host->resource, format);
    default:
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

static void sl_drm_callback_done(void* data,
                                 struct wl_callback* callback,
                                 uint32_t serial) {
  struct sl_host_drm* host = wl_callback_get_user_data(callback);

  if (host->ctx->drm_device)
    wl_drm_send_device(host->resource, host->ctx->drm_device);
  if (host->version >= WL_DRM_CREATE_PRIME_BUFFER_SINCE_VERSION)
    wl_drm_send_capabilities(host->resource, WL_DRM_CAPABILITY_PRIME);
}

static const struct wl_callback_listener sl_drm_callback_listener = {
    sl_drm_callback_done};

static void sl_bind_host_drm(struct wl_client* client,
                             void* data,
                             uint32_t version,
                             uint32_t id) {
  struct sl_context* ctx = (struct sl_context*)data;
  struct sl_host_drm* host;

  host = malloc(sizeof(*host));
  assert(host);
  host->ctx = ctx;
  host->version = MIN(version, 2);
  host->resource =
      wl_resource_create(client, &wl_drm_interface, host->version, id);
  wl_resource_set_implementation(host->resource, &sl_drm_implementation, host,
                                 sl_destroy_host_drm);

  host->linux_dmabuf_proxy = wl_registry_bind(
      wl_display_get_registry(ctx->display), ctx->linux_dmabuf->id,
      &zwp_linux_dmabuf_v1_interface, ctx->linux_dmabuf->version);
  zwp_linux_dmabuf_v1_set_user_data(host->linux_dmabuf_proxy, host);
  zwp_linux_dmabuf_v1_add_listener(host->linux_dmabuf_proxy,
                                   &sl_linux_dmabuf_listener, host);

  host->callback = wl_display_sync(ctx->display);
  wl_callback_set_user_data(host->callback, host);
  wl_callback_add_listener(host->callback, &sl_drm_callback_listener, host);
}

struct sl_global* sl_drm_global_create(struct sl_context* ctx) {
  assert(ctx->linux_dmabuf);

  // Early out if DMABuf protocol version is not sufficient.
  if (ctx->linux_dmabuf->version < 2)
    return NULL;

  return sl_global_create(ctx, &wl_drm_interface, 1, ctx, sl_bind_host_drm);
}