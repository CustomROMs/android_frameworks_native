/*
 * Copyright (C) 2007 The Android Open Source Project
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

#define LOG_TAG "GraphicBufferMapper"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS
//#define LOG_NDEBUG 0

#include <ui/GraphicBufferMapper.h>

#include <grallocusage/GrallocUsageConversion.h>

// We would eliminate the non-conforming zero-length array, but we can't since
// this is effectively included from the Linux kernel
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-length-array"
#include <sync/sync.h>
#pragma clang diagnostic pop

#include <utils/Log.h>
#include <utils/Trace.h>

#include <ui/Gralloc2.h>
#include <ui/GraphicBuffer.h>

#include <system/graphics.h>

namespace android {
// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE( GraphicBufferMapper )

void GraphicBufferMapper::preloadHal() {
    Gralloc2::Mapper::preload();
}

GraphicBufferMapper::GraphicBufferMapper()
  : mMapper(std::make_unique<const Gralloc2::Mapper>()),
    mAllocMod(0)
{
    hw_module_t const* module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    ALOGE_IF(err, "FATAL: can't find the %s module", GRALLOC_HARDWARE_MODULE_ID);
    if (err == 0) {
        mAllocMod = reinterpret_cast<gralloc_module_t const *>(module);
    }
}

status_t GraphicBufferMapper::importBuffer(buffer_handle_t rawHandle,
        uint32_t width, uint32_t height, uint32_t layerCount,
        PixelFormat format, uint64_t usage, uint32_t stride,
        buffer_handle_t* outHandle)
{

    buffer_handle_t bufferHandle;
    Gralloc2::Error error = mMapper->importBuffer(
            hardware::hidl_handle(rawHandle), &bufferHandle);
    if (error != Gralloc2::Error::NONE) {
        ALOGW("importBuffer(%p) failed: %d", rawHandle, error);
        return static_cast<status_t>(error);
    }

    Gralloc2::IMapper::BufferDescriptorInfo info = {};
    info.width = width;
    info.height = height;
    info.layerCount = layerCount;
    info.format = static_cast<Gralloc2::PixelFormat>(format);
    info.usage = usage;

    error = mMapper->validateBufferSize(bufferHandle, info, stride);
    if (error != Gralloc2::Error::NONE) {
        ALOGE("validateBufferSize(%p) failed: %d", rawHandle, error);
        freeBuffer(bufferHandle);
        return static_cast<status_t>(error);
    }

    *outHandle = bufferHandle;

    return static_cast<status_t>(error);
#if 0
    status_t err;
    err = mAllocMod->registerBuffer(mAllocMod, handle);

    ALOGW_IF(err, "registerBuffer(%p) failed %d (%s)",
            handle, err, strerror(-err));
    return err;
#endif
}

void GraphicBufferMapper::getTransportSize(buffer_handle_t handle,
            uint32_t* outTransportNumFds, uint32_t* outTransportNumInts)
{
    mMapper->getTransportSize(handle, outTransportNumFds, outTransportNumInts);
}

status_t GraphicBufferMapper::freeBuffer(buffer_handle_t handle)
{
    ATRACE_CALL();

    mMapper->freeBuffer(handle);

    return NO_ERROR;
#if 0
    status_t err;
    err = mAllocMod->unregisterBuffer(handle);

    ALOGW_IF(err, "unregisterBuffer(%p) failed %d (%s)",
            handle, err, strerror(-err));
    return err;
#endif
}

static inline Gralloc2::IMapper::Rect asGralloc2Rect(const Rect& rect) {
    Gralloc2::IMapper::Rect outRect{};
    outRect.left = rect.left;
    outRect.top = rect.top;
    outRect.width = rect.width();
    outRect.height = rect.height();
    return outRect;
}

status_t GraphicBufferMapper::lock(buffer_handle_t handle, uint32_t usage,
        const Rect& bounds, void** vaddr)
{
    return lockAsync(handle, usage, bounds, vaddr, -1);
}

status_t GraphicBufferMapper::lockYCbCr(buffer_handle_t handle, uint32_t usage,
        const Rect& bounds, android_ycbcr *ycbcr)
{
    return lockAsyncYCbCr(handle, usage, bounds, ycbcr, -1);
}

status_t GraphicBufferMapper::unlock(buffer_handle_t handle)
{
    int32_t fenceFd = -1;
    status_t error = unlockAsync(handle, &fenceFd);
    if (error == NO_ERROR && fenceFd >= 0) {
        sync_wait(fenceFd, -1);
        close(fenceFd);
    }
    return error;
}

status_t GraphicBufferMapper::lockAsync(buffer_handle_t handle,
        uint32_t usage, const Rect& bounds, void** vaddr, int fenceFd)
{
    return lockAsync(handle, usage, usage, bounds, vaddr, fenceFd);
}

status_t GraphicBufferMapper::lockAsync(buffer_handle_t handle,
        uint64_t producerUsage, uint64_t /*consumerUsage*/, const Rect& bounds,
        void** vaddr, int /*fenceFd*/)
{
    ATRACE_CALL();
    status_t err;

    err = mAllocMod->lock(mAllocMod, handle, static_cast<int>(producerUsage),
            bounds.left, bounds.top, bounds.width(), bounds.height(),
            vaddr);

    ALOGW_IF(err, "lock(...) failed %d (%s)", err, strerror(-err));
    return err;
}

status_t GraphicBufferMapper::lockAsyncYCbCr(buffer_handle_t handle,
        uint32_t usage, const Rect& bounds, android_ycbcr *ycbcr, int /*fenceFd*/)
{
    ATRACE_CALL();
    status_t err;

    if (mAllocMod->lock_ycbcr == NULL) {
        return -EINVAL; // do not log failure
    }

    err = mAllocMod->lock_ycbcr(mAllocMod, handle, static_cast<int>(usage),
            bounds.left, bounds.top, bounds.width(), bounds.height(),
            ycbcr);

    ALOGW_IF(err, "lock(...) failed %d (%s)", err, strerror(-err));

    return err;
}

status_t GraphicBufferMapper::unlockAsync(buffer_handle_t handle, int *fenceFd)
{
    ATRACE_CALL();

    status_t err;

    if (mAllocMod->common.module_api_version >= GRALLOC_MODULE_API_VERSION_0_3) {
        err = mAllocMod->unlockAsync(mAllocMod, handle, fenceFd);
    } else {
        *fenceFd = -1;
        err = mAllocMod->unlock(mAllocMod, handle);
    }

    ALOGW_IF(err, "unlockAsync(...) failed %d (%s)", err, strerror(-err));
    return err;
}

status_t GraphicBufferMapper::isSupported(uint32_t /*width*/, uint32_t /*height*/,
                                          android::PixelFormat /*format*/, uint32_t /*layerCount*/,
                                          uint64_t /*usage*/, bool* /*outSupported*/) const {
    return INVALID_OPERATION;
}
// ---------------------------------------------------------------------------
}; // namespace android
