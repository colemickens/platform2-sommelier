// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/libvda/gpu/gpu_vda_impl.h"

#include <fcntl.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/posix/eintr_wrapper.h>
#include <base/single_thread_task_runner.h>
#include <base/synchronization/waitable_event.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <mojo/public/cpp/system/platform_handle.h>
#include <sys/eventfd.h>

#include "arc/vm/libvda/gbm_util.h"

namespace arc {
namespace {

// TODO(alexlau): Query for this instead of hard coding.
constexpr vda_input_format_t kInputFormats[] = {
    {VP8PROFILE_MIN /* profile */, 2 /* min_width */, 2 /* min_height */,
     1920 /* max_width */, 1080 /* max_height */},
    {VP9PROFILE_PROFILE0 /* profile */, 2 /* min_width */, 2 /* min_height */,
     1920 /* max_width */, 1080 /* max_height */},
    {H264PROFILE_MAIN /* profile */, 2 /* min_width */, 2 /* min_height */,
     1920 /* max_width */, 1080 /* max_height */}};

// Minimum required version of VideoAcceleratorFactory interface.
// Set to 6 which is when CreateDecodeAccelerator was introduced.
constexpr uint32_t kRequiredVideoAcceleratorFactoryMojoVersion = 6;

// Global GpuVdaImpl object.
GpuVdaImpl* active_impl = nullptr;

void RunTaskOnThread(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                     base::OnceClosure task) {
  if (task_runner->BelongsToCurrentThread()) {
    LOG(WARNING) << "RunTaskOnThread called on target thread.";
    std::move(task).Run();
    return;
  }

  base::WaitableEvent task_complete_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure task, base::WaitableEvent* task_complete_event) {
            std::move(task).Run();
            task_complete_event->Signal();
          },
          std::move(task), &task_complete_event));
  task_complete_event.Wait();
}

inline vda_result_t ConvertResult(
    arc::mojom::VideoDecodeAccelerator::Result error) {
  switch (error) {
    case arc::mojom::VideoDecodeAccelerator::Result::SUCCESS:
      return SUCCESS;
    case arc::mojom::VideoDecodeAccelerator::Result::ILLEGAL_STATE:
      return ILLEGAL_STATE;
    case arc::mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT:
      return INVALID_ARGUMENT;
    case arc::mojom::VideoDecodeAccelerator::Result::UNREADABLE_INPUT:
      return UNREADABLE_INPUT;
    case arc::mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE:
      return PLATFORM_FAILURE;
    case arc::mojom::VideoDecodeAccelerator::Result::INSUFFICIENT_RESOURCES:
      return INSUFFICIENT_RESOURCES;
    case arc::mojom::VideoDecodeAccelerator::Result::CANCELLED:
      return CANCELLED;
    default:
      DLOG(ERROR) << "Unknown error code: " << error;
      return PLATFORM_FAILURE;
  }
}

inline arc::mojom::HalPixelFormat ConvertPixelFormatToHalPixelFormat(
    vda_pixel_format_t format) {
  switch (format) {
    case YV12:
      return arc::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YV12;
    case NV12:
      return arc::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_NV12;
    default:
      NOTREACHED();
  }
}

