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
#define LOG_CLASS "AppRtspSrc"
#include "AppRtspSrc.h"
#include "AppRtspSrcWrap.h"

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

#define GST_STRUCT_FIELD_MEDIA         "media"
#define GST_STRUCT_FIELD_MEDIA_VIDEO   "video"
#define GST_STRUCT_FIELD_MEDIA_AUDIO   "audio"
#define GST_STRUCT_FIELD_ENCODING      "encoding-name"
#define GST_STRUCT_FIELD_PKT_MODE      "packetization-mode"
#define GST_STRUCT_FIELD_PROFILE_LV_ID "profile-level-id"
#define GST_STRUCT_FIELD_ENCODING_H264 "H264"
#define GST_STRUCT_FIELD_ENCODING_VP8  "VP8"
#define GST_STRUCT_FIELD_ENCODING_PCMU "PCMU"
#define GST_STRUCT_FIELD_ENCODING_PCMA "PCMA"
#define GST_STRUCT_FIELD_ENCODING_OPUS "opus"
#define GST_STRUCT_FIELD_PAYLOAD_TYPE  "payload"
#define GST_STRUCT_FIELD_CLOCK_RATE    "clock-rate"

#define GST_CODEC_INVALID_VALUE   0xF
#define GST_ENCODING_NAME_MAX_LEN 256

typedef struct {
    RTC_CODEC codec;
    CHAR encodingName[GST_ENCODING_NAME_MAX_LEN];
    UINT32 payloadType;
    UINT32 clockRate;
} CodecStreamConf, *PCodecStreamConf;

typedef struct {
    STATUS codecStatus;
    PVOID mainLoop;       //!< the main runner for gstreamer.
    GstElement* pipeline; //!< the pipeline for the rtsp url.
    CodecStreamConf videoStream;
    CodecStreamConf audioStream;
} CodecConfiguration, *PCodecConfiguration;

typedef struct {
    CHAR url[MAX_URI_CHAR_LEN];                 //!< the rtsp url.
    CHAR username[APP_MEDIA_RTSP_USERNAME_LEN]; //!< the username to login the rtsp url.
    CHAR password[APP_MEDIA_RTSP_PASSWORD_LEN]; //!< the password to login the rtsp url.
} RtspServerConfiguration, *PRtspServerConfiguration;

typedef struct {
    MUTEX codecConfLock;
    RtspServerConfiguration rtspServerConf; //!< the configuration of rtsp camera.
    CodecConfiguration codecConfiguration;  //!< the configuration of gstreamer.
    // the codec.
    volatile ATOMIC_BOOL shutdownRtspSrc;
    volatile ATOMIC_BOOL codecConfigLatched;
    // for meida output.
    PVOID mediaSinkHookUserdata;
    MediaSinkHook mediaSinkHook;
    PVOID mediaEosHookUserdata;
    MediaEosHook mediaEosHook;
} RtspSrcContext, *PRtspSrcContext;

