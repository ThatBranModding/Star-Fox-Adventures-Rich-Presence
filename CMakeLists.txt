cmake_minimum_required(VERSION 3.25)
project(sfa_rich_presence VERSION 0.1.20 LANGUAGES C)
add_library(sfa_rich_presence SHARED src/mod.c)
target_include_directories(sfa_rich_presence PRIVATE include)
if(WIN32)
  target_link_libraries(sfa_rich_presence PRIVATE kernel32)
endif()
set_target_properties(sfa_rich_presence PROPERTIES PREFIX "" OUTPUT_NAME "mod")
set(MOD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/sfa-rich-presence")
set(MOD_PLATFORM_DIR "${MOD_OUTPUT_DIR}/lib/windows-amd64")
if(WIN32)
  set_target_properties(sfa_rich_presence PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MOD_PLATFORM_DIR}")
endif()
add_custom_command(TARGET sfa_rich_presence POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_PLATFORM_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:sfa_rich_presence>" "${MOD_PLATFORM_DIR}/mod.dll"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/mod.json" "${MOD_OUTPUT_DIR}/mod.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/discord_presence.ini" "${MOD_OUTPUT_DIR}/discord_presence.ini")
