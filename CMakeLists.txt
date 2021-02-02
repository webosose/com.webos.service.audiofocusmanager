# @@@LICENSE
#
#   Copyright (c) 2014 - 2021 LG Electronics Inc. All Rights Reserved
#
# LICENSE@@@

cmake_minimum_required(VERSION 2.8.7)
project(audiofocusmanager)

include(webOS/webOS)
webos_modules_init(1 0 0 QUALIFIER RC4)

include(FindPkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
add_definitions(${GLIB2_CFLAGS})

pkg_check_modules(LUNASERVICE REQUIRED luna-service2)
add_definitions(${LUNASERVICE_CFLAGS})

pkg_check_modules(LIBPBNJSON REQUIRED pbnjson_cpp)
add_definitions(${LIBPBNJSON_CFLAGS})

pkg_check_modules(PMLOGLIB REQUIRED PmLogLib)
add_definitions(${PMLOGLIB_CFLAGS})

include_directories("${PROJECT_SOURCE_DIR}/include")

set(LOCATION_SERVICE_NAME audiofocusmanager)
set(SRC
        ${PROJECT_SOURCE_DIR}/src/main.cpp
        ${PROJECT_SOURCE_DIR}/src/audioFocusManager.cpp
        ${PROJECT_SOURCE_DIR}/src/sessionManager.cpp
        ${PROJECT_SOURCE_DIR}/src/log.cpp
        ${PROJECT_SOURCE_DIR}/src/utils.cpp
        ${PROJECT_SOURCE_DIR}/src/messageUtils.cpp
        ${PROJECT_SOURCE_DIR}/src/ConstString.cpp
)
set(LIBRARIES
        ${GLIB2_LDFLAGS}
        ${GOBJ_LDFLAGS}
        ${GMOD_LDFLAGS}
        ${LUNASERVICE_LDFLAGS}
        ${PMLOGLIB_LDFLAGS}
        ${LIBPBNJSON_LDFLAGS}
)


if(WEBOS_TARGET_DISTRO STREQUAL "webos-auto")
        if(WEBOS_TARGET_MACHINE STREQUAL "sa8155" OR WEBOS_TARGET_MACHINE STREQUAL "qemux86" OR WEBOS_TARGET_MACHINE STREQUAL "qemux86-64")
            add_definitions(-DWEBOS_SOC_AUTO)
        endif()
endif()

add_executable(${LOCATION_SERVICE_NAME} ${SRC})
target_link_libraries(${LOCATION_SERVICE_NAME} ${LIBRARIES} pbnjson_cpp)

webos_build_system_bus_files()
webos_build_daemon()

install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/audiofocusmanager
        FILES_MATCHING PATTERN "*.h")
install(FILES files/config/audiofocuspolicy.json DESTINATION ${WEBOS_INSTALL_WEBOS_SYSCONFDIR}/audiofocusmanager)
