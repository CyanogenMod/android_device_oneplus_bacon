/* Copyright (c) 2012-2014, The Linux Foundataion. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#define ATRACE_TAG ATRACE_TAG_CAMERA
#define MEMLOG_THRESH 102400
#define LOG_TAG "QCameraHWI_Mem"

#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <utils/Errors.h>
#include <utils/Trace.h>
#include <gralloc_priv.h>
#include <QComOMXMetadata.h>
#include "QCamera2HWI.h"
#include "QCameraMem.h"
#include "QCameraParameters.h"

extern "C" {
#include <mm_camera_interface.h>
}

using namespace android;

namespace qcamera {

// QCaemra2Memory base class

/*===========================================================================
 * FUNCTION   : QCameraMemory
 *
 * DESCRIPTION: default constructor of QCameraMemory
 *
 * PARAMETERS :
 *   @cached  : flag indicates if using cached memory
 *
 * RETURN     : None
 *==========================================================================*/
QCameraMemory::QCameraMemory(bool cached,
        QCameraMemoryPool *pool,
        cam_stream_type_t streamType)
    :m_bCached(cached),
     mMemoryPool(pool),
     mStreamType(streamType)
{
    mBufferCount = 0;
    memset(mMemInfo, 0, sizeof(mMemInfo));
}

/*===========================================================================
 * FUNCTION   : ~QCameraMemory
 *
 * DESCRIPTION: deconstructor of QCameraMemory
 *
 * PARAMETERS : none
 *
 * RETURN     : None
 *==========================================================================*/
QCameraMemory::~QCameraMemory()
{
}

/*===========================================================================
 * FUNCTION   : cacheOpsInternal
 *
 * DESCRIPTION: ion related memory cache operations
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *   @cmd     : cache ops command
 *   @vaddr   : ptr to the virtual address
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraMemory::cacheOpsInternal(uint32_t index, unsigned int cmd, void *vaddr)
{
    if (!m_bCached) {
        // Memory is not cached, no need for cache ops
        CDBG("%s: No cache ops here for uncached memory", __func__);
        return OK;
    }

    struct ion_flush_data cache_inv_data;
    struct ion_custom_data custom_data;
    int ret = OK;

    if (index >= mBufferCount) {
        ALOGE("%s: index %d out of bound [0, %d)", __func__, index, mBufferCount);
        return BAD_INDEX;
    }

    memset(&cache_inv_data, 0, sizeof(cache_inv_data));
    memset(&custom_data, 0, sizeof(custom_data));
    cache_inv_data.vaddr = vaddr;
    cache_inv_data.fd = mMemInfo[index].fd;
    cache_inv_data.handle = mMemInfo[index].handle;
    cache_inv_data.length =
            ( /* FIXME: Should remove this after ION interface changes */ unsigned int)
            mMemInfo[index].size;
    custom_data.cmd = cmd;
    custom_data.arg = (unsigned long)&cache_inv_data;

    CDBG("%s: addr = %p, fd = %d, handle = %lx length = %d, ION Fd = %d",
         __func__, cache_inv_data.vaddr, cache_inv_data.fd,
         (unsigned long)cache_inv_data.handle, cache_inv_data.length,
         mMemInfo[index].main_ion_fd);
    ret = ioctl(mMemInfo[index].main_ion_fd, ION_IOC_CUSTOM, &custom_data);
    if (ret < 0)
        ALOGE("%s: Cache Invalidate failed: %s\n", __func__, strerror(errno));

    return ret;
}

/*===========================================================================
 * FUNCTION   : getFd
 *
 * DESCRIPTION: return file descriptor of the indexed buffer
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *
 * RETURN     : file descriptor
 *==========================================================================*/
int QCameraMemory::getFd(uint32_t index) const
{
    if (index >= mBufferCount)
        return BAD_INDEX;

    return mMemInfo[index].fd;
}

/*===========================================================================
 * FUNCTION   : getSize
 *
 * DESCRIPTION: return buffer size of the indexed buffer
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *
 * RETURN     : buffer size
 *==========================================================================*/
ssize_t QCameraMemory::getSize(uint32_t index) const
{
    if (index >= mBufferCount)
        return BAD_INDEX;

    return (ssize_t)mMemInfo[index].size;
}

/*===========================================================================
 * FUNCTION   : getCnt
 *
 * DESCRIPTION: query number of buffers allocated
 *
 * PARAMETERS : none
 *
 * RETURN     : number of buffers allocated
 *==========================================================================*/
uint8_t QCameraMemory::getCnt() const
{
    return mBufferCount;
}

/*===========================================================================
 * FUNCTION   : getBufDef
 *
 * DESCRIPTION: query detailed buffer information
 *
 * PARAMETERS :
 *   @offset  : [input] frame buffer offset
 *   @bufDef  : [output] reference to struct to store buffer definition
 *   @index   : [input] index of the buffer
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraMemory::getBufDef(const cam_frame_len_offset_t &offset,
        mm_camera_buf_def_t &bufDef, uint32_t index) const
{
    if (!mBufferCount) {
        ALOGE("Memory not allocated");
        return;
    }
    bufDef.fd = mMemInfo[index].fd;
    bufDef.frame_len = mMemInfo[index].size;
    bufDef.mem_info = (void *)this;
    bufDef.num_planes = (int8_t)offset.num_planes;
    bufDef.buffer = getPtr(index);
    bufDef.buf_idx = index;

    /* Plane 0 needs to be set separately. Set other planes in a loop */
    bufDef.planes[0].length = offset.mp[0].len;
    bufDef.planes[0].m.userptr = (long unsigned int)mMemInfo[index].fd;
    bufDef.planes[0].data_offset = offset.mp[0].offset;
    bufDef.planes[0].reserved[0] = 0;
    for (int i = 1; i < bufDef.num_planes; i++) {
         bufDef.planes[i].length = offset.mp[i].len;
         bufDef.planes[i].m.userptr = (long unsigned int)mMemInfo[i].fd;
         bufDef.planes[i].data_offset = offset.mp[i].offset;
         bufDef.planes[i].reserved[0] =
                 bufDef.planes[i-1].reserved[0] +
                 bufDef.planes[i-1].length;
    }
}