static void updateCodecStatus(PRtspSrcContext pRtspSrcContext, STATUS retStatus)
{
    PCodecConfiguration pGstConfiguration = &pRtspSrcContext->codecConfiguration;
    MUTEX_LOCK(pRtspSrcContext->codecConfLock);
    pGstConfiguration->codecStatus = retStatus;
    MUTEX_UNLOCK(pRtspSrcContext->codecConfLock);
}
/**
 * @brief quitting the main loop of gstreamer.
 *
 * @param[in] the context of rtspsrc.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
static STATUS closeGstRtspSrc(PRtspSrcContext pRtspSrcContext)
{
    STATUS retStatus = STATUS_SUCCESS;

    DLOGD("closing the gst rtspsrc.");
    MUTEX_LOCK(pRtspSrcContext->codecConfLock);
    PCodecConfiguration pGstConfiguration = &pRtspSrcContext->codecConfiguration;
    if (pGstConfiguration->mainLoop != NULL) {
        app_g_main_loop_quit(pGstConfiguration->mainLoop);
    }
    ATOMIC_STORE_BOOL(&pRtspSrcContext->shutdownRtspSrc, FALSE);
    MUTEX_UNLOCK(pRtspSrcContext->codecConfLock);
    return retStatus;
}
/**
 * @brief the callback is invoked when the error happens on the bus.
 *
 * @param[in] bus the bus of the callback.
 * @param[in] msg the msg of the callback.
 * @param[in] udata the user data.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
static void onMsgErrorFromBus(GstBus* bus, GstMessage* msg, gpointer* udata)
{
    STATUS retStatus = STATUS_SUCCESS;
    GError* err;
    gchar* debug_info;
    PRtspSrcContext pRtspSrcContext = NULL;
    PCodecConfiguration pGstConfiguration;

    CHK((bus != NULL) && (msg != NULL) && (udata != NULL), STATUS_MEDIA_NULL_ARG);
    pRtspSrcContext = (PRtspSrcContext) udata;
    pGstConfiguration = &pRtspSrcContext->codecConfiguration;

    app_gst_message_parse_error(msg, &err, &debug_info);
    if (err != NULL) {
        DLOGE("error code: %d: %d", err->code, GST_RTSP_EINVAL);
        DLOGE("error received from element %s: %s\n", app_gst_object_get_name(msg->src), err->message);
    }
    if (debug_info != NULL) {
        DLOGE("debugging information: %s\n", debug_info);
    }
    closeGstRtspSrc(pRtspSrcContext);

CleanUp:
    app_g_error_free(err);
    app_g_free(debug_info);

    if (pRtspSrcContext != NULL) {
        updateCodecStatus(pRtspSrcContext, STATUS_MEDIA_BUS_ERROR);
    }

    return;
}
/**
 * @brief the callback is invoked when the end of stream happens on the bus.
 *
 * @param[in] bus the bus of the callback.
 * @param[in] msg the msg of the callback.
 * @param[in] udata the user data.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
static void onMsgEosFromBus(GstBus* bus, GstMessage* msg, gpointer* udata)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) udata;
    PCodecConfiguration pGstConfiguration;

    CHK((bus != NULL) && (msg != NULL) && (udata != NULL), STATUS_MEDIA_NULL_ARG);
    pGstConfiguration = &pRtspSrcContext->codecConfiguration;
    closeGstRtspSrc(pRtspSrcContext);
    if (pRtspSrcContext->mediaEosHook != NULL) {
        retStatus = pRtspSrcContext->mediaEosHook(pRtspSrcContext->mediaEosHookUserdata);
    }

CleanUp:

    if (pRtspSrcContext != NULL) {
        updateCodecStatus(pRtspSrcContext, STATUS_MEDIA_BUS_EOS);
    }

    return;
}
/**
 * @brief the callback is invoked when the sample of stream comes.
 *
 * @param[in] bus the sink of the callback.
 * @param[in] udata the user data.
 * @param[in] trackid the track id of the callback.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
static GstFlowReturn onNewSampleFromAppSink(GstElement* sink, gpointer udata, UINT64 trackid)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) udata;
    Frame frame;
    BOOL isDroppable, delta;
    GstFlowReturn gstFlowReturn = GST_FLOW_OK;
    GstSample* sample = NULL;
    GstBuffer* buffer;
    GstMapInfo info;
    GstSegment* segment;
    GstClockTime buf_pts;

    info.data = NULL;
    CHK((sink != NULL) && (pRtspSrcContext != NULL), STATUS_MEDIA_NULL_ARG);

    sample = app_gst_app_sink_pull_sample(APP_GST_APP_SINK(sink));
    buffer = app_gst_sample_get_buffer(sample);

    isDroppable = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_CORRUPTED) || //!< the buffer data is corrupted.
        GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DECODE_ONLY) || (GST_BUFFER_FLAGS(buffer) == GST_BUFFER_FLAG_DISCONT) ||
        (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DISCONT) && GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT)) ||
        // drop if buffer contains header only and has invalid timestamp
        !GST_BUFFER_PTS_IS_VALID(buffer);

    if (!isDroppable) {
        delta = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);

        frame.flags = delta ? FRAME_FLAG_NONE : FRAME_FLAG_KEY_FRAME;
        segment = app_gst_sample_get_segment(sample);
        buf_pts = app_gst_segment_to_running_time(segment, GST_FORMAT_TIME, buffer->pts);
        if (!GST_CLOCK_TIME_IS_VALID(buf_pts)) {
            DLOGI("frame contains invalid PTS, dropping the frame.");
            goto CleanUp;
        }
        if (!(app_gst_buffer_map(buffer, &info, GST_MAP_READ))) {
            DLOGI("media buffer mapping failed");
            goto CleanUp;
        }
        frame.trackId = trackid;
        frame.duration = 0;
        frame.version = FRAME_CURRENT_VERSION;
        frame.size = (UINT32) info.size;
        frame.frameData = (PBYTE) info.data;
        frame.presentationTs = buf_pts * DEFAULT_TIME_UNIT_IN_NANOS;
        frame.decodingTs = frame.presentationTs;
        if (pRtspSrcContext->mediaSinkHook != NULL) {
            retStatus = pRtspSrcContext->mediaSinkHook(pRtspSrcContext->mediaSinkHookUserdata, &frame);
        }
    }

CleanUp:

    if (info.data != NULL) {
        app_gst_buffer_unmap(buffer, &info);
    }
    if (sample != NULL) {
        app_gst_sample_unref(sample);
    }
    if (STATUS_FAILED(retStatus)) {
        gstFlowReturn = GST_FLOW_EOS;
    }
    if (pRtspSrcContext != NULL && (gstFlowReturn == GST_FLOW_EOS || ATOMIC_LOAD_BOOL(&pRtspSrcContext->shutdownRtspSrc))) {
        closeGstRtspSrc(pRtspSrcContext);
    }
    return gstFlowReturn;
}
/**
 * @brief the callback is invoked when the video sample of stream comes.
 *
 * @param[in] bus the sink of the callback.
 * @param[in] udata the user data.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
static GstFlowReturn onNewSampleFromVideoAppSink(GstElement* sink, gpointer udata)
{
    return onNewSampleFromAppSink(sink, udata, DEFAULT_VIDEO_TRACK_ID);
}
/**
 * @brief the callback is invoked when the audio sample of stream comes.
 *
 * @param[in] bus the sink of the callback.
 * @param[in] udata the user data.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
static GstFlowReturn onNewSampleFromAudioAppSink(GstElement* sink, gpointer udata)
{
    return onNewSampleFromAppSink(sink, udata, DEFAULT_AUDIO_TRACK_ID);
}
/**
 * @brief the dummy sink for the output of rtspsrc.
 *
 * @param[in] pRtspSrcContext the context of rtspsrc.
 * @param[in, out] ppDummySink the pointer of the dummy element.
 * @param[in] name the name of this applink.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static STATUS createDummyAppSink(PRtspSrcContext pRtspSrcContext, GstElement** ppDummySink, PCHAR name)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR elementName[APP_MEDIA_GST_ELEMENT_NAME_MAX_LEN];
    GstElement* pipeline;
    GstElement* dummySink = NULL;

    MUTEX_LOCK(pRtspSrcContext->codecConfLock);

    pipeline = (GstElement*) pRtspSrcContext->codecConfiguration.pipeline;
    SNPRINTF(elementName, APP_MEDIA_GST_ELEMENT_NAME_MAX_LEN, "dummySink%s", name);
    dummySink = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_FAKE_SINK, elementName);
    CHK(dummySink != NULL, STATUS_MEDIA_DUMMY_SINK);
    app_gst_bin_add_many(APP_GST_BIN(pipeline), dummySink, NULL);

CleanUp:
    // release the resource when we fail to create the pipeline.
    if (STATUS_FAILED(retStatus)) {
        app_gst_object_unref(dummySink);
    }

    MUTEX_UNLOCK(pRtspSrcContext->codecConfLock);

    *ppDummySink = dummySink;
    return retStatus;
}
/**
 * @brief the video sink for the output of rtspsrc.
 *
 * @param[in] pRtspSrcContext the context of rtspsrc.
 * @param[in, out] ppVideoQueue the pointer of the video sink.
 * @param[in] name the name of this applink.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static STATUS createVideoAppSink(PRtspSrcContext pRtspSrcContext, GstElement** ppVideoQueue, PCHAR name)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR elementName[APP_MEDIA_GST_ELEMENT_NAME_MAX_LEN];
    PCodecStreamConf pCodecStreamConf;
    GstElement* pipeline;
    GstElement* videoQueue = NULL;
    GstElement *videoDepay = NULL, *videoFilter = NULL, *videoAppSink = NULL;
    GstCaps* videoCaps = NULL;

    MUTEX_LOCK(pRtspSrcContext->codecConfLock);

    pCodecStreamConf = &pRtspSrcContext->codecConfiguration.videoStream;
    pipeline = (GstElement*) pRtspSrcContext->codecConfiguration.pipeline;

    SNPRINTF(elementName, APP_MEDIA_GST_ELEMENT_NAME_MAX_LEN, "videoQueue%s", name);
    videoQueue = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_QUEUE, elementName);

    if (pCodecStreamConf->codec == RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE) {
        videoDepay = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_H264, "videoDepay");
        videoCaps = app_gst_caps_new_simple("video/x-h264", "stream-format", G_TYPE_STRING, "byte-stream", "alignment", G_TYPE_STRING, "au", NULL);
    } else {
        // this case is RTC_CODEC_VP8.
        videoDepay = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_VP8, "videoDepay");
        videoCaps = app_gst_caps_new_simple("video/x-vp8", "profile", G_TYPE_STRING, "0", NULL);
    }

    CHK(videoCaps != NULL, STATUS_MEDIA_VIDEO_CAPS);

    videoFilter = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_CAPS_FILTER, "videoFilter");
    videoAppSink = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_APP_SINK, "videoAppSink");

    CHK(videoQueue != NULL, STATUS_MEDIA_VIDEO_QUEUE);
    CHK((videoDepay != NULL) && (videoFilter != NULL) && (videoAppSink != NULL), STATUS_MEDIA_VIDEO_ELEMENT);

    app_g_object_set(APP_G_OBJECT(videoFilter), "caps", videoCaps, NULL);
    app_gst_caps_unref(videoCaps);
    videoCaps = NULL;
    // configure appsink
    app_g_object_set(APP_G_OBJECT(videoAppSink), "emit-signals", TRUE, "sync", FALSE, NULL);
    app_g_signal_connect(videoAppSink, GST_SIGNAL_CALLBACK_NEW_SAMPLE, G_CALLBACK(onNewSampleFromVideoAppSink), pRtspSrcContext);
    // link all the elements.
    app_gst_bin_add_many(APP_GST_BIN(pipeline), videoQueue, videoDepay, videoFilter, videoAppSink, NULL);
    CHK(app_gst_element_link_many(videoQueue, videoDepay, videoFilter, videoAppSink, NULL), STATUS_MEDIA_VIDEO_LINK);

CleanUp:
    // release the resource when we fail to create the pipeline.
    if (STATUS_FAILED(retStatus)) {
        app_gst_object_unref(videoQueue);
        videoQueue = NULL;
        app_gst_object_unref(videoDepay);
        app_gst_object_unref(videoCaps);
        app_gst_object_unref(videoFilter);
        app_gst_object_unref(videoAppSink);
    }
    MUTEX_UNLOCK(pRtspSrcContext->codecConfLock);

    *ppVideoQueue = videoQueue;
    return retStatus;
}
/**
 * @brief the audio sink for the output of rtspsrc.
 *
 * @param[in] pRtspSrcContext the context of rtspsrc.
 * @param[in, out] ppAudioQueue the pointer of the audio sink.
 * @param[in] name the name of this applink.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static STATUS createAudioAppSink(PRtspSrcContext pRtspSrcContext, GstElement** ppAudioQueue, PCHAR name)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCodecStreamConf pCodecStreamConf;
    CHAR elementName[APP_MEDIA_GST_ELEMENT_NAME_MAX_LEN];
    GstElement* pipeline;
    GstElement* audioQueue = NULL;
    GstElement *audioDepay = NULL, *audioFilter = NULL, *audioAppSink = NULL;
    GstCaps* audioCaps = NULL;

    MUTEX_LOCK(pRtspSrcContext->codecConfLock);

    pCodecStreamConf = &pRtspSrcContext->codecConfiguration.audioStream;
    pipeline = (GstElement*) pRtspSrcContext->codecConfiguration.pipeline;

    SNPRINTF(elementName, APP_MEDIA_GST_ELEMENT_NAME_MAX_LEN, "audioQueue%s", name);
    audioQueue = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_QUEUE, elementName);
    if (pCodecStreamConf->codec == RTC_CODEC_OPUS) {
        audioDepay = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_OPUS, "audioDepay");
        audioCaps = app_gst_caps_new_simple("audio/x-opus", "rate", G_TYPE_INT, 48000, "channels", G_TYPE_INT, 2, NULL);
    } else if (pCodecStreamConf->codec == RTC_CODEC_MULAW) {
        audioDepay = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_PCMU, "audioDepay");
        audioCaps = app_gst_caps_new_simple("audio/x-mulaw", "rate", G_TYPE_INT, 8000, "channels", G_TYPE_INT, 1, NULL);
    } else {
        // This case is RTC_CODEC_ALAW.
        audioDepay = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_RTP_DEPAY_PCMA, "audioDepay");
        audioCaps = app_gst_caps_new_simple("audio/x-alaw", "rate", G_TYPE_INT, 8000, "channels", G_TYPE_INT, 1, NULL);
    }

    CHK(audioCaps != NULL, STATUS_MEDIA_AUDIO_CAPS);

    audioFilter = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_CAPS_FILTER, "audioFilter");
    audioAppSink = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_APP_SINK, "audioAppSink");

    CHK(audioQueue != NULL, STATUS_MEDIA_AUDIO_QUEUE);
    CHK((audioDepay != NULL) && (audioFilter != NULL) && (audioAppSink != NULL), STATUS_MEDIA_AUDIO_ELEMENT);

    app_g_object_set(APP_G_OBJECT(audioFilter), "caps", audioCaps, NULL);
    app_gst_caps_unref(audioCaps);
    audioCaps = NULL;

    app_g_object_set(APP_G_OBJECT(audioAppSink), "emit-signals", TRUE, "sync", FALSE, NULL);
    app_g_signal_connect(audioAppSink, GST_SIGNAL_CALLBACK_NEW_SAMPLE, G_CALLBACK(onNewSampleFromAudioAppSink), pRtspSrcContext);
    app_gst_bin_add_many(APP_GST_BIN(pipeline), audioQueue, audioDepay, audioFilter, audioAppSink, NULL);
    CHK(app_gst_element_link_many(audioQueue, audioDepay, audioFilter, audioAppSink, NULL), STATUS_MEDIA_AUDIO_LINK);

CleanUp:
    // release the resource when we fail to create the pipeline.
    if (STATUS_FAILED(retStatus)) {
        app_gst_object_unref(audioQueue);
        audioQueue = NULL;
        app_gst_object_unref(audioDepay);
        app_gst_object_unref(audioFilter);
        app_gst_object_unref(audioAppSink);
    }

    MUTEX_UNLOCK(pRtspSrcContext->codecConfLock);

    *ppAudioQueue = audioQueue;
    return retStatus;
}
/**
 * @brief   the callback is invoked when the pad is added.
 *
 * @param[in] element the element.
 * @param[in] pad the pad of the element.
 * @param[in] udata the user data.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static void onRtspSrcPadAddedDiscovery(GstElement* element, GstPad* pad, gpointer udata)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) udata;
    PCodecConfiguration pGstConfiguration = NULL;
    PCodecStreamConf pCodecStreamConf = NULL;
    BOOL video = FALSE;
    BOOL audio = FALSE;
    // gstreamer
    gchar* srcPadName = NULL;
    GstCaps* srcPadTemplateCaps = NULL;
    GstCaps* srcPadCurrentCaps = NULL;
    GstStructure* srcPadStructure = NULL;
    GstElement* nextElement = NULL;
    gchar* media = NULL;
    guint curCapsNum;
    BOOL locked = FALSE;
    guint i;

    CHK((element != NULL) && (pad != NULL) && (pRtspSrcContext != NULL), STATUS_MEDIA_NULL_ARG);
    pGstConfiguration = &pRtspSrcContext->codecConfiguration;
    srcPadName = app_gst_pad_get_name(pad);
    srcPadTemplateCaps = app_gst_pad_get_pad_template_caps(pad);
    DLOGD("new pad template %s was created", srcPadName);
    srcPadCurrentCaps = app_gst_pad_get_current_caps(pad);
    curCapsNum = app_gst_caps_get_size(srcPadCurrentCaps);

    MUTEX_LOCK(pRtspSrcContext->codecConfLock);
    locked = TRUE;
    for (i = 0; i < curCapsNum; i++) {
        srcPadStructure = app_gst_caps_get_structure(srcPadCurrentCaps, i);
        pCodecStreamConf = NULL;

        if (app_gst_structure_has_field(srcPadStructure, GST_STRUCT_FIELD_MEDIA) == TRUE &&
            app_gst_structure_has_field(srcPadStructure, GST_STRUCT_FIELD_ENCODING) == TRUE) {
            media = app_gst_structure_get_string(srcPadStructure, GST_STRUCT_FIELD_MEDIA);
            const gchar* encoding_name = app_gst_structure_get_string(srcPadStructure, GST_STRUCT_FIELD_ENCODING);
            DLOGD("media:%s, encoding_name:%s", media, encoding_name);

            if (STRCMP(media, GST_STRUCT_FIELD_MEDIA_VIDEO) == 0) {
                video = TRUE;
                pCodecStreamConf = &pGstConfiguration->videoStream;
                // h264
                if (STRCMP(encoding_name, GST_STRUCT_FIELD_ENCODING_H264) == 0) {
                    pCodecStreamConf->codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
                    // vp8
                } else if (STRCMP(encoding_name, GST_STRUCT_FIELD_ENCODING_VP8) == 0) {
                    pCodecStreamConf->codec = RTC_CODEC_VP8;
                    // others
                } else {
                    DLOGW("unsupported video format");
                    continue;
                }
            } else if (STRCMP(media, GST_STRUCT_FIELD_MEDIA_AUDIO) == 0) {
                audio = TRUE;
                pCodecStreamConf = &pGstConfiguration->audioStream;
                if (STRCMP(encoding_name, GST_STRUCT_FIELD_ENCODING_PCMU) == 0) {
                    pCodecStreamConf->codec = RTC_CODEC_MULAW;
                } else if (STRCMP(encoding_name, GST_STRUCT_FIELD_ENCODING_PCMA) == 0) {
                    pCodecStreamConf->codec = RTC_CODEC_ALAW;
                } else if (STRCMP(encoding_name, GST_STRUCT_FIELD_ENCODING_OPUS) == 0) {
                    pCodecStreamConf->codec = RTC_CODEC_OPUS;
                } else {
                    DLOGW("unsupported audio format");
                    continue;
                }
            } else {
                DLOGW("unsupported media format");
                continue;
            }
            DLOGD("codec:%d", pCodecStreamConf->codec);
            if (app_gst_structure_has_field(srcPadStructure, GST_STRUCT_FIELD_PAYLOAD_TYPE) == TRUE) {
                gint payloadType;
                app_gst_structure_get_int(srcPadStructure, GST_STRUCT_FIELD_PAYLOAD_TYPE, &payloadType);
                DLOGD("payload:%d", payloadType);
                pCodecStreamConf->payloadType = payloadType;
            }
            if (app_gst_structure_has_field(srcPadStructure, GST_STRUCT_FIELD_CLOCK_RATE) == TRUE) {
                gint clock_rate;
                app_gst_structure_get_int(srcPadStructure, GST_STRUCT_FIELD_CLOCK_RATE, &clock_rate);
                DLOGD("clock-rate:%d", clock_rate);
                pCodecStreamConf->clockRate = clock_rate;
            }
        }
    }

    CHK_STATUS((createDummyAppSink(pRtspSrcContext, &nextElement, media)));
    CHK(app_gst_element_link_filtered(element, nextElement, srcPadTemplateCaps) == TRUE, STATUS_MEDIA_LINK_ELEMENT);

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pRtspSrcContext->codecConfLock);
    }
    if (srcPadName != NULL) {
        app_g_free(srcPadName);
    }
    if (srcPadCurrentCaps != NULL) {
        app_gst_caps_unref(srcPadCurrentCaps);
    }
    if (srcPadTemplateCaps != NULL) {
        app_gst_caps_unref(srcPadTemplateCaps);
    }
    if (STATUS_FAILED(retStatus)) {
        DLOGE("operation returned status code: 0x%08x", retStatus);
        if (nextElement != NULL) {
            app_gst_object_unref(nextElement);
        }
        if (pRtspSrcContext != NULL) {
            closeGstRtspSrc(pRtspSrcContext);
            updateCodecStatus(pRtspSrcContext, STATUS_MEDIA_BUS_ERROR);
        }
    }
}
/**
 * @brief   the callback is invoked when there is no pads coming.
 *
 * @param[in] element the element.
 * @param[in] udata the user data.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static void onRtspSrcNoMorePadsDiscovery(GstElement* element, gpointer udata)
{
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) udata;
    ATOMIC_STORE_BOOL(&pRtspSrcContext->codecConfigLatched, TRUE);
    DLOGD("no more pads.");
    closeGstRtspSrc(pRtspSrcContext);
}
/**
 * @brief   the callback is invoked when the pad is added.
 *
 * @param[in] element the element.
 * @param[in] pad the pad of the element.
 * @param[in] udata the user data.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static void onRtspSrcPadAdded(GstElement* element, GstPad* pad, gpointer udata)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) udata;
    PCodecConfiguration pGstConfiguration;
    PCodecStreamConf pCodecStreamConf = NULL;
    GstElement* pipeline = NULL;
    gchar* srcPadName = NULL;
    GstCaps* srcPadTemplateCaps = NULL;
    GstCaps* srcPadCurrentCaps = NULL;
    GstStructure* srcPadStructure = NULL;
    GstElement* nextElement = NULL;
    guint curCapsNum = 0;
    gint payloadType = 0;
    BOOL video = FALSE;
    BOOL audio = FALSE;
    BOOL locked = FALSE;
    guint i;

    CHK((element != NULL) && (pad != NULL) && (pRtspSrcContext != NULL), STATUS_MEDIA_NULL_ARG);

    pGstConfiguration = &pRtspSrcContext->codecConfiguration;
    pipeline = (GstElement*) pGstConfiguration->pipeline;

    MUTEX_LOCK(pRtspSrcContext->codecConfLock);
    locked = TRUE;
    srcPadName = app_gst_pad_get_name(pad);
    srcPadTemplateCaps = app_gst_pad_get_pad_template_caps(pad);
    DLOGD("a new pad template %s was created", srcPadName);

    srcPadCurrentCaps = app_gst_pad_get_current_caps(pad);
    curCapsNum = app_gst_caps_get_size(srcPadCurrentCaps);

    for (i = 0; i < curCapsNum; i++) {
        srcPadStructure = app_gst_caps_get_structure(srcPadCurrentCaps, i);
        if (app_gst_structure_has_field(srcPadStructure, GST_STRUCT_FIELD_MEDIA) == TRUE) {
            const gchar* media_value = app_gst_structure_get_string(srcPadStructure, GST_STRUCT_FIELD_MEDIA);
            DLOGD("media_value:%s", media_value);

            if (STRCMP(media_value, GST_STRUCT_FIELD_MEDIA_VIDEO) == 0) {
                video = TRUE;
                pCodecStreamConf = &pGstConfiguration->videoStream;

            } else if (STRCMP(media_value, GST_STRUCT_FIELD_MEDIA_AUDIO) == 0) {
                audio = TRUE;
                pCodecStreamConf = &pGstConfiguration->audioStream;
            } else {
                continue;
            }

            if (app_gst_structure_has_field(srcPadStructure, GST_STRUCT_FIELD_PAYLOAD_TYPE) == TRUE) {
                app_gst_structure_get_int(srcPadStructure, GST_STRUCT_FIELD_PAYLOAD_TYPE, &payloadType);
                DLOGD("payload type:%d", payloadType);
            }

            if (video == TRUE && pCodecStreamConf->payloadType == payloadType) {
                DLOGD("connecting video sink");
                CHK_STATUS((createVideoAppSink(pRtspSrcContext, &nextElement, srcPadName)));
            } else if (audio == TRUE && pCodecStreamConf->payloadType == payloadType) {
                DLOGD("connecting audio sink");
                CHK_STATUS((createAudioAppSink(pRtspSrcContext, &nextElement, srcPadName)));
            } else {
                DLOGW("payload type conflicts, and connecting dummy sink");
                CHK_STATUS((createDummyAppSink(pRtspSrcContext, &nextElement, srcPadName)));
            }
        }
    }

    CHK(nextElement != NULL, STATUS_MEDIA_EMPTY_ELEMENT);
    CHK(app_gst_element_link_filtered(element, nextElement, srcPadTemplateCaps) == TRUE, STATUS_MEDIA_LINK_ELEMENT);
    CHK(app_gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE, STATUS_MEDIA_PLAY);

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pRtspSrcContext->codecConfLock);
    }
    if (srcPadName != NULL) {
        app_g_free(srcPadName);
    }
    if (srcPadCurrentCaps != NULL) {
        app_gst_caps_unref(srcPadCurrentCaps);
    }
    if (srcPadTemplateCaps != NULL) {
        app_gst_caps_unref(srcPadTemplateCaps);
    }
    if (STATUS_FAILED(retStatus)) {
        DLOGE("operation returned status code: 0x%08x", retStatus);
        if (nextElement != NULL) {
            app_gst_object_unref(nextElement);
        }
        if (pRtspSrcContext != NULL) {
            closeGstRtspSrc(pRtspSrcContext);
            updateCodecStatus(pRtspSrcContext, STATUS_MEDIA_BUS_ERROR);
        }
    }
}
/**
 * @brief this callback is invoked when pad is removed.
 *
 * @param[in] element the element of this callback.
 * @param[in] pad the pad of this callback.
 * @param[in] udata the user data.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static void onRtspSrcPadRemoved(GstElement* element, GstPad* pad, gpointer udata)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) udata;
    PCodecConfiguration pGstConfiguration;

    CHK((element != NULL) && (pad != NULL) && (pRtspSrcContext != NULL), STATUS_MEDIA_NULL_ARG);
    closeGstRtspSrc(pRtspSrcContext);
    pGstConfiguration = &pRtspSrcContext->codecConfiguration;

CleanUp:
    if (pRtspSrcContext != NULL) {
        updateCodecStatus(pRtspSrcContext, STATUS_MEDIA_PAD_REMOVED);
    }
    return;
}
/**
 * @brief the initialization of the rtspsrc plugin of GStreamer.
 *
 * @param[in] pRtspSrcContext the context of app media.
 * @param[in] pipeline the pipeline of rtspsrc.
 * @param[in] enableProbe which mode do you use.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static STATUS initGstRtspSrc(PRtspSrcContext pRtspSrcContext, GstElement* pipeline, BOOL enableProbe)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspServerConfiguration pRtspServerConf = NULL;
    GstElement* rtspSource = NULL;

    MUTEX_LOCK(pRtspSrcContext->codecConfLock);
    rtspSource = app_gst_element_factory_make(GST_ELEMENT_FACTORY_NAME_RTSPSRC, "rtspSource");
    CHK(rtspSource != NULL, STATUS_MEDIA_MISSING_PLUGIN);
    // configure rtspsrc
    pRtspServerConf = &pRtspSrcContext->rtspServerConf;

    app_g_object_set(APP_G_OBJECT(rtspSource), "location", pRtspServerConf->url, "short-header", TRUE, NULL);

    if (pRtspServerConf->username[0] != '\0') {
        app_g_object_set(APP_G_OBJECT(rtspSource), "user-id", pRtspServerConf->username, NULL);
    }
    if (pRtspServerConf->password[0] != '\0') {
        app_g_object_set(APP_G_OBJECT(rtspSource), "user-pw", pRtspServerConf->password, NULL);
    }

    // setup the callbacks.
    if (enableProbe == FALSE) {
        DLOGD("initializing rtspsrc");
        app_g_signal_connect(APP_G_OBJECT(rtspSource), GST_SIGNAL_CALLBACK_PAD_ADDED, G_CALLBACK(onRtspSrcPadAdded), pRtspSrcContext);
        app_g_signal_connect(APP_G_OBJECT(rtspSource), GST_SIGNAL_CALLBACK_PAD_REMOVED, G_CALLBACK(onRtspSrcPadRemoved), pRtspSrcContext);
    } else {
        DLOGD("probing rtspsrc");
        app_g_signal_connect(APP_G_OBJECT(rtspSource), GST_SIGNAL_CALLBACK_PAD_ADDED, G_CALLBACK(onRtspSrcPadAddedDiscovery), pRtspSrcContext);
        app_g_signal_connect(APP_G_OBJECT(rtspSource), GST_SIGNAL_CALLBACK_NO_MORE_PADS, G_CALLBACK(onRtspSrcNoMorePadsDiscovery), pRtspSrcContext);
    }

    app_gst_bin_add_many(APP_GST_BIN(pipeline), rtspSource, NULL);

CleanUp:
    MUTEX_UNLOCK(pRtspSrcContext->codecConfLock);
    return retStatus;
}
/**
 * @brief start discovering the media source, and retrieve the suppported video/audio format.
 *
 * @param[in] userData the context of the media.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static PVOID discoverMediaSource(PVOID userData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) userData;
    PCodecConfiguration pGstConfiguration = &pRtspSrcContext->codecConfiguration;
    GstElement* pipeline = NULL;
    GstBus* bus = NULL;

    DLOGD("discovering the meida source");
    pGstConfiguration->codecStatus = STATUS_SUCCESS;
    CHK((pipeline = app_gst_pipeline_new("kinesis-rtsp-probe")) != NULL, STATUS_MEDIA_NULL_ARG);
    pGstConfiguration->pipeline = pipeline;

    CHK_STATUS((initGstRtspSrc(pRtspSrcContext, pipeline, TRUE)));

    // Instruct the bus to emit signals for each received message, and connect to the interesting signals
    CHK((bus = app_gst_element_get_bus(pipeline)) != NULL, STATUS_MEDIA_MISSING_BUS);
    app_gst_bus_add_signal_watch(bus);
    app_g_signal_connect(APP_G_OBJECT(bus), GST_SIGNAL_CALLBACK_MSG_ERROR, G_CALLBACK(onMsgErrorFromBus), pRtspSrcContext);
    app_g_signal_connect(APP_G_OBJECT(bus), GST_SIGNAL_CALLBACK_MSG_EOS, G_CALLBACK(onMsgEosFromBus), pRtspSrcContext);

    // start streaming
    CHK(app_gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE, STATUS_MEDIA_PLAY);

    pGstConfiguration->mainLoop = app_g_main_loop_new(NULL, FALSE);
    // start running the main loop, and it is blocking call.
    app_g_main_loop_run(pGstConfiguration->mainLoop);

CleanUp:

    /* free resources */
    DLOGD("release the media source");
    if (bus != NULL) {
        app_gst_bus_remove_signal_watch(bus);
        app_gst_object_unref(bus);
    }
    if (pipeline != NULL) {
        app_gst_element_set_state(pipeline, GST_STATE_NULL);
        app_gst_object_unref(pipeline);
        pGstConfiguration->pipeline = NULL;
    }
    if (pGstConfiguration->mainLoop != NULL) {
        app_g_main_loop_unref(pGstConfiguration->mainLoop);
        pGstConfiguration->mainLoop = NULL;
    }
    return (PVOID)(ULONG_PTR) retStatus;
}
/**
 * @brief start discovering the media source, and retrieve the suppported video/audio format.
 *
 * @param[in] userData the context of the media.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success.
 */
