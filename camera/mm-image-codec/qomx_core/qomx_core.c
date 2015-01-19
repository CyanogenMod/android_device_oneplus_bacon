/*Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/

#define LOG_NDEBUG 0
#define LOG_NIDEBUG 0
#define LOG_TAG "qomx_image_core"
#include <utils/Log.h>

#include "qomx_core.h"

#define BUFF_SIZE 255

static omx_core_t *g_omxcore;
static pthread_mutex_t g_omxcore_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_omxcore_cnt = 0;

//Map the library name with the component name
static const comp_info_t g_comp_info[] =
{
  { "OMX.qcom.image.jpeg.encoder", "libqomx_jpegenc.so" },
  { "OMX.qcom.image.jpeg.decoder", "libqomx_jpegdec.so" }
};

static int get_idx_from_handle(OMX_IN OMX_HANDLETYPE *ahComp, int *acompIndex,
  int *ainstanceIndex);

/*==============================================================================
* Function : OMX_Init
* Parameters: None
* Description: This is the first call that is made to the OMX Core
* and initializes the OMX IL core
==============================================================================*/
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Init()
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  int i = 0;
  int comp_cnt = sizeof(g_comp_info)/sizeof(g_comp_info[0]);

  pthread_mutex_lock(&g_omxcore_lock);

  /* check if core is created */
  if (g_omxcore) {
    g_omxcore_cnt++;
    pthread_mutex_unlock(&g_omxcore_lock);
    return rc;
  }

  if (comp_cnt > OMX_COMP_MAX_NUM) {
    ALOGE("%s:%d] cannot exceed max number of components",
      __func__, __LINE__);
    pthread_mutex_unlock(&g_omxcore_lock);
    return OMX_ErrorUndefined;
  }
  /* create new global object */
  g_omxcore = malloc(sizeof(omx_core_t));
  if (g_omxcore) {
    memset(g_omxcore, 0x0, sizeof(omx_core_t));

    /* populate the library name and component name */
    for (i = 0; i < comp_cnt; i++) {
      g_omxcore->component[i].comp_name = g_comp_info[i].comp_name;
      g_omxcore->component[i].lib_name = g_comp_info[i].lib_name;
    }
    g_omxcore->comp_cnt = comp_cnt;
    g_omxcore_cnt++;
  } else {
    rc = OMX_ErrorInsufficientResources;
  }
  pthread_mutex_unlock(&g_omxcore_lock);
  ALOGE("%s:%d] Complete %d", __func__, __LINE__, comp_cnt);
  return rc;
}

/*==============================================================================
* Function : OMX_Deinit
* Parameters: None
* Return Value : OMX_ERRORTYPE
* Description: Deinit all the OMX components
==============================================================================*/
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit()
{
  pthread_mutex_lock(&g_omxcore_lock);

  if (g_omxcore_cnt == 1) {
    if (g_omxcore) {
      free(g_omxcore);
      g_omxcore = NULL;
    }
  }
  if (g_omxcore_cnt) {
    g_omxcore_cnt--;
  }

  ALOGE("%s:%d] Complete", __func__, __LINE__);
  pthread_mutex_unlock(&g_omxcore_lock);
  return OMX_ErrorNone;
}

/*==============================================================================
* Function : get_comp_from_list
* Parameters: componentName
* Return Value : component_index
* Description: If the componnt is already present in the list, return the
* component index. If not return the next index to create the component.
==============================================================================*/
static int get_comp_from_list(char *comp_name)
{
  int index = -1, i = 0;

  if (NULL == comp_name)
    return -1;

  for (i = 0; i < g_omxcore->comp_cnt; i++) {
    if (!strcmp(g_omxcore->component[i].comp_name, comp_name)) {
      index = i;
      break;
    }
  }
  return index;
}

/*==============================================================================
* Function : get_free_inst_idx
* Parameters: p_comp
* Return Value : The next instance index if available
* Description: Get the next available index for to store the new instance of the
*            component being created.
*============================================================================*/
static int get_free_inst_idx(omx_core_component_t *p_comp)
{
  int idx = -1, i = 0;

  for (i = 0; i < OMX_COMP_MAX_INSTANCES; i++) {
    if (NULL == p_comp->handle[i]) {
      idx = i;
      break;
    }
  }
  return idx;
}