inline arc::mojom::VideoCodecProfile ConvertVdaProfileToMojoProfile(
    vda_profile_t profile) {
  switch (profile) {
    case H264PROFILE_MIN:
      return arc::mojom::VideoCodecProfile::H264PROFILE_MIN;
    case H264PROFILE_MAIN:
      return arc::mojom::VideoCodecProfile::H264PROFILE_MAIN;
    case H264PROFILE_EXTENDED:
      return arc::mojom::VideoCodecProfile::H264PROFILE_EXTENDED;
    case H264PROFILE_HIGH:
      return arc::mojom::VideoCodecProfile::H264PROFILE_HIGH;
    case H264PROFILE_HIGH10PROFILE:
      return arc::mojom::VideoCodecProfile::H264PROFILE_HIGH10PROFILE;
    case H264PROFILE_HIGH422PROFILE:
      return arc::mojom::VideoCodecProfile::H264PROFILE_HIGH422PROFILE;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return arc::mojom::VideoCodecProfile::
          H264PROFILE_HIGH444PREDICTIVEPROFILE;
    case H264PROFILE_SCALABLEBASELINE:
      return arc::mojom::VideoCodecProfile::H264PROFILE_SCALABLEBASELINE;
    case H264PROFILE_SCALABLEHIGH:
      return arc::mojom::VideoCodecProfile::H264PROFILE_SCALABLEHIGH;
    case H264PROFILE_STEREOHIGH:
      return arc::mojom::VideoCodecProfile::H264PROFILE_STEREOHIGH;
    case H264PROFILE_MULTIVIEWHIGH:
      return arc::mojom::VideoCodecProfile::H264PROFILE_MULTIVIEWHIGH;
    case VP8PROFILE_MIN:
      return arc::mojom::VideoCodecProfile::VP8PROFILE_MIN;
    case VP9PROFILE_MIN:
      return arc::mojom::VideoCodecProfile::VP9PROFILE_MIN;
    case VP9PROFILE_PROFILE1:
      return arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE1;
    case VP9PROFILE_PROFILE2:
      return arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE2;
    case VP9PROFILE_PROFILE3:
      return arc::mojom::VideoCodecProfile::VP9PROFILE_PROFILE3;
    case HEVCPROFILE_MIN:
      return arc::mojom::VideoCodecProfile::HEVCPROFILE_MIN;
    case HEVCPROFILE_MAIN10:
      return arc::mojom::VideoCodecProfile::HEVCPROFILE_MAIN10;
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return arc::mojom::VideoCodecProfile::HEVCPROFILE_MAIN_STILL_PICTURE;
    case DOLBYVISION_PROFILE0:
      return arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE0;
    case DOLBYVISION_PROFILE4:
      return arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE4;
    case DOLBYVISION_PROFILE5:
      return arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE5;
    case DOLBYVISION_PROFILE7:
      return arc::mojom::VideoCodecProfile::DOLBYVISION_PROFILE7;
    case THEORAPROFILE_MIN:
      return arc::mojom::VideoCodecProfile::THEORAPROFILE_MIN;
    case AV1PROFILE_PROFILE_MAIN:
      return arc::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
    case AV1PROFILE_PROFILE_HIGH:
      return arc::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_HIGH;
    case AV1PROFILE_PROFILE_PRO:
      return arc::mojom::VideoCodecProfile::AV1PROFILE_PROFILE_PRO;
    default:
      return arc::mojom::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

std::vector<vda_pixel_format_t> GetOutputPixelFormats() {
  // TODO(alexlau): don't hardcode this path, see
  // http://cs/chromeos_public/src/platform/minigbm/cros_gralloc/cros_gralloc_driver.cc?l=29&rcl=cc35e699f36cce0f0b3a130b0d6ce4e2a393b373
  base::ScopedFD fd(HANDLE_EINTR(open("/dev/dri/renderD128", O_RDWR)));
  if (!fd.is_valid()) {
    LOG(ERROR) << "Could not open vgem.";
    return {};
  }

  arc::ScopedGbmDevicePtr device(gbm_create_device(fd.get()));
  if (!device.get()) {
    LOG(ERROR) << "Could not create gbm device.";
    return {};
  }

  std::vector<vda_pixel_format_t> formats;
  constexpr vda_pixel_format_t pixel_formats[] = {YV12, NV12};
  for (vda_pixel_format_t pixel_format : pixel_formats) {
    uint32_t gbm_format = ConvertPixelFormatToGbmFormat(pixel_format);
    if (gbm_format == 0u)
      continue;
    if (!gbm_device_is_format_supported(
            device.get(), gbm_format,
            GBM_BO_USE_TEXTURING | GBM_BO_USE_HW_VIDEO_DECODER)) {
      DLOG(INFO) << "Not supported: " << pixel_format;
      continue;
    }
    formats.push_back(pixel_format);
  }
  return formats;
}

bool CheckValidOutputFormat(vda_pixel_format_t format, size_t num_planes) {
  switch (format) {
    case NV12:
      if (num_planes != 2) {
        LOG(ERROR) << "Invalid number of planes for NV12 format, expected 2 "
                      "but received "
                   << num_planes;
        return false;
      }
      break;
    case YV12:
      if (num_planes != 3) {
        LOG(ERROR) << "Invalid number of planes for YV12 format, expected 3 "
                      "but received "
                   << num_planes;
        return false;
      }
      break;
    default:
      LOG(WARNING) << "Unexpected format: " << format;
      return false;
  }
  return true;
}

// GpuVdaContext is the GPU decode session context created by GpuVdaImpl which
// handles all mojo VideoDecodeClient invocations and callbacks.
class GpuVdaContext : public VdaContext, arc::mojom::VideoDecodeClient {
 public:
  // Create a new GpuVdaContext. Must be called on |ipc_task_runner|.
  GpuVdaContext(
      const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner,
      arc::mojom::VideoDecodeAcceleratorPtr vda_ptr);
  ~GpuVdaContext();

  using InitializeCallback = base::OnceCallback<void(vda_result_t)>;

  // Initializes the VDA context object. When complete, callback is called with
  // the result. Must be called on |ipc_task_runner_|.
  void Initialize(vda_profile_t profile, InitializeCallback callback);

  // VdaContext overrides.
  vda_result_t Decode(int32_t bitstream_id,
                      base::ScopedFD fd,
                      uint32_t offset,
                      uint32_t bytes_used) override;
  vda_result_t SetOutputBufferCount(size_t num_output_buffers) override;
  vda_result_t UseOutputBuffer(int32_t picture_buffer_id,
                               vda_pixel_format_t format,
                               base::ScopedFD fd,
                               size_t num_planes,
                               video_frame_plane_t* planes) override;
  vda_result_t Reset() override;
  vda_result_t Flush() override;

  // arc::mojom::VideoDecodeClient overrides.
  void ProvidePictureBuffers(arc::mojom::PictureBufferFormatPtr format_ptr,
                             arc::mojom::RectPtr visible_rect_ptr) override;
  void ProvidePictureBuffersDeprecated(
      arc::mojom::PictureBufferFormatPtr format_ptr) override;
  void PictureReady(arc::mojom::PicturePtr) override;
  void NotifyError(arc::mojom::VideoDecodeAccelerator::Result error) override;
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_id) override;

 private:
  // Callback invoked when VideoDecodeAcceleratorPtr::Initialize completes.
  void OnInitialized(InitializeCallback callback,
                     arc::mojom::VideoDecodeAccelerator::Result result);

  // Callback for VideoDecodeAcceleratorPtr connection errors.
  void OnVdaError(uint32_t custom_reason, const std::string& description);

  // Callback for VideoDecodeClientPtr connection errors.
  void OnVdaClientError(uint32_t custom_reason, const std::string& description);

  // Callback invoked when VideoDecodeAcceleratorPtr::Reset completes.
  void OnResetDone(arc::mojom::VideoDecodeAccelerator::Result result);

  // Callback invoked when VideoDecodeAcceleratorPtr::Flush completes.
  void OnFlushDone(arc::mojom::VideoDecodeAccelerator::Result result);

  // Executes a decode. Called on |ipc_task_runner_|.
  void DecodeOnIpcThread(int32_t bitstream_id,
                         base::ScopedFD fd,
                         uint32_t offset,
                         uint32_t bytes_used);

  // Handles a SetOutputBuffer request by invoking a VideoDecodeAccelerator mojo
  // function. Called on |ipc_task_runner_|.
  void SetOutputBufferCountOnIpcThread(size_t num_output_buffers);

  // Handles a UseOutputBuffer request by invoking a VideoDecodeAccelerator mojo
  // function. Called on |ipc_task_runner_|.
  void UseOutputBufferOnIpcThread(int32_t picture_buffer_id,
                                  vda_pixel_format_t format,
                                  base::ScopedFD fd,
                                  std::vector<video_frame_plane_t> planes);

  // Handles a Reset request by invoking a VideoDecodeAccelerator mojo function.
  // Called on |ipc_task_runner_|.
  void ResetOnIpcThread();

  // Handles a SetOutputBuffer request by invoking a VideoDecodeAccelerator mojo
  // function. Called on |ipc_task_runner_|.
  void FlushOnIpcThread();

  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
  // TODO(alexlau): Use THREAD_CHECKER macro after libchrome uprev
  // (crbug.com/909719).
  base::ThreadChecker ipc_thread_checker_;
  arc::mojom::VideoDecodeAcceleratorPtr vda_ptr_;
  mojo::Binding<arc::mojom::VideoDecodeClient> binding_;

  std::set<int32_t> decoding_bitstream_ids_;

  DISALLOW_COPY_AND_ASSIGN(GpuVdaContext);
};

GpuVdaContext::GpuVdaContext(
    const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner,
    arc::mojom::VideoDecodeAcceleratorPtr vda_ptr)
    : ipc_task_runner_(std::move(ipc_task_runner)),
      vda_ptr_(std::move(vda_ptr)),
      binding_(this) {
  // Since ipc_thread_checker_ binds to whichever thread it's created on, check
  // that we're on the correct thread first using BelongsToCurrentThread.
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  vda_ptr_.set_connection_error_with_reason_handler(
      base::BindRepeating(&GpuVdaContext::OnVdaError, base::Unretained(this)));

  DLOG(INFO) << "Created new GPU context";
}

void GpuVdaContext::Initialize(vda_profile_t profile,
                               InitializeCallback callback) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  arc::mojom::VideoDecodeClientPtr client_ptr;
  binding_.Bind(mojo::MakeRequest(&client_ptr));
  binding_.set_connection_error_with_reason_handler(base::BindRepeating(
      &GpuVdaContext::OnVdaClientError, base::Unretained(this)));

  arc::mojom::VideoDecodeAcceleratorConfigPtr config =
      arc::mojom::VideoDecodeAcceleratorConfig::New();
  // TODO(alexlau): Think about how to specify secure mode dynamically.
  config->secure_mode = false;
  config->profile = ConvertVdaProfileToMojoProfile(profile);

  vda_ptr_->Initialize(
      std::move(config), std::move(client_ptr),
      base::BindRepeating(&GpuVdaContext::OnInitialized, base::Unretained(this),
                          base::Passed(std::move(callback))));
}

