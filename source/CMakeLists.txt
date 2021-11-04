cmake_minimum_required(VERSION 3.6.3)

project(AwsGreengrassLabsWebRTC LANGUAGES C)

find_package(PkgConfig REQUIRED)

option(CODE_COVERAGE "Enable coverage reporting" OFF)

# KVS WebRTCClient setting.
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build all libraries statically. (This includes third-party libraries.)")
set(BUILD_SAMPLE OFF CACHE BOOL "Build available samples")
set(OPEN_SRC_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/open-source" CACHE PATH "Libraries will be downloaded and built in this directory.")
add_subdirectory(amazon-kinesis-video-streams-webrtc-sdk-c)

message("building the app")

if("${CMAKE_C_COMPILER_ID}" MATCHES "GNU|Clang")

  if(CODE_COVERAGE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
  endif()

endif()

pkg_check_modules(GST gstreamer-1.0)
if(GST_FOUND)

  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/libffi/lib/pkgconfig")
    find_library(Intl "intl" REQUIRED PATHS "/usr/local/opt/gettext/lib")
    link_directories("/usr/local/opt/gettext/lib")
  endif()

  pkg_check_modules(GLIB2 REQUIRED glib-2.0)
  pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0 gstreamer-sdp-1.0)
  pkg_check_modules(GOBJ2 REQUIRED gobject-2.0)
  message("gstreamer found. Will build this application with")

  set(OPEN_SRC_INCLUDE_DIRS
      ${OPEN_SRC_INCLUDE_DIRS} ${GLIB2_INCLUDE_DIRS} ${GST_INCLUDE_DIRS}
      ${GST_APP_INCLUDE_DIRS} ${GOBJ2_INCLUDE_DIRS})

  set(GST_APPLICATION_LIBRARIES ${GLIB2_LIBRARIES} ${GST_LIBRARIES}
                           ${GST_APP_LIBRARIES} ${GOBJ2_LIBRARIES} ${Intl})
  
  include_directories(src/include)
  include_directories(${OPEN_SRC_INSTALL_PREFIX}/include)
  include_directories(${OPEN_SRC_INCLUDE_DIRS})

  link_directories(${OPEN_SRC_INSTALL_PREFIX}/lib)
  link_directories(${GLIB2_LIBRARY_DIRS})
  link_directories(${GST_LIBRARY_DIRS})
  link_directories(${GST_APP_LIBRARY_DIRS})
  link_directories(${GOBJ2_LIBRARY_DIRS})

  file(GLOB_RECURSE HEADERS "src/include/*.h")
  file(GLOB_RECURSE SOURCES "src/*.c")

  add_executable(awsGreengrassLabsWebRTC ${SOURCES})
  target_link_libraries(awsGreengrassLabsWebRTC kvsWebrtcClient kvsWebrtcSignalingClient ${GST_APPLICATION_LIBRARIES})
  install(TARGETS awsGreengrassLabsWebRTC RUNTIME DESTINATION bin)

else()
  message("gstreamer not found. Will not build this application")
endif()