static STATUS latchRtspConfig(PRtspServerConfiguration pRtspServerConf)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pRtspUrl, pRtspUsername, pRtspPassword;

    CHK_ERR((pRtspUrl = GETENV(APP_MEDIA_RTSP_URL)) != NULL, STATUS_MEDIA_RTSP_URL, "RTSP_URL must be set");
    STRNCPY(pRtspServerConf->url, pRtspUrl, MAX_URI_CHAR_LEN);

    pRtspUsername = GETENV(APP_MEDIA_RTSP_USERNAME);
    pRtspPassword = GETENV(APP_MEDIA_RTSP_PASSWORD);
    if (pRtspUsername != NULL && pRtspUsername[0] != '\0' && pRtspPassword != NULL && pRtspPassword[0] != '\0') {
        CHK((STRNLEN(pRtspUsername, APP_MEDIA_RTSP_USERNAME_LEN + 1) <= APP_MEDIA_RTSP_USERNAME_LEN) &&
                (STRNLEN(pRtspPassword, APP_MEDIA_RTSP_PASSWORD_LEN + 1) <= APP_MEDIA_RTSP_PASSWORD_LEN),
            STATUS_MEDIA_RTSP_CREDENTIAL);
        STRNCPY(pRtspServerConf->username, pRtspUsername, APP_MEDIA_RTSP_USERNAME_LEN);
        STRNCPY(pRtspServerConf->password, pRtspPassword, APP_MEDIA_RTSP_PASSWORD_LEN);
    } else {
        MEMSET(pRtspServerConf->username, 0, APP_MEDIA_RTSP_USERNAME_LEN);
        MEMSET(pRtspServerConf->password, 0, APP_MEDIA_RTSP_PASSWORD_LEN);
    }
