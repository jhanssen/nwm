cmake_minimum_required(VERSION 2.8.6)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  set(V8_CXXFLAGS "-stdlib=libc++ -std=c++0x")
  set(V8_LDFLAGS "-stdlib=libc++")
else ()
  set(V8_CXXFLAGS "-std=c++0x")
  set(V8_LDFLAGS "")
endif ()

add_custom_target(v8 ${MAKE_COMMAND} dependencies
  COMMAND ${MAKE_COMMAND} LDFLAGS=${V8_LDFLAGS} CXXFLAGS=${V8_CXXFLAGS} GYPFLAGS="-Dmac_deployment_target=10.7" native
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/v8)

function(FIND_V8_LIB _out_ _dir_ _libname_)
  file(GLOB_RECURSE V8LIBNAME "${_dir_}/lib${_libname_}*.a")
  if (NOT EXISTS ${V8LIBNAME})
    message(FATAL_ERROR "V8 library not found: ${_libname_} ${_out_}")
  endif()
  set(${_out_} ${V8LIBNAME} PARENT_SCOPE)
endfunction()

set(V8_DIR ${CMAKE_CURRENT_BINARY_DIR}/v8/out/native)
find_v8_lib(V8_BASE_LIB ${V8_DIR} "v8_base")
find_v8_lib(V8_NOSNAPSHOT_LIB ${V8_DIR} "v8_nosnapshot")
set(V8_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/v8/include)
set(V8_LIBRARIES "${V8_BASE_LIB} ${V8_NOSNAPSHOT_LIB}")