void GpuVdaContext::OnInitialized(
    InitializeCallback callback,
    arc::mojom::VideoDecodeAccelerator::Result result) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  std::move(callback).Run(ConvertResult(result));
}

GpuVdaContext::~GpuVdaContext() {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
}

void GpuVdaContext::OnVdaError(uint32_t custom_reason,
                               const std::string& description) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DLOG(ERROR) << "VideoDecodeAccelerator mojo connection error. custom_reason="
              << custom_reason << " description=" << description;
}

void GpuVdaContext::OnVdaClientError(uint32_t custom_reason,
                                     const std::string& description) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DLOG(ERROR) << "VideoDecodeClient mojo connection error. custom_reason="
              << custom_reason << " description=" << description;
}

vda_result_t GpuVdaContext::Decode(int32_t bitstream_id,
                                   base::ScopedFD fd,
                                   uint32_t offset,
                                   uint32_t bytes_used) {
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVdaContext::DecodeOnIpcThread, base::Unretained(this),
                     bitstream_id, std::move(fd), offset, bytes_used));
  return SUCCESS;
}

vda_result_t GpuVdaContext::SetOutputBufferCount(size_t num_output_buffers) {
  ipc_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuVdaContext::SetOutputBufferCountOnIpcThread,
                                base::Unretained(this), num_output_buffers));
  return SUCCESS;
}

