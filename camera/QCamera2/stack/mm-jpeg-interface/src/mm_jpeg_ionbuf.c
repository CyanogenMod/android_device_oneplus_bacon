/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include "mm_jpeg_ionbuf.h"
#include <linux/msm_ion.h>

/** buffer_allocate:
 *
 *  Arguments:
 *     @p_buffer: ION buffer
 *
 *  Return:
 *     buffer address
 *
 *  Description:
 *      allocates ION buffer
 *
 **/
void *buffer_allocate(buffer_t *p_buffer, int cached)
{
  void *l_buffer = NULL;

  int lrc = 0;
  struct ion_handle_data lhandle_data;

   p_buffer->alloc.len = p_buffer->size;
   p_buffer->alloc.align = 4096;
   p_buffer->alloc.flags = (cached) ? ION_FLAG_CACHED : 0;
   p_buffer->alloc.heap_mask = 0x1 << ION_IOMMU_HEAP_ID;

   p_buffer->ion_fd = open("/dev/ion", O_RDONLY);
   if(p_buffer->ion_fd < 0) {
    CDBG_ERROR("%s :Ion open failed", __func__);
    goto ION_ALLOC_FAILED;
  }

  /* Make it page size aligned */
  p_buffer->alloc.len = (p_buffer->alloc.len + 4095U) & (~4095U);
  lrc = ioctl(p_buffer->ion_fd, ION_IOC_ALLOC, &p_buffer->alloc);
  if (lrc < 0) {
    CDBG_ERROR("%s :ION allocation failed len %zu", __func__,
      p_buffer->alloc.len);
    goto ION_ALLOC_FAILED;
  }

  p_buffer->ion_info_fd.handle = p_buffer->alloc.handle;
  lrc = ioctl(p_buffer->ion_fd, ION_IOC_SHARE,
    &p_buffer->ion_info_fd);
  if (lrc < 0) {
    CDBG_ERROR("%s :ION map failed %s", __func__, strerror(errno));
    goto ION_MAP_FAILED;
  }

  p_buffer->p_pmem_fd = p_buffer->ion_info_fd.fd;

  l_buffer = mmap(NULL, p_buffer->alloc.len, PROT_READ  | PROT_WRITE,
    MAP_SHARED,p_buffer->p_pmem_fd, 0);

  if (l_buffer == MAP_FAILED) {
    CDBG_ERROR("%s :ION_MMAP_FAILED: %s (%d)", __func__,
      strerror(errno), errno);
    goto ION_MAP_FAILED;
  }

  return l_buffer;

ION_MAP_FAILED:
  lhandle_data.handle = p_buffer->ion_info_fd.handle;
  ioctl(p_buffer->ion_fd, ION_IOC_FREE, &lhandle_data);
  return NULL;
ION_ALLOC_FAILED:
  return NULL;

}

/** buffer_deallocate:
 *
 *  Arguments:
 *     @p_buffer: ION buffer
 *
 *  Return:
 *     buffer address
 *
 *  Description:
 *      deallocates ION buffer
 *
 **/
int buffer_deallocate(buffer_t *p_buffer)
{
  int lrc = 0;
  size_t lsize = (p_buffer->size + 4095U) & (~4095U);

  struct ion_handle_data lhandle_data;
  lrc = munmap(p_buffer->addr, lsize);

  close(p_buffer->ion_info_fd.fd);

  lhandle_data.handle = p_buffer->ion_info_fd.handle;
  ioctl(p_buffer->ion_fd, ION_IOC_FREE, &lhandle_data);

  close(p_buffer->ion_fd);
  return lrc;
}

/** buffer_invalidate:
 *
 *  Arguments:
 *     @p_buffer: ION buffer
 *
 *  Return:
 *     error val
 *
 *  Description:
 *      Invalidates the cached buffer
 *
 **/
int buffer_invalidate(buffer_t *p_buffer)
{
  int lrc = 0;
  struct ion_flush_data cache_inv_data;
  struct ion_custom_data custom_data;

  memset(&cache_inv_data, 0, sizeof(cache_inv_data));
  memset(&custom_data, 0, sizeof(custom_data));
  cache_inv_data.vaddr = p_buffer->addr;
  cache_inv_data.fd = p_buffer->ion_info_fd.fd;
  cache_inv_data.handle = p_buffer->ion_info_fd.handle;
  cache_inv_data.length = (unsigned int)p_buffer->size;
  custom_data.cmd = (unsigned int)ION_IOC_INV_CACHES;
  custom_data.arg = (unsigned long)&cache_inv_data;

  lrc = ioctl(p_buffer->ion_fd, ION_IOC_CUSTOM, &custom_data);
  if (lrc < 0)
    CDBG_ERROR("%s: Cache Invalidate failed: %s\n", __func__, strerror(errno));

  return lrc;
}