/*==============================================================================
* Function : OMX_GetHandle
* Parameters: handle, componentName, appData, callbacks
* Return Value : OMX_ERRORTYPE
* Description: Construct and load the requested omx library
==============================================================================*/
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
  OMX_OUT OMX_HANDLETYPE* handle,
  OMX_IN OMX_STRING componentName,
  OMX_IN OMX_PTR appData,
  OMX_IN OMX_CALLBACKTYPE* callBacks)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  int comp_idx = 0, inst_idx = 0;
  char libName[BUFF_SIZE] = {0};
  void *p_obj = NULL;
  OMX_COMPONENTTYPE *p_comp = NULL;
  omx_core_component_t *p_core_comp = NULL;
  OMX_BOOL close_handle = OMX_FALSE;

  if (NULL == handle) {
    ALOGE("%s:%d] Error invalid input ", __func__, __LINE__);
    return OMX_ErrorBadParameter;
  }

  pthread_mutex_lock(&g_omxcore_lock);

  comp_idx = get_comp_from_list(componentName);
  if (comp_idx < 0) {
    ALOGE("%s:%d] Cannot find the component", __func__, __LINE__);
    pthread_mutex_unlock(&g_omxcore_lock);
    return OMX_ErrorInvalidComponent;
  }
  p_core_comp = &g_omxcore->component[comp_idx];

  *handle = NULL;

  //If component already present get the instance index
  inst_idx = get_free_inst_idx(p_core_comp);
  if (inst_idx < 0) {
    ALOGE("%s:%d] Cannot alloc new instance", __func__, __LINE__);
    rc = OMX_ErrorInvalidComponent;
    goto error;
  }

  if (FALSE == p_core_comp->open) {
    /* load the library */
    p_core_comp->lib_handle = dlopen(p_core_comp->lib_name, RTLD_NOW);
    if (NULL == p_core_comp->lib_handle) {
      ALOGE("%s:%d] Cannot load the library", __func__, __LINE__);
      rc = OMX_ErrorInvalidComponent;
      goto error;
    }

    p_core_comp->open = TRUE;
    /* Init the component and get component functions */
    p_core_comp->create_comp_func = dlsym(p_core_comp->lib_handle,
      "create_component_fns");
    p_core_comp->get_instance = dlsym(p_core_comp->lib_handle, "getInstance");

    close_handle = OMX_TRUE;
    if (!p_core_comp->create_comp_func || !p_core_comp->get_instance) {
      ALOGE("%s:%d] Cannot maps the symbols", __func__, __LINE__);
      rc = OMX_ErrorInvalidComponent;
      goto error;
    }
  }

  /* Call the function from the address to create the obj */
  p_obj = (*p_core_comp->get_instance)();
  ALOGE("%s:%d] get instance pts is %p", __func__, __LINE__, p_obj);
  if (NULL == p_obj) {
    ALOGE("%s:%d] Error cannot create object", __func__, __LINE__);
    rc = OMX_ErrorInvalidComponent;
    goto error;
  }

  /* Call the function from the address to get the func ptrs */
  p_comp = (*p_core_comp->create_comp_func)(p_obj);
  if (NULL == p_comp) {
    ALOGE("%s:%d] Error cannot create component", __func__, __LINE__);
    rc = OMX_ErrorInvalidComponent;
    goto error;
  }

  *handle = p_core_comp->handle[inst_idx] = (OMX_HANDLETYPE)p_comp;

  ALOGD("%s:%d] handle = %p Instanceindex = %d,"
    "comp_idx %d g_ptr %p", __func__, __LINE__,
    p_core_comp->handle[inst_idx], inst_idx,
    comp_idx, g_omxcore);

  p_comp->SetCallbacks(p_comp, callBacks, appData);
  pthread_mutex_unlock(&g_omxcore_lock);
  ALOGE("%s:%d] Success", __func__, __LINE__);
  return OMX_ErrorNone;