void GpuVdaContext::SetOutputBufferCountOnIpcThread(size_t num_output_buffers) {
  vda_ptr_->AssignPictureBuffers(num_output_buffers);
}

void GpuVdaContext::DecodeOnIpcThread(int32_t bitstream_id,
                                      base::ScopedFD fd,
                                      uint32_t offset,
                                      uint32_t bytes_used) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());

  mojo::ScopedHandle handle_fd = mojo::WrapPlatformFile(fd.release());
  if (!handle_fd.is_valid()) {
    LOG(ERROR) << "Invalid bitstream handle.";
    return;
  }

  decoding_bitstream_ids_.insert(bitstream_id);

  arc::mojom::BitstreamBufferPtr buf = arc::mojom::BitstreamBuffer::New();
  buf->bitstream_id = bitstream_id;
  buf->handle_fd = std::move(handle_fd);
  buf->offset = offset;
  buf->bytes_used = bytes_used;

  vda_ptr_->Decode(std::move(buf));
}

vda_result_t GpuVdaContext::UseOutputBuffer(int32_t picture_buffer_id,
                                            vda_pixel_format_t format,
                                            base::ScopedFD fd,
                                            size_t num_planes,
                                            video_frame_plane_t* planes) {
  if (!CheckValidOutputFormat(format, num_planes))
    return INVALID_ARGUMENT;

  // Move semantics don't seem to work with mojo pointers so copy the
  // video_frame_plane_t objects and handle in the ipc thread. This allows
  // |planes| ownership to be retained by the user.
  std::vector<video_frame_plane_t> planes_vector(planes, planes + num_planes);
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVdaContext::UseOutputBufferOnIpcThread,
                     base::Unretained(this), picture_buffer_id, format,
                     std::move(fd), std::move(planes_vector)));
  return SUCCESS;
}

