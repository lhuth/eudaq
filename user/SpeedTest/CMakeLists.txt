get_filename_component(EUDAQ_USERNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(EUDAQ_MODULE module_${EUDAQ_USERNAME})

option(USER_SpeedTest_BUILD "build user/SpeedTest" OFF)

if(NOT USER_SpeedTest_BUILD)
    message(STATUS "user/SpeedTest is NOT to be built (USER_SpeedTest_BUILD=OFF)")
  return()
endif()

message(STATUS "user/SpeedTest is to be built (USER_SpeedTest_BUILD=ON)")

add_subdirectory(module)
