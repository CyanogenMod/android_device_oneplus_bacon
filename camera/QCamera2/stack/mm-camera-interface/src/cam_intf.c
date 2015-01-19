/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include "cam_intf.h"

void *POINTER_OF_PARAM(cam_intf_parm_type_t PARAM_ID,
                 void *table_ptr)
{
  parm_buffer_new_t *TABLE_PTR = (parm_buffer_new_t *)table_ptr;
  void *tmp_p;
  size_t j = 0, i = TABLE_PTR->num_entry;
  tmp_p = (void *) &TABLE_PTR->entry[0];
  parm_entry_type_new_t *curr_param = (parm_entry_type_new_t *) tmp_p;

  for (j = 0; j < i; j++) {
    if (PARAM_ID == curr_param->entry_type) {
      return (void *)&curr_param->data[0];
    }
    curr_param = GET_NEXT_PARAM(curr_param, parm_entry_type_new_t);
  }
  tmp_p = (void *) &TABLE_PTR->entry[0];
  curr_param = (parm_entry_type_new_t *) tmp_p;
  return (void *)&curr_param->data[0]; //should not be coming here
                                       //this is just to prevent a crash
                                       //for the caller
}

