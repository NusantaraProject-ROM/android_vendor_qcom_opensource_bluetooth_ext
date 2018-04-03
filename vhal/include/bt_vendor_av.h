/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

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
*/

#ifndef ANDROID_INCLUDE_BT_VENDOR_AV_H
#define ANDROID_INCLUDE_BT_VENDOR_AV_H
#include <hardware/bt_av.h>
#include "bt_target.h"
__BEGIN_DECLS
#if (TWS_ENABLED == TRUE)
enum {
  BTAV_A2DP_CODEC_INDEX_SOURCE_APTX_TWS = BTAV_A2DP_CODEC_INDEX_SOURCE_MAX,
  BTAV_VENDOR_A2DP_CODEC_INDEX_SOURCE_MAX,
  BTAV_VENDOR_A2DP_CODEC_INDEX_SINK_MIN = BTAV_VENDOR_A2DP_CODEC_INDEX_SOURCE_MAX,
  BTAV_VENDOR_A2DP_CODEC_INDEX_SINK_SBC = BTAV_VENDOR_A2DP_CODEC_INDEX_SINK_MIN,
  BTAV_VENDOR_A2DP_CODEC_INDEX_SINK_MAX,
  BTAV_VENDOR_A2DP_CODEC_INDEX_MAX = BTAV_VENDOR_A2DP_CODEC_INDEX_SINK_MAX,
};
#endif
__END_DECLS
#endif
