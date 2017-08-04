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

#ifndef _CAMERA3_HAL_V4L2DEVICE_H_
#define _CAMERA3_HAL_V4L2DEVICE_H_

#include <memory>
#include <atomic>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <poll.h>
#include <string>
#include <sstream>
#include <vector>
#include "PerformanceTraces.h"
#include "FrameInfo.h"
#include "SysCall.h"

NAMESPACE_DECLARATION {
#define pioctl(fd, ctrlId, attr, name) \
    SysCall::ioctl(fd, ctrlId, attr)

#define perfopen(name, attr, fd) \
    fd = SysCall::open(name, attr)

#define perfclose(name, fd) \
    SysCall::close(fd)

#define pbxioctl(ctrlId, attr) \
    V4L2DeviceBase::xioctl(ctrlId, attr)

#define perfpoll(fd, value, timeout) \
    SysCall::poll(fd, value, timeout)
/**
 * v4l2 buffer descriptor.
 *
 * This information is stored in the pool
 */
struct v4l2_buffer_info {
    void *data;
    size_t length;
    int width;
    int height;
    int format;
    int cache_flags;        /*!< initial flags used when creating buffers */
    struct v4l2_buffer vbuffer;
};

struct v4l2_sensor_mode {
    struct v4l2_fmtdesc fmt;
    struct v4l2_frmsizeenum size;
    struct v4l2_frmivalenum ival;
};

enum VideNodeDirection {
    INPUT_VIDEO_NODE,   /*!< input video devices like cameras or capture cards */
    OUTPUT_VIDEO_NODE  /*!< output video devices like displays */
};

/**
 * A class encapsulating simple V4L2 device operations
 *
 * Base class that contains common V4L2 operations used by video nodes and
 * subdevices. It provides a slightly higher interface than IOCTLS to access
 * the devices. It also stores:
 * - State of the device to protect from invalid usage sequence
 * - Name
 * - File descriptor
 */
class V4L2DeviceBase {
public:
    explicit V4L2DeviceBase(const char *name);
    virtual ~V4L2DeviceBase();

    virtual status_t open();
    virtual status_t close();

    virtual int xioctl(int request, void *arg, int *errnoCopy = nullptr) const;
    virtual int poll(int timeout);

    virtual status_t setControl(int aControlNum, const int value, const char *name);
    virtual status_t getControl(int aControlNum, int *value);
    virtual status_t queryMenu(v4l2_querymenu &menu);
    virtual status_t queryControl(v4l2_queryctrl &control);

    virtual int subscribeEvent(int event);
    virtual int unsubscribeEvent(int event);
    virtual int dequeueEvent(struct v4l2_event *event);

    bool isOpen() { return mFd != -1; }
    int getFd() { return mFd; }

    const char* name() { return mName.c_str(); }

    static int pollDevices(const std::vector<std::shared_ptr<V4L2DeviceBase>> &devices,
                               std::vector<std::shared_ptr<V4L2DeviceBase>> &activeDevices,
                               std::vector<std::shared_ptr<V4L2DeviceBase>> &inactiveDevices,
                               int timeOut, int flushFd = -1,
                               int events = POLLPRI | POLLIN | POLLERR);

    static int frmsizeWidth(struct v4l2_frmsizeenum &size);
    static int frmsizeHeight(struct v4l2_frmsizeenum &size);
    static void frmivalIval(struct v4l2_frmivalenum &frmival, struct v4l2_fract &ival);
    static int cmpFract(struct v4l2_fract &f1, struct v4l2_fract &f2);
    static int cmpIval(struct v4l2_frmivalenum &i1, struct v4l2_frmivalenum &i2);

protected:
    std::string  mName;     /*!< path to device in file system, ex: /dev/video0 */
    int          mFd;       /*!< file descriptor obtained when device is open */

};


/**
 * A class encapsulating simple V4L2 video device node operations
 *
 * This class extends V4L2DeviceBase and adds extra internal state
 * and more convenience methods to  manage an associated buffer pool
 * with the device.
 * This class introduces new methods specifics to control video device nodes
 *
 */
class V4L2VideoNode: public V4L2DeviceBase {
public:
    explicit V4L2VideoNode(const char *name,
                  VideNodeDirection nodeDirection = INPUT_VIDEO_NODE);
    virtual ~V4L2VideoNode();

    virtual status_t open();
    virtual status_t close();

    virtual status_t setInput(int index);
    virtual status_t queryCap(struct v4l2_capability *cap);
    virtual status_t enumerateInputs(struct v4l2_input *anInput);

    // Video node configuration
    virtual void setInputBufferType(enum v4l2_buf_type v4l2BufType);
    virtual void setOutputBufferType(enum v4l2_buf_type v4l2BufType);
    virtual int getFramerate(float * framerate, int width, int height, int pix_fmt);
    virtual status_t setParameter (struct v4l2_streamparm *aParam);
    virtual status_t getMaxCropRectangle(struct v4l2_rect *crop);
    virtual status_t setCropRectangle (struct v4l2_rect *crop);
    virtual status_t getCropRectangle (struct v4l2_rect *crop);
    virtual status_t setFormat(FrameInfo &aConfig);
    virtual status_t getFormat(struct v4l2_format &aFormat);
    virtual status_t setFormat(struct v4l2_format &aFormat);
    virtual status_t setSelection(const struct v4l2_selection &aSelection);
    virtual status_t queryCapturePixelFormats(std::vector<v4l2_fmtdesc> &formats);
    virtual int getMemoryType();

    // Buffer pool management -- DEPRECATED!
    virtual status_t setBufferPool(void **pool, unsigned int poolSize,
                                   FrameInfo *aFrameInfo, bool cached);
    virtual void destroyBufferPool();
    virtual int createBufferPool(unsigned int buffer_count);

     // New Buffer pool management
    virtual status_t setBufferPool(std::vector<struct v4l2_buffer> &pool,
                                   bool cached,
                                   int memType = V4L2_MEMORY_USERPTR);

    // Buffer flow control
    virtual int stop(bool keepBuffers = false);
    virtual int start(int initialSkips);

    virtual int grabFrame(struct v4l2_buffer_info *buf);
    virtual int putFrame(struct v4l2_buffer const *buf);
    virtual int putFrame(unsigned int index);
    virtual int exportFrame(unsigned int index);

    // Convenience accessors
    virtual bool isStarted() const { return mState == DEVICE_STARTED; }
    virtual unsigned int getFrameCount() const { return mFrameCounter; }
    virtual unsigned int getBufsInDeviceCount() const { return mBuffersInDevice; }
    virtual unsigned int getInitialFrameSkips() const { return mInitialSkips; }
    virtual void getConfig(FrameInfo &config) const { config = mConfig; }

    virtual status_t enumModes(std::vector<struct v4l2_sensor_mode> &modes);

protected:
    virtual int qbuf(struct v4l2_buffer_info *buf);
    virtual int dqbuf(struct v4l2_buffer_info *buf);
    virtual int newBuffer(int index, struct v4l2_buffer_info &buf,
                          int memType = V4L2_MEMORY_USERPTR);
    virtual int freeBuffer(struct v4l2_buffer_info *buf_info);
    virtual int requestBuffers(size_t num_buffers,
                               int memType = V4L2_MEMORY_USERPTR);
    virtual void printBufferInfo(const char *func, const struct v4l2_buffer &buf);

protected:

    enum VideoNodeState  {
            DEVICE_CLOSED = 0,  /*!< kernel device closed */
            DEVICE_OPEN,        /*!< device node opened */
            DEVICE_CONFIGURED,  /*!< device format set, IOC_S_FMT */
            DEVICE_PREPARED,    /*!< device has requested buffers (set_buffer_pool)*/
            DEVICE_STARTED,     /*!< stream started, IOC_STREAMON */
            DEVICE_ERROR        /*!< undefined state */
        };

    VideoNodeState  mState;
    // Device capture configuration
    FrameInfo mConfig;

    std::atomic<int32_t> mBuffersInDevice;      /*!< Tracks the amount of buffers inside the driver. Goes from 0 to the size of the pool*/
    unsigned int mFrameCounter;             /*!< Tracks the number of output buffers produced by the device. Running counter. It is reset when we start the device*/
    unsigned int mInitialSkips;

    std::vector<struct v4l2_buffer_info> mSetBufferPool; /*!< DEPRECATED:This is the buffer pool set before the device is prepared*/
    std::vector<struct v4l2_buffer_info> mBufferPool;    /*!< This is the active buffer pool */

    VideNodeDirection mDirection;
    enum v4l2_buf_type mInBufType;
    enum v4l2_buf_type mOutBufType;
    int                mMemoryType;
};

/**
 * A class encapsulating simple V4L2 sub device node operations
 *
 * Sub-devices are control points to the new media controller architecture
 * in V4L2
 *
 */
class V4L2Subdevice: public V4L2DeviceBase {
public:
    explicit V4L2Subdevice(const char *name);
    virtual ~V4L2Subdevice();

    virtual status_t open();
    virtual status_t close();

    status_t queryFormats(int pad, std::vector<uint32_t> &formats);
    status_t setFormat(int pad, int width, int height, int formatCode, int field);
    status_t setSelection(int pad, int target, int top, int left, int width, int height);
    status_t getPadFormat(int padIndex, int &width, int &height, int &code);

private:
    status_t setFormat(struct v4l2_subdev_format &aFormat);
    status_t getFormat(struct v4l2_subdev_format &aFormat);
    status_t setSelection(struct v4l2_subdev_selection &aSelection);

private:
    enum SubdevState  {
            DEVICE_CLOSED = 0,  /*!< kernel device closed */
            DEVICE_OPEN,        /*!< device node opened */
            DEVICE_CONFIGURED,  /*!< device format set, IOC_S_FMT */
            DEVICE_ERROR        /*!< undefined state */
        };

    SubdevState     mState;
    FrameInfo       mConfig;
};

} NAMESPACE_DECLARATION_END
#endif // _CAMERA3_HAL_V4L2DEVICE_H_
