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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ui/DisplayInfo.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>

#include <system/camera.h>

#include <camera/Camera.h>
#include <camera/ICamera.h>
#include <camera/CameraParameters.h>
#include <media/mediarecorder.h>

#include <utils/RefBase.h>
#include <utils/Mutex.h>
#include <utils/Condition.h>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <cutils/memory.h>
#include <SkImageDecoder.h>
#include <SkImageEncoder.h>
#include <MediaCodec.h>
#include <OMX_IVCommon.h>
#include <foundation/AMessage.h>
#include <media/ICrypto.h>
#include <MediaMuxer.h>
#include <foundation/ABuffer.h>
#include <MediaErrors.h>
#include <gralloc_priv.h>

#include "qcamera_test.h"

#define ERROR(format, ...) printf( \
    "%s[%d] : ERROR: "format"\n", __func__, __LINE__, ##__VA_ARGS__)
#define VIDEO_BUF_ALLIGN(size, allign) \
  (((size) + (allign-1)) & (typeof(size))(~(allign-1)))

namespace qcamera {

using namespace android;

int CameraContext::JpegIdx = 0;
int CameraContext::mPiPIdx = 0;
const char CameraContext::KEY_ZSL[] = "zsl";

/*===========================================================================
 * FUNCTION   : previewCallback
 *
 * DESCRIPTION: preview callback preview mesages are enabled
 *
 * PARAMETERS :
 *   @mem : preview buffer
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::previewCallback(const sp<IMemory>& mem)
{
    printf("PREVIEW Callback %p", mem->pointer());
    uint8_t *ptr = (uint8_t*) mem->pointer();
    printf("PRV_CB: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
           ptr[0],
           ptr[1],
           ptr[2],
           ptr[3],
           ptr[4],
           ptr[5],
           ptr[6],
           ptr[7],
           ptr[8],
           ptr[9]);
}

/*===========================================================================
 * FUNCTION   : useLock
 *
 * DESCRIPTION: Mutex lock for CameraContext
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void CameraContext::useLock()
{
    Mutex::Autolock l(mLock);
    if ( mInUse ) {
        mCond.wait(mLock);
    } else {
        mInUse = true;
    }
}

/*===========================================================================
 * FUNCTION   : signalFinished
 *
 * DESCRIPTION: Mutex unlock CameraContext
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void CameraContext::signalFinished()
{
    Mutex::Autolock l(mLock);
    mInUse = false;
    mCond.signal();
}

/*===========================================================================
 * FUNCTION   : saveFile
 *
 * DESCRIPTION: helper function for saving buffers on filesystem
 *
 * PARAMETERS :
 *   @mem : buffer to save to filesystem
 *   @path: File path
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::saveFile(const sp<IMemory>& mem, String8 path)
{
    unsigned char *buff = NULL;
    ssize_t size;
    int fd = -1;

    if (mem == NULL) {
        return BAD_VALUE;
    }

    fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0655);
    if(fd < 0) {
        printf("Unable to open file %s %s\n", path.string(), strerror(fd));
        return -errno;
    }

    size = (ssize_t)mem->size();
    if (size <= 0) {
        printf("IMemory object is of zero size\n");
        close(fd);
        return BAD_VALUE;
    }

    buff = (unsigned char *)mem->pointer();
    if (!buff) {
        printf("Buffer pointer is invalid\n");
        close(fd);
        return BAD_VALUE;
    }

    if (size != write(fd, buff, (size_t)size)) {
        printf("Bad Write error (%d)%s\n", errno, strerror(errno));
        close(fd);
        return INVALID_OPERATION;
    }

    printf("%s: buffer=%p, size=%lld stored at %s\n",
            __FUNCTION__, buff, (long long int) size, path.string());

    if (fd >= 0)
        close(fd);

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : PiPCopyToOneFile
 *
 * DESCRIPTION: Copy the smaller picture to the bigger one
 *
 * PARAMETERS :
 *   @bitmap0 : Decoded image buffer 0
 *   @bitmap1 : Decoded image buffer 1
 *
 * RETURN     : decoded picture in picture in SkBitmap
 *==========================================================================*/
SkBitmap * CameraContext::PiPCopyToOneFile(
    SkBitmap *bitmap0, SkBitmap *bitmap1)
{
    size_t size0;
    size_t size1;
    SkBitmap *src;
    SkBitmap *dst;
    unsigned int dstOffset;
    unsigned int srcOffset;

    if (bitmap0 == NULL && bitmap1 == NULL) {
        return NULL;
    }

    size0 = bitmap0->getSize();
    if (size0 <= 0) {
        printf("Decoded image 0 is of zero size\n");
        return NULL;
    }

    size1 = bitmap1->getSize();
        if (size1 <= 0) {
            printf("Decoded image 1 is of zero size\n");
            return NULL;
        }

    if (size0 > size1) {
        dst = bitmap0;
        src = bitmap1;
    } else if (size1 > size0){
        dst = bitmap1;
        src = bitmap0;
    } else {
        printf("Picture size should be with different size!\n");
        return NULL;
    }

    for (unsigned int i = 0; i < (unsigned int)src->height(); i++) {
        dstOffset = i * (unsigned int)dst->width() * mfmtMultiplier;
        srcOffset = i * (unsigned int)src->width() * mfmtMultiplier;
        memcpy(((unsigned char *)dst->getPixels()) + dstOffset,
                ((unsigned char *)src->getPixels()) + srcOffset,
                (unsigned int)src->width() * mfmtMultiplier);
    }

    return dst;
}

/*===========================================================================
 * FUNCTION   : decodeJPEG
 *
 * DESCRIPTION: decode jpeg input buffer.
 *
 * PARAMETERS :
 *   @mem     : buffer to decode
 *   @skBM    : decoded buffer
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code

 *==========================================================================*/
status_t CameraContext::decodeJPEG(const sp<IMemory>& mem, SkBitmap *skBM)
{
    SkBitmap::Config prefConfig = SkBitmap::kARGB_8888_Config;
    const void *buff = NULL;
    size_t size;

    buff = (const void *)mem->pointer();
    size= mem->size();

    switch(prefConfig) {
        case SkBitmap::kARGB_8888_Config:
        {
            mfmtMultiplier = 4;
        }
            break;

        case SkBitmap::kARGB_4444_Config:
        {
            mfmtMultiplier = 2;
        }
        break;

        case SkBitmap::kRGB_565_Config:
        {
            mfmtMultiplier = 2;
        }
        break;

        case SkBitmap::kIndex8_Config:
        {
            mfmtMultiplier = 4;
        }
        break;

        case SkBitmap::kA8_Config:
        {
            mfmtMultiplier = 4;
        }
        break;

        default:
        {
            mfmtMultiplier = 0;
            printf("Decode format is not correct!\n");
        }
        break;
    }

#ifdef USE_KK_CODE
    if (SkImageDecoder::DecodeMemory(buff, size, skBM, prefConfig,
            SkImageDecoder::kDecodePixels_Mode) == false) {
        printf("%s():%d:: Failed during jpeg decode\n",__FUNCTION__,__LINE__);
        return BAD_VALUE;
    }
#else
    //TODO: Need to investigate and fix it as skia library has been changed.
#endif

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : encodeJPEG
 *
 * DESCRIPTION: encode the decoded input buffer.
 *
 * PARAMETERS :
 *   @stream  : SkWStream
 *   @bitmap  : SkBitmap decoded image to encode
 *   @path    : File path
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code

 *==========================================================================*/
status_t CameraContext::encodeJPEG(SkWStream * stream,
    const SkBitmap *bitmap, String8 path)
{
    int qFactor = 100;

    skJpegEnc = SkImageEncoder::Create(SkImageEncoder::kJPEG_Type);

    if (skJpegEnc->encodeStream(stream, *bitmap, qFactor) == false) {
        return BAD_VALUE;
    }

    FILE *fh = fopen(path.string(), "r+");
    if ( !fh ) {
        printf("Could not open file %s\n", path.string());
        return BAD_VALUE;
    }

    fseek(fh, 0, SEEK_END);
    size_t len = (size_t)ftell(fh);
    rewind(fh);

    if( !len ) {
        printf("File %s is empty !\n", path.string());
        fclose(fh);
        return BAD_VALUE;
    }

    unsigned char *buff = (unsigned char*)malloc(len);
    if (!buff) {
        printf("Cannot allocate memory for buffer reading!\n");
        return BAD_VALUE;
    }

    size_t readSize = fread(buff, 1, len, fh);
    if (readSize != len) {
        printf("Reading error\n");
        return BAD_VALUE;
    }

    status_t ret = ReadSectionsFromBuffer(buff, len, READ_ALL);
    if (ret != NO_ERROR) {
        printf("Cannot read sections from buffer\n");
        DiscardData();
        DiscardSections();
        return BAD_VALUE;
    }
    free(buff);
    rewind(fh);

    unsigned char temp = 0xff;
    size_t writeSize = fwrite(&temp, sizeof(unsigned char), 1, fh);
    if (1 != writeSize) {
        printf("Writing error\n");
    }
    temp = 0xd8;
    fwrite(&temp, sizeof(unsigned char), 1, fh);

    for (size_t i = 0; i < mSectionsRead; i++) {
        switch((mSections[i].Type)) {

        case 0x123:
            fwrite(mSections[i].Data, sizeof(unsigned char),
                mSections[i].Size, fh);
            break;

        case 0xe0:
            temp = 0xff;
            fwrite(&temp, sizeof(unsigned char), 1, fh);
            temp = 0xe1;
            fwrite(&temp, sizeof(unsigned char), 1, fh);
            fwrite(mJEXIFSection.Data, sizeof(unsigned char),
                mJEXIFSection.Size, fh);
            break;

        default:
            temp = 0xff;
            fwrite(&temp, sizeof(unsigned char), 1, fh);
            fwrite(&mSections[i].Type, sizeof(unsigned char), 1, fh);
            fwrite(mSections[i].Data, sizeof(unsigned char),
                mSections[i].Size, fh);
            break;
        }
    }
    fseek(fh, 0, SEEK_END);
    len = (size_t)ftell(fh);
    rewind(fh);
    printf("%s: buffer=%p, size=%zu stored at %s\n",
            __FUNCTION__, bitmap->getPixels(), len, path.string());

    free(mJEXIFSection.Data);
    DiscardData();
    DiscardSections();
    fclose(fh);
    ret = NO_ERROR;

    return ret;
}

/*===========================================================================
 * FUNCTION   : readSectionsFromBuffer
 *
 * DESCRIPTION: read all jpeg sections of input buffer.
 *
 * PARAMETERS :
 *   @mem : buffer to read from Metadata Sections
 *   @buffer_size: buffer size
 *   @ReadMode: Read mode - all, jpeg or exif
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::ReadSectionsFromBuffer (unsigned char *buffer,
        size_t buffer_size, ReadMode_t ReadMode)
{
    int a;
    size_t pos = 0;
    int HaveCom = 0;
    mSectionsAllocated = 10;

    mSections = (Sections_t *)malloc(sizeof(Sections_t)*mSectionsAllocated);

    if (!buffer) {
        printf("Input buffer is null\n");
        return BAD_VALUE;
    }

    if (buffer_size < 1) {
        printf("Input size is 0\n");
        return BAD_VALUE;
    }

    a = (int) buffer[pos++];

    if (a != 0xff || buffer[pos++] != M_SOI){
        printf("No valid image\n");
        return BAD_VALUE;
    }

    for(;;){
        size_t itemlen;
        int marker = 0;
        size_t ll,lh;
        unsigned char * Data;

        CheckSectionsAllocated();

        for (a=0;a<=16;a++){
            marker = buffer[pos++];
            if (marker != 0xff) break;

            if (a >= 16){
                fprintf(stderr,"too many padding bytes\n");
                return BAD_VALUE;
            }
        }

        mSections[mSectionsRead].Type = marker;

        // Read the length of the section.
        lh = buffer[pos++];
        ll = buffer[pos++];

        itemlen = (lh << 8) | ll;

        if (itemlen < 2) {
            ALOGE("invalid marker");
            return BAD_VALUE;
        }

        mSections[mSectionsRead].Size = itemlen;

        Data = (unsigned char *)malloc(itemlen);
        if (Data == NULL) {
            ALOGE("Could not allocate memory");
            return NO_MEMORY;
        }
        mSections[mSectionsRead].Data = Data;

        // Store first two pre-read bytes.
        Data[0] = (unsigned char)lh;
        Data[1] = (unsigned char)ll;

        if (pos+itemlen-2 > buffer_size) {
           ALOGE("Premature end of file?");
          return BAD_VALUE;
        }

        memcpy(Data+2, buffer+pos, itemlen-2); // Read the whole section.
        pos += itemlen-2;

        mSectionsRead += 1;

        switch(marker){

            case M_SOS:   // stop before hitting compressed data
                // If reading entire image is requested, read the rest of the
                // data.
                if (ReadMode & READ_IMAGE){
                    size_t size;
                    // Determine how much file is left.
                    size = buffer_size - pos;

                    if (size < 1) {
                        ALOGE("could not read the rest of the image");
                        return BAD_VALUE;
                    }
                    Data = (unsigned char *)malloc(size);
                    if (Data == NULL) {
                        ALOGE("%d: could not allocate data for entire "
                            "image size: %d", __LINE__, size);
                        return BAD_VALUE;
                    }

                    memcpy(Data, buffer+pos, size);

                    CheckSectionsAllocated();
                    mSections[mSectionsRead].Data = Data;
                    mSections[mSectionsRead].Size = size;
                    mSections[mSectionsRead].Type = PSEUDO_IMAGE_MARKER;
                    mSectionsRead ++;
                    mHaveAll = 1;
                }
                return NO_ERROR;

            case M_EOI:   // in case it's a tables-only JPEG stream
                ALOGE("No image in jpeg!\n");
                return BAD_VALUE;

            case M_COM: // Comment section
                if (HaveCom || ((ReadMode & READ_METADATA) == 0)){
                    // Discard this section.
                    free(mSections[--mSectionsRead].Data);
                }
                break;

            case M_JFIF:
                // Regular jpegs always have this tag, exif images have the
                // exif marker instead, althogh ACDsee will write images
                // with both markers.
                // this program will re-create this marker on absence of exif
                // marker.
                // hence no need to keep the copy from the file.
                if (ReadMode & READ_METADATA){
                    if (memcmp(Data+2, "JFIF", 4) == 0) {
                        break;
                    }
                    free(mSections[--mSectionsRead].Data);
                }
                break;

            case M_EXIF:
                // There can be different section using the same marker.
                if (ReadMode & READ_METADATA){
                    if (memcmp(Data+2, "Exif", 4) == 0){
                        break;
                    }else if (memcmp(Data+2, "http:", 5) == 0){
                        // Change tag for internal purposes.
                        mSections[mSectionsRead-1].Type = M_XMP;
                        break;
                    }
                }
                // Oterwise, discard this section.
                free(mSections[--mSectionsRead].Data);
                break;

            case M_IPTC:
                if (ReadMode & READ_METADATA){
                    // Note: We just store the IPTC section.
                    // Its relatively straightforward
                    // and we don't act on any part of it,
                    // so just display it at parse time.
                }else{
                    free(mSections[--mSectionsRead].Data);
                }
                break;

            case M_SOF0:
            case M_SOF1:
            case M_SOF2:
            case M_SOF3:
            case M_SOF5:
            case M_SOF6:
            case M_SOF7:
            case M_SOF9:
            case M_SOF10:
            case M_SOF11:
            case M_SOF13:
            case M_SOF14:
            case M_SOF15:
                break;
            default:
                // Skip any other sections.
                break;
        }
    }
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : CheckSectionsAllocated
 *
 * DESCRIPTION: Check allocated jpeg sections.
 *
 * PARAMETERS : none
 *
 * RETURN     : none

 *==========================================================================*/
void CameraContext::CheckSectionsAllocated(void)
{
    if (mSectionsRead > mSectionsAllocated){
        ALOGE("allocation screw up");
    }
    if (mSectionsRead >= mSectionsAllocated){
        mSectionsAllocated += mSectionsAllocated +1;
        mSections = (Sections_t *)realloc(mSections,
            sizeof(Sections_t)*mSectionsAllocated);
        if (mSections == NULL){
            ALOGE("could not allocate data for entire image");
        }
    }
}

/*===========================================================================
 * FUNCTION   : findSection
 *
 * DESCRIPTION: find the desired Section of the JPEG buffer.
 *
 * PARAMETERS :
 *  @SectionType: Section type
 *
 * RETURN     : return the found section

 *==========================================================================*/
CameraContext::Sections_t *CameraContext::FindSection(int SectionType)
{
    for (unsigned int a = 0; a < mSectionsRead; a++) {
        if (mSections[a].Type == SectionType){
            return &mSections[a];
        }
    }
    // Could not be found.
    return NULL;
}


/*===========================================================================
 * FUNCTION   : DiscardData
 *
 * DESCRIPTION: DiscardData
 *
 * PARAMETERS : none
 *
 * RETURN     : none

 *==========================================================================*/
void CameraContext::DiscardData()
{
    for (unsigned int a = 0; a < mSectionsRead; a++) {
        free(mSections[a].Data);
    }

    mSectionsRead = 0;
    mHaveAll = 0;
}

/*===========================================================================
 * FUNCTION   : DiscardSections
 *
 * DESCRIPTION: Discard allocated sections
 *
 * PARAMETERS : none
 *
 * RETURN     : none

 *==========================================================================*/
void CameraContext::DiscardSections()
{
    free(mSections);
    mSectionsAllocated = 0;
    mHaveAll = 0;
}

/*===========================================================================
 * FUNCTION   : notify
 *
 * DESCRIPTION: notify callback
 *
 * PARAMETERS :
 *   @msgType : type of callback
 *   @ext1: extended parameters
 *   @ext2: extended parameters
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::notify(int32_t msgType, int32_t ext1, int32_t ext2)
{
    printf("Notify cb: %d %d %d\n", msgType, ext1, ext2);

    if ( msgType & CAMERA_MSG_FOCUS ) {
        printf("AutoFocus %s \n",
               (ext1) ? "OK" : "FAIL");
    }

    if ( msgType & CAMERA_MSG_SHUTTER ) {
        printf("Shutter done \n");
    }

    if ( msgType & CAMERA_MSG_ERROR) {
        printf("Camera Test CAMERA_MSG_ERROR\n");
        stopPreview();
        closeCamera();
    }
}

/*===========================================================================
 * FUNCTION   : postData
 *
 * DESCRIPTION: handles data callbacks
 *
 * PARAMETERS :
 *   @msgType : type of callback
 *   @dataPtr: buffer data
 *   @metadata: additional metadata where available
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::postData(int32_t msgType,
                             const sp<IMemory>& dataPtr,
                             camera_frame_metadata_t *metadata)
{
    mInterpr->PiPLock();
    Size currentPictureSize = mSupportedPictureSizes.itemAt(
        mCurrentPictureSizeIdx);
    unsigned char *buff = NULL;
    size_t size;
    status_t ret = 0;

    memset(&mJEXIFSection, 0, sizeof(Sections_t)),

    printf("Data cb: %d\n", msgType);

    if ( msgType & CAMERA_MSG_PREVIEW_FRAME ) {
        previewCallback(dataPtr);
    }

    if ( msgType & CAMERA_MSG_RAW_IMAGE ) {
        printf("RAW done \n");
    }

    if (msgType & CAMERA_MSG_POSTVIEW_FRAME) {
        printf("Postview frame \n");
    }

    if (msgType & CAMERA_MSG_COMPRESSED_IMAGE ) {
        String8 jpegPath;
        jpegPath = jpegPath.format("/sdcard/img_%d.jpg", JpegIdx);
        if (!mPiPCapture) {
            // Normal capture case
            printf("JPEG done\n");
            saveFile(dataPtr, jpegPath);
            JpegIdx++;
        } else {
            // PiP capture case
            SkFILEWStream *wStream;
            ret = decodeJPEG(dataPtr, &skBMtmp);
            if (NO_ERROR != ret) {
                printf("Error in decoding JPEG!\n");
                return;
            }

            mWidthTmp = currentPictureSize.width;
            mHeightTmp = currentPictureSize.height;
            PiPPtrTmp = dataPtr;
            // If there are two jpeg buffers
            if (mPiPIdx == 1) {
                printf("PiP done\n");

                // Find the the capture with higher width and height and read
                // its jpeg sections
                if ((mInterpr->camera[0]->mWidthTmp * mInterpr->camera[0]->mHeightTmp) >
                        (mInterpr->camera[1]->mWidthTmp * mInterpr->camera[1]->mHeightTmp)) {
                    buff = (unsigned char *)PiPPtrTmp->pointer();
                    size= PiPPtrTmp->size();
                } else if ((mInterpr->camera[0]->mWidthTmp * mInterpr->camera[0]->mHeightTmp) <
                        (mInterpr->camera[1]->mWidthTmp * mInterpr->camera[1]->mHeightTmp)) {
                    buff = (unsigned char *)PiPPtrTmp->pointer();
                    size= PiPPtrTmp->size();
                } else {
                    printf("Cannot take PiP. Images are with the same width"
                            " and height size!!!\n");
                    return;
                }

                if (buff != NULL && size != 0) {
                    ret = ReadSectionsFromBuffer(buff, size, READ_ALL);
                    if (ret != NO_ERROR) {
                        printf("Cannot read sections from buffer\n");
                        DiscardData();
                        DiscardSections();
                        return;
                    }

                    mJEXIFTmp = FindSection(M_EXIF);
                    mJEXIFSection = *mJEXIFTmp;
                    mJEXIFSection.Data = (unsigned char*)malloc(mJEXIFTmp->Size);
                    memcpy(mJEXIFSection.Data,
                        mJEXIFTmp->Data, mJEXIFTmp->Size);
                    DiscardData();
                    DiscardSections();

                    wStream = new SkFILEWStream(jpegPath.string());
                    skBMDec = PiPCopyToOneFile(&mInterpr->camera[0]->skBMtmp,
                            &mInterpr->camera[1]->skBMtmp);
                    if (encodeJPEG(wStream, skBMDec, jpegPath) != false) {
                        printf("%s():%d:: Failed during jpeg encode\n",
                                __FUNCTION__, __LINE__);
                        return;
                    }
                    mPiPIdx = 0;
                    JpegIdx++;
                    delete wStream;
                }
            } else {
                mPiPIdx++;
            }
            disablePiPCapture();
        }
    }

    if ((msgType & CAMERA_MSG_PREVIEW_METADATA) && (NULL != metadata)) {
        printf("Face detected %d \n", metadata->number_of_faces);
    }
    mInterpr->PiPUnlock();

}

/*===========================================================================
 * FUNCTION   : postDataTimestamp
 *
 * DESCRIPTION: handles recording callbacks
 *
 * PARAMETERS :
 *   @timestamp : timestamp of buffer
 *   @msgType : type of buffer
 *   @dataPtr : buffer data
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::postDataTimestamp(nsecs_t timestamp,
                                      int32_t msgType,
                                      const sp<IMemory>& dataPtr)
{
    printf("Recording cb: %d %lld %p\n",
            msgType, (long long int)timestamp, dataPtr.get());
}

/*===========================================================================
 * FUNCTION   : dataCallbackTimestamp
 *
 * DESCRIPTION: handles recording callbacks. Used for ViV recording
 *
 * PARAMETERS :
 *   @timestamp : timestamp of buffer
 *   @msgType : type of buffer
 *   @dataPtr : buffer data
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::dataCallbackTimestamp(nsecs_t timestamp,
        int32_t msgType,
        const sp<IMemory>& dataPtr)
{
    mInterpr->ViVLock();
    // Not needed check. Just avoiding warnings of not used variables.
    if (timestamp > 0)
        timestamp = 0;
    // Not needed check. Just avoiding warnings of not used variables.
    if (msgType > 0)
        msgType = 0;
    size_t i = 0;
    void * srcBuff = NULL;
    void * dstBuff = NULL;

    size_t srcYStride = 0, dstYStride = 0;
    size_t srcUVStride = 0, dstUVStride = 0;
    size_t srcYScanLines = 0, dstYScanLines = 0;
    size_t srcUVScanLines = 0, dstUVScanLines = 0;
    size_t srcOffset = 0, dstOffset = 0;
    size_t srcBaseOffset = 0;
    size_t dstBaseOffset = 0;
    Size currentVideoSize = mSupportedVideoSizes.itemAt(mCurrentVideoSizeIdx);
    status_t err = NO_ERROR;
    ANativeWindowBuffer* anb = NULL;

    dstBuff = (void *) dataPtr->pointer();

    if (mCameraIndex == mInterpr->mViVVid.sourceCameraID) {
        srcYStride = calcStride(currentVideoSize.width);
        srcUVStride = calcStride(currentVideoSize.width);
        srcYScanLines = calcYScanLines(currentVideoSize.height);
        srcUVScanLines = calcUVScanLines(currentVideoSize.height);
        mInterpr->mViVBuff.srcWidth = (size_t)currentVideoSize.width;
        mInterpr->mViVBuff.srcHeight = (size_t)currentVideoSize.height;


        mInterpr->mViVBuff.YStride = srcYStride;
        mInterpr->mViVBuff.UVStride = srcUVStride;
        mInterpr->mViVBuff.YScanLines = srcYScanLines;
        mInterpr->mViVBuff.UVScanLines = srcUVScanLines;

        memcpy( mInterpr->mViVBuff.buff, (void *) dataPtr->pointer(),
            mInterpr->mViVBuff.buffSize);

        mInterpr->mViVVid.isBuffValid = true;
    } else if (mCameraIndex == mInterpr->mViVVid.destinationCameraID) {
        if(mInterpr->mViVVid.isBuffValid == true) {
            dstYStride = calcStride(currentVideoSize.width);
            dstUVStride = calcStride(currentVideoSize.width);
            dstYScanLines = calcYScanLines(currentVideoSize.height);
            dstUVScanLines = calcUVScanLines(currentVideoSize.height);

            srcYStride = mInterpr->mViVBuff.YStride;
            srcUVStride = mInterpr->mViVBuff.UVStride;
            srcYScanLines = mInterpr->mViVBuff.YScanLines;
            srcUVScanLines = mInterpr->mViVBuff.UVScanLines;


            for (i = 0; i < mInterpr->mViVBuff.srcHeight; i++) {
                srcOffset = i*srcYStride;
                dstOffset = i*dstYStride;
                memcpy((unsigned char *) dstBuff + dstOffset,
                    (unsigned char *) mInterpr->mViVBuff.buff +
                    srcOffset, mInterpr->mViVBuff.srcWidth);
            }
            srcBaseOffset = srcYStride * srcYScanLines;
            dstBaseOffset = dstYStride * dstYScanLines;
            for (i = 0; i < mInterpr->mViVBuff.srcHeight / 2; i++) {
                srcOffset = i*srcUVStride + srcBaseOffset;
                dstOffset = i*dstUVStride + dstBaseOffset;
                memcpy((unsigned char *) dstBuff + dstOffset,
                    (unsigned char *) mInterpr->mViVBuff.buff +
                    srcOffset, mInterpr->mViVBuff.srcWidth);
            }

            err = native_window_dequeue_buffer_and_wait(
                mInterpr->mViVVid.ANW.get(),&anb);
            if (err != NO_ERROR) {
                printf("Cannot dequeue anb for sensor %d!!!\n", mCameraIndex);
                return;
            }
            mInterpr->mViVVid.graphBuf = new GraphicBuffer(anb, false);
            if(NULL == mInterpr->mViVVid.graphBuf.get()) {
                printf("Invalid Graphic buffer\n");
                return;
            }
            err = mInterpr->mViVVid.graphBuf->lock(
                GRALLOC_USAGE_SW_WRITE_OFTEN,
                (void**)(&mInterpr->mViVVid.mappedBuff));
            if (err != NO_ERROR) {
                printf("Graphic buffer could not be locked %d!!!\n", err);
                return;
            }

            srcYStride = dstYStride;
            srcUVStride = dstUVStride;
            srcYScanLines = dstYScanLines;
            srcUVScanLines = dstUVScanLines;
            srcBuff = dstBuff;

            for (i = 0; i < (size_t)currentVideoSize.height; i++) {
                srcOffset = i*srcYStride;
                dstOffset = i*dstYStride;
                memcpy((unsigned char *) mInterpr->mViVVid.mappedBuff +
                    dstOffset, (unsigned char *) srcBuff +
                    srcOffset, (size_t)currentVideoSize.width);
            }

            srcBaseOffset = srcYStride * srcYScanLines;
            dstBaseOffset = dstUVStride * (size_t)currentVideoSize.height;

            for (i = 0; i < (size_t)currentVideoSize.height / 2; i++) {
                srcOffset = i*srcUVStride + srcBaseOffset;
                dstOffset = i*dstUVStride + dstBaseOffset;
                memcpy((unsigned char *) mInterpr->mViVVid.mappedBuff +
                    dstOffset, (unsigned char *) srcBuff +
                    srcOffset, (size_t)currentVideoSize.width);
            }


            mInterpr->mViVVid.graphBuf->unlock();

            err = mInterpr->mViVVid.ANW->queueBuffer(
                mInterpr->mViVVid.ANW.get(), anb, -1);
            if(err)
                printf("Failed to enqueue buffer to recorder!!!\n");
        }
    }
    mCamera->releaseRecordingFrame(dataPtr);

    mInterpr->ViVUnlock();
}

/*===========================================================================
 * FUNCTION   : ViVEncoderThread
 *
 * DESCRIPTION: Creates a separate thread for ViV recording
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
status_t Interpreter::ViVEncoderThread()
{
    int ret = NO_ERROR;
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    ret = pthread_create(&mViVEncThread, &attr, ThreadWrapper, this);
    ret = pthread_attr_destroy(&attr);

    return ret;
}

/*===========================================================================
 * FUNCTION   : ThreadWrapper
 *
 * DESCRIPTION: Helper function for for ViV recording thread
 *
 * PARAMETERS : Interpreter context
 *
 * RETURN     : None
 *==========================================================================*/
void *Interpreter::ThreadWrapper(void *context) {
    Interpreter *writer = static_cast<Interpreter *>(context);
    writer->ViVEncode();
    return NULL;
}

/*===========================================================================
 * FUNCTION   : ViVEncode
 *
 * DESCRIPTION: Thread for ViV encode. Buffers from video codec are sent to
 *              muxer and saved in a file.
 *
 * PARAMETERS : Interpreter context
 *
 * RETURN     : None
 *==========================================================================*/
void Interpreter::ViVEncode()
{
    status_t err = NO_ERROR;
    ssize_t trackIdx = -1;
    uint32_t debugNumFrames = 0;

    size_t bufIndex, offset, size;
    int64_t ptsUsec;
    uint32_t flags;
    bool DoRecording = true;


    err = mTestContext->mViVVid.codec->getOutputBuffers(
        &mTestContext->mViVVid.buffers);
    if (err != NO_ERROR) {
        printf("Unable to get output buffers (err=%d)\n", err);
    }

    while (DoRecording) {
        err = mTestContext->mViVVid.codec->dequeueOutputBuffer(
            &bufIndex,
            &offset,
            &size,
            &ptsUsec,
            &flags, -1);

        switch (err) {

        case NO_ERROR:
            // got a buffer
            if ((flags & MediaCodec::BUFFER_FLAG_CODECCONFIG) != 0) {
                // ignore this -- we passed the CSD into MediaMuxer when
                // we got the format change notification
                size = 0;
            }
            if (size != 0 && trackIdx >= 0) {
                // If the virtual display isn't providing us with timestamps,
                // use the current time.
                if (ptsUsec == 0) {
                    ptsUsec = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
                }

                // The MediaMuxer docs are unclear, but it appears that we
                // need to pass either the full set of BufferInfo flags, or
                // (flags & BUFFER_FLAG_SYNCFRAME).
                err = mTestContext->mViVVid.muxer->writeSampleData(
                    mTestContext->mViVVid.buffers[bufIndex],
                    (size_t) trackIdx,
                    ptsUsec,
                    flags);
                if (err != NO_ERROR) {
                    fprintf(stderr, "Failed writing data to muxer (err=%d)\n",
                            err);
                }
                debugNumFrames++;
            }
            err = mTestContext->mViVVid.codec->releaseOutputBuffer(bufIndex);
            if (err != NO_ERROR) {
                fprintf(stderr, "Unable to release output buffer (err=%d)\n",
                        err);
            }
            if ((flags & MediaCodec::BUFFER_FLAG_EOS) != 0) {
                // Not expecting EOS from SurfaceFlinger.  Go with it.
                printf("Received end-of-stream\n");
                //DoRecording = false;
            }
            break;
        case -EAGAIN:                       // INFO_TRY_AGAIN_LATER
            ALOGV("Got -EAGAIN, looping");
            break;
        case INFO_FORMAT_CHANGED:           // INFO_OUTPUT_FORMAT_CHANGED
        {
            // format includes CSD, which we must provide to muxer
            sp<AMessage> newFormat;
            mTestContext->mViVVid.codec->getOutputFormat(&newFormat);
            trackIdx = mTestContext->mViVVid.muxer->addTrack(newFormat);
            err = mTestContext->mViVVid.muxer->start();
            if (err != NO_ERROR) {
                printf("Unable to start muxer (err=%d)\n", err);
            }
        }
        break;
        case INFO_OUTPUT_BUFFERS_CHANGED:   // INFO_OUTPUT_BUFFERS_CHANGED
            // not expected for an encoder; handle it anyway
            ALOGV("Encoder buffers changed");
            err = mTestContext->mViVVid.codec->getOutputBuffers(
                &mTestContext->mViVVid.buffers);
            if (err != NO_ERROR) {
                printf("Unable to get new output buffers (err=%d)\n", err);
            }
        break;
        case INVALID_OPERATION:
            DoRecording = false;
        break;
        default:
            printf("Got weird result %d from dequeueOutputBuffer\n", err);
        break;
        }
    }

    return;
}

/*===========================================================================
 * FUNCTION   : calcBufferSize
 *
 * DESCRIPTION: Temp buffer size calculation. Temp buffer is used to store
 *              the buffer from the camera with smaller resolution. It is
 *              copied to the buffer from camera with higher resolution.
 *
 * PARAMETERS :
 *   @width   : video size width
 *   @height  : video size height
 *
 * RETURN     : size_t
 *==========================================================================*/
size_t CameraContext::calcBufferSize(int width, int height)
{
    size_t size = 0;
    size_t UVAlignment;
    size_t YPlane, UVPlane, YStride, UVStride, YScanlines, UVScanlines;
    if (!width || !height) {
        return size;
    }
    UVAlignment = 4096;
    YStride = calcStride(width);
    UVStride = calcStride(width);
    YScanlines = calcYScanLines(height);
    UVScanlines = calcUVScanLines(height);
    YPlane = YStride * YScanlines;
    UVPlane = UVStride * UVScanlines + UVAlignment;
    size = YPlane + UVPlane;
    size = VIDEO_BUF_ALLIGN(size, 4096);

    return size;
}

/*===========================================================================
 * FUNCTION   : calcStride
 *
 * DESCRIPTION: Temp buffer stride calculation.
 *
 * PARAMETERS :
 *   @width   : video size width
 *
 * RETURN     : size_t
 *==========================================================================*/
size_t CameraContext::calcStride(int width)
{
    size_t alignment, stride = 0;
    if (!width) {
        return stride;
    }
    alignment = 128;
    stride = VIDEO_BUF_ALLIGN((size_t)width, alignment);

    return stride;
}

/*===========================================================================
 * FUNCTION   : calcYScanLines
 *
 * DESCRIPTION: Temp buffer scanlines calculation for Y plane.
 *
 * PARAMETERS :
 *   @width   : video size height
 *
 * RETURN     : size_t
 *==========================================================================*/
size_t CameraContext::calcYScanLines(int height)
{
    size_t alignment, scanlines = 0;
        if (!height) {
            return scanlines;
        }
    alignment = 32;
    scanlines = VIDEO_BUF_ALLIGN((size_t)height, alignment);

    return scanlines;
}

/*===========================================================================
 * FUNCTION   : calcUVScanLines
 *
 * DESCRIPTION: Temp buffer scanlines calculation for UV plane.
 *
 * PARAMETERS :
 *   @width   : video size height
 *
 * RETURN     : size_t
 *==========================================================================*/
size_t CameraContext::calcUVScanLines(int height)
{
    size_t alignment, scanlines = 0;
    if (!height) {
        return scanlines;
    }
    alignment = 16;
    scanlines = VIDEO_BUF_ALLIGN((size_t)((height + 1) >> 1), alignment);

    return scanlines;
}

/*===========================================================================
 * FUNCTION   : printSupportedParams
 *
 * DESCRIPTION: dump common supported parameters
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::printSupportedParams()
{
    printf("\n\r\tSupported Cameras: %s",
           mParams.get("camera-indexes")?
               mParams.get("camera-indexes") : "NULL");
    printf("\n\r\tSupported Picture Sizes: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES)?
           mParams.get(
               CameraParameters::KEY_SUPPORTED_PICTURE_SIZES) : "NULL");
    printf("\n\r\tSupported Picture Formats: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS)?
           mParams.get(
               CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS) : "NULL");
    printf("\n\r\tSupported Preview Sizes: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES)?
           mParams.get(
               CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES) : "NULL");
    printf("\n\r\tSupported Video Sizes: %s",
            mParams.get(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES)?
            mParams.get(
               CameraParameters::KEY_SUPPORTED_VIDEO_SIZES) : "NULL");
    printf("\n\r\tSupported Preview Formats: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS)?
           mParams.get(
               CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS) : "NULL");
    printf("\n\r\tSupported Preview Frame Rates: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES)?
           mParams.get(
               CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES) : "NULL");
    printf("\n\r\tSupported Thumbnail Sizes: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES)?
           mParams.get(
               CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES) : "NULL");
    printf("\n\r\tSupported Whitebalance Modes: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE)?
           mParams.get(
               CameraParameters::KEY_SUPPORTED_WHITE_BALANCE) : "NULL");
    printf("\n\r\tSupported Effects: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_EFFECTS)?
           mParams.get(CameraParameters::KEY_SUPPORTED_EFFECTS) : "NULL");
    printf("\n\r\tSupported Scene Modes: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_SCENE_MODES)?
           mParams.get(CameraParameters::KEY_SUPPORTED_SCENE_MODES) : "NULL");
    printf("\n\r\tSupported Focus Modes: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_FOCUS_MODES)?
           mParams.get(CameraParameters::KEY_SUPPORTED_FOCUS_MODES) : "NULL");
    printf("\n\r\tSupported Antibanding Options: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_ANTIBANDING)?
           mParams.get(CameraParameters::KEY_SUPPORTED_ANTIBANDING) : "NULL");
    printf("\n\r\tSupported Flash Modes: %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_FLASH_MODES)?
           mParams.get(CameraParameters::KEY_SUPPORTED_FLASH_MODES) : "NULL");
    printf("\n\r\tSupported Focus Areas: %d",
           mParams.getInt(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS));
    printf("\n\r\tSupported FPS ranges : %s",
           mParams.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE)?
           mParams.get(
               CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE) : "NULL");
    printf("\n\r\tFocus Distances: %s \n",
           mParams.get(CameraParameters::KEY_FOCUS_DISTANCES)?
           mParams.get(CameraParameters::KEY_FOCUS_DISTANCES) : "NULL");
}

/*===========================================================================
 * FUNCTION   : createPreviewSurface
 *
 * DESCRIPTION: helper function for creating preview surfaces
 *
 * PARAMETERS :
 *   @width : preview width
 *   @height: preview height
 *   @pixFormat : surface pixelformat
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::createPreviewSurface(int width, int height, int32_t pixFormat)
{
    int ret = NO_ERROR;
    DisplayInfo dinfo;
    sp<IBinder> display(SurfaceComposerClient::getBuiltInDisplay(
                        ISurfaceComposer::eDisplayIdMain));
    SurfaceComposerClient::getDisplayInfo(display, &dinfo);
    uint32_t previewWidth, previewHeight;

    if ((0 >= width) || (0 >= height)) {
        printf("Bad preview surface size %dx%d\n", width, height);
        return BAD_VALUE;
    }

    if ((int)dinfo.w < width) {
        previewWidth = dinfo.w;
    } else {
        previewWidth = (unsigned int)width;
    }

    if ((int)dinfo.h < height) {
        previewHeight = dinfo.h;
    } else {
        previewHeight = (unsigned int)height;
    }

    mClient = new SurfaceComposerClient();

    if ( NULL == mClient.get() ) {
        printf("Unable to establish connection to Surface Composer \n");
        return NO_INIT;
    }

    mSurfaceControl = mClient->createSurface(String8("QCamera_Test"),
                                             previewWidth,
                                             previewHeight,
                                             pixFormat,
                                             0);
    if ( NULL == mSurfaceControl.get() ) {
        printf("Unable to create preview surface \n");
        return NO_INIT;
    }

    mPreviewSurface = mSurfaceControl->getSurface();
    if ( NULL != mPreviewSurface.get() ) {
        mClient->openGlobalTransaction();
        ret |= mSurfaceControl->setLayer(0x7fffffff);
        if ( mCameraIndex == 0 )
            ret |= mSurfaceControl->setPosition(0, 0);
        else
            ret |= mSurfaceControl->setPosition((float)(dinfo.w - previewWidth),
                    (float)(dinfo.h - previewHeight));

        ret |= mSurfaceControl->setSize(previewWidth, previewHeight);
        ret |= mSurfaceControl->show();
        mClient->closeGlobalTransaction();

        if ( NO_ERROR != ret ) {
            printf("Preview surface configuration failed! \n");
        }
    } else {
        ret = NO_INIT;
    }

    return ret;
}

/*===========================================================================
 * FUNCTION   : destroyPreviewSurface
 *
 * DESCRIPTION: closes previously open preview surface
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::destroyPreviewSurface()
{
    if ( NULL != mPreviewSurface.get() ) {
        mPreviewSurface.clear();
    }

    if ( NULL != mSurfaceControl.get() ) {
        mSurfaceControl->clear();
        mSurfaceControl.clear();
    }

    if ( NULL != mClient.get() ) {
        mClient->dispose();
        mClient.clear();
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : CameraContext
 *
 * DESCRIPTION: camera context constructor
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
CameraContext::CameraContext(int cameraIndex) :
    mCameraIndex(cameraIndex),
    mResizePreview(true),
    mHardwareActive(false),
    mPreviewRunning(false),
    mRecordRunning(false),
    mVideoFd(-1),
    mVideoIdx(0),
    mRecordingHint(false),
    mDoPrintMenu(true),
    mPiPCapture(false),
    mfmtMultiplier(1),
    mSectionsRead(false),
    mSectionsAllocated(0),
    mSections(NULL),
    mJEXIFTmp(NULL),
    mHaveAll(false),
    mCamera(NULL),
    mClient(NULL),
    mSurfaceControl(NULL),
    mPreviewSurface(NULL),
    mInUse(false)
{
    mRecorder = new MediaRecorder();
}

/*===========================================================================
 * FUNCTION     : setTestCtxInstance
 *
 * DESCRIPTION  : Sends TestContext instance to CameraContext
 *
 * PARAMETERS   :
 *    @instance : TestContext instance
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::setTestCtxInstance(TestContext  *instance)
{
    mInterpr = instance;
}

/*===========================================================================
 * FUNCTION     : setTestCtxInst
 *
 * DESCRIPTION  : Sends TestContext instance to Interpreter
 *
 * PARAMETERS   :
 *    @instance : TestContext instance
 *
 * RETURN     : None
 *==========================================================================*/
void Interpreter::setTestCtxInst(TestContext  *instance)
{
    mTestContext = instance;
}

/*===========================================================================
 * FUNCTION   : ~CameraContext
 *
 * DESCRIPTION: camera context destructor
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
CameraContext::~CameraContext()
{
    stopPreview();
    closeCamera();
}

/*===========================================================================
 * FUNCTION   : openCamera
 *
 * DESCRIPTION: connects to and initializes camera
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t  CameraContext::openCamera()
{
    useLock();
    const char *ZSLStr = NULL;

    if ( NULL != mCamera.get() ) {
        printf("Camera already open! \n");
        return NO_ERROR;
    }

    printf("openCamera(camera_index=%d)\n", mCameraIndex);

#ifndef USE_JB_MR1

    String16 packageName("CameraTest");

    mCamera = Camera::connect(mCameraIndex,
                              packageName,
                              Camera::USE_CALLING_UID);

#else

    mCamera = Camera::connect(mCameraIndex);

#endif

    if ( NULL == mCamera.get() ) {
        printf("Unable to connect to CameraService\n");
        return NO_INIT;
    }

    mParams = mCamera->getParameters();
    mParams.getSupportedPreviewSizes(mSupportedPreviewSizes);
    mParams.getSupportedPictureSizes(mSupportedPictureSizes);
    mParams.getSupportedVideoSizes(mSupportedVideoSizes);

    mCurrentPictureSizeIdx = mSupportedPictureSizes.size() / 2;
    mCurrentPreviewSizeIdx = mSupportedPreviewSizes.size() / 2;
    mCurrentVideoSizeIdx   = mSupportedVideoSizes.size() / 2;

    mCamera->setListener(this);
    mHardwareActive = true;

    mInterpr->setViVSize((Size) mSupportedVideoSizes.itemAt(
        mCurrentVideoSizeIdx),
        mCameraIndex);

    ZSLStr = mParams.get(CameraContext::KEY_ZSL);
    if (NULL != ZSLStr) {
        if (strcmp(ZSLStr, "on") == 0){
            mInterpr->mIsZSLOn = true;
        } else if (strcmp(ZSLStr, "off") == 0) {
            mInterpr->mIsZSLOn = false;
        } else {
            printf("zsl value is not valid!\n");
        }
    } else {
        printf("zsl is NULL");
    }

    signalFinished();

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : onAsBinder
 *
 * DESCRIPTION: onAsBinder
 *
 * PARAMETERS : None
 *
 * RETURN     : Pointer to IBinder
 *==========================================================================*/
IBinder* CameraContext::onAsBinder() {
    return NULL;
}

/*===========================================================================
 * FUNCTION   : getNumberOfCameras
 *
 * DESCRIPTION: returns the number of supported camera by the system
 *
 * PARAMETERS : None
 *
 * RETURN     : supported camera count
 *==========================================================================*/
int CameraContext::getNumberOfCameras()
{
    int ret = -1;

    if ( NULL != mCamera.get() ) {
        ret = mCamera->getNumberOfCameras();
    }

    return ret;
}

/*===========================================================================
 * FUNCTION   : closeCamera
 *
 * DESCRIPTION: closes a previously the initialized camera reference
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::closeCamera()
{
    useLock();
    if ( NULL == mCamera.get() ) {
        return NO_INIT;
    }

    mCamera->disconnect();
    mCamera.clear();

    mRecorder->init();
    mRecorder->close();
    mRecorder->release();
    mRecorder.clear();

    mHardwareActive = false;
    mPreviewRunning = false;
    mRecordRunning = false;

    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : startPreview
 *
 * DESCRIPTION: starts camera preview
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::startPreview()
{
    useLock();

    int ret = NO_ERROR;
    int previewWidth, previewHeight;
    Size currentPreviewSize = mSupportedPreviewSizes.itemAt(
        mCurrentPreviewSizeIdx);
    Size currentPictureSize = mSupportedPictureSizes.itemAt(
        mCurrentPictureSizeIdx);
    Size currentVideoSize   = mSupportedVideoSizes.itemAt(
        mCurrentVideoSizeIdx);

#ifndef USE_JB_MR1

    sp<IGraphicBufferProducer> gbp;

#endif

    if (!mHardwareActive ) {
        printf("Camera not active! \n");
        return NO_INIT;
    }

    if (mPreviewRunning) {
        printf("Preview is already running! \n");
        signalFinished();
        return NO_ERROR;
    }

    if (mResizePreview) {
        mPreviewRunning = false;

        if ( mRecordingHint ) {
            previewWidth = currentVideoSize.width;
            previewHeight = currentVideoSize.height;
        } else {
            previewWidth = currentPreviewSize.width;
            previewHeight = currentPreviewSize.height;
        }

        ret = createPreviewSurface(previewWidth,
                                   previewHeight,
                                   HAL_PIXEL_FORMAT_YCrCb_420_SP);
        if (  NO_ERROR != ret ) {
            printf("Error while creating preview surface\n");
            return ret;
        }

        mParams.setPreviewSize(previewWidth, previewHeight);
        mParams.setPictureSize(currentPictureSize.width,
            currentPictureSize.height);
        mParams.setVideoSize(
            currentVideoSize.width, currentVideoSize.height);

        ret |= mCamera->setParameters(mParams.flatten());

#ifndef USE_JB_MR1

        gbp = mPreviewSurface->getIGraphicBufferProducer();
#ifndef USE_VENDOR_CAMERA_EXT
        ret |= mCamera->setPreviewTarget(gbp);
#else
        ret |= mCamera->setPreviewTexture(gbp);
#endif

#else

        ret |= mCamera->setPreviewDisplay(mPreviewSurface);

#endif
        mResizePreview = false;
    }

    if ( !mPreviewRunning ) {
        ret |= mCamera->startPreview();
        if ( NO_ERROR != ret ) {
            printf("Preview start failed! \n");
            return ret;
        }

        mPreviewRunning = true;
    }

    signalFinished();

    return ret;
}

/*===========================================================================
 * FUNCTION   : autoFocus
 *
 * DESCRIPTION: Triggers autofocus
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::autoFocus()
{
    useLock();
    status_t ret = NO_ERROR;

    if ( mPreviewRunning ) {
        ret = mCamera->autoFocus();
    }

    signalFinished();
    return ret;
}

/*===========================================================================
 * FUNCTION   : enablePreviewCallbacks
 *
 * DESCRIPTION: Enables preview callback messages
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::enablePreviewCallbacks()
{
    useLock();
    if ( mHardwareActive ) {
        mCamera->setPreviewCallbackFlags(
            CAMERA_FRAME_CALLBACK_FLAG_ENABLE_MASK);
    }

    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : takePicture
 *
 * DESCRIPTION: triggers image capture
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::takePicture()
{
    status_t ret = NO_ERROR;
    useLock();
    if ( mPreviewRunning ) {
        ret = mCamera->takePicture(
            CAMERA_MSG_COMPRESSED_IMAGE|
            CAMERA_MSG_RAW_IMAGE);
        if (!mRecordingHint && !mInterpr->mIsZSLOn) {
            mPreviewRunning = false;
        }
    } else {
        printf("Please resume/start the preview before taking a picture!\n");
    }
    signalFinished();
    return ret;
}

/*===========================================================================
 * FUNCTION   : configureRecorder
 *
 * DESCRIPTION: Configure video recorder
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::configureRecorder()
{
    useLock();
    status_t ret = NO_ERROR;

    mResizePreview = true;
    mParams.set("recording-hint", "true");
    mRecordingHint = true;
    mCamera->setParameters(mParams.flatten());

    Size videoSize = mSupportedVideoSizes.itemAt(mCurrentVideoSizeIdx);
    ret = mRecorder->setParameters(
        String8("video-param-encoding-bitrate=64000"));
    if ( ret != NO_ERROR ) {
        ERROR("Could not configure recorder (%d)", ret);
        return ret;
    }

    ret = mRecorder->setCamera(
        mCamera->remote(), mCamera->getRecordingProxy());
    if ( ret != NO_ERROR ) {
        ERROR("Could not set camera (%d)", ret);
        return ret;
    }
    ret = mRecorder->setVideoSource(VIDEO_SOURCE_CAMERA);
    if ( ret != NO_ERROR ) {
        ERROR("Could not set video soruce (%d)", ret);
        return ret;
    }
    ret = mRecorder->setAudioSource(AUDIO_SOURCE_DEFAULT);
    if ( ret != NO_ERROR ) {
        ERROR("Could not set audio source (%d)", ret);
        return ret;
    }
    ret = mRecorder->setOutputFormat(OUTPUT_FORMAT_DEFAULT);
    if ( ret != NO_ERROR ) {
        ERROR("Could not set output format (%d)", ret);
        return ret;
    }

    ret = mRecorder->setVideoEncoder(VIDEO_ENCODER_DEFAULT);
    if ( ret != NO_ERROR ) {
        ERROR("Could not set video encoder (%d)", ret);
        return ret;
    }

    char fileName[100];

    snprintf(fileName, 100,  "/sdcard/vid_cam%d_%dx%d_%d.mpeg", mCameraIndex,
            videoSize.width, videoSize.height, mVideoIdx++);

    mVideoFd = open(fileName, O_CREAT | O_RDWR );

    if ( mVideoFd < 0 ) {
        ERROR("Could not open video file for writing %s!", fileName);
        return UNKNOWN_ERROR;
    }

    ret = mRecorder->setOutputFile(mVideoFd, 0, 0);
    if ( ret != NO_ERROR ) {
        ERROR("Could not set output file (%d)", ret);
        return ret;
    }

    ret = mRecorder->setVideoSize(videoSize.width, videoSize.height);
    if ( ret  != NO_ERROR ) {
        ERROR("Could not set video size %dx%d", videoSize.width,
            videoSize.height);
        return ret;
    }

    ret = mRecorder->setVideoFrameRate(30);
    if ( ret != NO_ERROR ) {
        ERROR("Could not set video frame rate (%d)", ret);
        return ret;
    }

    ret = mRecorder->setAudioEncoder(AUDIO_ENCODER_DEFAULT);
    if ( ret != NO_ERROR ) {
        ERROR("Could not set audio encoder (%d)", ret);
        return ret;
    }

    signalFinished();
    return ret;
}

/*===========================================================================
 * FUNCTION   : unconfigureViVRecording
 *
 * DESCRIPTION: Unconfigures video in video recording
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::unconfigureRecorder()
{
    useLock();

    if ( !mRecordRunning ) {
        mResizePreview = true;
        mParams.set("recording-hint", "false");
        mRecordingHint = false;
        mCamera->setParameters(mParams.flatten());
    }

    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : configureViVRecording
 *
 * DESCRIPTION: Configures video in video recording
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::configureViVRecording()
{
    status_t ret = NO_ERROR;

    mResizePreview = true;
    mParams.set("recording-hint", "true");
    mRecordingHint = true;
    mCamera->setParameters(mParams.flatten());
    mCamera->setRecordingProxyListener(this);

    signalFinished();
    return ret;
}

/*===========================================================================
 * FUNCTION   : startRecording
 *
 * DESCRIPTION: triggers start recording
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::startRecording()
{
    useLock();
    status_t ret = NO_ERROR;

    if ( mPreviewRunning ) {

        mCamera->unlock();

        ret = mRecorder->prepare();
        if ( ret != NO_ERROR ) {
            ERROR("Could not prepare recorder");
            return ret;
        }

        ret = mRecorder->start();
        if ( ret != NO_ERROR ) {
            ERROR("Could not start recorder");
            return ret;
        }

        mRecordRunning = true;
    }
    signalFinished();
    return ret;
}

/*===========================================================================
 * FUNCTION   : stopRecording
 *
 * DESCRIPTION: triggers start recording
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::stopRecording()
{
    useLock();
    status_t ret = NO_ERROR;

    if ( mRecordRunning ) {
            mRecorder->stop();
            close(mVideoFd);
            mVideoFd = -1;

        mRecordRunning = false;
    }

    signalFinished();

    return ret;
}

/*===========================================================================
 * FUNCTION   : startViVRecording
 *
 * DESCRIPTION: Starts video in video recording
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::startViVRecording()
{
    useLock();
    status_t ret;

    if (mInterpr->mViVVid.VideoSizes[0].width *
            mInterpr->mViVVid.VideoSizes[0].height >=
            mInterpr->mViVVid.VideoSizes[1].width *
            mInterpr->mViVVid.VideoSizes[1].height) {
        mInterpr->mViVBuff.buffSize = calcBufferSize(
            mInterpr->mViVVid.VideoSizes[1].width,
            mInterpr->mViVVid.VideoSizes[1].height);
        if (mInterpr->mViVBuff.buff == NULL) {
            mInterpr->mViVBuff.buff =
                (void *)malloc(mInterpr->mViVBuff.buffSize);
        }
        mInterpr->mViVVid.sourceCameraID = 1;
        mInterpr->mViVVid.destinationCameraID = 0;

    } else {
        mInterpr->mViVBuff.buffSize = calcBufferSize(
            mInterpr->mViVVid.VideoSizes[0].width,
            mInterpr->mViVVid.VideoSizes[0].height);
        if (mInterpr->mViVBuff.buff == NULL) {
            mInterpr->mViVBuff.buff =
                (void *)malloc(mInterpr->mViVBuff.buffSize);
        }
        mInterpr->mViVVid.sourceCameraID = 0;
        mInterpr->mViVVid.destinationCameraID = 1;
    }

    ret = mCamera->startRecording();

    signalFinished();
    return ret;
}

/*===========================================================================
 * FUNCTION   : stopViVRecording
 *
 * DESCRIPTION: Stops video in video recording
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::stopViVRecording()
{
    useLock();
    status_t ret = NO_ERROR;

    mCamera->stopRecording();

    signalFinished();
    return ret;
}

/*===========================================================================
 * FUNCTION   : stopPreview
 *
 * DESCRIPTION: stops camera preview
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::stopPreview()
{
    useLock();
    status_t ret = NO_ERROR;

    if ( mHardwareActive ) {
        mCamera->stopPreview();
        ret = destroyPreviewSurface();
    }

    mPreviewRunning  = false;
    mResizePreview = true;

    signalFinished();

    return ret;
}

/*===========================================================================
 * FUNCTION   : resumePreview
 *
 * DESCRIPTION: resumes camera preview after image capture
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::resumePreview()
{
    useLock();
    status_t ret = NO_ERROR;

    if ( mHardwareActive ) {
        ret = mCamera->startPreview();
        mPreviewRunning = true;
    } else {
        ret = NO_INIT;
    }

    signalFinished();
    return ret;
}

/*===========================================================================
 * FUNCTION   : nextPreviewSize
 *
 * DESCRIPTION: Iterates through all supported preview sizes.
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::nextPreviewSize()
{
    useLock();
    if ( mHardwareActive ) {
        mCurrentPreviewSizeIdx += 1;
        mCurrentPreviewSizeIdx %= mSupportedPreviewSizes.size();
        Size previewSize = mSupportedPreviewSizes.itemAt(
            mCurrentPreviewSizeIdx);
        mParams.setPreviewSize(previewSize.width,
                               previewSize.height);
        mResizePreview = true;

        if ( mPreviewRunning ) {
            mCamera->stopPreview();
            mCamera->setParameters(mParams.flatten());
            mCamera->startPreview();
        } else {
            mCamera->setParameters(mParams.flatten());
        }
    }

    signalFinished();
    return NO_ERROR;
}


/*===========================================================================
 * FUNCTION   : setPreviewSize
 *
 * DESCRIPTION: Sets exact preview size if supported
 *
 * PARAMETERS : format size in the form of WIDTHxHEIGHT
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::setPreviewSize(const char *format)
{
    useLock();
    if ( mHardwareActive ) {
        int newHeight;
        int newWidth;
        sscanf(format, "%dx%d", &newWidth, &newHeight);

        unsigned int i;
        for (i = 0; i < mSupportedPreviewSizes.size(); ++i) {
            Size previewSize = mSupportedPreviewSizes.itemAt(i);
            if ( newWidth == previewSize.width &&
                 newHeight == previewSize.height )
            {
                break;
            }

        }
        if ( i == mSupportedPreviewSizes.size())
        {
            printf("Preview size %dx%d not supported !\n",
                newWidth, newHeight);
            return INVALID_OPERATION;
        }

        mParams.setPreviewSize(newWidth,
                               newHeight);
        mResizePreview = true;

        if ( mPreviewRunning ) {
            mCamera->stopPreview();
            mCamera->setParameters(mParams.flatten());
            mCamera->startPreview();
        } else {
            mCamera->setParameters(mParams.flatten());
        }
    }

    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getCurrentPreviewSize
 *
 * DESCRIPTION: queries the currently configured preview size
 *
 * PARAMETERS :
 *  @previewSize : preview size currently configured
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::getCurrentPreviewSize(Size &previewSize)
{
    useLock();
    if ( mHardwareActive ) {
        previewSize = mSupportedPreviewSizes.itemAt(mCurrentPreviewSizeIdx);
    }
    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : nextPictureSize
 *
 * DESCRIPTION: Iterates through all supported picture sizes.
 *
 * PARAMETERS : None
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::nextPictureSize()
{
    useLock();
    if ( mHardwareActive ) {
        mCurrentPictureSizeIdx += 1;
        mCurrentPictureSizeIdx %= mSupportedPictureSizes.size();
        Size pictureSize = mSupportedPictureSizes.itemAt(
            mCurrentPictureSizeIdx);
        mParams.setPictureSize(pictureSize.width,
            pictureSize.height);
        mCamera->setParameters(mParams.flatten());
    }
    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setPictureSize
 *
 * DESCRIPTION: Sets exact preview size if supported
 *
 * PARAMETERS : format size in the form of WIDTHxHEIGHT
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::setPictureSize(const char *format)
{
    useLock();
    if ( mHardwareActive ) {
        int newHeight;
        int newWidth;
        sscanf(format, "%dx%d", &newWidth, &newHeight);

        unsigned int i;
        for (i = 0; i < mSupportedPictureSizes.size(); ++i) {
            Size PictureSize = mSupportedPictureSizes.itemAt(i);
            if ( newWidth == PictureSize.width &&
                 newHeight == PictureSize.height )
            {
                break;
            }

        }
        if ( i == mSupportedPictureSizes.size())
        {
            printf("Preview size %dx%d not supported !\n",
                newWidth, newHeight);
            return INVALID_OPERATION;
        }

        mParams.setPictureSize(newWidth,
                               newHeight);
        mCamera->setParameters(mParams.flatten());
    }

    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : nextVideoSize
 *
 * DESCRIPTION: Select the next available video size
 *
 * PARAMETERS : none
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::nextVideoSize()
{
    useLock();
    if ( mHardwareActive ) {
        mCurrentVideoSizeIdx += 1;
        mCurrentVideoSizeIdx %= mSupportedVideoSizes.size();
        Size videoSize = mSupportedVideoSizes.itemAt(mCurrentVideoSizeIdx);
        mParams.setVideoSize(videoSize.width,
                             videoSize.height);
        mCamera->setParameters(mParams.flatten());
        mInterpr->setViVSize((Size) mSupportedVideoSizes.itemAt(
            mCurrentVideoSizeIdx), mCameraIndex);
    }
    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : setVideoSize
 *
 * DESCRIPTION: Set video size
 *
 * PARAMETERS :
 *   @format  : format
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::setVideoSize(const char *format)
{
    useLock();
    if ( mHardwareActive ) {
        int newHeight;
        int newWidth;
        sscanf(format, "%dx%d", &newWidth, &newHeight);

        unsigned int i;
        for (i = 0; i < mSupportedVideoSizes.size(); ++i) {
            Size PictureSize = mSupportedVideoSizes.itemAt(i);
            if ( newWidth == PictureSize.width &&
                 newHeight == PictureSize.height )
            {
                break;
            }

        }
        if ( i == mSupportedVideoSizes.size())
        {
            printf("Preview size %dx%d not supported !\n",
                newWidth, newHeight);
            return INVALID_OPERATION;
        }

        mParams.setVideoSize(newWidth,
                             newHeight);
        mCamera->setParameters(mParams.flatten());
    }

    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION    : getCurrentVideoSize
 *
 * DESCRIPTION : Get current video size
 *
 * PARAMETERS  :
 *   @videoSize: video Size
 *
 * RETURN      : status_t type of status
 *               NO_ERROR  -- success
 *               none-zero failure code
 *==========================================================================*/
status_t CameraContext::getCurrentVideoSize(Size &videoSize)
{
    useLock();
    if ( mHardwareActive ) {
        videoSize = mSupportedVideoSizes.itemAt(mCurrentVideoSizeIdx);
    }
    signalFinished();
    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : getCurrentPictureSize
 *
 * DESCRIPTION: queries the currently configured picture size
 *
 * PARAMETERS :
 *  @pictureSize : picture size currently configured
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t CameraContext::getCurrentPictureSize(Size &pictureSize)
{
    useLock();
    if ( mHardwareActive ) {
        pictureSize = mSupportedPictureSizes.itemAt(mCurrentPictureSizeIdx);
    }
    signalFinished();
    return NO_ERROR;
}

}; //namespace qcamera ends here

using namespace qcamera;

/*===========================================================================
 * FUNCTION   : printMenu
 *
 * DESCRIPTION: prints the available camera options
 *
 * PARAMETERS :
 *  @currentCamera : camera context currently being used
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::printMenu(sp<CameraContext> currentCamera)
{
    if ( !mDoPrintMenu ) return;
    Size currentPictureSize, currentPreviewSize, currentVideoSize;

    assert(currentCamera.get());

    currentCamera->getCurrentPictureSize(currentPictureSize);
    currentCamera->getCurrentPreviewSize(currentPreviewSize);
    currentCamera->getCurrentVideoSize(currentVideoSize);

    printf("\n\n=========== FUNCTIONAL TEST MENU ===================\n\n");

    printf(" \n\nSTART / STOP / GENERAL SERVICES \n");
    printf(" -----------------------------\n");
    printf("   %c. Switch camera - Current Index: %d\n",
            Interpreter::SWITCH_CAMERA_CMD,
            currentCamera->getCameraIndex());
    printf("   %c. Resume Preview after capture \n",
            Interpreter::RESUME_PREVIEW_CMD);
    printf("   %c. Quit \n",
            Interpreter::EXIT_CMD);
    printf("   %c. Camera Capability Dump",
            Interpreter::DUMP_CAPS_CMD);

    printf(" \n\n PREVIEW SUB MENU \n");
    printf(" -----------------------------\n");
    printf("   %c. Start Preview\n",
            Interpreter::START_PREVIEW_CMD);
    printf("   %c. Stop Preview\n",
            Interpreter::STOP_PREVIEW_CMD);
    printf("   %c. Preview size:  %dx%d\n",
            Interpreter::CHANGE_PREVIEW_SIZE_CMD,
            currentPreviewSize.width,
            currentPreviewSize.height);
    printf("   %c. Video size:  %dx%d\n",
            Interpreter::CHANGE_VIDEO_SIZE_CMD,
            currentVideoSize.width,
            currentVideoSize.height);
    printf("   %c. Start Recording\n",
            Interpreter::START_RECORD_CMD);
    printf("   %c. Stop Recording\n",
            Interpreter::STOP_RECORD_CMD);
    printf("   %c. Start ViV Recording\n",
            Interpreter::START_VIV_RECORD_CMD);
    printf("   %c. Stop ViV Recording\n",
            Interpreter::STOP_VIV_RECORD_CMD);
    printf("   %c. Enable preview frames\n",
            Interpreter::ENABLE_PRV_CALLBACKS_CMD);
    printf("   %c. Trigger autofocus \n",
            Interpreter::AUTOFOCUS_CMD);

    printf(" \n\n IMAGE CAPTURE SUB MENU \n");
    printf(" -----------------------------\n");
    printf("   %c. Take picture/Full Press\n",
            Interpreter::TAKEPICTURE_CMD);
    printf("   %c. Take picture in picture\n",
            Interpreter::TAKEPICTURE_IN_PICTURE_CMD);
    printf("   %c. Picture size:  %dx%d\n",
            Interpreter::CHANGE_PICTURE_SIZE_CMD,
            currentPictureSize.width,
            currentPictureSize.height);
    printf("   %c. zsl:  %s\n", Interpreter::ZSL_CMD, mParams.get(CameraContext::KEY_ZSL) ?
            mParams.get(CameraContext::KEY_ZSL) : "NULL");

    printf("\n   Choice: ");
}

/*===========================================================================
 * FUNCTION   : enablePrintPreview
 *
 * DESCRIPTION: Enables printing the preview
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::enablePrintPreview()
{
    mDoPrintMenu = true;
}

/*===========================================================================
 * FUNCTION   : disablePrintPreview
 *
 * DESCRIPTION: Disables printing the preview
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::disablePrintPreview()
{
    mDoPrintMenu = false;
}

/*===========================================================================
 * FUNCTION   : enablePiPCapture
 *
 * DESCRIPTION: Enables picture in picture capture
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::enablePiPCapture()
{
    mPiPCapture = true;
}

/*===========================================================================
 * FUNCTION   : disablePiPCapture
 *
 * DESCRIPTION: Disables picture in picture capture
 *
 * PARAMETERS : None
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::disablePiPCapture()
{
    mPiPCapture = false;
}

/*===========================================================================
 * FUNCTION   : getZSL
 *
 * DESCRIPTION: get ZSL value of current camera
 *
 * PARAMETERS : None
 *
 * RETURN     : current zsl value
 *==========================================================================*/
const char *CameraContext::getZSL()
{
    return mParams.get(CameraContext::KEY_ZSL);
}

/*===========================================================================
 * FUNCTION   : setZSL
 *
 * DESCRIPTION: set ZSL value of current camera
 *
 * PARAMETERS : zsl value to be set
 *
 * RETURN     : None
 *==========================================================================*/
void CameraContext::setZSL(const char *value)
{
    mParams.set(CameraContext::KEY_ZSL, value);
    mCamera->setParameters(mParams.flatten());
}

/*===========================================================================
 * FUNCTION   : configureViVCodec
 *
 * DESCRIPTION: Configures video in video codec
 *
 * PARAMETERS : none
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t Interpreter::configureViVCodec()
{
    status_t ret = NO_ERROR;
    char fileName[100];
    sp<AMessage> format = new AMessage;
    sp<ALooper> looper = new ALooper;

    if (mTestContext->mViVVid.VideoSizes[0].width *
            mTestContext->mViVVid.VideoSizes[0].height >=
            mTestContext->mViVVid.VideoSizes[1].width *
            mTestContext->mViVVid.VideoSizes[1].height) {
        snprintf(fileName, 100,  "/sdcard/ViV_vid_%dx%d_%d.mp4",
            mTestContext->mViVVid.VideoSizes[0].width,
            mTestContext->mViVVid.VideoSizes[0].height,
            mTestContext->mViVVid.ViVIdx++);
        format->setInt32("width", mTestContext->mViVVid.VideoSizes[0].width);
        format->setInt32("height", mTestContext->mViVVid.VideoSizes[0].height);
    } else {
        snprintf(fileName, 100,  "/sdcard/ViV_vid_%dx%d_%d.mp4",
            mTestContext->mViVVid.VideoSizes[1].width,
            mTestContext->mViVVid.VideoSizes[1].height,
            mTestContext->mViVVid.ViVIdx++);
        format->setInt32("width", mTestContext->mViVVid.VideoSizes[1].width);
        format->setInt32("height", mTestContext->mViVVid.VideoSizes[1].height);
    }
    mTestContext->mViVVid.muxer = new MediaMuxer(
        fileName, MediaMuxer::OUTPUT_FORMAT_MPEG_4);

    format->setString("mime", "video/avc");
    format->setInt32("color-format", OMX_COLOR_FormatAndroidOpaque);

    format->setInt32("bitrate", 1000000);
    format->setFloat("frame-rate", 30);
    format->setInt32("i-frame-interval", 10);

    looper->setName("ViV_recording_looper");
    looper->start();
    ALOGV("Creating codec");
    mTestContext->mViVVid.codec = MediaCodec::CreateByType(
        looper, "video/avc", true);
    if (mTestContext->mViVVid.codec == NULL) {
        fprintf(stderr, "ERROR: unable to create video/avc codec instance\n");
        return UNKNOWN_ERROR;
    }
    ret = mTestContext->mViVVid.codec->configure(format, NULL, NULL,
            MediaCodec::CONFIGURE_FLAG_ENCODE);
    if (ret != NO_ERROR) {
        mTestContext->mViVVid.codec->release();
        mTestContext->mViVVid.codec.clear();

        fprintf(stderr, "ERROR: unable to configure codec (err=%d)\n", ret);
        return ret;
    }

    ALOGV("Creating buffer producer");
    ret = mTestContext->mViVVid.codec->createInputSurface(
        &mTestContext->mViVVid.bufferProducer);
    if (ret != NO_ERROR) {
        mTestContext->mViVVid.codec->release();
        mTestContext->mViVVid.codec.clear();

        fprintf(stderr,
            "ERROR: unable to create encoder input surface (err=%d)\n", ret);
        return ret;
    }

    ret = mTestContext->mViVVid.codec->start();
    if (ret != NO_ERROR) {
        mTestContext->mViVVid.codec->release();
        mTestContext->mViVVid.codec.clear();

        fprintf(stderr, "ERROR: unable to start codec (err=%d)\n", ret);
        return ret;
    }
    ALOGV("Codec prepared");

    mTestContext->mViVVid.surface = new Surface(
        mTestContext->mViVVid.bufferProducer);
    mTestContext->mViVVid.ANW = mTestContext->mViVVid.surface;
    ret = native_window_api_connect(mTestContext->mViVVid.ANW.get(),
        NATIVE_WINDOW_API_CPU);
    if (mTestContext->mViVVid.VideoSizes[0].width *
        mTestContext->mViVVid.VideoSizes[0].height >=
        mTestContext->mViVVid.VideoSizes[1].width *
        mTestContext->mViVVid.VideoSizes[1].height) {
        native_window_set_buffers_geometry(mTestContext->mViVVid.ANW.get(),
            mTestContext->mViVVid.VideoSizes[0].width,
            mTestContext->mViVVid.VideoSizes[0].height,
            HAL_PIXEL_FORMAT_NV12_ENCODEABLE);
    } else {
        native_window_set_buffers_geometry(mTestContext->mViVVid.ANW.get(),
            mTestContext->mViVVid.VideoSizes[1].width,
            mTestContext->mViVVid.VideoSizes[1].height,
            HAL_PIXEL_FORMAT_NV12_ENCODEABLE);
    }
    native_window_set_usage(mTestContext->mViVVid.ANW.get(),
        GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
    native_window_set_buffer_count(mTestContext->mViVVid.ANW.get(),
        mTestContext->mViVVid.buff_cnt);

    ViVEncoderThread();

    return ret;
}

/*===========================================================================
 * FUNCTION   : unconfigureViVCodec
 *
 * DESCRIPTION: Unconfigures video in video codec
 *
 * PARAMETERS : none
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- success
 *              none-zero failure code
 *==========================================================================*/
status_t Interpreter::unconfigureViVCodec()
{
    status_t ret = NO_ERROR;

    ret = native_window_api_disconnect(mTestContext->mViVVid.ANW.get(),
        NATIVE_WINDOW_API_CPU);
    mTestContext->mViVVid.bufferProducer = NULL;
    mTestContext->mViVVid.codec->stop();
    pthread_join(mViVEncThread, NULL);
    mTestContext->mViVVid.muxer->stop();
    mTestContext->mViVVid.codec->release();
    mTestContext->mViVVid.codec.clear();
    mTestContext->mViVVid.muxer.clear();
    mTestContext->mViVVid.surface.clear();
  return ret;
}

/*===========================================================================
 * FUNCTION   : Interpreter
 *
 * DESCRIPTION: Interpreter constructor
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
Interpreter::Interpreter(const char *file)
    : mCmdIndex(0)
    , mScript(NULL)
{
    if (!file){
        printf("no File Given\n");
        mUseScript = false;
        return;
    }

    FILE *fh = fopen(file, "r");
    if ( !fh ) {
        printf("Could not open file %s\n", file);
        mUseScript = false;
        return;
    }

    fseek(fh, 0, SEEK_END);
    size_t len = (size_t)ftell(fh);
    rewind(fh);

    if( !len ) {
        printf("Script file %s is empty !\n", file);
        fclose(fh);
        return;
    }

    mScript = new char[len + 1];
    if ( !mScript ) {
        fclose(fh);
        return;
    }

    fread(mScript, sizeof(char), len, fh);
    mScript[len] = '\0'; // ensure null terminated;
    fclose(fh);


    char *p1;
    char *p2;
    p1 = p2 = mScript;

    do {
        switch (*p1) {
        case '\0':
        case '|':
            p1++;
            break;
        case SWITCH_CAMERA_CMD:
        case RESUME_PREVIEW_CMD:
        case START_PREVIEW_CMD:
        case STOP_PREVIEW_CMD:
        case CHANGE_PREVIEW_SIZE_CMD:
        case CHANGE_PICTURE_SIZE_CMD:
        case START_RECORD_CMD:
        case STOP_RECORD_CMD:
        case START_VIV_RECORD_CMD:
        case STOP_VIV_RECORD_CMD:
        case DUMP_CAPS_CMD:
        case AUTOFOCUS_CMD:
        case TAKEPICTURE_CMD:
        case TAKEPICTURE_IN_PICTURE_CMD:
        case ENABLE_PRV_CALLBACKS_CMD:
        case EXIT_CMD:
        case ZSL_CMD:
        case DELAY:
            p2 = p1;
            while( (p2 != (mScript + len)) && (*p2 != '|')) {
                p2++;
            }
            *p2 = '\0';
            if (p2 == (p1 + 1))
                mCommands.push_back(Command(
                    static_cast<Interpreter::Commands_e>(*p1)));
            else
                mCommands.push_back(Command(
                    static_cast<Interpreter::Commands_e>(*p1), (p1 + 1)));
            p1 = p2;
            break;
        default:
            printf("Invalid cmd %c \n", *p1);
            do {
                p1++;

            } while(*p1 != '|' && p1 != (mScript + len));

        }
    } while(p1 != (mScript + len));
    mUseScript = true;
}

/*===========================================================================
 * FUNCTION   : ~Interpreter
 *
 * DESCRIPTION: Interpreter destructor
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
Interpreter::~Interpreter()
{
    if ( mScript )
        delete[] mScript;

    mCommands.clear();
}

/*===========================================================================
 * FUNCTION        : getCommand
 *
 * DESCRIPTION     : Get a command from interpreter
 *
 * PARAMETERS      :
 *   @currentCamera: Current camera context
 *
 * RETURN          : command
 *==========================================================================*/
Interpreter::Command Interpreter::getCommand(
    sp<CameraContext> currentCamera)
{
    if( mUseScript ) {
        return mCommands[mCmdIndex++];
    } else {
        currentCamera->printMenu(currentCamera);
        return Interpreter::Command(
            static_cast<Interpreter::Commands_e>(getchar()));
    }
}

/*===========================================================================
 * FUNCTION        : TestContext
 *
 * DESCRIPTION     : TestContext constructor
 *
 * PARAMETERS      : None
 *
 * RETURN          : None
 *==========================================================================*/
TestContext::TestContext()
{
    int i = 0;
    mTestRunning = false;
    mInterpreter = NULL;
    mViVVid.ViVIdx = 0;
    mViVVid.buff_cnt = 9;
    mViVVid.graphBuf = 0;
    mViVVid.mappedBuff = NULL;
    mViVVid.isBuffValid = false;
    mViVVid.sourceCameraID = -1;
    mViVVid.destinationCameraID = -1;
    mPiPinUse = false;
    mViVinUse = false;
    mIsZSLOn = false;
    memset(&mViVBuff, 0, sizeof(ViVBuff_t));

    ProcessState::self()->startThreadPool();

    do {
        camera[i] = new CameraContext(i);
        if ( NULL == camera[i].get() ) {
            break;
        }
        camera[i]->setTestCtxInstance(this);

        status_t stat = camera[i]->openCamera();
        if ( NO_ERROR != stat ) {
            printf("Error encountered Openging camera id : %d\n", i);
            break;
        }
        mAvailableCameras.add(camera[i]);
        i++;
    } while ( i < camera[0]->getNumberOfCameras() ) ;

    if (i < camera[0]->getNumberOfCameras() ) {
        for (size_t j = 0; j < mAvailableCameras.size(); j++) {
            camera[j] = mAvailableCameras.itemAt(j);
            camera[j]->closeCamera();
            camera[j].clear();
        }

        mAvailableCameras.clear();
    }
}

/*===========================================================================
 * FUNCTION        : ~TestContext
 *
 * DESCRIPTION     : TestContext destructor
 *
 * PARAMETERS      : None
 *
 * RETURN          : None
 *==========================================================================*/
TestContext::~TestContext()
{
    delete mInterpreter;

    for (size_t j = 0; j < mAvailableCameras.size(); j++) {
        camera[j] = mAvailableCameras.itemAt(j);
        camera[j]->closeCamera();
        camera[j].clear();
    }

    mAvailableCameras.clear();
}

/*===========================================================================
 * FUNCTION        : GetCamerasNum
 *
 * DESCRIPTION     : Get the number of available cameras
 *
 * PARAMETERS      : None
 *
 * RETURN          : Number of cameras
 *==========================================================================*/
size_t TestContext::GetCamerasNum()
{
    return mAvailableCameras.size();
}

/*===========================================================================
 * FUNCTION        : AddScriptFromFile
 *
 * DESCRIPTION     : Add script from file
 *
 * PARAMETERS      :
 *   @scriptFile   : Script file
 *
 * RETURN          : status_t type of status
 *                   NO_ERROR  -- success
 *                   none-zero failure code
 *==========================================================================*/
status_t TestContext::AddScriptFromFile(const char *scriptFile)
{
    mInterpreter = new Interpreter(scriptFile);
    mInterpreter->setTestCtxInst(this);

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION        : releasePiPBuff
 *
 * DESCRIPTION     : Release video in video temp buffer
 *
 * PARAMETERS      : None
 *
 * RETURN          : None
 *==========================================================================*/
void Interpreter::releasePiPBuff() {
    free(mTestContext->mViVBuff.buff);
    mTestContext->mViVBuff.buff = NULL;
}

/*===========================================================================
 * FUNCTION   : functionalTest
 *
 * DESCRIPTION: queries and executes client supplied commands for testing a
 *              particular camera.
 *
 * PARAMETERS :
 *  @availableCameras : List with all cameras supported
 *
 * RETURN     : status_t type of status
 *              NO_ERROR  -- continue testing
 *              none-zero -- quit test
 *==========================================================================*/
status_t TestContext::FunctionalTest()
{
    status_t stat = NO_ERROR;
    const char *ZSLStr = NULL;

    assert(mAvailableCameras.size());

    if ( !mInterpreter ) {
        mInterpreter = new Interpreter();
        mInterpreter->setTestCtxInst(this);
    }


    mTestRunning = true;

    while (mTestRunning) {
        sp<CameraContext> currentCamera =
            mAvailableCameras.itemAt(mCurrentCameraIndex);
        Interpreter::Command command =
            mInterpreter->getCommand(currentCamera);
        currentCamera->enablePrintPreview();

        switch (command.cmd) {
        case Interpreter::SWITCH_CAMERA_CMD:
        {
            mCurrentCameraIndex++;
            mCurrentCameraIndex %= mAvailableCameras.size();
            currentCamera = mAvailableCameras.itemAt(mCurrentCameraIndex);
        }
            break;

        case Interpreter::RESUME_PREVIEW_CMD:
        {
            stat = currentCamera->resumePreview();
        }
            break;

        case Interpreter::START_PREVIEW_CMD:
        {
            stat = currentCamera->startPreview();
        }
            break;

        case Interpreter::STOP_PREVIEW_CMD:
        {
            stat = currentCamera->stopPreview();
        }
            break;

        case Interpreter::CHANGE_VIDEO_SIZE_CMD:
        {
            if ( command.arg )
                stat = currentCamera->setVideoSize(command.arg);
            else
                stat = currentCamera->nextVideoSize();
        }
        break;

        case Interpreter::CHANGE_PREVIEW_SIZE_CMD:
        {
            if ( command.arg )
                stat = currentCamera->setPreviewSize(command.arg);
            else
                stat = currentCamera->nextPreviewSize();
        }
            break;

        case Interpreter::CHANGE_PICTURE_SIZE_CMD:
        {
            if ( command.arg )
                stat = currentCamera->setPictureSize(command.arg);
            else
                stat = currentCamera->nextPictureSize();
        }
            break;

        case Interpreter::DUMP_CAPS_CMD:
        {
            currentCamera->printSupportedParams();
        }
            break;

        case Interpreter::AUTOFOCUS_CMD:
        {
            stat = currentCamera->autoFocus();
        }
            break;

        case Interpreter::TAKEPICTURE_CMD:
        {
            stat = currentCamera->takePicture();
        }
            break;

        case Interpreter::TAKEPICTURE_IN_PICTURE_CMD:
        {
            if (mAvailableCameras.size() == 2) {
                mSaveCurrentCameraIndex = mCurrentCameraIndex;
                for ( size_t i = 0; i < mAvailableCameras.size(); i++ ) {
                    mCurrentCameraIndex = i;
                    currentCamera = mAvailableCameras.itemAt(mCurrentCameraIndex);
                    currentCamera->enablePiPCapture();
                    stat = currentCamera->takePicture();
                }
                mCurrentCameraIndex = mSaveCurrentCameraIndex;
            } else {
                printf("Number of available sensors should be 2\n");
            }
        }
        break;

        case Interpreter::ENABLE_PRV_CALLBACKS_CMD:
        {
            stat = currentCamera->enablePreviewCallbacks();
        }
            break;

        case Interpreter::START_RECORD_CMD:
        {
            stat = currentCamera->stopPreview();
            stat = currentCamera->configureRecorder();
            stat = currentCamera->startPreview();
            stat = currentCamera->startRecording();
        }
            break;

        case Interpreter::STOP_RECORD_CMD:
        {
            stat = currentCamera->stopRecording();

            stat = currentCamera->stopPreview();
            stat = currentCamera->unconfigureRecorder();
            stat = currentCamera->startPreview();
        }
            break;

        case Interpreter::START_VIV_RECORD_CMD:
        {

            if (mAvailableCameras.size() == 2) {
                mSaveCurrentCameraIndex = mCurrentCameraIndex;
                stat = mInterpreter->configureViVCodec();
                for ( size_t i = 0; i < mAvailableCameras.size(); i++ ) {
                    mCurrentCameraIndex = i;
                    currentCamera = mAvailableCameras.itemAt(
                        mCurrentCameraIndex);
                    stat = currentCamera->stopPreview();
                    stat = currentCamera->configureViVRecording();
                    stat = currentCamera->startPreview();
                    stat = currentCamera->startViVRecording();
                }
                mCurrentCameraIndex = mSaveCurrentCameraIndex;
            } else {
                printf("Number of available sensors should be 2\n");
            }

        }
            break;

        case Interpreter::STOP_VIV_RECORD_CMD:
        {
            if (mAvailableCameras.size() == 2) {
                mSaveCurrentCameraIndex = mCurrentCameraIndex;
                for ( size_t i = 0; i < mAvailableCameras.size(); i++ ) {
                    mCurrentCameraIndex = i;
                    currentCamera = mAvailableCameras.itemAt(
                        mCurrentCameraIndex);
                    stat = currentCamera->stopViVRecording();
                    stat = currentCamera->stopPreview();
                    stat = currentCamera->unconfigureRecorder();
                    stat = currentCamera->startPreview();
                }
                stat = mInterpreter->unconfigureViVCodec();
                mCurrentCameraIndex = mSaveCurrentCameraIndex;

                mInterpreter->releasePiPBuff();
            } else {
                printf("Number of available sensors should be 2\n");
            }
        }
        break;

        case Interpreter::EXIT_CMD:
        {
            currentCamera->stopPreview();
            mTestRunning = false;
        }
            break;

        case Interpreter::DELAY:
        {
            if ( command.arg )
                usleep(1000LU * (long unsigned int)atoi(command.arg));
        }
            break;

        case Interpreter::ZSL_CMD:
        {
            currentCamera = mAvailableCameras.itemAt(
                    mCurrentCameraIndex);
            ZSLStr = currentCamera->getZSL();

            if (NULL != ZSLStr) {
                if (strcmp(ZSLStr, "off") == 0) {
                    currentCamera->setZSL("on");
                    mIsZSLOn = true;
                } else if (strcmp(ZSLStr, "on") == 0) {
                    currentCamera->setZSL("off");
                    mIsZSLOn = false;
                } else {
                    printf("Set zsl failed!\n");
                }
            } else {
                printf("zsl is NULL");
            }
        }
            break;

        default:
        {
            currentCamera->disablePrintPreview();
        }
            break;
        }
        printf("Command status 0x%x \n", stat);
    }

    return NO_ERROR;
}

/*===========================================================================
 * FUNCTION   : PiPLock
 *
 * DESCRIPTION: Mutex lock for PiP capture
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/

void TestContext::PiPLock()
{
    Mutex::Autolock l(mPiPLock);
    if (mPiPinUse ) {
        mPiPCond.wait(mPiPLock);
    } else {
        mPiPinUse = true;
    }
}

/*===========================================================================
 * FUNCTION   : PiPUnLock
 *
 * DESCRIPTION: Mutex unlock for PiP capture
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void TestContext::PiPUnlock()
{
    Mutex::Autolock l(mPiPLock);
    mPiPinUse = false;
    mPiPCond.signal();
}

/*===========================================================================
 * FUNCTION   : ViVLock
 *
 * DESCRIPTION: Mutex lock for ViV Video
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void TestContext::ViVLock()
{
    Mutex::Autolock l(mViVLock);
    if (mViVinUse ) {
        mViVCond.wait(mViVLock);
    } else {
        mViVinUse = true;
    }
}

/*===========================================================================
 * FUNCTION   : ViVUnlock
 *
 * DESCRIPTION: Mutex unlock for ViV Video
 *
 * PARAMETERS : none
 *
 * RETURN     : none
 *==========================================================================*/
void TestContext::ViVUnlock()
{
    Mutex::Autolock l(mViVLock);
    mViVinUse = false;
    mViVCond.signal();
}

/*===========================================================================
 * FUNCTION     : setViVSize
 *
 * DESCRIPTION  : Set video in video size
 *
 * PARAMETERS   :
 *   @VideoSize : video size
 *   @camIndex  : camera index
 *
 * RETURN       : none
 *==========================================================================*/
void TestContext::setViVSize(Size VideoSize, int camIndex)
{
    mViVVid.VideoSizes[camIndex] = VideoSize;
}

/*===========================================================================
 * FUNCTION     : main
 *
 * DESCRIPTION  : main function
 *
 * PARAMETERS   :
 *   @argc      : argc
 *   @argv      : argv
 *
 * RETURN       : int status
 *==========================================================================*/
int main(int argc, char *argv[])
{
    TestContext ctx;

    if (argc > 1) {
        if ( ctx.AddScriptFromFile((const char *)argv[1]) ) {
            printf("Could not add script file... "
                "continuing in normal menu mode! \n");
        }
    }

    ctx.FunctionalTest();

    return 0;
}
