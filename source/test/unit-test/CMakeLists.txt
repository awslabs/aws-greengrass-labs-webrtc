# Include filepaths for source and include.
include( ${MODULE_ROOT_DIR}/WebrtcAppFilePaths.cmake )

# ====================  Define your project name (edit) ========================
set(project_name "WebrtcApp")

# =====================  Create your mock here  (edit)  ========================
include_directories("${MODULE_ROOT_DIR}/amazon-kinesis-video-streams-webrtc-sdk-c/src/include/com/amazonaws/kinesis/video/webrtcclient/")
link_directories("${MODULE_ROOT_DIR}/open-source/lib")

# for gdb
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")

# list the files to mock here
list(APPEND modules_mock_list
            "${MODULE_ROOT_DIR}/src/include/AppRtspSrcWrap.h"
            "${MODULE_ROOT_DIR}/src/include/AppCredentialWrap.h"
            "${MODULE_ROOT_DIR}/src/include/AppMetricsWrap.h"
            "${MODULE_ROOT_DIR}/src/include/AppQueueWrap.h"
            "${MODULE_ROOT_DIR}/amazon-kinesis-video-streams-webrtc-sdk-c/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h"
        )

list(APPEND common_mock_list
        "${MODULE_ROOT_DIR}/src/include/AppCredential.h"
        "${MODULE_ROOT_DIR}/src/include/AppDataChannel.h"
        "${MODULE_ROOT_DIR}/src/include/AppMetrics.h"
        "${MODULE_ROOT_DIR}/src/include/AppRtspSrc.h"
        "${MODULE_ROOT_DIR}/src/include/AppSignaling.h"
        "${MODULE_ROOT_DIR}/src/include/AppWebRTC.h"
        "${MODULE_ROOT_DIR}/src/include/AppHashTableWrap.h"
        "${MODULE_ROOT_DIR}/src/include/AppMessageQueue.h"
        "${MODULE_ROOT_DIR}/src/include/AppTimerWrap.h"
        "${MODULE_ROOT_DIR}/amazon-kinesis-video-streams-webrtc-sdk-c/src/include/com/amazonaws/kinesis/video/webrtcclient/Include.h"
        )

# list the directories your mocks need
list(APPEND modules_mock_include_list
            .
            ${WEBRTC_APP_INCLUDE_PUBLIC_DIRS}
        )
#list the definitions of your mocks to control what to be included
list(APPEND modules_mock_define_list
            ""
       )

# list the directories your mocks need
list(APPEND common_mock_include_list
            .
            ${WEBRTC_APP_INCLUDE_PUBLIC_DIRS}
        )
#list the definitions of your mocks to control what to be included
list(APPEND common_mock_define_list
            ""
       )
# ================= Create the library under test here (edit) ==================

# list the files you would like to test here
list(APPEND real_source_files
            ${WEBRTC_APP_SOURCES}
        )
# list the directories the module under test includes
list(APPEND real_include_directories
            .
            ${WEBRTC_APP_INCLUDE_PUBLIC_DIRS}
        )

# =====================  Create UnitTest Code here (edit)  =====================

# list the directories your test needs to include
list(APPEND test_include_directories
            .
            ${WEBRTC_APP_INCLUDE_PUBLIC_DIRS}
        )

# =============================  (end edit)  ===================================

set(modules_mock_name "${project_name}_modules_mock")
set(modules_real_name "${project_name}_modules_real")

# The unit tests for AppCredential AppDataChannel AppMetrics AppSignaling AppSignaling AppWebRTC AppRtspSrc
create_mock_list(${modules_mock_name}
                "${modules_mock_list}"
                "${MODULE_ROOT_DIR}/tools/cmock/project.yml"
                "${modules_mock_include_list}"
                "${modules_mock_define_list}"
        )

create_real_library(${modules_real_name}
        "${real_source_files}"
        "${real_include_directories}"
        "${modules_mock_name}"
)

list(APPEND utest_link_list
            -l${modules_mock_name}
            lib${modules_real_name}.a
            libkvspic.a
            -lpthread
        )

list(APPEND utest_dep_list
            ${modules_real_name}
        )

set(utest_name "AppCredentialUTest")
set(utest_source "AppCredentialUTest.c")
create_test(${utest_name}
            ${utest_source}
            "${utest_link_list}"
            "${utest_dep_list}"
            "${test_include_directories}"
        )


set(utest_name "AppDataChannelUTest")
set(utest_source "AppDataChannelUTest.c")
create_test(${utest_name}
                ${utest_source}
                "${utest_link_list}"
                "${utest_dep_list}"
                "${test_include_directories}"
        )

set(utest_name "AppMetricsUTest")
set(utest_source "AppMetricsUTest.c")
create_test(${utest_name}
                ${utest_source}
                "${utest_link_list}"
                "${utest_dep_list}"
                "${test_include_directories}"
        )

set(utest_name "AppSignalingUTest")
set(utest_source "AppSignalingUTest.c")
create_test(${utest_name}
                ${utest_source}
                "${utest_link_list}"
                "${utest_dep_list}"
                "${test_include_directories}"
        )

set(utest_name "AppWebRTCUTest")
set(utest_source "AppWebRTCUTest.c")
create_test(${utest_name}
                ${utest_source}
                "${utest_link_list}"
                "${utest_dep_list}"
                "${test_include_directories}"
        )

set(utest_name "AppRtspSrcUTest")
set(utest_source "AppRtspSrcUTest.c")
create_test(${utest_name}
                ${utest_source}
                "${utest_link_list}"
                "${utest_dep_list}"
                "${test_include_directories}"
        )

set(utest_name "AppMessageQueueUTest")
set(utest_source "AppMessageQueueUTest.c")
create_test(${utest_name}
                ${utest_source}
                "${utest_link_list}"
                "${utest_dep_list}"
                "${test_include_directories}"
        )

# The unit tests for AppCommon
set(common_mock_name "${project_name}_common_mock")
set(common_real_name "${project_name}_common_real")

create_mock_list(${common_mock_name}
        "${common_mock_list}"
        "${MODULE_ROOT_DIR}/tools/cmock/project.yml"
        "${common_mock_include_list}"
        "${common_mock_define_list}"
)

create_real_library(${common_real_name}
        "${real_source_files}"
        "${real_include_directories}"
        "${common_mock_name}"
)

set(utest_link_list "")

list(APPEND utest_link_list
        -l${common_mock_name}
        lib${common_real_name}.a
        libkvspic.a
        -lpthread
    )

list(APPEND utest_dep_list
        ${common_real_name}
    )

set(utest_name "AppCommonUTest")
set(utest_source "AppCommonUTest.c")
create_test(${utest_name}
                ${utest_source}
                "${utest_link_list}"
                "${utest_dep_list}"
                "${test_include_directories}"
        )
