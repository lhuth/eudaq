get_filename_component(EUDAQ_USERNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(EUDAQ_MODULE module_${EUDAQ_USERNAME})

option(USER_ATLASPix_BUILD "build user/ATLASPix" OFF)

if(NOT USER_ATLASPix_BUILD)
    message(STATUS "user/ATLASPix is NOT to be built (USER_ATLASPix_BUILD=OFF)")
  return()
endif()

message(STATUS "user/ATLASPix is to be built (USER_ATLASPix_BUILD=ON)")

add_subdirectory(module)