error:

  if (OMX_TRUE == close_handle) {
    dlclose(p_core_comp->lib_handle);
    p_core_comp->lib_handle = NULL;
  }
  pthread_mutex_unlock(&g_omxcore_lock);
  ALOGE("%s:%d] Error %d", __func__, __LINE__, rc);
  return rc;
}

/*==============================================================================
* Function : getIndexFromComponent
* Parameters: handle,
* Return Value : Component present - true or false, Instance Index, Component
* Index
* Description: Check if the handle is present in the list and get the component
* index and instance index for the component handle.
==============================================================================*/
static int get_idx_from_handle(OMX_IN OMX_HANDLETYPE *ahComp, int *aCompIdx,
  int *aInstIdx)
{
  int i = 0, j = 0;
  for (i = 0; i < g_omxcore->comp_cnt; i++) {
    for (j = 0; j < OMX_COMP_MAX_INSTANCES; j++) {
      if ((OMX_COMPONENTTYPE *)g_omxcore->component[i].handle[j] ==
        (OMX_COMPONENTTYPE *)ahComp) {
        ALOGE("%s:%d] comp_idx %d inst_idx %d", __func__, __LINE__, i, j);
        *aCompIdx = i;
        *aInstIdx = j;
        return TRUE;
      }
    }
  }
  return FALSE;
}

/*==============================================================================
* Function : is_comp_active
* Parameters: p_core_comp
* Return Value : int
* Description: Check if the component has any active instances
==============================================================================*/
static uint8_t is_comp_active(omx_core_component_t *p_core_comp)
{
  uint8_t i = 0;
  for (i = 0; i < OMX_COMP_MAX_INSTANCES; i++) {
    if (NULL != p_core_comp->handle[i]) {
      return TRUE;
    }
  }
  return FALSE;
}

/*==============================================================================
* Function : OMX_FreeHandle
* Parameters: hComp
* Return Value : OMX_ERRORTYPE
* Description: Deinit the omx component and remove it from the global list
==============================================================================*/
OMX_API OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
  OMX_IN OMX_HANDLETYPE hComp)
{
  OMX_ERRORTYPE rc = OMX_ErrorNone;
  int comp_idx, inst_idx;
  OMX_COMPONENTTYPE *p_comp = NULL;
  omx_core_component_t *p_core_comp = NULL;

  ALOGE("%s:%d] ", __func__, __LINE__);
  if (hComp == NULL) {
    return OMX_ErrorBadParameter;
  }

  pthread_mutex_lock(&g_omxcore_lock);

  p_comp = (OMX_COMPONENTTYPE *)hComp;
  if (FALSE == get_idx_from_handle(hComp, &comp_idx, &inst_idx)) {
    ALOGE("%s:%d] Error invalid component", __func__, __LINE__);
    pthread_mutex_unlock(&g_omxcore_lock);
    return OMX_ErrorInvalidComponent;
  }


  //Deinit the component;
  rc = p_comp->ComponentDeInit(hComp);
  if (rc != OMX_ErrorNone) {
    /* Remove the handle from the comp structure */
    ALOGE("%s:%d] Error comp deinit failed", __func__, __LINE__);
    pthread_mutex_unlock(&g_omxcore_lock);
    return OMX_ErrorInvalidComponent;
  }
  p_core_comp = &g_omxcore->component[comp_idx];
  p_core_comp->handle[inst_idx] = NULL;
  if (!is_comp_active(p_core_comp)) {
    rc = dlclose(p_core_comp->lib_handle);
    p_core_comp->lib_handle = NULL;
    p_core_comp->get_instance = NULL;
    p_core_comp->create_comp_func = NULL;
    p_core_comp->open = FALSE;
  } else {
    ALOGE("%s:%d] Error Component is still Active", __func__, __LINE__);
  }
  pthread_mutex_unlock(&g_omxcore_lock);
  ALOGE("%s:%d] Success", __func__, __LINE__);
  return rc;
}