CleanUp:
    return retStatus;
}

STATUS initMediaSource(PMediaContext* ppMediaContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = NULL;
    PCodecConfiguration pGstConfiguration;
    PCodecStreamConf pVideoStream;
    PCodecStreamConf pAudioStream;

    CHK(ppMediaContext != NULL, STATUS_MEDIA_NULL_ARG);
    *ppMediaContext = NULL;
    CHK(NULL != (pRtspSrcContext = (PRtspSrcContext) MEMCALLOC(1, SIZEOF(RtspSrcContext))), STATUS_MEDIA_NOT_ENOUGH_MEMORY);
    ATOMIC_STORE_BOOL(&pRtspSrcContext->shutdownRtspSrc, FALSE);
    ATOMIC_STORE_BOOL(&pRtspSrcContext->codecConfigLatched, FALSE);

    pGstConfiguration = &pRtspSrcContext->codecConfiguration;
    pGstConfiguration->codecStatus = STATUS_SUCCESS;
    pVideoStream = &pGstConfiguration->videoStream;
    pAudioStream = &pGstConfiguration->audioStream;
    pVideoStream->codec = GST_CODEC_INVALID_VALUE;
    pAudioStream->codec = GST_CODEC_INVALID_VALUE;

    pRtspSrcContext->codecConfLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(pRtspSrcContext->codecConfLock), STATUS_MEDIA_INVALID_MUTEX);
    // initialize the gstreamer
    app_gst_init(NULL, NULL);
    // latch the configuration of rtsp server.
    CHK_STATUS((latchRtspConfig(&pRtspSrcContext->rtspServerConf)));
    // get the sdp information of rtsp server.
    CHK_STATUS((discoverMediaSource(pRtspSrcContext)));
    *ppMediaContext = pRtspSrcContext;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        if (pRtspSrcContext != NULL) {
            detroyMediaSource(pRtspSrcContext);
        }
    }

    return retStatus;
}

