/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include "unity.h"
#include "AppRtspSrc.h"
#include "mock_AppRtspSrcWrap.h"

#define APP_RTSPSRC_UTEST_RTSP_URL              "rtsp://192.168.1.1:8554/live.sdp"
#define APP_RTSPSRC_UTEST_RTSP_USERNAME         "username"
#define APP_RTSPSRC_UTEST_RTSP_USERNAME_NULL    ""
#define APP_RTSPSRC_UTEST_RTSP_PASSWORD         "password"
#define APP_RTSPSRC_UTEST_RTSP_PASSWORD_NULL    ""
#define APP_RTSPSRC_UTEST_BUS_MSG_ERROR         "bus-msg-error"
#define APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION FRAME_CURRENT_VERSION + 1

#define GST_ELEMENT_FACTORY_NAME_RTSPSRC        "rtspsrc"
#define GST_ELEMENT_FACTORY_NAME_QUEUE          "queue"
#define GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_H264 "rtph264depay"
#define GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_VP8  "rtpvp8depay"
#define GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_OPUS "rtpopusdepay"
#define GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_PCMU "rtppcmudepay"
#define GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_PCMA "rtppcmadepay"
#define GST_ELEMENT_FACTORY_NAME_CAPS_FILTER    "capsfilter"
#define GST_ELEMENT_FACTORY_NAME_APP_SINK       "appsink"
#define GST_ELEMENT_FACTORY_NAME_FAKE_SINK      "fakesink"

#define GST_SIGNAL_CALLBACK_NEW_SAMPLE   "new-sample"
#define GST_SIGNAL_CALLBACK_PAD_ADDED    "pad-added"
#define GST_SIGNAL_CALLBACK_PAD_REMOVED  "pad-removed"
#define GST_SIGNAL_CALLBACK_NO_MORE_PADS "no-more-pads"
#define GST_SIGNAL_CALLBACK_MSG_ERROR    "message::error"
#define GST_SIGNAL_CALLBACK_MSG_EOS      "message::eos"

#define GST_STRUCT_FIELD_MEDIA              "media"
#define GST_STRUCT_FIELD_MEDIA_VIDEO        "video"
#define GST_STRUCT_FIELD_MEDIA_AUDIO        "audio"
#define GST_STRUCT_FIELD_ENCODING           "encoding-name"
#define GST_STRUCT_FIELD_PKT_MODE           "packetization-mode"
#define GST_STRUCT_FIELD_PROFILE_LV_ID      "profile-level-id"
#define GST_STRUCT_FIELD_ENCODING_H264      "H264"
#define GST_STRUCT_FIELD_ENCODING_VP8       "VP8"
#define GST_STRUCT_FIELD_ENCODING_PCMU      "PCMU"
#define GST_STRUCT_FIELD_ENCODING_PCMA      "PCMA"
#define GST_STRUCT_FIELD_ENCODING_OPUS      "opus"
#define GST_STRUCT_FIELD_ENCODING_G722      "G722"
#define GST_STRUCT_FIELD_PAYLOAD_TYPE       "payload"
#define GST_STRUCT_FIELD_PAYLOAD_TYPE_VIDEO 96
#define GST_STRUCT_FIELD_PAYLOAD_TYPE_AUDIO 97
#define GST_STRUCT_FIELD_NA                 "NA"
#define GST_STRUCT_FIELD_CLOCK_RATE         "clock-rate"
#define GST_STRUCT_FIELD_CLOCK_RATE_VIDEO   90000
#define GST_STRUCT_FIELD_CLOCK_RATE_PCMU    8000

typedef void (*RtspSrcNoMorePads)(GstElement* element, gpointer udata);
typedef void (*RtspSrcPadAdded)(GstElement* element, GstPad* pad, gpointer udata);
typedef void (*RtspSrcPadRemoved)(GstElement* element, GstPad* pad, gpointer udata);
typedef GstFlowReturn (*NewSampleFromAppSink)(GstElement* sink, gpointer udata);
typedef void (*MsgErrorFromBus)(GstBus* bus, GstMessage* msg, gpointer* udata);
typedef void (*MsgEosFromBus)(GstBus* bus, GstMessage* msg, gpointer* udata);

typedef struct {
    GstBus* pBus;
    GstElement* pPipeline;
    GstElement* pRtspSrc;
    GstElement* pVideoQueue;
    GstElement* pAudioQueue;
    GstElement* pRtph264depay;
    GstElement* pRtpvp8depay;
    GstElement* pRtpopusdepay;
    GstElement* pRtppcmudepay;
    GstElement* pRtppcmadepay;
    GstElement* pVideoCaps;
    GstElement* pAudioCaps;
    GstElement* pVideoCapsfilter;
    GstElement* pAudioCapsfilter;
    GstElement* pVideoAppsink;
    GstElement* pAudioAppsink;
    GstElement* pDummyAppsink;
    GTypeInstance* pDummyInstance;
    GType* pDummyGType;
} GstMockElementList, *PGstMockElementList;

typedef struct {
    RtspSrcNoMorePads noMorePads;
    RtspSrcPadAdded padAdded;
    RtspSrcPadRemoved padRemoved;
    NewSampleFromAppSink newSampleFromAppSink;
    MsgErrorFromBus msgErrorFromBus;
    MsgEosFromBus msgEosFromBus;
    void* uData;
    GstMockElementList elementList;
    Frame mediaSinkFrame;
    BOOL mediaEosVal;
    CHAR msgErrorMessage[16];
    GError msgError;
    GstMapInfo mapInfo;
} GstMock, *PGstMock;