/*===========================================================================
 * FUNCTION   : traceLogAllocStart
 *
 * DESCRIPTION: query detailed buffer information
 *
 * PARAMETERS :
 *   @size  : [input] alloc
 *   @count  : [input] number of buffers
 *   @allocName   : [input] name for the alloc
 *
 * RETURN     : none
 *==========================================================================*/
inline void QCameraMemory::traceLogAllocStart(size_t size, int count, const char *allocName)
{
    ALOGD("%s : alloc E count=%d size=%zu", __func__, count, size);
#ifdef ATRACE_TAG_CAMERA
    char atracer[30];
    if ((size * (size_t)count) > MEMLOG_THRESH) {
        snprintf(atracer,sizeof(atracer), "%s %zu",allocName, size);
        ATRACE_BEGIN(atracer);
        ALOGE("%s:%s", __func__, atracer);
    } else {
        ATRACE_CALL();
    }
#endif
}

/*===========================================================================
 * FUNCTION   : traceLogAllocEnd
 *
 * DESCRIPTION: query detailed buffer information
 *
 * PARAMETERS :
 *   @size  : [input] alloc
 *   @count  : [input] number of buffers
 *
 * RETURN     : none
 *==========================================================================*/
inline void QCameraMemory::traceLogAllocEnd(size_t size)
{
    ALOGD(" %s : X", __func__);
#ifdef ATRACE_TAG_CAMERA
    if (size > MEMLOG_THRESH) {
        ATRACE_END();
        ALOGE("%s %zu", __func__, size);
    }
#endif
}

/*===========================================================================
 * FUNCTION   : alloc
 *
 * DESCRIPTION: allocate requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *   @heap_id : heap id to indicate where the buffers will be allocated from
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraMemory::alloc(int count, size_t size, unsigned int heap_id)
{
    int rc = OK;

    if (0 > count) {
        ALOGE("%s: Negative count %d", __func__, count);
        return BAD_INDEX;
    }

    int new_bufCnt = mBufferCount + count;
    traceLogAllocStart(size, count, "Memsize");

    if (new_bufCnt > MM_CAMERA_MAX_NUM_FRAMES) {
        ALOGE("%s: Buffer count %d out of bound. Max is %d",
              __func__, new_bufCnt, MM_CAMERA_MAX_NUM_FRAMES);
        return BAD_INDEX;
    }

    for (int i = mBufferCount; i < new_bufCnt; i ++) {
        if ( NULL == mMemoryPool ) {
            ALOGE("%s : No memory pool available", __func__);
            rc = allocOneBuffer(mMemInfo[i], heap_id, size, m_bCached);
            if (rc < 0) {
                ALOGE("%s: AllocateIonMemory failed", __func__);
                for (int j = i-1; j >= 0; j--)
                    deallocOneBuffer(mMemInfo[j]);
                break;
            }
        } else {
            rc = mMemoryPool->allocateBuffer(mMemInfo[i],
                                             heap_id,
                                             size,
                                             m_bCached,
                                             mStreamType);
            if (rc < 0) {
                ALOGE("%s: Memory pool allocation failed", __func__);
                for (int j = i-1; j >= 0; j--)
                    mMemoryPool->releaseBuffer(mMemInfo[j],
                                               mStreamType);
                break;
            }
        }

    }
    traceLogAllocEnd (size * (size_t)count);
    return rc;
}

/*===========================================================================
 * FUNCTION   : dealloc
 *
 * DESCRIPTION: deallocate buffers
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraMemory::dealloc()
{
    for (int i = 0; i < mBufferCount; i++) {
        if ( NULL == mMemoryPool ) {
            deallocOneBuffer(mMemInfo[i]);
        } else {
            mMemoryPool->releaseBuffer(mMemInfo[i], mStreamType);
        }
    }
}

/*===========================================================================
 * FUNCTION   : allocOneBuffer
 *
 * DESCRIPTION: impl of allocating one buffers of certain size
 *
 * PARAMETERS :
 *   @memInfo : [output] reference to struct to store additional memory allocation info
 *   @heap    : [input] heap id to indicate where the buffers will be allocated from
 *   @size    : [input] lenght of the buffer to be allocated
 *   @cached  : [input] flag whether buffer needs to be cached
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraMemory::allocOneBuffer(QCameraMemInfo &memInfo,
        unsigned int heap_id, size_t size, bool cached)
{
    int rc = OK;
    struct ion_handle_data handle_data;
    struct ion_allocation_data alloc;
    struct ion_fd_data ion_info_fd;
    int main_ion_fd = 0;

    main_ion_fd = open("/dev/ion", O_RDONLY);
    if (main_ion_fd < 0) {
        ALOGE("Ion dev open failed: %s\n", strerror(errno));
        goto ION_OPEN_FAILED;
    }

    memset(&alloc, 0, sizeof(alloc));
    alloc.len = size;
    /* to make it page size aligned */
    alloc.len = (alloc.len + 4095U) & (~4095U);
    alloc.align = 4096;
    if (cached) {
        alloc.flags = ION_FLAG_CACHED;
    }
    alloc.heap_mask = heap_id;
    rc = ioctl(main_ion_fd, ION_IOC_ALLOC, &alloc);
    if (rc < 0) {
        ALOGE("ION allocation failed: %s\n", strerror(errno));
        goto ION_ALLOC_FAILED;
    }

    memset(&ion_info_fd, 0, sizeof(ion_info_fd));
    ion_info_fd.handle = alloc.handle;
    rc = ioctl(main_ion_fd, ION_IOC_SHARE, &ion_info_fd);
    if (rc < 0) {
        ALOGE("ION map failed %s\n", strerror(errno));
        goto ION_MAP_FAILED;
    }

    memInfo.main_ion_fd = main_ion_fd;
    memInfo.fd = ion_info_fd.fd;
    memInfo.handle = ion_info_fd.handle;
    memInfo.size = alloc.len;
    memInfo.cached = cached;
    memInfo.heap_id = heap_id;

    CDBG_HIGH("%s : ION buffer %lx with size %d allocated",
            __func__, (unsigned long)memInfo.handle, memInfo.size);
    return OK;

ION_MAP_FAILED:
    memset(&handle_data, 0, sizeof(handle_data));
    handle_data.handle = ion_info_fd.handle;
    ioctl(main_ion_fd, ION_IOC_FREE, &handle_data);
ION_ALLOC_FAILED:
    close(main_ion_fd);
ION_OPEN_FAILED:
    return NO_MEMORY;
}