STATUS isMediaSourceReady(PMediaContext pMediaContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) pMediaContext;
    CHK(pRtspSrcContext != NULL, STATUS_MEDIA_NULL_ARG);
    if (!ATOMIC_LOAD_BOOL(&pRtspSrcContext->codecConfigLatched)) {
        discoverMediaSource(pRtspSrcContext);
    }

    if (ATOMIC_LOAD_BOOL(&pRtspSrcContext->codecConfigLatched)) {
        retStatus = STATUS_SUCCESS;
    } else {
        retStatus = STATUS_MEDIA_NOT_READY;
    }

CleanUp:

    return retStatus;
}

STATUS queryMediaVideoCap(PMediaContext pMediaContext, RTC_CODEC* pCodec)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) pMediaContext;
    PCodecStreamConf pVideoStream;
    CHK((pRtspSrcContext != NULL) && (pCodec != NULL), STATUS_MEDIA_NULL_ARG);
    CHK(ATOMIC_LOAD_BOOL(&pRtspSrcContext->codecConfigLatched), STATUS_MEDIA_NOT_READY);
    pVideoStream = &pRtspSrcContext->codecConfiguration.videoStream;
    CHK(pVideoStream->codec != GST_CODEC_INVALID_VALUE, STATUS_MEDIA_NOT_EXISTED);
    *pCodec = pVideoStream->codec;
