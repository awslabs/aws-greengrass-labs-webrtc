# This file is to add source files and include directories
# into variables so that it can be reused from different repositories
# in their Cmake based build system by including this file.
#
# Files specific to the repository such as test runner, platform tests
# are not added to the variables.

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST gstreamer-1.0)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0 gstreamer-sdp-1.0)
pkg_check_modules(GOBJ2 REQUIRED gobject-2.0)

# WebRTC App library source files.
set( WEBRTC_APP_SOURCES
     "${CMAKE_CURRENT_LIST_DIR}/src/AppCommon.c"
     "${CMAKE_CURRENT_LIST_DIR}/src/AppCredential.c"
     "${CMAKE_CURRENT_LIST_DIR}/src/AppDataChannel.c"
     "${CMAKE_CURRENT_LIST_DIR}/src/AppMessageQueue.c"
     "${CMAKE_CURRENT_LIST_DIR}/src/AppMetrics.c"
     "${CMAKE_CURRENT_LIST_DIR}/src/AppRtspSrc.c"
     "${CMAKE_CURRENT_LIST_DIR}/src/AppSignaling.c"
     "${CMAKE_CURRENT_LIST_DIR}/src/AppWebRTC.c" )

# WebRTC App library Public Include directories.
set( WEBRTC_APP_INCLUDE_PUBLIC_DIRS
     "${CMAKE_CURRENT_LIST_DIR}/src/include"
     "${CMAKE_CURRENT_LIST_DIR}/amazon-kinesis-video-streams-webrtc-sdk-c/src/include"
     "${CMAKE_CURRENT_LIST_DIR}/open-source/include"
     ${GLIB2_INCLUDE_DIRS}
     ${GST_INCLUDE_DIRS}
     ${GST_APP_INCLUDE_DIRS}
     ${GOBJ2_INCLUDE_DIRS} )
