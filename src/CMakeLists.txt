cmake_minimum_required(VERSION 2.8.6)
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wstack-protector")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstack-protector")

set(RCT_BUILD_SCRIPTENGINE 1)
add_subdirectory(rct)

include(${CMAKE_CURRENT_LIST_DIR}/rct/compiler.cmake)
# set(RCT_INCLUDE_DIR rct)
# include(${CMAKE_CURRENT_LIST_DIR}/rct/rct.cmake)

if (CMAKE_BUILD_TYPE MATCHES "Debug")
  set(RCT_EVENTLOOP_CALLBACK_TIME_THRESHOLD 2000)
endif ()

add_definitions("-Wall")
add_definitions("-DOS_${CMAKE_SYSTEM_NAME}")

set(NWM_SOURCES
    main.cpp
    Atoms.cpp
    Client.cpp
    ClientGroup.cpp
    Graphics.cpp
    GridLayout.cpp
    Handlers.cpp
    JavaScript.cpp
    Keybinding.cpp
    Keybindings.cpp
    StackLayout.cpp
    Util.cpp
    WindowManager.cpp
    Workspace.cpp)

include(FindPkgConfig)
pkg_check_modules(XKBCOMMON REQUIRED xkbcommon)
pkg_check_modules(XKBCOMMON_X11 REQUIRED xkbcommon-x11)
pkg_check_modules(XCB REQUIRED xcb)
pkg_check_modules(XCB_UTIL REQUIRED xcb-util)
pkg_check_modules(XCB_ICCCM REQUIRED xcb-icccm)
pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh)
pkg_check_modules(XCB_XKB REQUIRED xcb-xkb)
pkg_check_modules(XCB_KEYSYMS REQUIRED xcb-keysyms)
pkg_check_modules(CAIRO cairo)
pkg_check_modules(PANGO pango)
pkg_check_modules(PANGO_CAIRO pangocairo)
if(CAIRO_LIBRARIES)
    set(HAVE_CAIRO 1)
endif()
if(PANGO_LIBRARIES AND PANGO_CAIRO_LIBRARIES)
    set(HAVE_PANGO 1)
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}
                    ${XCB_INCLUDE_DIRS}
                    ${XCB_UTIL_INCLUDE_DIRS}
                    ${XCB_ICCCM_INCLUDE_DIRS}
                    ${XCB_EWMH_INCLUDE_DIRS}
                    ${XCB_XKB_INCLUDE_DIRS}
                    ${XCB_KEYSYMS_INCLUDE_DIRS}
                    ${XKBCOMMON_INCLUDE_DIRS}
                    ${XKBCOMMON_X11_INCLUDE_DIRS}
                    ${PANGO_INCLUDE_DIRS}
                    ${PANGO_CAIRO_INCLUDE_DIRS}
                    ${CAIRO_INCLUDE_DIRS}
                    rct/include/
                    rct
                    ${CMAKE_CURRENT_BINARY_DIR}/rct/include/rct)

configure_file(nwm-config.h.in nwm-config.h)

add_executable(nwm ${NWM_SOURCES})
add_dependencies(nwm rct)
target_link_libraries(nwm
                      rct
                      ${XCB_LIBRARIES}
                      ${XCB_UTIL_LIBRARIES}
                      ${XCB_ICCCM_LIBRARIES}
                      ${XCB_EWMH_LIBRARIES}
                      ${XCB_XKB_LIBRARIES}
                      ${XCB_KEYSYMS_LIBRARIES}
                      ${XKBCOMMON_LIBRARIES}
                      ${XKBCOMMON_X11_LIBRARIES}
                      ${PANGO_LIBRARIES}
                      ${PANGO_CAIRO_LIBRARIES}
                      ${CAIRO_LIBRARIES})