void GpuVdaContext::UseOutputBufferOnIpcThread(
    int32_t picture_buffer_id,
    vda_pixel_format_t format,
    base::ScopedFD fd,
    std::vector<video_frame_plane_t> planes) {
  mojo::ScopedHandle handle_fd = mojo::WrapPlatformFile(fd.release());
  if (!handle_fd.is_valid()) {
    LOG(ERROR) << "Invalid output buffer handle.";
    return;
  }

  std::vector<arc::mojom::VideoFramePlanePtr> mojo_planes;
  for (const auto& plane : planes) {
    arc::mojom::VideoFramePlanePtr mojo_plane =
        arc::mojom::VideoFramePlane::New();
    mojo_plane->offset = plane.offset;
    mojo_plane->stride = plane.stride;
    mojo_planes.push_back(std::move(mojo_plane));
  }

  vda_ptr_->ImportBufferForPicture(
      picture_buffer_id, ConvertPixelFormatToHalPixelFormat(format),
      std::move(handle_fd), std::move(mojo_planes));
}

vda_result GpuVdaContext::Reset() {
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVdaContext::ResetOnIpcThread, base::Unretained(this)));
  return SUCCESS;
}

void GpuVdaContext::ResetOnIpcThread() {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  vda_ptr_->Reset(
      base::BindRepeating(&GpuVdaContext::OnResetDone, base::Unretained(this)));
}

void GpuVdaContext::OnResetDone(
    arc::mojom::VideoDecodeAccelerator::Result result) {
  DispatchResetResponse(ConvertResult(result));
}

vda_result GpuVdaContext::Flush() {
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuVdaContext::FlushOnIpcThread, base::Unretained(this)));
  return SUCCESS;
}

void GpuVdaContext::FlushOnIpcThread() {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  vda_ptr_->Flush(
      base::BindRepeating(&GpuVdaContext::OnFlushDone, base::Unretained(this)));
}

void GpuVdaContext::OnFlushDone(
    arc::mojom::VideoDecodeAccelerator::Result result) {
  DispatchFlushResponse(ConvertResult(result));
}

// VideoDecodeClient implementation function.
void GpuVdaContext::ProvidePictureBuffers(
    arc::mojom::PictureBufferFormatPtr format_ptr,
    arc::mojom::RectPtr visible_rect_ptr) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DispatchProvidePictureBuffers(
      format_ptr->min_num_buffers, format_ptr->coded_size->width,
      format_ptr->coded_size->height, visible_rect_ptr->left,
      visible_rect_ptr->top, visible_rect_ptr->right, visible_rect_ptr->bottom);
}

void GpuVdaContext::ProvidePictureBuffersDeprecated(
    arc::mojom::PictureBufferFormatPtr format_ptr) {
  NOTIMPLEMENTED();
}

// VideoDecodeClient implementation function.
void GpuVdaContext::PictureReady(arc::mojom::PicturePtr picture_ptr) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DispatchPictureReady(
      picture_ptr->picture_buffer_id, picture_ptr->bitstream_id,
      picture_ptr->crop_rect->left, picture_ptr->crop_rect->top,
      picture_ptr->crop_rect->right, picture_ptr->crop_rect->bottom);
}