/*===========================================================================
 * FUNCTION   : deallocOneBuffer
 *
 * DESCRIPTION: impl of deallocating one buffers
 *
 * PARAMETERS :
 *   @memInfo : reference to struct that stores additional memory allocation info
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraMemory::deallocOneBuffer(QCameraMemInfo &memInfo)
{
    struct ion_handle_data handle_data;

    if (memInfo.fd > 0) {
        close(memInfo.fd);
        memInfo.fd = 0;
    }

    if (memInfo.main_ion_fd > 0) {
        memset(&handle_data, 0, sizeof(handle_data));
        handle_data.handle = memInfo.handle;
        ioctl(memInfo.main_ion_fd, ION_IOC_FREE, &handle_data);
        close(memInfo.main_ion_fd);
        memInfo.main_ion_fd = 0;
    }
    memInfo.handle = 0;
    memInfo.size = 0;
}

/*===========================================================================
 * FUNCTION   : QCameraMemoryPool
 *
 * DESCRIPTION: default constructor of QCameraMemoryPool
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
QCameraMemoryPool::QCameraMemoryPool()
{
    pthread_mutex_init(&mLock, NULL);
}


/*===========================================================================
 * FUNCTION   : ~QCameraMemoryPool
 *
 * DESCRIPTION: deconstructor of QCameraMemoryPool
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
QCameraMemoryPool::~QCameraMemoryPool()
{
    clear();
    pthread_mutex_destroy(&mLock);
}

/*===========================================================================
 * FUNCTION   : releaseBuffer
 *
 * DESCRIPTION: release one cached buffers
 *
 * PARAMETERS :
 *   @memInfo : reference to struct that stores additional memory allocation info
 *   @streamType: Type of stream the buffers belongs to
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraMemoryPool::releaseBuffer(
        struct QCameraMemory::QCameraMemInfo &memInfo,
        cam_stream_type_t streamType)
{
    pthread_mutex_lock(&mLock);

    mPools[streamType].push_back(memInfo);

    pthread_mutex_unlock(&mLock);
}

/*===========================================================================
 * FUNCTION   : clear
 *
 * DESCRIPTION: clears all cached buffers
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraMemoryPool::clear()
{
    pthread_mutex_lock(&mLock);

    for (int i = CAM_STREAM_TYPE_DEFAULT; i < CAM_STREAM_TYPE_MAX; i++ ) {
        List<struct QCameraMemory::QCameraMemInfo>::iterator it = mPools[i].begin();
        for( ; it != mPools[i].end() ; it++) {
            QCameraMemory::deallocOneBuffer(*it);
        }

        mPools[i].clear();
    }

    pthread_mutex_unlock(&mLock);
}

/*===========================================================================
 * FUNCTION   : findBufferLocked
 *
 * DESCRIPTION: search for a appropriate cached buffer
 *
 * PARAMETERS :
 *   @memInfo : reference to struct that stores additional memory allocation info
 *   @heap_id : type of heap
 *   @size    : size of the buffer
 *   @cached  : whether the buffer should be cached
 *   @streaType: type of stream this buffer belongs to
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraMemoryPool::findBufferLocked(
        struct QCameraMemory::QCameraMemInfo &memInfo, unsigned int heap_id,
        size_t size, bool cached, cam_stream_type_t streamType)
{
    int rc = NAME_NOT_FOUND;

    if (mPools[streamType].empty()) {
        return NAME_NOT_FOUND;
    }

    List<struct QCameraMemory::QCameraMemInfo>::iterator it = mPools[streamType].begin();
    for( ; it != mPools[streamType].end() ; it++) {
        if( ((*it).size >= size) &&
            ((*it).heap_id == heap_id) &&
            ((*it).cached == cached) ) {
            memInfo = *it;
            ALOGE("%s : Found buffer %lx size %d",
                    __func__, (unsigned long)memInfo.handle, memInfo.size);
            mPools[streamType].erase(it);
            rc = NO_ERROR;
            break;
        }
    }

    return rc;
}

/*===========================================================================
 * FUNCTION   : allocateBuffer
 *
 * DESCRIPTION: allocates a buffer from the memory pool,
 *              it will re-use cached buffers if possible
 *
 * PARAMETERS :
 *   @memInfo : reference to struct that stores additional memory allocation info
 *   @heap_id : type of heap
 *   @size    : size of the buffer
 *   @cached  : whether the buffer should be cached
 *   @streaType: type of stream this buffer belongs to
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraMemoryPool::allocateBuffer(
        struct QCameraMemory::QCameraMemInfo &memInfo, unsigned int heap_id,
        size_t size, bool cached, cam_stream_type_t streamType)
{
    int rc = NO_ERROR;

    pthread_mutex_lock(&mLock);

    rc = findBufferLocked(memInfo, heap_id, size, cached, streamType);
    if (NAME_NOT_FOUND == rc ) {
        ALOGE("%s : Buffer not found!", __func__);
        rc = QCameraMemory::allocOneBuffer(memInfo, heap_id, size, cached);
    }

    pthread_mutex_unlock(&mLock);

    return rc;
}

/*===========================================================================
 * FUNCTION   : QCameraHeapMemory
 *
 * DESCRIPTION: constructor of QCameraHeapMemory for ion memory used internally in HAL
 *
 * PARAMETERS :
 *   @cached  : flag indicates if using cached memory
 *
 * RETURN     : none
 *==========================================================================*/
QCameraHeapMemory::QCameraHeapMemory(bool cached)
    : QCameraMemory(cached)
{
    for (int i = 0; i < MM_CAMERA_MAX_NUM_FRAMES; i ++)
        mPtr[i] = NULL;
}

/*===========================================================================
 * FUNCTION   : ~QCameraHeapMemory
 *
 * DESCRIPTION: deconstructor of QCameraHeapMemory
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
QCameraHeapMemory::~QCameraHeapMemory()
{
}

/*===========================================================================
 * FUNCTION   : getPtr
 *
 * DESCRIPTION: return buffer pointer
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *
 * RETURN     : buffer ptr
 *==========================================================================*/
void *QCameraHeapMemory::getPtr(uint32_t index) const
{
    if (index >= mBufferCount) {
        ALOGE("index out of bound");
        return (void *)BAD_INDEX;
    }
    return mPtr[index];
}

