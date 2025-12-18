set(_vcpkg_toolchain "${CMAKE_CURRENT_LIST_DIR}/../../vcpkg/scripts/buildsystems/vcpkg.cmake")

if(EXISTS "${_vcpkg_toolchain}")
  message(STATUS "Using vcpkg toolchain: ${_vcpkg_toolchain}")
  include("${_vcpkg_toolchain}")
else()
  message(STATUS "vcpkg not found at ${_vcpkg_toolchain}; building without vcpkg (system deps / optional features).")
endif()