CleanUp:
    return retStatus;
}

STATUS queryMediaAudioCap(PMediaContext pMediaContext, RTC_CODEC* pCodec)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) pMediaContext;
    PCodecStreamConf pAudioStream;
    CHK((pRtspSrcContext != NULL), STATUS_MEDIA_NULL_ARG);
    CHK(ATOMIC_LOAD_BOOL(&pRtspSrcContext->codecConfigLatched), STATUS_MEDIA_NOT_READY);
    pAudioStream = &pRtspSrcContext->codecConfiguration.audioStream;
    CHK(pAudioStream->codec != GST_CODEC_INVALID_VALUE, STATUS_MEDIA_NOT_EXISTED);
    *pCodec = pAudioStream->codec;
CleanUp:
    return retStatus;
}

STATUS linkMeidaSinkHook(PMediaContext pMediaContext, MediaSinkHook mediaSinkHook, PVOID udata)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) pMediaContext;
    CHK(pRtspSrcContext != NULL, STATUS_MEDIA_NULL_ARG);
    pRtspSrcContext->mediaSinkHook = mediaSinkHook;
    pRtspSrcContext->mediaSinkHookUserdata = udata;
CleanUp:
    return retStatus;
}

STATUS linkMeidaEosHook(PMediaContext pMediaContext, MediaEosHook mediaEosHook, PVOID udata)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) pMediaContext;
    CHK(pRtspSrcContext != NULL, STATUS_MEDIA_NULL_ARG);
    pRtspSrcContext->mediaEosHook = mediaEosHook;
    pRtspSrcContext->mediaEosHookUserdata = udata;
