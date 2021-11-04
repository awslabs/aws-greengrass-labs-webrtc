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
#ifndef __KINESIS_VIDEO_WEBRTC_APP_RTSP_SRC_WRAP_INCLUDE__
#define __KINESIS_VIDEO_WEBRTC_APP_RTSP_SRC_WRAP_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "AppConfig.h"
#include "AppError.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/gststructure.h>
#include <gst/gstcaps.h>
#include <gst/rtsp/rtsp.h>

#if !defined(APP_RTSP_SRC_WRAP)
#define APP_G_OBJECT                   G_OBJECT
#define app_g_object_set               g_object_set
#define app_g_signal_connect           g_signal_connect
#define app_g_free                     g_free
#define app_g_error_free               g_error_free
#define app_g_main_loop_new            g_main_loop_new
#define app_g_main_loop_run            g_main_loop_run
#define app_g_main_loop_unref          g_main_loop_unref
#define app_g_main_loop_quit           g_main_loop_quit
#define app_g_type_check_instance_cast g_type_check_instance_cast

#define APP_GST_APP_SINK                  GST_APP_SINK
#define APP_GST_BIN                       GST_BIN
#define app_gst_init                      gst_init
#define app_gst_app_sink_pull_sample      gst_app_sink_pull_sample
#define app_gst_sample_get_buffer         gst_sample_get_buffer
#define app_gst_sample_get_segment        gst_sample_get_segment
#define app_gst_sample_unref              gst_sample_unref
#define app_gst_segment_to_running_time   gst_segment_to_running_time
#define app_gst_buffer_map                gst_buffer_map
#define app_gst_buffer_unmap              gst_buffer_unmap
#define app_gst_element_factory_make      gst_element_factory_make
#define app_gst_caps_unref                gst_caps_unref
#define app_gst_caps_new_simple           gst_caps_new_simple
#define app_gst_caps_get_size             gst_caps_get_size
#define app_gst_caps_get_structure        gst_caps_get_structure
#define app_gst_bin_add_many              gst_bin_add_many
#define app_gst_element_link_many         gst_element_link_many
#define app_gst_element_link_filtered     gst_element_link_filtered
#define app_gst_object_unref              gst_object_unref
#define app_gst_pad_get_name              gst_pad_get_name
#define app_gst_object_get_name           gst_object_get_name
#define app_gst_pad_get_pad_template_caps gst_pad_get_pad_template_caps
#define app_gst_pad_get_current_caps      gst_pad_get_current_caps
#define app_gst_structure_has_field       gst_structure_has_field
#define app_gst_structure_get_string      gst_structure_get_string
#define app_gst_structure_get_int         gst_structure_get_int
#define app_gst_element_set_state         gst_element_set_state
#define app_gst_message_parse_error       gst_message_parse_error
#define app_gst_pipeline_new              gst_pipeline_new
#define app_gst_element_get_bus           gst_element_get_bus
#define app_gst_bus_add_signal_watch      gst_bus_add_signal_watch
#define app_gst_bus_remove_signal_watch   gst_bus_remove_signal_watch
#define app_gst_app_sink_get_type         gst_app_sink_get_type
#define app_gst_bin_get_type              gst_bin_get_type
#else //!< !defined(APP_RTSP_SRC_WRAP)
void app_g_object_set(gpointer object, const gchar* first_property_name, ...);
gulong app_g_signal_connect(gpointer instance, const gchar* detailed_signal, GCallback c_handler, gpointer data);
void app_g_free(gpointer mem);
void app_g_error_free(GError** err);
PVOID app_g_main_loop_new(PVOID context, gboolean is_running);
void app_g_main_loop_run(PVOID loop);
void app_g_main_loop_unref(PVOID loop);
void app_g_main_loop_quit(PVOID loop);
GTypeInstance* app_g_type_check_instance_cast(GTypeInstance* instance, GType iface_type);
void app_gst_init(int* argc, char** argv[]);
PVOID app_gst_app_sink_pull_sample(GstAppSink* appsink);
GstBuffer* app_gst_sample_get_buffer(PVOID sample);
GstSegment* app_gst_sample_get_segment(PVOID sample);
void app_gst_sample_unref(PVOID sample);
guint64 app_gst_segment_to_running_time(const GstSegment* segment, GstFormat format, guint64 position);
gboolean app_gst_buffer_map(GstBuffer* buffer, GstMapInfo* info, GstMapFlags flags);
void app_gst_buffer_unmap(GstBuffer* buffer, GstMapInfo* info);
GstElement* app_gst_element_factory_make(const gchar* factoryname, const gchar* name);
void app_gst_caps_unref(GstCaps* caps);
GstCaps* app_gst_caps_new_simple(const char* media_type, const char* fieldname, ...);
guint app_gst_caps_get_size(const GstCaps* caps);
GstStructure* app_gst_caps_get_structure(const GstCaps* caps, guint index);
void app_gst_bin_add_many(GstBin* bin, GstElement* element_1, ...);
gboolean app_gst_element_link_many(GstElement* element_1, GstElement* element_2, ...);
gboolean app_gst_element_link_filtered(GstElement* src, GstElement* dest, GstCaps* filter);
void app_gst_object_unref(gpointer object);
gchar* app_gst_pad_get_name(GstPad* pad);
gchar* app_gst_object_get_name(GstObject* object);
GstCaps* app_gst_pad_get_pad_template_caps(GstPad* pad);
GstCaps* app_gst_pad_get_current_caps(GstPad* pad);
gboolean app_gst_structure_has_field(const GstStructure* structure, const gchar* fieldname);
gchar* app_gst_structure_get_string(const GstStructure* structure, const gchar* fieldname);
gboolean app_gst_structure_get_int(const GstStructure* structure, const gchar* fieldname, gint* value);
GstStateChangeReturn app_gst_element_set_state(GstElement* element, GstState state);
void app_gst_message_parse_error(GstMessage* message, GError** gerror, gchar** debug);
GstElement* app_gst_pipeline_new(const gchar* name);
GstBus* app_gst_element_get_bus(GstElement* element);
void app_gst_bus_add_signal_watch(GstBus* bus);
void app_gst_bus_remove_signal_watch(GstBus* bus);
GType app_gst_app_sink_get_type(void);
GType app_gst_bin_get_type(void);

#define APP_G_TYPE_CIC(ip, gt, ct)                               ((ct*) app_g_type_check_instance_cast((GTypeInstance*) ip, gt))
#define APP_G_TYPE_CHECK_INSTANCE_CAST(instance, g_type, c_type) (APP_G_TYPE_CIC((instance), (g_type), c_type))
#define APP_G_OBJECT(object)                                     (APP_G_TYPE_CHECK_INSTANCE_CAST((object), G_TYPE_OBJECT, GObject))
#define APP_GST_TYPE_APP_SINK                                    (app_gst_app_sink_get_type())
#define APP_GST_APP_SINK(obj)                                    (APP_G_TYPE_CHECK_INSTANCE_CAST((obj), APP_GST_TYPE_APP_SINK, GstAppSink))
#define APP_GST_TYPE_BIN                                         (app_gst_bin_get_type())
#define APP_GST_BIN(obj)                                         (APP_G_TYPE_CHECK_INSTANCE_CAST((obj), APP_GST_TYPE_BIN, GstBin))
#endif //!< !defined(APP_RTSP_SRC_WRAP)

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_APP_RTSP_SRC_WRAP_INCLUDE__ */