/*===========================================================================
 * FUNCTION   : allocate
 *
 * DESCRIPTION: allocate requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraHeapMemory::allocate(uint8_t count, size_t size)
{
    traceLogAllocStart(size, count, "HeapMemsize");
    unsigned int heap_mask = 0x1 << ION_IOMMU_HEAP_ID;
    int rc = alloc(count, size, heap_mask);
    if (rc < 0)
        return rc;

    for (int i = 0; i < count; i ++) {
        void *vaddr = mmap(NULL,
                    mMemInfo[i].size,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED,
                    mMemInfo[i].fd, 0);
        if (vaddr == MAP_FAILED) {
            for (int j = i-1; j >= 0; j --) {
                munmap(mPtr[j], mMemInfo[j].size);
                mPtr[j] = NULL;
                deallocOneBuffer(mMemInfo[j]);
            }
            return NO_MEMORY;
        } else
            mPtr[i] = vaddr;
    }
    if (rc == 0)
        mBufferCount = count;
    traceLogAllocEnd((size * count));
    return OK;
}

/*===========================================================================
 * FUNCTION   : allocateMore
 *
 * DESCRIPTION: allocate more requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraHeapMemory::allocateMore(uint8_t count, size_t size)
{
    traceLogAllocStart(size, count, "HeapMemsize");
    unsigned int heap_mask = 0x1 << ION_IOMMU_HEAP_ID;
    int rc = alloc(count, size, heap_mask);
    if (rc < 0)
        return rc;

    for (int i = mBufferCount; i < count + mBufferCount; i ++) {
        void *vaddr = mmap(NULL,
                    mMemInfo[i].size,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED,
                    mMemInfo[i].fd, 0);
        if (vaddr == MAP_FAILED) {
            for (int j = i-1; j >= mBufferCount; j --) {
                munmap(mPtr[j], mMemInfo[j].size);
                mPtr[j] = NULL;
                deallocOneBuffer(mMemInfo[j]);
            }
            return NO_MEMORY;
        } else {
            mPtr[i] = vaddr;
        }
    }
    mBufferCount = (uint8_t)(mBufferCount + count);
    traceLogAllocEnd((size * count));
    return OK;
}

/*===========================================================================
 * FUNCTION   : deallocate
 *
 * DESCRIPTION: deallocate buffers
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraHeapMemory::deallocate()
{
    for (int i = 0; i < mBufferCount; i++) {
        munmap(mPtr[i], mMemInfo[i].size);
        mPtr[i] = NULL;
    }
    dealloc();
    mBufferCount = 0;
}

/*===========================================================================
 * FUNCTION   : cacheOps
 *
 * DESCRIPTION: ion related memory cache operations
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *   @cmd     : cache ops command
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraHeapMemory::cacheOps(uint32_t index, unsigned int cmd)
{
    if (index >= mBufferCount)
        return BAD_INDEX;
    return cacheOpsInternal(index, cmd, mPtr[index]);
}

/*===========================================================================
 * FUNCTION   : getRegFlags
 *
 * DESCRIPTION: query initial reg flags
 *
 * PARAMETERS :
 *   @regFlags: initial reg flags of the allocated buffers
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraHeapMemory::getRegFlags(uint8_t * /*regFlags*/) const
{
    return INVALID_OPERATION;
}

/*===========================================================================
 * FUNCTION   : getMemory
 *
 * DESCRIPTION: get camera memory
 *
 * PARAMETERS :
 *   @index   : buffer index
 *   @metadata: flag if it's metadata
 *
 * RETURN     : camera memory ptr
 *              NULL if not supported or failed
 *==========================================================================*/
camera_memory_t *QCameraHeapMemory::getMemory(uint32_t /*index*/, bool /*metadata*/) const
{
    return NULL;
}

/*===========================================================================
 * FUNCTION   : getMatchBufIndex
 *
 * DESCRIPTION: query buffer index by opaque ptr
 *
 * PARAMETERS :
 *   @opaque  : opaque ptr
 *   @metadata: flag if it's metadata
 *
 * RETURN     : buffer index if match found,
 *              -1 if failed
 *==========================================================================*/
int QCameraHeapMemory::getMatchBufIndex(const void *opaque,
                                        bool metadata) const
{
    int index = -1;
    if (metadata) {
        return -1;
    }
    for (int i = 0; i < mBufferCount; i++) {
        if (mPtr[i] == opaque) {
            index = i;
            break;
        }
    }
    return index;
}

/*===========================================================================
 * FUNCTION   : QCameraStreamMemory
 *
 * DESCRIPTION: constructor of QCameraStreamMemory
 *              ION memory allocated directly from /dev/ion and shared with framework
 *
 * PARAMETERS :
 *   @getMemory : camera memory request ops table
 *   @cached    : flag indicates if using cached memory
 *
 * RETURN     : none
 *==========================================================================*/
QCameraStreamMemory::QCameraStreamMemory(camera_request_memory getMemory,
        bool cached,
        QCameraMemoryPool *pool,
        cam_stream_type_t streamType)
    :QCameraMemory(cached, pool, streamType),
     mGetMemory(getMemory)
{
    for (int i = 0; i < MM_CAMERA_MAX_NUM_FRAMES; i ++)
        mCameraMemory[i] = NULL;
}

