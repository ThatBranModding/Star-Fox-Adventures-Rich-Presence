cmake_minimum_required(VERSION 3.25)
project(sfa_rich_presence VERSION 0.1.22 LANGUAGES C)

add_library(sfa_rich_presence SHARED src/mod.c)
target_include_directories(sfa_rich_presence PRIVATE include src)

if(WIN32)
  target_sources(sfa_rich_presence PRIVATE src/rpc_windows.c src/platform_windows.c)
  set(MOD_PLATFORM_DIR "windows-amd64")
  set(MOD_LIBRARY_NAME "mod.dll")
elseif(APPLE)
  target_sources(sfa_rich_presence PRIVATE src/rpc_unix.c src/platform_unix.c)

  if(CMAKE_OSX_ARCHITECTURES MATCHES "(^|;)x86_64(;|$)")
    message(FATAL_ERROR "Foxhollow publishing currently supports Apple Silicon macOS only")
  elseif(NOT CMAKE_OSX_ARCHITECTURES MATCHES "(^|;)arm64(;|$)" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    message(FATAL_ERROR "Foxhollow publishing currently supports Apple Silicon macOS only")
  endif()

  set(MOD_PLATFORM_DIR "macos-arm64")
  set(MOD_LIBRARY_NAME "mod.so")
elseif(UNIX)
  target_sources(sfa_rich_presence PRIVATE src/rpc_unix.c src/platform_unix.c)
  set(MOD_PLATFORM_DIR "linux-amd64")
  set(MOD_LIBRARY_NAME "mod.so")
else()
  message(FATAL_ERROR "Unsupported platform")
endif()

set_target_properties(sfa_rich_presence PROPERTIES PREFIX "" OUTPUT_NAME "mod")

set(MOD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/sfa-rich-presence")
set(MOD_LIB_DIR "${MOD_OUTPUT_DIR}/lib/${MOD_PLATFORM_DIR}")

add_custom_command(TARGET sfa_rich_presence POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_LIB_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:sfa_rich_presence>" "${MOD_LIB_DIR}/${MOD_LIBRARY_NAME}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/mod.json" "${MOD_OUTPUT_DIR}/mod.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/discord_presence.ini" "${MOD_OUTPUT_DIR}/discord_presence.ini")
