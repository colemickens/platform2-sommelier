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

#define LOG_TAG "V4L2VideoNode"

#include "LogHelper.h"

#include "v4l2device.h"
#include <linux/media.h>
#include "Camera3V4l2Format.h"
#include <limits.h>
#include <fcntl.h>
#include "UtilityMacros.h"

////////////////////////////////////////////////////////////////////
//                          PUBLIC METHODS
////////////////////////////////////////////////////////////////////

#define MAX_CAMERA_BUFFERS_NUM  32  //MAX_CAMERA_BUFFERS_NUM

NAMESPACE_DECLARATION {
V4L2VideoNode::V4L2VideoNode(const char *name, VideNodeDirection nodeDirection):
                                                        V4L2DeviceBase(name),
                                                        mState(DEVICE_CLOSED),
                                                        mBuffersInDevice(0),
                                                        mFrameCounter(0),
                                                        mInitialSkips(0),
                                                        mDirection(nodeDirection),
                                                        mInBufType(V4L2_BUF_TYPE_VIDEO_CAPTURE),
                                                        mOutBufType(V4L2_BUF_TYPE_VIDEO_OUTPUT),
                                                        mMemoryType(V4L2_MEMORY_USERPTR)
{
    LOG1("@%s: device: %s", __FUNCTION__, name);
    mBufferPool.reserve(MAX_CAMERA_BUFFERS_NUM);
    mSetBufferPool.reserve(MAX_CAMERA_BUFFERS_NUM);
    CLEAR(mConfig);
}

V4L2VideoNode::~V4L2VideoNode()
{
    LOG1("@%s device : %s", __FUNCTION__, mName.c_str());

    /**
     * Do something for the buffer pool ?
     */
}

status_t V4L2VideoNode::open()
{
    status_t status(NO_ERROR);
    status = V4L2DeviceBase::open();
    if (status == NO_ERROR)
        mState = DEVICE_OPEN;

    mBuffersInDevice = 0;
    return status;
}

status_t V4L2VideoNode::close()
{
    status_t status(NO_ERROR);

    if (mState == DEVICE_STARTED) {
        stop();
    }
    if (!mBufferPool.empty()) {
        destroyBufferPool();
    }

    status = V4L2DeviceBase::close();
    if (status == NO_ERROR)
        mState = DEVICE_CLOSED;

    mBuffersInDevice = 0;
    return status;
}

/**
 * queries the capabilities of the device and it does some basic sanity checks
 * based on the direction of the video device node
 *
 * \param cap: [OUT] V4L2 capability structure
 *
 * \return NO_ERROR  if everything went ok
 * \return INVALID_OPERATION if the device was not in correct state
 * \return UNKNOWN_ERROR if IOCTL operation failed
 * \return DEAD_OBJECT if the basic checks for this object failed
 */
status_t V4L2VideoNode::queryCap(struct v4l2_capability *cap)
{
    LOG1("@%s device : %s", __FUNCTION__, mName.c_str());
    int ret(0);

    if (mState != DEVICE_OPEN) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    ret = pioctl(mFd, VIDIOC_QUERYCAP, cap, mName.c_str());

    if (ret < 0) {
        LOGE("VIDIOC_QUERYCAP returned: %d (%s)", ret, strerror(errno));
        return UNKNOWN_ERROR;
    }

    LOG1( "driver:      '%s'", cap->driver);
    LOG1( "card:        '%s'", cap->card);
    LOG1( "bus_info:      '%s'", cap->bus_info);
    LOG1( "version:      %x", cap->version);
    LOG1( "capabilities:      %x", cap->capabilities);

    /* Do some basic sanity check */

    if (mDirection == INPUT_VIDEO_NODE) {
        if (!(cap->capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
           LOGW("No capture devices - But this is an input video node!");
           return DEAD_OBJECT;
        }

        if (!(cap->capabilities & V4L2_CAP_STREAMING)) {
            LOGW("Is not a video streaming device");
            return DEAD_OBJECT;
        }

    } else {
        if (!(cap->capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
            LOGW("No output devices - but this is an output video node!");
            return DEAD_OBJECT;
        }
    }

    return NO_ERROR;
}

status_t V4L2VideoNode::enumerateInputs(struct v4l2_input *anInput)
{
    LOG1("@%s device : %s", __FUNCTION__, mName.c_str());
    int ret(0);

    if (mState == DEVICE_CLOSED) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    ret = pioctl(mFd, VIDIOC_ENUMINPUT, anInput, mName.c_str());
    int errno_copy = errno;

    if (ret < 0) {
        LOGE("VIDIOC_ENUMINPUT failed returned: %d (%s)", ret, strerror(errno_copy));
        if (errno_copy == EINVAL)
            return BAD_INDEX;
        else
            return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t V4L2VideoNode::queryCapturePixelFormats(std::vector<v4l2_fmtdesc> &formats)
{
    LOG1("@%s device : %s", __FUNCTION__, mName.c_str());
    struct v4l2_fmtdesc aFormat;

    if (mState == DEVICE_CLOSED) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    formats.clear();
    CLEAR(aFormat);

    aFormat.index = 0;
    aFormat.type = mInBufType;

    while (pioctl(mFd, VIDIOC_ENUM_FMT , &aFormat, mName.c_str()) == 0) {
        formats.push_back(aFormat);
        aFormat.index++;
    };

    aFormat.index = 0;
    aFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    while (pioctl(mFd, VIDIOC_ENUM_FMT , &aFormat, mName.c_str()) == 0) {
        formats.push_back(aFormat);
        aFormat.index++;
    };

    LOG1("@%s device : %s %zu format retrieved", __FUNCTION__, mName.c_str(), formats.size());
    return NO_ERROR;
}

status_t V4L2VideoNode::setInput(int index)
{
    LOG1("@%s %s", __FUNCTION__, mName.c_str());
    struct v4l2_input input;
    status_t status = NO_ERROR;
    int ret(0);

    if (mState == DEVICE_CLOSED) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    input.index = index;

    ret = pioctl(mFd, VIDIOC_S_INPUT, &input, mName.c_str());

    if (ret < 0) {
       LOGE("VIDIOC_S_INPUT index %d returned: %d (%s)",
           input.index, ret, strerror(errno));
       status = UNKNOWN_ERROR;
    }

    return status;
}

/**
 * Stop the streaming of buffers of a video device
 * This method is basically a stream-off IOCTL but it has a parameter to
 * stop and destroy the current active-buffer-pool
 *
 * After this method completes the device is in state DEVICE_PREPARED
 *
 * \param leavePopulated: boolean to control the state change
 * \return   0 on success
 *          -1 on failure
 */
int V4L2VideoNode::stop(bool keepBuffers)
{
    LOG1("@%s: device = %s", __FUNCTION__, mName.c_str());
    int ret = 0;

    if (mState == DEVICE_STARTED) {
        enum v4l2_buf_type type;
        if (CC_LIKELY(mDirection == INPUT_VIDEO_NODE))
            type = mInBufType;
        else
            type = mOutBufType;

        //stream off
        ret = pioctl(mFd, VIDIOC_STREAMOFF, &type, mName.c_str());
        if (ret < 0) {
            LOGE("VIDIOC_STREAMOFF returned: %d (%s)", ret, strerror(errno));
            return ret;
        }
        mState = DEVICE_PREPARED;
    }

    if (mState == DEVICE_PREPARED) {
        if (!keepBuffers) {
            destroyBufferPool();
            mState = DEVICE_CONFIGURED;
        }
   } else {
        LOGW("Trying to stop a device not started");
        ret = -1;
    }

    return ret;
}

/**
 * Start the streaming of buffers in a video device
 *
 * This called is allowed in the following states:
 * - PREPARED
 *
 * This method just calls  call the stream on IOCTL
 */
int V4L2VideoNode::start(int initialSkips)
{
    LOG1("@%s, device = %s, initialSkips:%d", __FUNCTION__, mName.c_str(), initialSkips);
    int ret(0);

    if (mState != DEVICE_PREPARED) {
        LOGE("%s: Invalid state to start %d", __FUNCTION__, mState);
        return -1;
    }

    //stream on
    enum v4l2_buf_type type;
    if (CC_LIKELY(mDirection == INPUT_VIDEO_NODE))
        type = mInBufType;
    else
        type = mOutBufType;

    ret = pioctl(mFd, VIDIOC_STREAMON, &type, mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_STREAMON returned: %d (%s)", ret, strerror(errno));
        return ret;
    }

    mFrameCounter = 0;
    mState = DEVICE_STARTED;
    mInitialSkips = initialSkips;

    return ret;
}

/**
 * Update the current device node configuration
 *
 * This called is allowed in the following states:
 * - OPEN
 * - CONFIGURED
 * - PREPARED
 *
 * This method is a convenience method for use in the context of video capture
 * (INPUT_VIDEO_NODE)
 * It makes use of the more detailed method that uses as input parameter the
 * v4l2_format structure
 * This method queries first the current format and updates capture format.
 *
 *
 * \param aConfig:[IN/OUT] reference to the new configuration.
 *                 This structure contains new values for width,height and format
 *                 parameters, but the stride value is not known by the caller
 *                 of this method. The stride value is retrieved from the ISP
 *                 and the value updated, so aConfig.stride is an OUTPUT parameter
 *                 The same applies for the expected size of the buffer
 *                 aConfig.size is also an OUTPUT parameter
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOW_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::setFormat(FrameInfo &aConfig)
{
    LOG1("@%s device = %s", __FUNCTION__, mName.c_str());
    int ret(0);
    struct v4l2_format v4l2_fmt;
    CLEAR(v4l2_fmt);

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED) &&
        (mState != DEVICE_PREPARED) ){
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    v4l2_fmt.type = mInBufType;
    LOG1("VIDIOC_G_FMT");
    ret = pioctl (mFd, VIDIOC_G_FMT, &v4l2_fmt, mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_G_FMT failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    v4l2_fmt.type = mInBufType;

    v4l2_fmt.fmt.pix.width = aConfig.width;
    v4l2_fmt.fmt.pix.height = aConfig.height;
    v4l2_fmt.fmt.pix.pixelformat = aConfig.format;
    v4l2_fmt.fmt.pix.bytesperline = pixelsToBytes(aConfig.format, aConfig.stride);
    v4l2_fmt.fmt.pix.sizeimage = 0;
    v4l2_fmt.fmt.pix.field = aConfig.field;

    // Update current configuration with the new one
    ret = setFormat(v4l2_fmt);
    if (ret != NO_ERROR)
        return ret;

    // .. but get the stride from ISP
    // and update the new configuration struct with it
    aConfig.stride = mConfig.stride;
    aConfig.width = mConfig.width;
    aConfig.height = mConfig.height;
    aConfig.field = mConfig.field;

    // Do the same for the frame size
    aConfig.size = mConfig.size;

    return NO_ERROR;
}

/**
 * Update the current device node configuration (low-level)
 *
 * This called is allowed in the following states:
 * - OPEN
 * - CONFIGURED
 * - PREPARED
 *
 * This methods allows more detailed control of the format than the previous one
 * It updates the internal configuration used to check for discrepancies between
 * configuration and buffer pool properties
 *
 * \param aFormat:[IN] reference to the new v4l2_format .
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOW_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::setFormat(struct v4l2_format &aFormat)
{

    LOG1("@%s device = %s", __FUNCTION__, mName.c_str());
    int ret(0);

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED) &&
        (mState != DEVICE_PREPARED) ){
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }


    LOG1("VIDIOC_S_FMT: %s width: %d, height: %d, bpl: %d, fourcc: %s, field: %d",
         mName.c_str(),
         aFormat.fmt.pix.width,
         aFormat.fmt.pix.height,
         aFormat.fmt.pix.bytesperline,
         v4l2Fmt2Str(aFormat.fmt.pix.pixelformat),
         aFormat.fmt.pix.field);

    ret = pioctl(mFd, VIDIOC_S_FMT, &aFormat, mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_S_FMT failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    LOG2("after VIDIOC_S_FMT: %s width: %d, height: %d, bpl: %d, fourcc: %s, field: %d",
         mName.c_str(),
         aFormat.fmt.pix.width,
         aFormat.fmt.pix.height,
         aFormat.fmt.pix.bytesperline,
         v4l2Fmt2Str(aFormat.fmt.pix.pixelformat),
         aFormat.fmt.pix.field);

    // Update current configuration with the new one
    mConfig.format = aFormat.fmt.pix.pixelformat;
    mConfig.width = aFormat.fmt.pix.width;
    mConfig.height = aFormat.fmt.pix.height;
    mConfig.stride = bytesToPixels(mConfig.format,aFormat.fmt.pix.bytesperline);
    mConfig.size = frameSize(mConfig.format, mConfig.stride, mConfig.height);

    if (mConfig.stride != mConfig.width)
        LOG1("stride: %d from ISP width: %d", mConfig.stride,mConfig.width);

    mState = DEVICE_CONFIGURED;
    mSetBufferPool.clear();
    return NO_ERROR;
}

status_t V4L2VideoNode::setSelection(const struct v4l2_selection &aSelection)
{
    LOG1("@%s device = %s", __FUNCTION__, mName.c_str());
    int ret = 0;

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED)){
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    LOG2("VIDIOC_S_SELECTION name %s type: %u, target: 0x%x, flags: 0x%x, rect left: %d, rect top: %d, width: %d, height: %d",
        mName.c_str(),
        aSelection.type,
        aSelection.target,
        aSelection.flags,
        aSelection.r.left,
        aSelection.r.top,
        aSelection.r.width,
        aSelection.r.height);

    ret = pbxioctl(VIDIOC_S_SELECTION, const_cast<v4l2_selection*>(&aSelection));
    if (ret < 0) {
        LOGE("VIDIOC_S_SELECTION failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}


int V4L2VideoNode::grabFrame(struct v4l2_buffer_info *buf)
{
    LOG2("@%s %s", __FUNCTION__, mName.c_str());
    int ret(0);

    if (mState != DEVICE_STARTED) {
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return -1;
    }
    if (buf == nullptr) {
        LOGE("%s: invalid parameter buf is nullptr",__FUNCTION__);
        return -1;
    }

    ret = dqbuf(buf);

    if (ret < 0)
        return ret;

    // inc frame counter but do no wrap to negative numbers
    mFrameCounter++;
    mFrameCounter &= INT_MAX;

    LOG2("@%s, index:%d, addr:%p", __FUNCTION__, buf->vbuffer.index, (void *)buf->vbuffer.m.userptr);
    return buf->vbuffer.index;
}

/*
 * In some cases like in timeout situation there is no need to add buffer to
 * traced buffers list because it is already there.
 */
status_t V4L2VideoNode::putFrame(struct v4l2_buffer const *buf)
{
    unsigned int index = buf->index;
    LOG2("@%s, index:%d, addr:%p name:%s", __FUNCTION__, buf->index, (void *)buf->m.userptr, mName.c_str());

    if (index >= mBufferPool.size()) {
        LOGE("Invalid index %d pool size %zu", index, mBufferPool.size());
        return BAD_INDEX;
    }

    mBufferPool.at(index).vbuffer.m = buf->m;
    mBufferPool.at(index).cache_flags = buf->flags;
    mBufferPool.at(index).vbuffer.reserved = buf->reserved;
    if (putFrame(index) < 0)
        return UNKNOWN_ERROR;

    return NO_ERROR;
}

/*
 * In some cases like in timeout situation there is no need to add buffer to
 * traced buffers list because it is already there.
 */
int V4L2VideoNode::putFrame(unsigned int index)
{
    LOG2("@%s %s", __FUNCTION__, mName.c_str());
    int ret(0);

    if (index >= mBufferPool.size()) {
        LOGE("Invalid index %d pool size %zu", index, mBufferPool.size());
        return BAD_INDEX;
    }
    struct v4l2_buffer_info vbuf = mBufferPool.at(index);
    LOG2("@%s: userptr = %p", __FUNCTION__, (void*)vbuf.vbuffer.m.userptr);
    ret = qbuf(&vbuf);

    return ret;
}

status_t V4L2VideoNode::setParameter (struct v4l2_streamparm *aParam)
{
    LOG2("@%s %s", __FUNCTION__, mName.c_str());
    status_t ret = NO_ERROR;

    if (mState == DEVICE_CLOSED)
        return INVALID_OPERATION;

    ret = pioctl(mFd, VIDIOC_S_PARM, aParam, mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_S_PARM failed ret %d : %s", ret, strerror(errno));
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

/**
 * Get the maximum rectangle for cropping.
 *
 * This call is allowed in all other states except in DEVICE_CLOSED.
 *
 * \param crop:[OUT] pointer to the maximum crop rectangle.
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOW_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::getMaxCropRectangle(struct v4l2_rect *crop)
{
    LOG1("@%s", __FUNCTION__);
    int ret;
    struct v4l2_cropcap cropcap;

    if (!crop)
        return UNKNOWN_ERROR;

    if (mState == DEVICE_CLOSED)
        return INVALID_OPERATION;

    CLEAR(cropcap);
    cropcap.type = mInBufType;
    ret = pioctl(mFd, VIDIOC_CROPCAP, &cropcap, mName.c_str());
    if (ret != NO_ERROR)
        return ret;

    *crop = cropcap.defrect;
    return NO_ERROR;
}

/**
 * Update the current device node cropping settings.
 *
 * This call is allowed in all other states except in DEVICE_CLOSED.
 *
 * It makes use of the more detailed method that uses as input parameter the
 * v4l2_format structure.
 *
 *
 * \param crop:[IN] pointer to the new cropping settings.
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOW_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::setCropRectangle(struct v4l2_rect *crop)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    struct v4l2_crop v4l2_crop;
    CLEAR(v4l2_crop);

    if (!crop)
        return UNKNOWN_ERROR;

    if (mState == DEVICE_CLOSED)
        return INVALID_OPERATION;

    v4l2_crop.type     = mInBufType;
    v4l2_crop.c.left   = crop->left;
    v4l2_crop.c.top    = crop->top;
    v4l2_crop.c.width  = crop->width;
    v4l2_crop.c.height = crop->height;

    // Update current configuration with the new one
    ret = pioctl(mFd, VIDIOC_S_CROP, &v4l2_crop, mName.c_str());
    if (ret != NO_ERROR)
        return ret;

    return NO_ERROR;
}

/**
 * Get the current device node cropping settings.
 *
 * This call is allowed in all other states except in DEVICE_CLOSED.
 *
 * \param[out] crop pointer to the cropping settings read from driver.
 *
 *  \return NO_ERROR if everything went well
 *          INVALID_OPERATION if device is not in correct state (open)
 *          UNKNOWN_ERROR if we get an error from the v4l2 ioctl's
 */
status_t V4L2VideoNode::getCropRectangle(struct v4l2_rect *crop)
{
    LOG2("@%s", __FUNCTION__);
    int ret;
    struct v4l2_crop v4l2_crop;
    CLEAR(v4l2_crop);

    if (!crop)
        return BAD_VALUE;

    if (mState == DEVICE_CLOSED)
        return INVALID_OPERATION;

    v4l2_crop.type = mInBufType;

    // Update current configuration with the new one
    ret = pioctl(mFd, VIDIOC_G_CROP, &v4l2_crop, mName.c_str());
    if (ret != NO_ERROR)
        return ret;

    crop->left   = v4l2_crop.c.left;
    crop->top    = v4l2_crop.c.top;
    crop->width  = v4l2_crop.c.width;
    crop->height = v4l2_crop.c.height;

    return NO_ERROR;
}

int V4L2VideoNode::getFramerate(float * framerate, int width, int height, int pix_fmt)
{
    LOG1("@%s %s", __FUNCTION__, mName.c_str());
    int ret(0);
    struct v4l2_frmivalenum frm_interval;

    if (nullptr == framerate)
        return BAD_VALUE;

    if (mState == DEVICE_CLOSED) {
        LOGE("Invalid state (%d) to set an attribute",mState);
        return UNKNOWN_ERROR;
    }

    CLEAR(frm_interval);
    frm_interval.pixel_format = pix_fmt;
    frm_interval.width = width;
    frm_interval.height = height;
    *framerate = -1.0;

    ret = pioctl(mFd, VIDIOC_ENUM_FRAMEINTERVALS, &frm_interval, mName.c_str());
    if (ret < 0) {
        LOGW("ioctl VIDIOC_ENUM_FRAMEINTERVALS failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    } else if (0 == frm_interval.discrete.denominator) {
        LOGE("ioctl VIDIOC_ENUM_FRAMEINTERVALS get invalid denominator value");
        *framerate = 0;
        return UNKNOWN_ERROR;
    }

    *framerate = 1.0 / (1.0 * frm_interval.discrete.numerator / frm_interval.discrete.denominator);

    return NO_ERROR;
}

/**
 * setBufferPool
 * updates the set buffer pool with externally allocated memory
 *
 * The device has to be at least in CONFIGURED state but once configured
 * it the buffer pool can be reset in PREPARED state.
 *
 * This pool will become active after calling start()
 * \param pool: array of void* where the memory is available
 * \param poolSize: amount of buffers in the pool
 * \param aFrameInfo: description of the properties of the buffers
 *                   it should match the configuration passed during setFormat
 * \param cached: boolean to detect whether the buffers are cached or not
 *                A cached buffer in this context means that the buffer
 *                memory may be accessed through some system caches, and
 *                the V4L2 driver must do cache invalidation in case
 *                the image data source is not updating system caches
 *                in hardware.
 *                When false (not cached), V4L2 driver can assume no cache
 *                invalidation/flushes are needed for this buffer.
 */
status_t V4L2VideoNode::setBufferPool(void **pool, unsigned int poolSize,
                                     FrameInfo *aFrameInfo, bool cached)
{
    LOG1("@%s: device = %s", __FUNCTION__, mName.c_str());
    struct v4l2_buffer_info vinfo;
    CLEAR(vinfo);
    uint32_t cacheflags = V4L2_BUF_FLAG_NO_CACHE_INVALIDATE |
                          V4L2_BUF_FLAG_NO_CACHE_CLEAN;

    if ((mState != DEVICE_CONFIGURED) && (mState != DEVICE_PREPARED)) {
        LOGE("%s:Invalid operation, device %s not configured (state = %d)",
                __FUNCTION__, mName.c_str(), mState);
        return INVALID_OPERATION;
    }

    if (pool == nullptr || aFrameInfo == nullptr) {
        LOGE("Invalid parameters, pool %p frameInfo %p",pool, aFrameInfo);
        return BAD_TYPE;
    }

    /**
     * check that the configuration of these buffers matches what we have already
     * told the driver.
     */
    if ((aFrameInfo->width != mConfig.width) ||
        (aFrameInfo->height != mConfig.height) ||
        (aFrameInfo->stride != mConfig.stride) ||
        (aFrameInfo->format != mConfig.format) ) {
        LOGE("Pool configuration does not match device configuration: (%dx%d) s:%d f:%s Pool is: (%dx%d) s:%d f:%s ",
                mConfig.width, mConfig.height, mConfig.stride, v4l2Fmt2Str(mConfig.format),
                aFrameInfo->width, aFrameInfo->height, aFrameInfo->stride, v4l2Fmt2Str(aFrameInfo->format));
        return BAD_VALUE;
    }

    mSetBufferPool.clear();

    for (unsigned int i = 0; i < poolSize; i++) {
        vinfo.data = pool[i];
        vinfo.width = aFrameInfo->stride;
        vinfo.height = aFrameInfo->height;
        vinfo.format = aFrameInfo->format;
        vinfo.length = aFrameInfo->size;
        if (cached)
           vinfo.cache_flags = 0;
       else
           vinfo.cache_flags = cacheflags;

        mSetBufferPool.push_back(vinfo);
    }

    mState = DEVICE_PREPARED;
    return NO_ERROR;
}

/**
 * setBufferPool
 * Presents the pool of buffers to the device.
 *
 * The device has to be  in CONFIGURED state.
 *
 * In this stage we request the buffer slots to the device and present
 * them to the driver assigning one index to each buffer.
 *
 * After this method the device is PREPARED and ready to Q
 * buffers
 * The structures in the pool are empty.
 * After this call the buffers have been presented to the device an index is assigned.
 * The structure is updated with this index and other details  (this is the output)
 *
 * \param pool: [IN/OUT]std::vector of v4l2_buffers structures
 * \param cached: [IN]boolean to detect whether the buffers are cached or not
 *                A cached buffer in this context means that the buffer
 *                memory may be accessed through some system caches, and
 *                the V4L2 driver must do cache invalidation in case
 *                the image data source is not updating system caches
 *                in hardware.
 *                When false (not cached), V4L2 driver can assume no cache
 *                invalidation/flushes are needed for this buffer.
 *\return  INVALID_OPERATION if the device was not configured
 *\return  UNKNOWN_ERROR if any of the v4l2 commands fails
 *\return  NO_ERROR if everything went AOK
 */
status_t V4L2VideoNode::setBufferPool(std::vector<struct v4l2_buffer> &pool,
                                      bool cached, int memType)
{
    LOG1("@%s: device = %s", __FUNCTION__, mName.c_str());
    struct v4l2_buffer_info vinfo;
    CLEAR(vinfo);
    int ret;
    uint32_t cacheflags = V4L2_BUF_FLAG_NO_CACHE_INVALIDATE |
                         V4L2_BUF_FLAG_NO_CACHE_CLEAN;

    if ((mState != DEVICE_CONFIGURED)) {
        LOGE("%s:Invalid operation, device %s not configured (state = %d)",
                __FUNCTION__, mName.c_str(), mState);
        return INVALID_OPERATION;
    }

    mBufferPool.clear();
    int num_buffers = requestBuffers(pool.size(), memType);
    if (num_buffers <= 0) {
        LOGE("%s: Could not complete buffer request",__FUNCTION__);
        return UNKNOWN_ERROR;
    }

    vinfo.width = mConfig.stride;
    vinfo.height = mConfig.height;
    vinfo.format = mConfig.format;
    vinfo.length = mConfig.size;

    for (unsigned int i = 0; i < pool.size(); i++) {
        if (cached)
            vinfo.cache_flags = 0;
        else
            vinfo.cache_flags = cacheflags;

        vinfo.vbuffer = pool.at(i);
        if (memType == V4L2_MEMORY_USERPTR) {
            vinfo.data = (void*)(pool[i].m.userptr);
        }

        ret = newBuffer(i, vinfo, memType);
        if (ret < 0) {
            LOGE("Error querying buffers status");
            mBufferPool.clear();
            mState = DEVICE_ERROR;
            return UNKNOWN_ERROR;
        }
        pool.at(i) = vinfo.vbuffer;
        mBufferPool.push_back(vinfo);
    }

    mMemoryType = memType;
    mState = DEVICE_PREPARED;
    return NO_ERROR;
}

status_t V4L2VideoNode::enumModes(std::vector<struct v4l2_sensor_mode> &modes)
{
    static const int MAX_ENUMS = 100000;
    static const __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_sensor_mode mode;
    int r, id, is, ii;

    for (id = 0; id < MAX_ENUMS; id++) {
        CLEAR(mode.fmt);
        mode.fmt.index = id;
        mode.fmt.type  = type;
        r = pioctl(mFd, VIDIOC_ENUM_FMT, &mode.fmt, mName.c_str());
        if (r < 0 && errno == EINVAL)
            break;
        if (r < 0)
            return UNKNOWN_ERROR;
        for (is = 0; is < MAX_ENUMS; is++) {
            int width = 0, height = 0;
            CLEAR(mode.size);
            mode.size.index = is;
            mode.size.pixel_format = mode.fmt.pixelformat;
            r = pioctl(mFd, VIDIOC_ENUM_FRAMESIZES, &mode.size, mName.c_str());
            if (r < 0 && errno == EINVAL)
                break;
            if (r < 0)
                return UNKNOWN_ERROR;
            switch (mode.size.type) {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                width  = mode.size.discrete.width;
                height = mode.size.discrete.height;
                break;
            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
            case V4L2_FRMSIZE_TYPE_STEPWISE:
                width  = mode.size.stepwise.min_width;
                height = mode.size.stepwise.min_height;
                break;
            }
            for (ii = 0; ii < MAX_ENUMS; ii++) {
                CLEAR(mode.ival);
                mode.ival.index = ii;
                mode.ival.pixel_format = mode.fmt.pixelformat;
                mode.ival.width  = width;
                mode.ival.height = height;
                r = pioctl(mFd, VIDIOC_ENUM_FRAMEINTERVALS, &mode.ival, mName.c_str());
                if (r < 0 && errno == EINVAL)
                    break;
                if (r < 0)
                    return UNKNOWN_ERROR;
                modes.push_back(mode);
            }
            if (ii >= MAX_ENUMS)
                LOGE("%s too many frame intervals", __FUNCTION__);
        }
        if (is >= MAX_ENUMS)
            LOGE("%s too many frame sizes", __FUNCTION__);
    }
    if (id >= MAX_ENUMS)
        LOGE("%s too many frame formats", __FUNCTION__);
    return NO_ERROR;
}

////////////////////////////////////////////////////////////////////
//                          PRIVATE METHODS
////////////////////////////////////////////////////////////////////

void V4L2VideoNode::destroyBufferPool()
{

    LOG1("@%s: device = %s", __FUNCTION__, mName.c_str());

    mBufferPool.clear();

    requestBuffers(0, mMemoryType);
}

int V4L2VideoNode::requestBuffers(size_t num_buffers, int memType)
{
    LOG1("@%s, num_buffers:%zu %s", __FUNCTION__, num_buffers, mName.c_str());
    struct v4l2_requestbuffers req_buf;
    int ret;
    CLEAR(req_buf);

    if (mState == DEVICE_CLOSED)
        return 0;

    req_buf.memory = memType;
    req_buf.count = num_buffers;

    if (mDirection == INPUT_VIDEO_NODE)
        req_buf.type = mInBufType;
    else if (mDirection == OUTPUT_VIDEO_NODE)
        req_buf.type = mOutBufType;
    else {
        LOGE("Unknown node direction (in/out) this should not happen");
        return -1;
    }

    LOG1("VIDIOC_REQBUFS, count=%u, memory=%u, type=%u", req_buf.count, req_buf.memory, req_buf.type);
    ret = pioctl(mFd, VIDIOC_REQBUFS, &req_buf, mName.c_str());

    if (ret < 0) {
        LOGE("VIDIOC_REQBUFS(%zu) returned: %d (%s)",
            num_buffers, ret, strerror(errno));
        return ret;
    }

    if (req_buf.count < num_buffers)
        LOGW("Got less buffers than requested! %u < %zu",req_buf.count, num_buffers);

    return req_buf.count;
}

int V4L2VideoNode::qbuf(struct v4l2_buffer_info *buf)
{
    LOG2("@%s %s", __FUNCTION__, mName.c_str());
    struct v4l2_buffer *v4l2_buf = &buf->vbuffer;
    int ret = 0;

    v4l2_buf->flags = buf->cache_flags;
    ret = pioctl(mFd, VIDIOC_QBUF, v4l2_buf, mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_QBUF on %s failed: %s", mName.c_str(), strerror(errno));
        return ret;
    }
    mBuffersInDevice++;
    return ret;
}

int V4L2VideoNode::dqbuf(struct v4l2_buffer_info *buf)
{
    LOG2("@%s %s", __FUNCTION__, mName.c_str());
    struct v4l2_buffer *v4l2_buf = &buf->vbuffer;
    int ret = 0;

    v4l2_buf->memory = V4L2_MEMORY_USERPTR;
    if (CC_LIKELY(mDirection == INPUT_VIDEO_NODE))
        v4l2_buf->type = mInBufType;
    else
        v4l2_buf->type = mOutBufType;

    ret = pioctl(mFd, VIDIOC_DQBUF, v4l2_buf, mName.c_str());
    if (ret < 0) {
        LOGE("VIDIOC_DQBUF failed: %s", strerror(errno));
        return ret;
    }
    mBuffersInDevice--;
    return ret;
}

/**
 * creates an active buffer pool from the set-buffer-pool that is provided
 * to the device by the call setBufferPool.
 *
 * We request to the V4L2 driver certain amount of buffer slots with the
 * buffer configuration.
 *
 * Then copy the required number from the set-buffer-pool to the active-buffer-pool
 *
 * \param buffer_count: number of buffers that the active pool contains
 * This number is at maximum the number of buffers in the set-buffer-pool
 * \return 0  success
 *        -1  failure
 */
int V4L2VideoNode::createBufferPool(unsigned int buffer_count)
{
    LOG1("@%s: device = %s buf count %d", __FUNCTION__, mName.c_str(), buffer_count);
    int i(0);
    int ret(0);

    if (mState != DEVICE_PREPARED) {
        LOGE("%s: Incorrect device state  %d", __FUNCTION__, mState);
        return -1;
    }

    if (buffer_count > mSetBufferPool.size()) {
        LOGE("%s: Incorrect parameter requested %u, but only %zu provided",
                __FUNCTION__, buffer_count, mSetBufferPool.size());
        return -1;
    }

    int num_buffers = requestBuffers(buffer_count);
    if (num_buffers <= 0) {
        LOGE("%s: Could not complete buffer request",__FUNCTION__);
        return -1;
    }

    mBufferPool.clear();

    for (i = 0; i < num_buffers; i++) {
        ret = newBuffer(i, mSetBufferPool.at(i));
        if (ret < 0)
            goto error;
        mBufferPool.push_back(mSetBufferPool[i]);
    }

    return 0;

error:
    LOGE("Failed to VIDIOC_QUERYBUF some of the buffers, clearing the active buffer pool");
    mBufferPool.clear();
    return ret;
}


int V4L2VideoNode::newBuffer(int index, struct v4l2_buffer_info &buf, int memType)
{
    LOG1("@%s", __FUNCTION__);
    int ret;
    struct v4l2_buffer *vbuf = &buf.vbuffer;

    vbuf->flags = 0x0;
    vbuf->memory = memType;

    if (mDirection == INPUT_VIDEO_NODE)
        vbuf->type = mInBufType;
    else if (mDirection == OUTPUT_VIDEO_NODE)
        vbuf->type = mOutBufType;
    else {
        LOGE("Unknown node direction (in/out) this should not happen");
        return -1;
    }


    vbuf->index = index;
    ret = pioctl(mFd , VIDIOC_QUERYBUF, vbuf, mName.c_str());

    if (ret < 0) {
        LOGE("VIDIOC_QUERYBUF failed: %s", strerror(errno));
        return ret;
    }

    if (memType == V4L2_MEMORY_USERPTR) {
        vbuf->m.userptr = (unsigned long)(buf.data);
    } // For MMAP memory the user will do the mmap to get the ptr

    buf.length = vbuf->length;
    LOG1("index %u", vbuf->index);
    LOG1("type %d", vbuf->type);
    LOG1("bytesused %u", vbuf->bytesused);
    LOG1("flags %08x", vbuf->flags);
    if (memType == V4L2_MEMORY_MMAP) {
        LOG1("memory MMAP: offset 0x%X", vbuf->m.offset);
    } else if (memType == V4L2_MEMORY_USERPTR) {
        LOG1("memory USRPTR:  %p", (void*)vbuf->m.userptr);
    }
    LOG1("length %u", vbuf->length);
    return ret;
}

int V4L2VideoNode::freeBuffer(struct v4l2_buffer_info *buf_info)
{
    /**
     * For devices using usr ptr as data this method is a no-op
     */
    UNUSED(buf_info);
    return 0;
}

void V4L2VideoNode::setInputBufferType(enum v4l2_buf_type v4l2BufType)
{
    mInBufType = v4l2BufType;
}

void V4L2VideoNode::setOutputBufferType(enum v4l2_buf_type v4l2BufType)
{
    mOutBufType = v4l2BufType;
}

status_t V4L2VideoNode::getFormat(struct v4l2_format &aFormat)
{
    LOG1("@%s device = %s", __FUNCTION__, mName.c_str());
    int ret = 0;

    if ((mState != DEVICE_OPEN) &&
        (mState != DEVICE_CONFIGURED)){
        LOGE("%s invalid device state %d",__FUNCTION__, mState);
        return INVALID_OPERATION;
    }

    aFormat.type = mInBufType;
    ret = pioctl(mFd, VIDIOC_G_FMT, &aFormat, mName.c_str());

    if (ret < 0) {
        LOGE("VIDIOC_G_FMT failed: %s", strerror(errno));
        return UNKNOWN_ERROR;
    }

    LOG1("VIDIOC_G_FMT: %s width: %d, height: %d, bpl: %d, fourcc: %s, field: %d",
             mName.c_str(),
             aFormat.fmt.pix.width,
             aFormat.fmt.pix.height,
             aFormat.fmt.pix.bytesperline,
             v4l2Fmt2Str(aFormat.fmt.pix.pixelformat),
             aFormat.fmt.pix.field);

    return NO_ERROR;
}

} NAMESPACE_DECLARATION_END