/*===========================================================================
 * FUNCTION   : ~QCameraStreamMemory
 *
 * DESCRIPTION: deconstructor of QCameraStreamMemory
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
QCameraStreamMemory::~QCameraStreamMemory()
{
}

/*===========================================================================
 * FUNCTION   : allocate
 *
 * DESCRIPTION: allocate requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraStreamMemory::allocate(uint8_t count, size_t size)
{
    traceLogAllocStart(size, count, "StreamMemsize");
    unsigned int heap_mask = 0x1 << ION_IOMMU_HEAP_ID;
    int rc = alloc(count, size, heap_mask);
    if (rc < 0)
        return rc;

    for (int i = 0; i < count; i ++) {
        mCameraMemory[i] = mGetMemory(mMemInfo[i].fd, mMemInfo[i].size, 1, this);
    }
    mBufferCount = count;
    traceLogAllocEnd((size * count));
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : allocateMore
 *
 * DESCRIPTION: allocate more requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraStreamMemory::allocateMore(uint8_t count, size_t size)
{
    traceLogAllocStart(size, count, "StreamMemsize");
    unsigned int heap_mask = 0x1 << ION_IOMMU_HEAP_ID;
    int rc = alloc(count, size, heap_mask);
    if (rc < 0)
        return rc;

    for (int i = mBufferCount; i < mBufferCount + count; i++) {
        mCameraMemory[i] = mGetMemory(mMemInfo[i].fd, mMemInfo[i].size, 1, this);
    }
    mBufferCount = (uint8_t)(mBufferCount + count);
    traceLogAllocEnd((size * count));
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : deallocate
 *
 * DESCRIPTION: deallocate buffers
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraStreamMemory::deallocate()
{
    for (int i = 0; i < mBufferCount; i ++) {
        mCameraMemory[i]->release(mCameraMemory[i]);
        mCameraMemory[i] = NULL;
    }
    dealloc();
    mBufferCount = 0;
}

/*===========================================================================
 * FUNCTION   : cacheOps
 *
 * DESCRIPTION: ion related memory cache operations
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *   @cmd     : cache ops command
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraStreamMemory::cacheOps(uint32_t index, unsigned int cmd)
{
    if (index >= mBufferCount)
        return BAD_INDEX;
    return cacheOpsInternal(index, cmd, mCameraMemory[index]->data);
}

/*===========================================================================
 * FUNCTION   : getRegFlags
 *
 * DESCRIPTION: query initial reg flags
 *
 * PARAMETERS :
 *   @regFlags: initial reg flags of the allocated buffers
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraStreamMemory::getRegFlags(uint8_t *regFlags) const
{
    for (int i = 0; i < mBufferCount; i ++)
        regFlags[i] = 1;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getMemory
 *
 * DESCRIPTION: get camera memory
 *
 * PARAMETERS :
 *   @index   : buffer index
 *   @metadata: flag if it's metadata
 *
 * RETURN     : camera memory ptr
 *              NULL if not supported or failed
 *==========================================================================*/
camera_memory_t *QCameraStreamMemory::getMemory(uint32_t index,
        bool metadata) const
{
    if (index >= mBufferCount || metadata)
        return NULL;
    return mCameraMemory[index];
}

/*===========================================================================
 * FUNCTION   : getMatchBufIndex
 *
 * DESCRIPTION: query buffer index by opaque ptr
 *
 * PARAMETERS :
 *   @opaque  : opaque ptr
 *   @metadata: flag if it's metadata
 *
 * RETURN     : buffer index if match found,
 *              -1 if failed
 *==========================================================================*/
int QCameraStreamMemory::getMatchBufIndex(const void *opaque,
                                          bool metadata) const
{
    int index = -1;
    if (metadata) {
        return -1;
    }
    for (int i = 0; i < mBufferCount; i++) {
        if (mCameraMemory[i]->data == opaque) {
            index = i;
            break;
        }
    }
    return index;
}

/*===========================================================================
 * FUNCTION   : getPtr
 *
 * DESCRIPTION: return buffer pointer
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *
 * RETURN     : buffer ptr
 *==========================================================================*/
void *QCameraStreamMemory::getPtr(uint32_t index) const
{
    if (index >= mBufferCount) {
        ALOGE("index out of bound");
        return (void *)BAD_INDEX;
    }
    return mCameraMemory[index]->data;
}

/*===========================================================================
 * FUNCTION   : QCameraVideoMemory
 *
 * DESCRIPTION: constructor of QCameraVideoMemory
 *              VideoStream buffers also include metadata buffers
 *
 * PARAMETERS :
 *   @getMemory : camera memory request ops table
 *   @cached    : flag indicates if using cached ION memory
 *
 * RETURN     : none
 *==========================================================================*/
QCameraVideoMemory::QCameraVideoMemory(camera_request_memory getMemory,
                                       bool cached)
    : QCameraStreamMemory(getMemory, cached)
{
    memset(mMetadata, 0, sizeof(mMetadata));
}

/*===========================================================================
 * FUNCTION   : ~QCameraVideoMemory
 *
 * DESCRIPTION: deconstructor of QCameraVideoMemory
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
QCameraVideoMemory::~QCameraVideoMemory()
{
}

/*===========================================================================
 * FUNCTION   : allocate
 *
 * DESCRIPTION: allocate requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraVideoMemory::allocate(uint8_t count, size_t size)
{
    traceLogAllocStart(size, count, "VideoMemsize");
    int rc = QCameraStreamMemory::allocate(count, size);
    if (rc < 0)
        return rc;

    for (int i = 0; i < count; i ++) {
        mMetadata[i] = mGetMemory(-1,
                sizeof(struct encoder_media_buffer_type), 1, this);
        if (!mMetadata[i]) {
            ALOGE("allocation of video metadata failed.");
            for (int j = 0; j <= i-1; j ++)
                mMetadata[j]->release(mMetadata[j]);
            QCameraStreamMemory::deallocate();
            return NO_MEMORY;
        }
        struct encoder_media_buffer_type * packet =
            (struct encoder_media_buffer_type *)mMetadata[i]->data;
        packet->meta_handle = native_handle_create(1, 2); //1 fd, 1 offset and 1 size
        packet->buffer_type = kMetadataBufferTypeCameraSource;
        native_handle_t * nh = const_cast<native_handle_t *>(packet->meta_handle);
        nh->data[0] = mMemInfo[i].fd;
        nh->data[1] = 0;
        nh->data[2] = (int)mMemInfo[i].size;
    }
    mBufferCount = count;
    traceLogAllocEnd((size * count));
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : allocateMore
 *
 * DESCRIPTION: allocate more requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraVideoMemory::allocateMore(uint8_t count, size_t size)
{
    traceLogAllocStart(size, count, "VideoMemsize");
    int rc = QCameraStreamMemory::allocateMore(count, size);
    if (rc < 0)
        return rc;

    for (int i = mBufferCount; i < count + mBufferCount; i ++) {
        mMetadata[i] = mGetMemory(-1,
                sizeof(struct encoder_media_buffer_type), 1, this);
        if (!mMetadata[i]) {
            ALOGE("allocation of video metadata failed.");
            for (int j = mBufferCount; j <= i-1; j ++) {
                mMetadata[j]->release(mMetadata[j]);
                mCameraMemory[j]->release(mCameraMemory[j]);
                mCameraMemory[j] = NULL;
                deallocOneBuffer(mMemInfo[j]);;
            }
            return NO_MEMORY;
        }
        struct encoder_media_buffer_type * packet =
            (struct encoder_media_buffer_type *)mMetadata[i]->data;
        packet->meta_handle = native_handle_create(1, 2); //1 fd, 1 offset and 1 size
        packet->buffer_type = kMetadataBufferTypeCameraSource;
        native_handle_t * nh = const_cast<native_handle_t *>(packet->meta_handle);
        nh->data[0] = mMemInfo[i].fd;
        nh->data[1] = 0;
        nh->data[2] = (int)mMemInfo[i].size;
    }
    mBufferCount = (uint8_t)(mBufferCount + count);
    traceLogAllocEnd((size * count));
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : deallocate
 *
 * DESCRIPTION: deallocate buffers
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraVideoMemory::deallocate()
{
    for (int i = 0; i < mBufferCount; i ++) {
        struct encoder_media_buffer_type * packet =
            (struct encoder_media_buffer_type *)mMetadata[i]->data;
        if (NULL != packet) {
            native_handle_t * nh = const_cast<native_handle_t *>(packet->meta_handle);
            if (NULL != nh) {
               if (native_handle_delete(nh)) {
                   ALOGE("Unable to delete native handle");
               }
            }
            else {
               ALOGE("native handle not available");
            }
        }
        else {
            ALOGE("packet not available");
        }
        mMetadata[i]->release(mMetadata[i]);
        mMetadata[i] = NULL;
    }
    QCameraStreamMemory::deallocate();
    mBufferCount = 0;
}

/*===========================================================================
 * FUNCTION   : getMemory
 *
 * DESCRIPTION: get camera memory
 *
 * PARAMETERS :
 *   @index   : buffer index
 *   @metadata: flag if it's metadata
 *
 * RETURN     : camera memory ptr
 *              NULL if not supported or failed
 *==========================================================================*/