CleanUp:
    return retStatus;
}

PVOID runMediaSource(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) args;
    PCodecConfiguration pGstConfiguration = NULL;
    /* init GStreamer */
    GstElement* pipeline = NULL;
    GstBus* bus = NULL;
    PVOID mainLoop = NULL;

    CHK(pRtspSrcContext != NULL, STATUS_MEDIA_NULL_ARG);
    pGstConfiguration = &pRtspSrcContext->codecConfiguration;
    pGstConfiguration->codecStatus = STATUS_SUCCESS;
    DLOGI("media source is starting");
    CHK((pipeline = app_gst_pipeline_new("kinesis-rtsp-pipeline")) != NULL, STATUS_MEDIA_MISSING_PIPELINE);
    pGstConfiguration->pipeline = pipeline;
    CHK_STATUS((initGstRtspSrc(pRtspSrcContext, pipeline, FALSE)));
    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    CHK((bus = app_gst_element_get_bus(pipeline)) != NULL, STATUS_MEDIA_MISSING_BUS);
    app_gst_bus_add_signal_watch(bus);
    app_g_signal_connect(APP_G_OBJECT(bus), GST_SIGNAL_CALLBACK_MSG_ERROR, G_CALLBACK(onMsgErrorFromBus), pRtspSrcContext);
    app_g_signal_connect(APP_G_OBJECT(bus), GST_SIGNAL_CALLBACK_MSG_EOS, G_CALLBACK(onMsgEosFromBus), pRtspSrcContext);

    /* start streaming */
    CHK(app_gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE, STATUS_MEDIA_PLAY);

    mainLoop = app_g_main_loop_new(NULL, FALSE);
    pGstConfiguration->mainLoop = mainLoop;
    // start running the main loop, and it is blocking call.
    DLOGI("media source is running");
    app_g_main_loop_run(pGstConfiguration->mainLoop);

