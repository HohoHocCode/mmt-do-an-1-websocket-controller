# Optional vcpkg toolchain loader for Codex/Linux builds.
# If the local vcpkg installation exists, reuse its toolchain; otherwise continue without it.

set(_vcpkg_root "${CMAKE_CURRENT_LIST_DIR}/../../vcpkg")
set(_vcpkg_toolchain "${_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")

if(EXISTS "${_vcpkg_toolchain}")
  message(STATUS "Using vcpkg toolchain: ${_vcpkg_toolchain}")
  include("${_vcpkg_toolchain}")
else()
  message(STATUS "vcpkg toolchain not found at ${_vcpkg_toolchain}; building without vcpkg")
endif()