camera_memory_t *QCameraVideoMemory::getMemory(uint32_t index,
        bool metadata) const
{
    if (index >= mBufferCount)
        return NULL;
    if (metadata)
        return mMetadata[index];
    else
        return mCameraMemory[index];
}

/*===========================================================================
 * FUNCTION   : getMatchBufIndex
 *
 * DESCRIPTION: query buffer index by opaque ptr
 *
 * PARAMETERS :
 *   @opaque  : opaque ptr
 *   @metadata: flag if it's metadata
 *
 * RETURN     : buffer index if match found,
 *              -1 if failed
 *==========================================================================*/
int QCameraVideoMemory::getMatchBufIndex(const void *opaque,
                                         bool metadata) const
{
    int index = -1;
    for (int i = 0; i < mBufferCount; i++) {
        if (metadata) {
            if (mMetadata[i]->data == opaque) {
                index = i;
                break;
            }
        } else {
            if (mCameraMemory[i]->data == opaque) {
                index = i;
                break;
            }
        }
    }
    return index;
}

/*===========================================================================
 * FUNCTION   : QCameraGrallocMemory
 *
 * DESCRIPTION: constructor of QCameraGrallocMemory
 *              preview stream buffers are allocated from gralloc native_windoe
 *
 * PARAMETERS :
 *   @getMemory : camera memory request ops table
 *
 * RETURN     : none
 *==========================================================================*/
QCameraGrallocMemory::QCameraGrallocMemory(camera_request_memory getMemory)
        : QCameraMemory(true)
{
    mMinUndequeuedBuffers = 0;
    mWindow = NULL;
    mWidth = mHeight = mStride = mScanline = 0;
    mFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    mGetMemory = getMemory;
    for (int i = 0; i < MM_CAMERA_MAX_NUM_FRAMES; i ++) {
        mBufferHandle[i] = NULL;
        mLocalFlag[i] = BUFFER_NOT_OWNED;
        mPrivateHandle[i] = NULL;
    }
}