static GstElement mDummyElement;
static GTypeInstance mDummyInstance;
static GType mDummyGType;
static GstBus mGstBus;
static GstMock mGstMock;
static memCalloc BackGlobalMemCalloc;
static createMutex BackGlobalCreateMutex;

/* Called before each test method. */
void setUp()
{
    PGstMock pGstMock = &mGstMock;
    memset(pGstMock, 0, sizeof(GstMock));
    PGstMockElementList pGstMockElementList = &pGstMock->elementList;
    pGstMockElementList->pBus = &mGstBus;
    pGstMockElementList->pPipeline = &mDummyElement;
    pGstMockElementList->pRtspSrc = &mDummyElement;
    pGstMockElementList->pVideoQueue = &mDummyElement;
    pGstMockElementList->pAudioQueue = &mDummyElement;
    pGstMockElementList->pRtph264depay = &mDummyElement;
    pGstMockElementList->pRtpvp8depay = &mDummyElement;
    pGstMockElementList->pRtpopusdepay = &mDummyElement;
    pGstMockElementList->pRtppcmudepay = &mDummyElement;
    pGstMockElementList->pRtppcmadepay = &mDummyElement;
    pGstMockElementList->pVideoCaps = &mDummyElement;
    pGstMockElementList->pAudioCaps = &mDummyElement;
    pGstMockElementList->pVideoCapsfilter = &mDummyElement;
    pGstMockElementList->pAudioCapsfilter = &mDummyElement;
    pGstMockElementList->pVideoAppsink = &mDummyElement;
    pGstMockElementList->pAudioAppsink = &mDummyElement;
    pGstMockElementList->pDummyAppsink = &mDummyElement;
    pGstMockElementList->pDummyInstance = &mDummyInstance;
    pGstMockElementList->pDummyGType = &mDummyGType;
    memset(pGstMock->msgErrorMessage, 0, 16);
    memcpy(pGstMock->msgErrorMessage, APP_RTSPSRC_UTEST_BUS_MSG_ERROR, strlen(APP_RTSPSRC_UTEST_BUS_MSG_ERROR));
}

/* Called after each test method. */
void tearDown()
{
}

static PGstMock getGstMock()
{
    return &mGstMock;
}

static gulong app_g_signal_connect_callback(gpointer instance, const gchar* detailed_signal, GCallback c_handler, gpointer data, int NumCalls)
{
    PGstMock pGstMock = getGstMock();
    pGstMock->uData = data;
    if (strcmp(GST_SIGNAL_CALLBACK_NEW_SAMPLE, detailed_signal) == 0) {
        pGstMock->newSampleFromAppSink = c_handler;
    } else if (strcmp(GST_SIGNAL_CALLBACK_PAD_ADDED, detailed_signal) == 0) {
        pGstMock->padAdded = c_handler;
    } else if (strcmp(GST_SIGNAL_CALLBACK_PAD_REMOVED, detailed_signal) == 0) {
        pGstMock->padRemoved = c_handler;
    } else if (strcmp(GST_SIGNAL_CALLBACK_NO_MORE_PADS, detailed_signal) == 0) {
        pGstMock->noMorePads = c_handler;
    } else if (strcmp(GST_SIGNAL_CALLBACK_MSG_ERROR, detailed_signal) == 0) {
        pGstMock->msgErrorFromBus = c_handler;
    } else if (strcmp(GST_SIGNAL_CALLBACK_MSG_EOS, detailed_signal) == 0) {
        pGstMock->msgEosFromBus = c_handler;
    } else {
    }
    return NumCalls;
}

static GstElement* app_gst_element_factory_make_video_only_callback(const gchar* factoryname, const gchar* name, int NumCalls)
{
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTSPSRC) == 0) {
        return pElementList->pRtspSrc;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_QUEUE) == 0) {
        return pElementList->pVideoQueue;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_H264) == 0) {
        return pElementList->pRtph264depay;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_VP8) == 0) {
        return pElementList->pRtpvp8depay;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_OPUS) == 0) {
        return pElementList->pRtpopusdepay;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_PCMU) == 0) {
        return pElementList->pRtppcmudepay;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_PCMA) == 0) {
        return pElementList->pRtppcmadepay;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_CAPS_FILTER) == 0) {
        return pElementList->pVideoCapsfilter;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_APP_SINK) == 0) {
        return pElementList->pVideoAppsink;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_FAKE_SINK) == 0) {
        return pElementList->pDummyAppsink;
    }
    return NULL;
}

static GstElement* app_gst_element_factory_make_audio_only_callback(const gchar* factoryname, const gchar* name, int NumCalls)
{
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTSPSRC) == 0) {
        return pElementList->pRtspSrc;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_QUEUE) == 0) {
        return pElementList->pAudioQueue;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_OPUS) == 0) {
        return pElementList->pRtpopusdepay;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_PCMU) == 0) {
        return pElementList->pRtppcmudepay;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_PCMA) == 0) {
        return pElementList->pRtppcmadepay;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_CAPS_FILTER) == 0) {
        return pElementList->pAudioCapsfilter;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_APP_SINK) == 0) {
        return pElementList->pAudioAppsink;
    } else if (strcmp(factoryname, GST_ELEMENT_FACTORY_NAME_FAKE_SINK) == 0) {
        return pElementList->pDummyAppsink;
    }
    return NULL;
}