// VideoDecodeClient implementation function.
void GpuVdaContext::NotifyError(
    arc::mojom::VideoDecodeAccelerator::Result error) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DispatchNotifyError(ConvertResult(error));
}

// VideoDecodeClient implementation function.
void GpuVdaContext::NotifyEndOfBitstreamBuffer(int32_t bitstream_id) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());

  DispatchNotifyEndOfBitstreamBuffer(bitstream_id);

  if (decoding_bitstream_ids_.erase(bitstream_id) == 0) {
    LOG(ERROR) << "Could not find bitstream id: " << bitstream_id;
    return;
  }
}

}  // namespace

// static
GpuVdaImpl* GpuVdaImpl::Create() {
  if (active_impl) {
    // A instantiated GpuVdaImpl object already exists, return nullptr.
    return nullptr;
  }

  auto impl = std::make_unique<GpuVdaImpl>();
  if (!impl->Initialize()) {
    LOG(ERROR) << "Could not initialize GpuVdaImpl.";
    return nullptr;
  }

  return impl.release();
}

GpuVdaImpl::GpuVdaImpl() : ipc_thread_("MojoIpcThread") {
  CHECK_EQ(active_impl, nullptr);
  active_impl = this;

  mojo::edk::Init();
  CHECK(ipc_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
  mojo::edk::InitIPCSupport(ipc_thread_.task_runner());
}

GpuVdaImpl::~GpuVdaImpl() {
  RunTaskOnThread(
      ipc_thread_.task_runner(),
      base::BindOnce(&GpuVdaImpl::CleanupOnIpcThread, base::Unretained(this)));
  mojo::edk::ShutdownIPCSupport(base::BindRepeating(&base::DoNothing));

  CHECK_EQ(active_impl, this);
  active_impl = nullptr;
}

bool GpuVdaImpl::PopulateCapabilities() {
  capabilities_.num_input_formats = arraysize(kInputFormats);
  capabilities_.input_formats = kInputFormats;

  output_formats_ = GetOutputPixelFormats();
  if (output_formats_.empty())
    return false;

  capabilities_.num_output_formats = output_formats_.size();
  capabilities_.output_formats = output_formats_.data();
  return true;
}

bool GpuVdaImpl::Initialize() {
  if (!PopulateCapabilities())
    return false;

  bool init_success = false;
  RunTaskOnThread(ipc_thread_.task_runner(),
                  base::BindOnce(&GpuVdaImpl::InitializeOnIpcThread,
                                 base::Unretained(this), &init_success));
  return init_success;
}

void GpuVdaImpl::InitializeOnIpcThread(bool* init_success) {
  // Since ipc_thread_checker_ binds to whichever thread it's created on, check
  // that we're on the correct thread first using BelongsToCurrentThread.
  DCHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  // TODO(alexlau): Use DETACH_FROM_THREAD macro after libchrome uprev
  // (crbug.com/909719).
  ipc_thread_checker_.DetachFromThread();
  // TODO(alexlau): Use DCHECK_CALLED_ON_VALID_THREAD macro after libchrome
  // uprev (crbug.com/909719).
  DCHECK(ipc_thread_checker_.CalledOnValidThread());

  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(std::move(opts));
  if (!bus->Connect()) {
    DLOG(ERROR) << "Failed to connect to system bus";
    return;
  }

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      libvda::kLibvdaServiceName, dbus::ObjectPath(libvda::kLibvdaServicePath));
  if (!proxy) {
    // TODO(alexlau): Would this ever start before Chrome such that we should
    //                call WaitForServiceToBeAvailable here?
    DLOG(ERROR) << "Unable to get dbus proxy for "
                << libvda::kLibvdaServiceName;
    return;
  }

  dbus::MethodCall method_call(libvda::kLibvdaServiceInterface,
                               libvda::kProvideMojoConnectionMethod);
  std::unique_ptr<dbus::Response> response(proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT));
  if (!response.get()) {
    DLOG(ERROR) << "Unable to get response from method call "
                << libvda::kProvideMojoConnectionMethod;
    return;
  }

  dbus::MessageReader reader(response.get());

  // Read the mojo pipe FD.
  base::ScopedFD fd;
  if (!reader.PopFileDescriptor(&fd)) {
    DLOG(ERROR) << "Unable to read mojo pipe fd";
    return;
  }
  if (!fd.is_valid()) {
    DLOG(ERROR) << "Received invalid mojo pipe fd";
    return;
  }

  std::string pipe_name;
  if (!reader.PopString(&pipe_name)) {
    DLOG(ERROR) << "Unable to read mojo pipe name.";
    return;
  }

  // Setup the mojo pipe.
  mojo::edk::SetParentPipeHandle(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(fd.release())));
  mojo::ScopedMessagePipeHandle scoped_message_pipe_handle =
      mojo::edk::CreateChildMessagePipe(pipe_name);
  mojo::InterfacePtrInfo<arc::mojom::VideoAcceleratorFactory>
      interface_ptr_info(std::move(scoped_message_pipe_handle),
                         kRequiredVideoAcceleratorFactoryMojoVersion);
  vda_factory_ptr_ = mojo::MakeProxy(std::move(interface_ptr_info));
  vda_factory_ptr_.set_connection_error_with_reason_handler(base::BindRepeating(
      &GpuVdaImpl::OnVdaFactoryError, base::Unretained(this)));

  *init_success = true;
}