/*===========================================================================
 * FUNCTION   : ~QCameraGrallocMemory
 *
 * DESCRIPTION: deconstructor of QCameraGrallocMemory
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
QCameraGrallocMemory::~QCameraGrallocMemory()
{
}

/*===========================================================================
 * FUNCTION   : setWindowInfo
 *
 * DESCRIPTION: set native window gralloc ops table
 *
 * PARAMETERS :
 *   @window  : gralloc ops table ptr
 *   @width   : width of preview frame
 *   @height  : height of preview frame
 *   @stride  : stride of preview frame
 *   @scanline: scanline of preview frame
 *   @foramt  : format of preview image
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraGrallocMemory::setWindowInfo(preview_stream_ops_t *window,
        int width, int height, int stride, int scanline, int format)
{
    mWindow = window;
    mWidth = width;
    mHeight = height;
    mStride = stride;
    mScanline = scanline;
    mFormat = format;
}

/*===========================================================================
 * FUNCTION   : displayBuffer
 *
 * DESCRIPTION: send received frame to display
 *
 * PARAMETERS :
 *   @index   : index of preview frame
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraGrallocMemory::displayBuffer(uint32_t index)
{
    int err = NO_ERROR;
    int dequeuedIdx = BAD_INDEX;

    if (BUFFER_NOT_OWNED == mLocalFlag[index]) {
        ALOGE("%s: buffer to be enqueued is not owned", __func__);
        return INVALID_OPERATION;
    }

    err = mWindow->enqueue_buffer(mWindow, (buffer_handle_t *)mBufferHandle[index]);
    if(err != 0) {
        ALOGE("%s: enqueue_buffer failed, err = %d", __func__, err);
    } else {
        CDBG("%s: enqueue_buffer hdl=%p", __func__, *mBufferHandle[index]);
        mLocalFlag[index] = BUFFER_NOT_OWNED;
    }

    buffer_handle_t *buffer_handle = NULL;
    int stride = 0;
    err = mWindow->dequeue_buffer(mWindow, &buffer_handle, &stride);
    if (err == NO_ERROR && buffer_handle != NULL) {
        int i;
        CDBG("%s: dequed buf hdl =%p", __func__, *buffer_handle);
        for(i = 0; i < mBufferCount; i++) {
            if(mBufferHandle[i] == buffer_handle) {
                CDBG("%s: Found buffer in idx:%d", __func__, i);
                mLocalFlag[i] = BUFFER_OWNED;
                dequeuedIdx = i;
                break;
            }
        }
    } else {
        CDBG_HIGH("%s: dequeue_buffer, no free buffer from display now", __func__);
    }
    return dequeuedIdx;
}

/*===========================================================================
 * FUNCTION   : allocate
 *
 * DESCRIPTION: allocate requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraGrallocMemory::allocate(uint8_t count, size_t /*size*/)
{
    traceLogAllocStart(0,count, "Grallocbufcnt");
    int err = 0;
    status_t ret = NO_ERROR;
    int gralloc_usage = 0;
    struct ion_fd_data ion_info_fd;
    memset(&ion_info_fd, 0, sizeof(ion_info_fd));

    CDBG_HIGH(" %s : E ", __func__);

    if (!mWindow) {
        ALOGE("Invalid native window");
        return INVALID_OPERATION;
    }

    // Increment buffer count by min undequeued buffer.
    err = mWindow->get_min_undequeued_buffer_count(mWindow,&mMinUndequeuedBuffers);
    if (err != 0) {
        ALOGE("get_min_undequeued_buffer_count  failed: %s (%d)",
                strerror(-err), -err);
        ret = UNKNOWN_ERROR;
        goto end;
    }

    err = mWindow->set_buffer_count(mWindow, count);
    if (err != 0) {
         ALOGE("set_buffer_count failed: %s (%d)",
                    strerror(-err), -err);
         ret = UNKNOWN_ERROR;
         goto end;
    }

    err = mWindow->set_buffers_geometry(mWindow, mStride, mScanline, mFormat);
    if (err != 0) {
         ALOGE("%s: set_buffers_geometry failed: %s (%d)",
               __func__, strerror(-err), -err);
         ret = UNKNOWN_ERROR;
         goto end;
    }

    err = mWindow->set_crop(mWindow, 0, 0, mWidth, mHeight);
    if (err != 0) {
         ALOGE("%s: set_crop failed: %s (%d)",
               __func__, strerror(-err), -err);
         ret = UNKNOWN_ERROR;
         goto end;
    }

    gralloc_usage = GRALLOC_USAGE_HW_CAMERA_WRITE | GRALLOC_USAGE_PRIVATE_IOMMU_HEAP;
    err = mWindow->set_usage(mWindow, gralloc_usage);
    if(err != 0) {
        /* set_usage error out */
        ALOGE("%s: set_usage rc = %d", __func__, err);
        ret = UNKNOWN_ERROR;
        goto end;
    }
    CDBG_HIGH("%s: usage = %d, geometry: %p, %d, %d, %d, %d, %d",
          __func__, gralloc_usage, mWindow, mWidth, mHeight, mStride,
          mScanline, mFormat);

    //Allocate cnt number of buffers from native window
    for (int cnt = 0; cnt < count; cnt++) {
        int stride;
        err = mWindow->dequeue_buffer(mWindow, &mBufferHandle[cnt], &stride);
        if(!err) {
            CDBG("dequeue buf hdl =%p", mBufferHandle[cnt]);
            mLocalFlag[cnt] = BUFFER_OWNED;
        } else {
            mLocalFlag[cnt] = BUFFER_NOT_OWNED;
            ALOGE("%s: dequeue_buffer idx = %d err = %d", __func__, cnt, err);
        }

        CDBG("%s: dequeue buf: %p\n", __func__, mBufferHandle[cnt]);

        if(err != 0) {
            ALOGE("%s: dequeue_buffer failed: %s (%d)",
                  __func__, strerror(-err), -err);
            ret = UNKNOWN_ERROR;
            for(int i = 0; i < cnt; i++) {
                struct ion_handle_data ion_handle;
                memset(&ion_handle, 0, sizeof(ion_handle));
                ion_handle.handle = mMemInfo[i].handle;
                if (ioctl(mMemInfo[i].main_ion_fd, ION_IOC_FREE, &ion_handle) < 0) {
                    ALOGE("ion free failed");
                }
                if(mLocalFlag[i] != BUFFER_NOT_OWNED) {
                    err = mWindow->cancel_buffer(mWindow, mBufferHandle[i]);
                    CDBG_HIGH("%s: cancel_buffer: hdl =%p", __func__, (*mBufferHandle[i]));
                }
                mLocalFlag[i] = BUFFER_NOT_OWNED;
                mBufferHandle[i] = NULL;
            }
            memset(&mMemInfo, 0, sizeof(mMemInfo));
            goto end;
        }

        mPrivateHandle[cnt] =
            (struct private_handle_t *)(*mBufferHandle[cnt]);
        mMemInfo[cnt].main_ion_fd = open("/dev/ion", O_RDONLY);
        if (mMemInfo[cnt].main_ion_fd < 0) {
            ALOGE("%s: failed: could not open ion device", __func__);
            for(int i = 0; i < cnt; i++) {
                struct ion_handle_data ion_handle;
                memset(&ion_handle, 0, sizeof(ion_handle));
                ion_handle.handle = mMemInfo[i].handle;
                if (ioctl(mMemInfo[i].main_ion_fd, ION_IOC_FREE, &ion_handle) < 0) {
                    ALOGE("%s: ion free failed", __func__);
                }
                close(mMemInfo[i].main_ion_fd);
                if(mLocalFlag[i] != BUFFER_NOT_OWNED) {
                    err = mWindow->cancel_buffer(mWindow, mBufferHandle[i]);
                    CDBG_HIGH("%s: cancel_buffer: hdl =%p", __func__, (*mBufferHandle[i]));
                }
                mLocalFlag[i] = BUFFER_NOT_OWNED;
                mBufferHandle[i] = NULL;
            }
            memset(&mMemInfo, 0, sizeof(mMemInfo));
            ret = UNKNOWN_ERROR;
            goto end;
        } else {
            ion_info_fd.fd = mPrivateHandle[cnt]->fd;
            if (ioctl(mMemInfo[cnt].main_ion_fd,
                      ION_IOC_IMPORT, &ion_info_fd) < 0) {
                ALOGE("%s: ION import failed\n", __func__);
                for(int i = 0; i < cnt; i++) {
                    struct ion_handle_data ion_handle;
                    memset(&ion_handle, 0, sizeof(ion_handle));
                    ion_handle.handle = mMemInfo[i].handle;
                    if (ioctl(mMemInfo[i].main_ion_fd, ION_IOC_FREE, &ion_handle) < 0) {
                        ALOGE("ion free failed");
                    }
                    close(mMemInfo[i].main_ion_fd);

                    if(mLocalFlag[i] != BUFFER_NOT_OWNED) {
                        err = mWindow->cancel_buffer(mWindow, mBufferHandle[i]);
                        CDBG_HIGH("%s: cancel_buffer: hdl =%p", __func__, (*mBufferHandle[i]));
                    }
                    mLocalFlag[i] = BUFFER_NOT_OWNED;
                    mBufferHandle[i] = NULL;
                }
                close(mMemInfo[cnt].main_ion_fd);
                memset(&mMemInfo, 0, sizeof(mMemInfo));
                ret = UNKNOWN_ERROR;
                goto end;
            }
        }
        mCameraMemory[cnt] =
            mGetMemory(mPrivateHandle[cnt]->fd,
                    (size_t)mPrivateHandle[cnt]->size,
                    1,
                    (void *)this);
        CDBG("%s: idx = %d, fd = %d, size = %d, offset = %d",
              __func__, cnt, mPrivateHandle[cnt]->fd,
              mPrivateHandle[cnt]->size,
              mPrivateHandle[cnt]->offset);
        mMemInfo[cnt].fd = mPrivateHandle[cnt]->fd;
        mMemInfo[cnt].size = (size_t)mPrivateHandle[cnt]->size;
        mMemInfo[cnt].handle = ion_info_fd.handle;
    }
    mBufferCount = count;

    //Cancel min_undequeued_buffer buffers back to the window
    for (int i = 0; i < mMinUndequeuedBuffers; i ++) {
        err = mWindow->cancel_buffer(mWindow, mBufferHandle[i]);
        mLocalFlag[i] = BUFFER_NOT_OWNED;
    }

end:
    CDBG_HIGH(" %s : X ",__func__);
    traceLogAllocEnd(count);
    return ret;
}