static gboolean app_gst_structure_has_field_full_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_PAYLOAD_TYPE, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_CLOCK_RATE, fieldname) == 0) {
        return TRUE;
    }
    return FALSE;
}

static gboolean app_gst_structure_has_field_no_encoding_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_PAYLOAD_TYPE, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_CLOCK_RATE, fieldname) == 0) {
        return TRUE;
    }
    return FALSE;
}

static gboolean app_gst_structure_has_field_no_media_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_PAYLOAD_TYPE, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_CLOCK_RATE, fieldname) == 0) {
        return TRUE;
    }
    return FALSE;
}

static gboolean app_gst_structure_has_field_no_payload_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_CLOCK_RATE, fieldname) == 0) {
        return TRUE;
    }
    return FALSE;
}

static gboolean app_gst_structure_has_field_no_clock_rate_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return TRUE;
    } else if (strcmp(GST_STRUCT_FIELD_PAYLOAD_TYPE, fieldname) == 0) {
        return TRUE;
    }
    return FALSE;
}

static GstStateChangeReturn app_gst_element_set_state_callback(GstElement* element, GstState state, int NumCalls)
{
    return GST_STATE_CHANGE_SUCCESS;
}

static STATUS mediaSinkHook_callback(PVOID udata, PFrame pFrame)
{
    PFrame pUserFrame = (PFrame) udata;
    memcpy(pUserFrame, pFrame, sizeof(Frame));
    return STATUS_SUCCESS;
}

static STATUS mediaEosHook_callback(PVOID udata)
{
    *(PBOOL*) udata = TRUE;
    return STATUS_SUCCESS;
}

static gchar* app_gst_structure_get_string_video_only_device_h264_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return GST_STRUCT_FIELD_MEDIA_VIDEO;
    } else if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return GST_STRUCT_FIELD_ENCODING_H264;
    }

    return GST_STRUCT_FIELD_NA;
}

static gchar* app_gst_structure_get_string_video_only_device_vp8_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return GST_STRUCT_FIELD_MEDIA_VIDEO;
    } else if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return GST_STRUCT_FIELD_ENCODING_VP8;
    }

    return GST_STRUCT_FIELD_NA;
}

static gchar* app_gst_structure_get_string_video_only_device_na_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return GST_STRUCT_FIELD_MEDIA_VIDEO;
    }

    return GST_STRUCT_FIELD_NA;
}

static gchar* app_gst_structure_get_string_audio_only_device_opus_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return GST_STRUCT_FIELD_MEDIA_AUDIO;
    } else if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return GST_STRUCT_FIELD_ENCODING_OPUS;
    }

    return GST_STRUCT_FIELD_NA;
}

static gchar* app_gst_structure_get_string_audio_only_device_pcmu_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return GST_STRUCT_FIELD_MEDIA_AUDIO;
    } else if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return GST_STRUCT_FIELD_ENCODING_PCMU;
    }

    return GST_STRUCT_FIELD_NA;
}

static gchar* app_gst_structure_get_string_audio_only_device_pcma_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return GST_STRUCT_FIELD_MEDIA_AUDIO;
    } else if (strcmp(GST_STRUCT_FIELD_ENCODING, fieldname) == 0) {
        return GST_STRUCT_FIELD_ENCODING_PCMA;
    }

    return GST_STRUCT_FIELD_NA;
}

static gchar* app_gst_structure_get_string_audio_only_device_na_callback(const GstStructure* structure, const gchar* fieldname, int NumCalls)
{
    if (strcmp(GST_STRUCT_FIELD_MEDIA, fieldname) == 0) {
        return GST_STRUCT_FIELD_MEDIA_AUDIO;
    }

    return GST_STRUCT_FIELD_NA;
}

static gboolean app_gst_structure_get_int_video_only_callback(const GstStructure* structure, const gchar* fieldname, gint* value, int NumCalls)
{
    *value = -1;
    if (strcmp(GST_STRUCT_FIELD_PAYLOAD_TYPE, fieldname) == 0) {
        *value = GST_STRUCT_FIELD_PAYLOAD_TYPE_VIDEO;
        return true;
    } else if (strcmp(GST_STRUCT_FIELD_CLOCK_RATE, fieldname) == 0) {
        *value = GST_STRUCT_FIELD_CLOCK_RATE_VIDEO;
        return true;
    }
    return false;
}

static gboolean app_gst_structure_get_int_audio_only_callback(const GstStructure* structure, const gchar* fieldname, gint* value, int NumCalls)
{
    *value = -1;
    if (strcmp(GST_STRUCT_FIELD_PAYLOAD_TYPE, fieldname) == 0) {
        *value = GST_STRUCT_FIELD_PAYLOAD_TYPE_AUDIO;
        return true;
    } else if (strcmp(GST_STRUCT_FIELD_CLOCK_RATE, fieldname) == 0) {
        *value = GST_STRUCT_FIELD_CLOCK_RATE_PCMU;
        return true;
    }
    return false;
}

static void app_gst_message_parse_error_callback(GstMessage* message, GError** gerror, gchar** debug, int NumCalls)
{
    PGstMock pGstMock = getGstMock();

    if (NumCalls == 0) {
        *gerror = NULL;
        *debug = NULL;
    } else if (NumCalls == 1) {
        pGstMock->msgError.code = 1;
        pGstMock->msgError.message = pGstMock->msgErrorMessage;
        *gerror = &pGstMock->msgError;
        *debug = NULL;
    } else if (NumCalls == 2) {
        *gerror = NULL;
        *debug = pGstMock->msgErrorMessage;
    } else {
        pGstMock->msgError.code = 3;
        pGstMock->msgError.message = pGstMock->msgErrorMessage;
        *gerror = &pGstMock->msgError;
        *debug = pGstMock->msgErrorMessage;
    }
}