void GpuVdaImpl::OnVdaFactoryError(uint32_t custom_reason,
                                   const std::string& description) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  DLOG(ERROR)
      << "VideoDecodeAcceleratorFactory mojo connection error. custom_reason="
      << custom_reason << " description=" << description;
}

VdaContext* GpuVdaImpl::InitDecodeSession(vda_profile_t profile) {
  DCHECK(!ipc_thread_checker_.CalledOnValidThread());

  DLOG(INFO) << "Initializing decode session with profile " << profile;

  base::WaitableEvent init_complete_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  VdaContext* context = nullptr;
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuVdaImpl::InitDecodeSessionOnIpcThread,
                                base::Unretained(this), profile,
                                &init_complete_event, &context));

  init_complete_event.Wait();

  return context;
}

void GpuVdaImpl::InitDecodeSessionOnIpcThread(
    vda_profile_t profile,
    base::WaitableEvent* init_complete_event,
    VdaContext** out_context) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());

  arc::mojom::VideoDecodeAcceleratorPtr vda_ptr;
  vda_factory_ptr_->CreateDecodeAccelerator(mojo::MakeRequest(&vda_ptr));

  std::unique_ptr<GpuVdaContext> context = std::make_unique<GpuVdaContext>(
      ipc_thread_.task_runner(), std::move(vda_ptr));
  GpuVdaContext* context_ptr = context.get();
  context_ptr->Initialize(
      profile,
      base::BindRepeating(
          &GpuVdaImpl::InitDecodeSessionAfterContextInitializedOnIpcThread,
          base::Unretained(this), init_complete_event, out_context,
          base::Passed(std::move(context))));
}

void GpuVdaImpl::InitDecodeSessionAfterContextInitializedOnIpcThread(
    base::WaitableEvent* init_complete_event,
    VdaContext** out_context,
    std::unique_ptr<VdaContext> context,
    vda_result_t result) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());

  if (result == SUCCESS) {
    *out_context = context.release();
  } else {
    DLOG(ERROR) << "Failed to initialize decode session.";
  }
  init_complete_event->Signal();
}

void GpuVdaImpl::CloseDecodeSession(VdaContext* context) {
  DLOG(INFO) << "Closing decode session";
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuVdaImpl::CloseDecodeSessionOnIpcThread,
                                base::Unretained(this), context));
}

void GpuVdaImpl::CloseDecodeSessionOnIpcThread(VdaContext* context) {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  delete context;
}

void GpuVdaImpl::CleanupOnIpcThread() {
  DCHECK(ipc_thread_checker_.CalledOnValidThread());
  if (vda_factory_ptr_.is_bound())
    vda_factory_ptr_.reset();
}

}  // namespace arc