CleanUp:

    /* free resources */
    DLOGD("terminating media source");
    if (bus != NULL) {
        app_gst_bus_remove_signal_watch(bus);
        app_gst_object_unref(bus);
    }

    if (pipeline != NULL) {
        app_gst_element_set_state(pipeline, GST_STATE_NULL);
        app_gst_object_unref(pipeline);
        pGstConfiguration->pipeline = NULL;
    }

    if (mainLoop != NULL) {
        app_g_main_loop_unref(pGstConfiguration->mainLoop);
        pGstConfiguration->mainLoop = NULL;
    }
    return (PVOID)(ULONG_PTR) retStatus;
}

STATUS shutdownMediaSource(PMediaContext pMediaContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext = (PRtspSrcContext) pMediaContext;
    CHK(pRtspSrcContext != NULL, STATUS_MEDIA_NULL_ARG);
    ATOMIC_STORE_BOOL(&pRtspSrcContext->shutdownRtspSrc, TRUE);

CleanUp:
    return retStatus;
}

STATUS detroyMediaSource(PMediaContext* ppMediaContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtspSrcContext pRtspSrcContext;

    CHK(ppMediaContext != NULL, STATUS_MEDIA_NULL_ARG);
    pRtspSrcContext = (PRtspSrcContext) *ppMediaContext;
    CHK(pRtspSrcContext != NULL, STATUS_MEDIA_NULL_ARG);
    if (IS_VALID_MUTEX_VALUE(pRtspSrcContext->codecConfLock)) {
        MUTEX_FREE(pRtspSrcContext->codecConfLock);
    }

    MEMFREE(pRtspSrcContext);
    *ppMediaContext = pRtspSrcContext = NULL;
CleanUp:
    return retStatus;
}