static gboolean app_gst_buffer_map_callback(GstBuffer* buffer, GstMapInfo* info, GstMapFlags flags)
{
    PGstMock pGstMock = getGstMock();
    *info = pGstMock->mapInfo;
    return TRUE;
}

static GstCaps* app_gst_caps_new_simple_video_only_callback(const char* media_type, const char* fieldname, ...)
{
    PGstMock pGstMock = getGstMock();
    return pGstMock->elementList.pVideoCaps;
}

static GstCaps* app_gst_caps_new_simple_audio_only_callback(const char* media_type, const char* fieldname, ...)
{
    PGstMock pGstMock = getGstMock();
    return pGstMock->elementList.pAudioCaps;
}

static void app_g_main_loop_run_discovery_callback(PVOID loop)
{
    GstElement element;
    GstPad pad;
    PGstMock pGstMock = getGstMock();

    pGstMock->padAdded(NULL, &pad, pGstMock->uData);
    pGstMock->padAdded(&element, NULL, pGstMock->uData);
    pGstMock->padAdded(&element, &pad, NULL);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    pGstMock->noMorePads(&element, pGstMock->uData);
}

static void app_g_main_loop_run_discovery_without_no_more_pads_callback(PVOID loop)
{
    GstElement element;
    GstPad pad;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    pGstMock->padAdded(NULL, &pad, pGstMock->uData);
    pGstMock->padAdded(&element, NULL, pGstMock->uData);
    pGstMock->padAdded(&element, &pad, NULL);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_media_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_encoding_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_structure_get_string_IgnoreAndReturn(GST_STRUCT_FIELD_NA);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_payload_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_clock_rate_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    pElementList->pDummyAppsink = NULL;
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    pElementList->pDummyAppsink = &mDummyElement;

    app_gst_element_link_filtered_IgnoreAndReturn(FALSE);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);

    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_FAILURE);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);

    pGstMock->padAdded(&element, &pad, pGstMock->uData);
}

static void app_g_main_loop_run_normal_video_only_callback(PVOID loop)
{
    STATUS retStatus = STATUS_SUCCESS;
    PGstMock pGstMock = getGstMock();
    // pad added
    GstElement element;
    GstPad pad;
    // new sample
    PFrame pFrame;
    GstFlowReturn gstFlowReturn;
    GstElement sink;
    GstElement sample;
    GstBuffer buffer;
    GstBuffer* pbuffer = &buffer;
    GstSegment segment;
    GstClockTime buf_pts = GST_CLOCK_TIME_NONE + 1;
    GstMapInfo* pMapInfo;
    GType dummyGType;

    pGstMock->padAdded(NULL, &pad, pGstMock->uData);
    pGstMock->padAdded(&element, NULL, pGstMock->uData);
    pGstMock->padAdded(&element, &pad, NULL);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);

    if (pGstMock->newSampleFromAppSink == NULL) {
        return;
    }
    pFrame = &pGstMock->mediaSinkFrame;
    gstFlowReturn = pGstMock->newSampleFromAppSink(NULL, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_EOS, gstFlowReturn);

    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, NULL);
    TEST_ASSERT_EQUAL(GST_FLOW_EOS, gstFlowReturn);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_app_sink_get_type_IgnoreAndReturn(dummyGType);
    app_gst_app_sink_pull_sample_IgnoreAndReturn(&sample);
    app_gst_sample_get_buffer_IgnoreAndReturn(pbuffer);
    app_gst_sample_get_segment_IgnoreAndReturn(&segment);
    app_gst_segment_to_running_time_IgnoreAndReturn(buf_pts);
    app_gst_buffer_map_IgnoreAndReturn(TRUE);
    app_gst_sample_unref_Ignore();
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(FRAME_CURRENT_VERSION, pFrame->version);

    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION, pFrame->version);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION, pFrame->version);

    GST_BUFFER_FLAGS(pbuffer) = GST_BUFFER_FLAG_LIVE;

    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DISCONT);

    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(FRAME_CURRENT_VERSION, pFrame->version);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(FRAME_CURRENT_VERSION, pFrame->version);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION, pFrame->version);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE;
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION, pFrame->version);

    GST_BUFFER_FLAGS(pbuffer) = GST_BUFFER_FLAG_DISCONT;
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE;
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION, pFrame->version);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_buffer_map_StubWithCallback(app_gst_buffer_map_callback);
    app_gst_buffer_unmap_Ignore();
    pMapInfo = &pGstMock->mapInfo;
    pMapInfo->size = 1024;
    pMapInfo->data = malloc(pMapInfo->size);
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(FRAME_CURRENT_VERSION, pFrame->version);
    TEST_ASSERT_EQUAL(pMapInfo->data, pFrame->frameData);
    TEST_ASSERT_EQUAL(pMapInfo->size, pFrame->size);
    free(pMapInfo->data);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_buffer_map_StubWithCallback(app_gst_buffer_map_callback);
    app_gst_buffer_unmap_Ignore();
    pMapInfo = &pGstMock->mapInfo;
    pMapInfo->size = 1024;
    pMapInfo->data = malloc(pMapInfo->size);
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    buf_pts = GST_CLOCK_TIME_NONE;
    app_gst_segment_to_running_time_IgnoreAndReturn(buf_pts);
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION, pFrame->version);
    TEST_ASSERT_EQUAL(0, pFrame->frameData);
    TEST_ASSERT_EQUAL(0, pFrame->size);
    buf_pts = GST_CLOCK_TIME_NONE + 1;
    app_gst_segment_to_running_time_IgnoreAndReturn(buf_pts);
    free(pMapInfo->data);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_buffer_map_IgnoreAndReturn(FALSE);
    ;
    app_gst_buffer_unmap_Ignore();
    pMapInfo = &pGstMock->mapInfo;
    pMapInfo->size = 1024;
    pMapInfo->data = malloc(pMapInfo->size);
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION, pFrame->version);
    TEST_ASSERT_EQUAL(0, pFrame->frameData);
    TEST_ASSERT_EQUAL(0, pFrame->size);
    free(pMapInfo->data);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_buffer_map_StubWithCallback(app_gst_buffer_map_callback);
    app_gst_buffer_unmap_Ignore();
    pMapInfo = &pGstMock->mapInfo;
    pMapInfo->size = 1024;
    pMapInfo->data = malloc(pMapInfo->size);
    memset(pFrame, 0, sizeof(Frame));
    pFrame->version = APP_RTSPSRC_UTEST_FRAME_INVALID_VERSION;
    shutdownMediaSource(pGstMock->uData);
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    TEST_ASSERT_EQUAL(FRAME_CURRENT_VERSION, pFrame->version);
    TEST_ASSERT_EQUAL(pMapInfo->data, pFrame->frameData);
    TEST_ASSERT_EQUAL(pMapInfo->size, pFrame->size);
    free(pMapInfo->data);
}

