/*
 * Copyright (C) 2013-2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "CameraBuffer"
#include "LogHelper.h"
#include <sys/mman.h>
#include "PlatformData.h"
#include "CameraBuffer.h"
#include "CameraStream.h"
#include "Camera3GFXFormat.h"
#include <unistd.h>
#include <sync/sync.h>

NAMESPACE_DECLARATION {
////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
////////////////////////////////////////////////////////////////////

/**
 * CameraBuffer
 *
 * Default constructor
 * This constructor is used when we pre-allocate the CameraBuffer object
 * The initialization will be done as a second stage wit the method
 * init(), where we initialize the wrapper with the gralloc buffer provided by
 * the framework
 */
CameraBuffer::CameraBuffer() :  mWidth(0),
                                mHeight(0),
                                mSize(0),
                                mFormat(0),
                                mV4L2Fmt(0),
                                mStride(0),
                                mInit(false),
                                mLocked(false),
                                mType(BUF_TYPE_HANDLE),
                                mOwner(nullptr),
                                mDataPtr(nullptr),
                                mRequestID(0),
                                mCameraId(0)
{
    LOG1("%s default constructor for buf %p", __FUNCTION__, this);
    CLEAR(mUserBuffer);
    CLEAR(mTimestamp);
    mUserBuffer.release_fence = -1;
    mUserBuffer.acquire_fence = -1;

}

/**
 * CameraBuffer
 *
 * Constructor for buffers allocated using MemoryUtils::allocateHeapBuffer
 *
 * \param w [IN] width
 * \param h [IN] height
 * \param s [IN] stride
 * \param v4l2fmt [IN] V4l2 format
 * \param usrPtr [IN] Data pointer
 * \param cameraId [IN] id of camera being used
 * \param dataSizeOverride [IN] buffer size input. Default is 0 and frameSize()
                                is used in that case.
 */
CameraBuffer::CameraBuffer(int w,
                           int h,
                           int s,
                           int v4l2fmt,
                           void* usrPtr,
                           int cameraId,
                           int dataSizeOverride):
        mWidth(w),
        mHeight(h),
        mSize(0),
        mFormat(0),
        mV4L2Fmt(v4l2fmt),
        mStride(s),
        mInit(false),
        mLocked(true),
        mType(BUF_TYPE_MALLOC),
        mOwner(nullptr),
        mDataPtr(nullptr),
        mRequestID(0),
        mCameraId(cameraId),
        mHandle(nullptr),
        mHandlePtr(nullptr),
        mGbmBufferMapper(nullptr)
{
    if (usrPtr != nullptr) {
        mDataPtr = usrPtr;
        mInit = true;
        mSize = dataSizeOverride ? dataSizeOverride : frameSize(mV4L2Fmt, mStride, mHeight);
        mFormat = v4L2Fmt2GFXFmt(v4l2fmt);
    } else {
        LOGE("Tried to initialize a buffer with nullptr ptr!!");
    }
    CLEAR(mUserBuffer);
    CLEAR(mTimestamp);
    mUserBuffer.release_fence = -1;
    mUserBuffer.acquire_fence = -1;
}

/**
 * CameraBuffer
 *
 * Constructor for buffers allocated using mmap
 *
 * \param fd [IN] File descriptor to map
 * \param length [IN] amount of data to map
 * \param v4l2fmt [IN] Pixel format in V4L2 enum
 * \param offset [IN] offset from the begining of the file (mmap param)
 * \param prot [IN] memory protection (mmap param)
 * \param flags [IN] flags (mmap param)
 *
 * Success of the mmap can be queried by checking the size of the resulting
 * buffer
 */
CameraBuffer::CameraBuffer(int fd, int length, int v4l2fmt, int offset,
                           int prot, int flags):
        mWidth(1),
        mHeight(length),
        mSize(0),
        mFormat(0),
        mV4L2Fmt(v4l2fmt),
        mStride(1),
        mInit(false),
        mLocked(false),
        mType(BUF_TYPE_MMAP),
        mOwner(nullptr),
        mDataPtr(nullptr),
        mRequestID(0),
        mCameraId(-1),
        mHandle(nullptr),
        mHandlePtr(nullptr),
        mGbmBufferMapper(nullptr)
{
    mLocked = true;
    mInit = true;
    mSize = length;
    CLEAR(mUserBuffer);
    CLEAR(mTimestamp);
    mUserBuffer.release_fence = -1;
    mUserBuffer.acquire_fence = -1;

    mDataPtr = mmap(nullptr, length, prot, flags, fd, offset);
    if (CC_UNLIKELY(mDataPtr == MAP_FAILED)) {
        LOGE("Failed to MMAP the buffer %s", strerror(errno));
        mDataPtr = nullptr;
        return;
    }
    LOG1("mmaped address for  %p length %d", mDataPtr, mSize);
}

/**
 * init
 *
 * Constructor to wrap a camera3_stream_buffer
 *
 * \param aBuffer [IN] camera3_stream_buffer buffer
 */