/*===========================================================================
 * FUNCTION   : allocateMore
 *
 * DESCRIPTION: allocate more requested number of buffers of certain size
 *
 * PARAMETERS :
 *   @count   : number of buffers to be allocated
 *   @size    : lenght of the buffer to be allocated
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraGrallocMemory::allocateMore(uint8_t /*count*/, size_t /*size*/)
{
    ALOGE("%s: Not implenmented yet", __func__);
    return UNKNOWN_ERROR;
}

/*===========================================================================
 * FUNCTION   : deallocate
 *
 * DESCRIPTION: deallocate buffers
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void QCameraGrallocMemory::deallocate()
{
    CDBG("%s: E ", __FUNCTION__);

    for (int cnt = 0; cnt < mBufferCount; cnt++) {
        mCameraMemory[cnt]->release(mCameraMemory[cnt]);
        struct ion_handle_data ion_handle;
        memset(&ion_handle, 0, sizeof(ion_handle));
        ion_handle.handle = mMemInfo[cnt].handle;
        if (ioctl(mMemInfo[cnt].main_ion_fd, ION_IOC_FREE, &ion_handle) < 0) {
            ALOGE("ion free failed");
        }
        close(mMemInfo[cnt].main_ion_fd);
        if(mLocalFlag[cnt] != BUFFER_NOT_OWNED) {
            if (mWindow) {
                mWindow->cancel_buffer(mWindow, mBufferHandle[cnt]);
                CDBG_HIGH("cancel_buffer: hdl =%p", (*mBufferHandle[cnt]));
            } else {
                ALOGE("Preview window is NULL, cannot cancel_buffer: hdl =%p",
                      (*mBufferHandle[cnt]));
            }
        }
        mLocalFlag[cnt] = BUFFER_NOT_OWNED;
        CDBG_HIGH("put buffer %d successfully", cnt);
    }
    mBufferCount = 0;
    CDBG(" %s : X ",__FUNCTION__);
}

/*===========================================================================
 * FUNCTION   : cacheOps
 *
 * DESCRIPTION: ion related memory cache operations
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *   @cmd     : cache ops command
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraGrallocMemory::cacheOps(uint32_t index, unsigned int cmd)
{
    if (index >= mBufferCount)
        return BAD_INDEX;
    return cacheOpsInternal(index, cmd, mCameraMemory[index]->data);
}

/*===========================================================================
 * FUNCTION   : getRegFlags
 *
 * DESCRIPTION: query initial reg flags
 *
 * PARAMETERS :
 *   @regFlags: initial reg flags of the allocated buffers
 *
 * RETURN     : int32_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
int QCameraGrallocMemory::getRegFlags(uint8_t *regFlags) const
{
    int i = 0;
    for (i = 0; i < mMinUndequeuedBuffers; i ++)
        regFlags[i] = 0;
    for (; i < mBufferCount; i ++)
        regFlags[i] = 1;
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getMemory
 *
 * DESCRIPTION: get camera memory
 *
 * PARAMETERS :
 *   @index   : buffer index
 *   @metadata: flag if it's metadata
 *
 * RETURN     : camera memory ptr
 *              NULL if not supported or failed
 *==========================================================================*/
camera_memory_t *QCameraGrallocMemory::getMemory(uint32_t index,
        bool metadata) const
{
    if (index >= mBufferCount || metadata)
        return NULL;
    return mCameraMemory[index];
}

/*===========================================================================
 * FUNCTION   : getMatchBufIndex
 *
 * DESCRIPTION: query buffer index by opaque ptr
 *
 * PARAMETERS :
 *   @opaque  : opaque ptr
 *   @metadata: flag if it's metadata
 *
 * RETURN     : buffer index if match found,
 *              -1 if failed
 *==========================================================================*/
int QCameraGrallocMemory::getMatchBufIndex(const void *opaque,
                                           bool metadata) const
{
    int index = -1;
    if (metadata) {
        return -1;
    }
    for (int i = 0; i < mBufferCount; i++) {
        if (mCameraMemory[i]->data == opaque) {
            index = i;
            break;
        }
    }
    return index;
}

/*===========================================================================
 * FUNCTION   : getPtr
 *
 * DESCRIPTION: return buffer pointer
 *
 * PARAMETERS :
 *   @index   : index of the buffer
 *
 * RETURN     : buffer ptr
 *==========================================================================*/
void *QCameraGrallocMemory::getPtr(uint32_t index) const
{
    if (index >= mBufferCount) {
        ALOGE("index out of bound");
        return (void *)BAD_INDEX;
    }
    return mCameraMemory[index]->data;
}

}; //namespace qcamera