static void app_g_main_loop_run_normal_video_only_pad_added_callback(PVOID loop)
{
    STATUS retStatus = STATUS_SUCCESS;
    PGstMock pGstMock = getGstMock();
    // pad added
    GstElement element;
    GstPad pad;
    // new sample
    PFrame pFrame;
    GstFlowReturn gstFlowReturn;
    GstElement sink;
    GstElement sample;
    GstBuffer buffer;
    GstBuffer* pbuffer = &buffer;
    GstSegment segment;
    GstClockTime buf_pts = GST_CLOCK_TIME_NONE + 1;
    GstMapInfo* pMapInfo;
    GType dummyGType;

    pGstMock->padAdded(NULL, &pad, pGstMock->uData);
    pGstMock->padAdded(&element, NULL, pGstMock->uData);
    pGstMock->padAdded(&element, &pad, NULL);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_media_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_structure_get_string_IgnoreAndReturn(GST_STRUCT_FIELD_NA);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_payload_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_clock_rate_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_element_link_filtered_IgnoreAndReturn(FALSE);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);

    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_FAILURE);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);

    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    if (pGstMock->newSampleFromAppSink == NULL) {
        return;
    }

    pFrame = &pGstMock->mediaSinkFrame;
    gstFlowReturn = pGstMock->newSampleFromAppSink(NULL, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_EOS, gstFlowReturn);

    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, NULL);
    TEST_ASSERT_EQUAL(GST_FLOW_EOS, gstFlowReturn);
}

static void app_g_main_loop_run_normal_video_only_pad_removed_callback(PVOID loop)
{
    PGstMock pGstMock = getGstMock();
    GstElement element;
    GstPad pad;
    pGstMock->padRemoved(NULL, &pad, pGstMock->uData);
    pGstMock->padRemoved(&element, NULL, pGstMock->uData);
    pGstMock->padRemoved(&element, &pad, NULL);
    pGstMock->padRemoved(&element, &pad, pGstMock->uData);
}

static void app_g_main_loop_run_normal_video_only_msg_error_callback(PVOID loop)
{
    PGstMock pGstMock = getGstMock();
    GstBus bus;
    GstMessage msg;
    app_g_error_free_Ignore();
    app_gst_object_get_name_IgnoreAndReturn("error src");
    app_gst_message_parse_error_StubWithCallback(app_gst_message_parse_error_callback);
    pGstMock->msgErrorFromBus(NULL, &msg, pGstMock->uData);
    pGstMock->msgErrorFromBus(&bus, NULL, pGstMock->uData);
    pGstMock->msgErrorFromBus(&bus, &msg, NULL);
    pGstMock->msgErrorFromBus(&bus, &msg, pGstMock->uData);
    pGstMock->msgErrorFromBus(&bus, &msg, pGstMock->uData);
    pGstMock->msgErrorFromBus(&bus, &msg, pGstMock->uData);
    pGstMock->msgErrorFromBus(&bus, &msg, pGstMock->uData);
}

static void app_g_main_loop_run_normal_video_only_msg_eos_callback(PVOID loop)
{
    STATUS retStatus = STATUS_SUCCESS;
    PGstMock pGstMock = getGstMock();
    GstBus bus;
    GstMessage msg;
    pGstMock->msgEosFromBus(NULL, &msg, pGstMock->uData);
    pGstMock->msgEosFromBus(&bus, NULL, pGstMock->uData);
    pGstMock->msgEosFromBus(&bus, &msg, NULL);
    pGstMock->msgEosFromBus(&bus, &msg, pGstMock->uData);
    TEST_ASSERT_EQUAL(TRUE, pGstMock->mediaEosVal);
}

