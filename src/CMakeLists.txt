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
  Handlers.cpp
  WindowManager.cpp)

include_directories(${CMAKE_CURRENT_LIST_DIR}
                    rct/include/
                    rct
                    ${CMAKE_CURRENT_BINARY_DIR}/rct/include/rct)

add_executable(nwm ${NWM_SOURCES})
add_dependencies(nwm rct)
target_link_libraries(nwm rct xcb xcb-util)