status_t CameraBuffer::init(const camera3_stream_buffer *aBuffer, int cameraId)
{
    mType = BUF_TYPE_HANDLE;
    mGbmBufferMapper = arc::CameraBufferMapper::GetInstance();
    mHandle = *aBuffer->buffer;
    mHandlePtr = aBuffer->buffer;
    mWidth = aBuffer->stream->width;
    mHeight = aBuffer->stream->height;
    mFormat = aBuffer->stream->format;
    mV4L2Fmt = mGbmBufferMapper->GetV4L2PixelFormat(mHandle);
    // Use actual width from platform native handle for stride
    mStride = mGbmBufferMapper->GetPlaneStride(*aBuffer->buffer, 0);
    mSize = 0;
    mLocked = false;
    mOwner = static_cast<CameraStream*>(aBuffer->stream->priv);
    mInit = true;
    mDataPtr = nullptr;
    mUserBuffer = *aBuffer;
    mUserBuffer.release_fence = -1;
    mCameraId = cameraId;
    LOGD("@%s, mHandle:%p, mFormat:%d, mWidth:%d, mHeight:%d, mStride:%d",
        __FUNCTION__, mHandle, mFormat, mWidth, mHeight, mStride);

    int ret = mGbmBufferMapper->Register(mHandle);
    if (ret) {
        LOGE("@%s: call Register fail, mHandle:%p, ret:%d", __FUNCTION__, mHandle, ret);
        return UNKNOWN_ERROR;
    }

    /* TODO: add some consistency checks here and return an error */
    return NO_ERROR;
}

CameraBuffer::~CameraBuffer()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    if (mInit) {
        switch(mType) {
        case BUF_TYPE_MALLOC:
            free(mDataPtr);
            mDataPtr = nullptr;
            break;
        case BUF_TYPE_MMAP:
            if (mDataPtr != nullptr)
                munmap(mDataPtr, mSize);
            mDataPtr = nullptr;
            mSize = 0;
            break;
        case BUF_TYPE_HANDLE:
            break;
        default:
            break;
        }
    }
    LOG1("%s destroying buf %p", __FUNCTION__, this);
}

status_t CameraBuffer::waitOnAcquireFence()
{
    const int WAIT_TIME_OUT_MS = 300;
    const int BUFFER_READY = -1;
    if (mUserBuffer.acquire_fence != BUFFER_READY) {
        LOG2("%s: Fence in HAL is %d", __FUNCTION__, mUserBuffer.acquire_fence);
        int ret = sync_wait(mUserBuffer.acquire_fence, WAIT_TIME_OUT_MS);
        if (ret) {
            mUserBuffer.release_fence = mUserBuffer.acquire_fence;
            LOGE("Buffer sync_wait fail!");
        } else {
            close(mUserBuffer.acquire_fence);
        }
        mUserBuffer.acquire_fence = BUFFER_READY;

        if (ret)
            return TIMED_OUT;
    }
    return NO_ERROR;
}

/**
 * getFence
 *
 * return the fecne to request result
 */
status_t CameraBuffer::getFence(camera3_stream_buffer* buf)
{
    if (!buf)
        return BAD_VALUE;

    buf->acquire_fence = mUserBuffer.acquire_fence;
    buf->release_fence = mUserBuffer.release_fence;

    return NO_ERROR;
}


/**
 * lock
 *
 * lock the gralloc buffer with specified flags
 *
 * \param aBuffer [IN] int flags
 */