static void app_g_main_loop_run_normal_audio_only_pad_added_callback(PVOID loop)
{
    STATUS retStatus = STATUS_SUCCESS;
    PGstMock pGstMock = getGstMock();
    // pad added
    GstElement element;
    GstPad pad;
    // new sample
    PFrame pFrame;
    GstFlowReturn gstFlowReturn;
    GstElement sink;
    GstElement sample;
    GstBuffer buffer;
    GstBuffer* pbuffer = &buffer;
    GstSegment segment;
    GstClockTime buf_pts = GST_CLOCK_TIME_NONE + 1;
    GstMapInfo* pMapInfo;
    GType dummyGType;

    pGstMock->padAdded(NULL, &pad, pGstMock->uData);
    pGstMock->padAdded(&element, NULL, pGstMock->uData);
    pGstMock->padAdded(&element, &pad, NULL);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_media_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_structure_get_string_IgnoreAndReturn(GST_STRUCT_FIELD_NA);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_audio_only_device_opus_callback);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_payload_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_no_clock_rate_callback);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);

    app_gst_element_link_filtered_IgnoreAndReturn(FALSE);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);

    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_FAILURE);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);

    pGstMock->padAdded(&element, &pad, pGstMock->uData);
    if (pGstMock->newSampleFromAppSink == NULL) {
        return;
    }

    pFrame = &pGstMock->mediaSinkFrame;
    gstFlowReturn = pGstMock->newSampleFromAppSink(NULL, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_EOS, gstFlowReturn);

    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, NULL);
    TEST_ASSERT_EQUAL(GST_FLOW_EOS, gstFlowReturn);
}

static void app_g_main_loop_run_normal_video_only_no_hook_callback(PVOID loop)
{
    STATUS retStatus = STATUS_SUCCESS;
    PGstMock pGstMock = getGstMock();
    // pad added
    GstElement element;
    GstPad pad;
    // new sample
    GstFlowReturn gstFlowReturn;
    GstElement sink;
    GstElement sample;
    GstBuffer buffer;
    GstBuffer* pbuffer = &buffer;
    GstSegment segment;
    GstClockTime buf_pts = GST_CLOCK_TIME_NONE + 1;
    GstMapInfo* pMapInfo;
    GType dummyGType;

    pGstMock->padAdded(NULL, &pad, pGstMock->uData);
    pGstMock->padAdded(&element, NULL, pGstMock->uData);
    pGstMock->padAdded(&element, &pad, NULL);
    pGstMock->padAdded(&element, &pad, pGstMock->uData);

    if (pGstMock->newSampleFromAppSink == NULL) {
        return;
    }

    gstFlowReturn = pGstMock->newSampleFromAppSink(NULL, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_EOS, gstFlowReturn);

    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, NULL);
    TEST_ASSERT_EQUAL(GST_FLOW_EOS, gstFlowReturn);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_app_sink_get_type_IgnoreAndReturn(dummyGType);
    app_gst_app_sink_pull_sample_IgnoreAndReturn(&sample);
    app_gst_sample_get_buffer_IgnoreAndReturn(pbuffer);
    app_gst_sample_get_segment_IgnoreAndReturn(&segment);
    app_gst_segment_to_running_time_IgnoreAndReturn(buf_pts);
    app_gst_buffer_map_IgnoreAndReturn(TRUE);
    app_gst_sample_unref_Ignore();
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);

    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_SET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);

    GST_BUFFER_FLAGS(pbuffer) = GST_BUFFER_FLAG_DISCONT;
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE;
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_buffer_map_StubWithCallback(app_gst_buffer_map_callback);
    app_gst_buffer_unmap_Ignore();
    pMapInfo = &pGstMock->mapInfo;
    pMapInfo->size = 1024;
    pMapInfo->data = malloc(pMapInfo->size);
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    free(pMapInfo->data);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_buffer_map_StubWithCallback(app_gst_buffer_map_callback);
    app_gst_buffer_unmap_Ignore();
    pMapInfo = &pGstMock->mapInfo;
    pMapInfo->size = 1024;
    pMapInfo->data = malloc(pMapInfo->size);
    buf_pts = GST_CLOCK_TIME_NONE;
    app_gst_segment_to_running_time_IgnoreAndReturn(buf_pts);
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    buf_pts = GST_CLOCK_TIME_NONE + 1;
    app_gst_segment_to_running_time_IgnoreAndReturn(buf_pts);
    free(pMapInfo->data);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_buffer_map_IgnoreAndReturn(FALSE);
    app_gst_buffer_unmap_Ignore();
    pMapInfo = &pGstMock->mapInfo;
    pMapInfo->size = 1024;
    pMapInfo->data = malloc(pMapInfo->size);
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    free(pMapInfo->data);

    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_CORRUPTED);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DECODE_ONLY);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DISCONT);
    GST_BUFFER_FLAG_UNSET(pbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    GST_BUFFER_PTS(pbuffer) = GST_CLOCK_TIME_NONE + 1;
    app_gst_buffer_map_StubWithCallback(app_gst_buffer_map_callback);
    app_gst_buffer_unmap_Ignore();
    pMapInfo = &pGstMock->mapInfo;
    pMapInfo->size = 1024;
    pMapInfo->data = malloc(pMapInfo->size);
    shutdownMediaSource(pGstMock->uData);
    gstFlowReturn = pGstMock->newSampleFromAppSink(&sink, pGstMock->uData);
    TEST_ASSERT_EQUAL(GST_FLOW_OK, gstFlowReturn);
    free(pMapInfo->data);
}

static PVOID null_memCalloc(SIZE_T num, SIZE_T size)
{
    return NULL;
}

static MUTEX null_createMutex(BOOL reentrant)
{
    return NULL;
}

void test_null_context(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext = NULL;
    RTC_CODEC codec;

    retStatus = initMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = isMediaSourceReady(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = queryMediaVideoCap(NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(NULL, &codec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = queryMediaAudioCap(NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaAudioCap(NULL, &codec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = linkMeidaSinkHook(NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = linkMeidaEosHook(NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = runMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = detroyMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
}

void test_initMediaSource(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    unsetenv(APP_MEDIA_RTSP_URL);
    unsetenv(APP_MEDIA_RTSP_USERNAME);
    unsetenv(APP_MEDIA_RTSP_PASSWORD);

    retStatus = initMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    app_gst_init_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_RTSP_URL, retStatus);

    // testing discoverMediaSource().
    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);

    app_gst_pipeline_new_IgnoreAndReturn(NULL);
    // the cleanup sequence
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);

    // testing initGstRtspSrc().
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    pElementList->pRtspSrc = NULL;
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_MISSING_PLUGIN, retStatus);
    pElementList->pRtspSrc = &mDummyElement;

    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();

    app_gst_element_get_bus_IgnoreAndReturn(NULL);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_MISSING_BUS, retStatus);

    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_FAILURE);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_PLAY, retStatus);

    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    unsetenv(APP_MEDIA_RTSP_USERNAME);
    unsetenv(APP_MEDIA_RTSP_PASSWORD);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    unsetenv(APP_MEDIA_RTSP_USERNAME);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD_NULL, 1);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    unsetenv(APP_MEDIA_RTSP_PASSWORD);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD_NULL, 1);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME_NULL, 1);
    unsetenv(APP_MEDIA_RTSP_PASSWORD);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    CHAR username[APP_MEDIA_RTSP_USERNAME_LEN << 1];
    CHAR password[APP_MEDIA_RTSP_USERNAME_LEN << 1];
    MEMSET(username, 'T', APP_MEDIA_RTSP_USERNAME_LEN << 1);
    username[(APP_MEDIA_RTSP_USERNAME_LEN << 1) - 1] = '\0';
    MEMSET(password, 'T', APP_MEDIA_RTSP_USERNAME_LEN << 1);
    password[(APP_MEDIA_RTSP_USERNAME_LEN << 1) - 1] = '\0';

    setenv(APP_MEDIA_RTSP_USERNAME, username, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, password, 1);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_RTSP_CREDENTIAL, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, password, 1);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_RTSP_CREDENTIAL, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    setenv(APP_MEDIA_RTSP_USERNAME, username, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_RTSP_CREDENTIAL, retStatus);
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_initMediaSource_null(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    BackGlobalMemCalloc = globalMemCalloc;
    globalMemCalloc = null_memCalloc;

    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_ENOUGH_MEMORY, retStatus);

    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    globalMemCalloc = BackGlobalMemCalloc;
}

void test_initMediaSource_null_mutex(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    BackGlobalCreateMutex = globalCreateMutex;
    globalCreateMutex = null_createMutex;

    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_INVALID_MUTEX, retStatus);

    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);

    globalCreateMutex = BackGlobalCreateMutex;
}