status_t CameraBuffer::lock(int flags)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mDataPtr = nullptr;
    mSize = 0;
    int ret = 0;
    uint32_t planeNum = mGbmBufferMapper->GetNumPlanes(mHandle);
    LOG2("@%s, planeNum:%d, mHandle:%p, mFormat:%d", __FUNCTION__, planeNum, mHandle, mFormat);

    if (planeNum == 1) {
        void* data = nullptr;
        ret = (mFormat == HAL_PIXEL_FORMAT_BLOB)
                ? mGbmBufferMapper->Lock(mHandle, 0, 0, 0, mStride, 1, &data)
                : mGbmBufferMapper->Lock(mHandle, 0, 0, 0, mWidth, mHeight, &data);
        if (ret) {
            LOGE("@%s: call Lock fail, mHandle:%p", __FUNCTION__, mHandle);
            return UNKNOWN_ERROR;
        }
        mDataPtr = data;
    } else if (planeNum > 1) {
        struct android_ycbcr ycbrData;
        ret = mGbmBufferMapper->LockYCbCr(mHandle, 0, 0, 0, mWidth, mHeight, &ycbrData);
        if (ret) {
            LOGE("@%s: call LockYCbCr fail, mHandle:%p", __FUNCTION__, mHandle);
            return UNKNOWN_ERROR;
        }
        mDataPtr = ycbrData.y;
    } else {
        LOGE("ERROR @%s: planeNum is 0", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    if (ret) {
        LOGE("ERROR @%s: Failed to lock buffer! %d", __FUNCTION__, ret);
        return UNKNOWN_ERROR;
    }

    for (int i = 0; i < planeNum; i++) {
        mSize += mGbmBufferMapper->GetPlaneSize(mHandle, i);
    }
    LOG2("@%s, mDataPtr:%p, mSize:%d", __FUNCTION__, mDataPtr, mSize);
    if (!mSize) {
        LOGE("ERROR @%s: Failed to GetPlaneSize, it's 0", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    mLocked = true;

    return NO_ERROR;
}

status_t CameraBuffer::lock()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status;
    int lockMode;

    if (!mInit) {
        LOGE("@%s: Error: Cannot lock now this buffer, not initialized", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (mType == BUF_TYPE_MALLOC) {
         mLocked = true;
         return NO_ERROR;
    }

    if (mLocked) {
        LOGE("@%s: Error: Cannot lock buffer from stream(%d), already locked",
             __FUNCTION__,mOwner->seqNo());
        return INVALID_OPERATION;
    }

    lockMode = mOwner->usage() & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK |
        GRALLOC_USAGE_HW_CAMERA_MASK);
    if (!lockMode) {
        LOGW("@%s:trying to lock a buffer with no flags", __FUNCTION__);
        return INVALID_OPERATION;
    }

    status = lock(lockMode);

    return status;
}

status_t CameraBuffer::unlock()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    if (mLocked && mType == BUF_TYPE_MALLOC) {
         mLocked = false;
         return NO_ERROR;
    }

    if (mLocked) {
        int ret = mGbmBufferMapper->Unlock(mHandle);
        if (ret) {
            LOGE("@%s: call Unlock fail, mHandle:%p, ret:%d", __FUNCTION__, mHandle, ret);
            return UNKNOWN_ERROR;
        }

        ret = mGbmBufferMapper->Deregister(mHandle);
        if (ret) {
            LOGE("@%s: call Deregister fail, mHandle:%p, ret:%d", __FUNCTION__, mHandle, ret);
            return UNKNOWN_ERROR;
        }
        mLocked = false;
        return NO_ERROR;
    }
    LOGW("@%s:trying to unlock a buffer that is not locked", __FUNCTION__);
    return INVALID_OPERATION;
}

void CameraBuffer::dump()
{
    if (mInit) {
        LOG1("Buffer dump: handle %p: locked :%d: dataPtr:%p",
            (void*)&mHandle, mLocked, mDataPtr);
    } else {
        LOG1("Buffer dump: Buffer not initialized");
    }
}

void CameraBuffer::dumpImage(const int type, const char *name) const
{
    if (CC_UNLIKELY(LogHelper::isDumpTypeEnable(type))) {
        dumpImage(name);
    }
}

void CameraBuffer::dumpImage(const char *name) const
{
    dumpImage(mDataPtr, mSize, mWidth, mHeight, name);
}


void CameraBuffer::dumpImage(const void *data, const int size, int width, int height,
                                 const char *name) const
{
    static unsigned int count = 0;
    count++;

    std::string fileName(gDumpPath);
    fileName += "dump_" + std::to_string(width) +"x" + std::to_string(height)
                             + "_" + std::to_string(count)
                             + "_" + name
                             + "_" + std::to_string(mRequestID);
    LOG2("%s filename is %s", __FUNCTION__, fileName.data());

    FILE *fp = fopen (fileName.data(), "w+");
    if (fp == nullptr) {
        LOGE("open file failed");
        return;
    }
    LOG1("Begin write image %s", fileName.data());

    if ((fwrite(data, size, 1, fp)) != 1)
        LOGW("Error or short count writing %d bytes to %s", size, fileName.data());
    fclose (fp);
}

/**
 * Utility methods to allocate CameraBuffers from HEAP or Gfx memory
 */
namespace MemoryUtils {

/**
 * Allocates the memory needed to store the image described by the parameters
 * passed during construction
 */
std::shared_ptr<CameraBuffer>
allocateHeapBuffer(int w,
                   int h,
                   int s,
                   int v4l2Fmt,
                   int cameraId,
                   int dataSizeOverride)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    void *dataPtr;

    int dataSize = dataSizeOverride ? dataSizeOverride : frameSize(v4l2Fmt, s, h);
    LOG1("@%s, dataSize:%d", __FUNCTION__, dataSize);

    int ret = posix_memalign(&dataPtr, sysconf(_SC_PAGESIZE), dataSize);
    if (dataPtr == nullptr || ret != 0) {
        LOGE("Could not allocate heap camera buffer of size %d", dataSize);
        return nullptr;
    }

    return std::shared_ptr<CameraBuffer>(new CameraBuffer(w, h, s, v4l2Fmt, dataPtr, cameraId, dataSizeOverride));
}

} // namespace MemoryUtils

} NAMESPACE_DECLARATION_END