void test_video_only_device_h264_discover_fail(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);

    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_without_no_more_pads_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_READY, retStatus);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_READY, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_READY, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_h264_run_fail(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);

    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_gst_pipeline_new_IgnoreAndReturn(NULL);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_no_hook_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_MISSING_PIPELINE, retStatus);
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);

    pElementList->pRtspSrc = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_MISSING_PLUGIN, retStatus);
    pElementList->pRtspSrc = &mDummyElement;

    app_gst_element_get_bus_IgnoreAndReturn(NULL);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_MISSING_BUS, retStatus);
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);

    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_FAILURE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_PLAY, retStatus);
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_h264_pad_added_hook(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);
    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_pad_added_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_h264_no_hook(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);

    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_no_hook_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_h264_null_pipe(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);

    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    GstMessage msg;
    pGstMock->msgEosFromBus(pElementList->pBus, &msg, pGstMock->uData);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.

    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_h264_null_sink(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    pElementList->pDummyAppsink = NULL;
    pElementList->pVideoAppsink = NULL;
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_pad_added_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pDummyAppsink = &mDummyElement;
    pElementList->pVideoAppsink = &mDummyElement;

    pElementList->pVideoQueue = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pVideoQueue = &mDummyElement;

    pElementList->pRtph264depay = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pRtph264depay = &mDummyElement;

    pElementList->pVideoCaps = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pVideoCaps = &mDummyElement;

    pElementList->pVideoCapsfilter = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pVideoCapsfilter = &mDummyElement;

    app_gst_element_link_many_IgnoreAndReturn(FALSE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_h264(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_pad_removed(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);

    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_pad_removed_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_msg_error(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_msg_error_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    retStatus = linkMeidaEosHook(pMediaContext, NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_msg_error_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_msg_eos(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_h264_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_msg_eos_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    retStatus = linkMeidaEosHook(pMediaContext, NULL, NULL);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_vp8(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_vp8_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_VP8, pCodec);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_video_only_device_na(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_video_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_video_only_device_na_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_video_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_video_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_audio_only_device_opus_null_sink(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_audio_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_audio_only_device_opus_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_audio_only_callback);
    pElementList->pDummyAppsink = NULL;
    pElementList->pAudioAppsink = NULL;
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_OPUS, pCodec);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_audio_only_pad_added_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_audio_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pDummyAppsink = &mDummyElement;
    pElementList->pAudioAppsink = &mDummyElement;

    pElementList->pAudioQueue = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pAudioQueue = &mDummyElement;

    pElementList->pRtpopusdepay = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pRtpopusdepay = &mDummyElement;

    pElementList->pAudioCaps = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pAudioCaps = &mDummyElement;

    pElementList->pAudioCapsfilter = NULL;
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    pElementList->pAudioCapsfilter = &mDummyElement;

    app_gst_element_link_many_IgnoreAndReturn(FALSE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);

    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_audio_only_device_opus(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_audio_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_audio_only_device_opus_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_audio_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_OPUS, pCodec);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_audio_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_audio_only_device_pcmu(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_audio_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_audio_only_device_pcmu_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_audio_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_MULAW, pCodec);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_audio_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_audio_only_device_pcma(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_audio_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_audio_only_device_pcma_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_audio_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);
    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(RTC_CODEC_ALAW, pCodec);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_audio_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}

void test_audio_only_device_na(void)
{
    STATUS retStatus = STATUS_SUCCESS;
    PMediaContext pMediaContext;
    PGstMock pGstMock = getGstMock();
    PGstMockElementList pElementList = &pGstMock->elementList;

    setenv(APP_MEDIA_RTSP_URL, APP_RTSPSRC_UTEST_RTSP_URL, 1);
    setenv(APP_MEDIA_RTSP_USERNAME, APP_RTSPSRC_UTEST_RTSP_USERNAME, 1);
    setenv(APP_MEDIA_RTSP_PASSWORD, APP_RTSPSRC_UTEST_RTSP_PASSWORD, 1);
    // step.1: initilaize this media source as video-only device.
    app_gst_init_Ignore();
    app_gst_pipeline_new_IgnoreAndReturn(pElementList->pPipeline);
    app_gst_element_factory_make_StubWithCallback(app_gst_element_factory_make_audio_only_callback);
    app_g_type_check_instance_cast_IgnoreAndReturn(pElementList->pDummyInstance);
    app_g_object_set_Ignore();
    app_g_signal_connect_StubWithCallback(app_g_signal_connect_callback);
    app_gst_bin_get_type_IgnoreAndReturn(pElementList->pDummyGType);
    app_gst_bin_add_many_Ignore();
    app_gst_element_get_bus_IgnoreAndReturn(pElementList->pBus);
    app_gst_bus_add_signal_watch_Ignore();
    app_gst_element_set_state_IgnoreAndReturn(GST_STATE_CHANGE_SUCCESS);
    app_g_main_loop_new_IgnoreAndReturn(pGstMock);
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_discovery_callback);

    GstCaps template_caps;
    GstCaps current_caps;
    GstStructure srcPadStructure;
    app_gst_pad_get_name_IgnoreAndReturn("srcPadName");
    app_gst_pad_get_pad_template_caps_IgnoreAndReturn(&template_caps);
    app_gst_pad_get_current_caps_IgnoreAndReturn(&current_caps);
    app_gst_caps_get_size_IgnoreAndReturn(1);
    app_gst_caps_get_structure_IgnoreAndReturn(&srcPadStructure);
    app_gst_structure_has_field_StubWithCallback(app_gst_structure_has_field_full_callback);
    app_gst_structure_get_string_StubWithCallback(app_gst_structure_get_string_audio_only_device_na_callback);
    app_gst_structure_get_int_StubWithCallback(app_gst_structure_get_int_audio_only_callback);
    app_gst_element_link_filtered_IgnoreAndReturn(TRUE);
    app_g_free_Ignore();
    app_gst_caps_unref_Ignore();
    app_gst_bus_remove_signal_watch_Ignore();
    app_gst_object_unref_Ignore();
    app_g_main_loop_unref_Ignore();
    app_g_main_loop_quit_Ignore();
    retStatus = initMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.2: query the capabilaity of this device.
    RTC_CODEC pCodec;
    retStatus = queryMediaVideoCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    retStatus = queryMediaAudioCap(pMediaContext, &pCodec);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NOT_EXISTED, retStatus);

    // step.3: confirm the media source is ready or not.
    retStatus = isMediaSourceReady(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.4: link the hook function of media sink .
    retStatus = linkMeidaSinkHook(pMediaContext, mediaSinkHook_callback, &pGstMock->mediaSinkFrame);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.5: link the hook function of media eos.
    pGstMock->mediaEosVal = FALSE;
    retStatus = linkMeidaEosHook(pMediaContext, mediaEosHook_callback, &pGstMock->mediaEosVal);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);

    // step.6: run the media source.
    app_g_main_loop_run_StubWithCallback(app_g_main_loop_run_normal_video_only_callback);
    app_gst_caps_new_simple_StubWithCallback(app_gst_caps_new_simple_audio_only_callback);
    app_gst_element_link_many_IgnoreAndReturn(TRUE);
    retStatus = (STATUS) runMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.7 shut down the media source.
    retStatus = shutdownMediaSource(NULL);
    TEST_ASSERT_EQUAL(STATUS_MEDIA_NULL_ARG, retStatus);
    retStatus = shutdownMediaSource(pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    // step.8 destroy the meida source.
    retStatus = detroyMediaSource(&pMediaContext);
    TEST_ASSERT_EQUAL(STATUS_SUCCESS, retStatus);
    TEST_ASSERT_EQUAL(NULL, pMediaContext);
}
