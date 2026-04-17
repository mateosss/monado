if(NOT CMAKE_SYSROOT)
  # Nested try_compile runs may not carry CMAKE_SYSROOT from presets.
  set(_radxa_default_sysroot "${CMAKE_CURRENT_LIST_DIR}/mona7z-arm64-sysroot")
  if(EXISTS "${_radxa_default_sysroot}")
    set(CMAKE_SYSROOT "${_radxa_default_sysroot}" CACHE PATH "Radxa cross sysroot" FORCE)
    message(STATUS "Radxa CMAKE_SYSROOT not preset, defaulting to: ${CMAKE_SYSROOT}")
  else()
    message(FATAL_ERROR "CMAKE_SYSROOT not set.")
  endif()
else()
  message(STATUS "Radxa CMAKE_SYSROOT: ${CMAKE_SYSROOT}")
endif()

# CMake runs nested try_compile projects (for example, IPO/LTO checks) that
# re-load this toolchain file. Explicitly forward sysroot to those checks.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_SYSROOT)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CROSS_GNU aarch64-rpi3-linux-gnu)
set(CMAKE_C_COMPILER ${CROSS_GNU}-gcc)
set(CMAKE_CXX_COMPILER ${CROSS_GNU}-g++)
set(CMAKE_Fortran_COMPILER ${CROSS_GNU}-gfortran)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

if(EXISTS "${CMAKE_SYSROOT}/lib/aarch64-linux-gnu/libdl.so.2")
  set(CMAKE_DL_LIBS "${CMAKE_SYSROOT}/lib/aarch64-linux-gnu/libdl.so.2" CACHE STRING "Dynamic loader library" FORCE)
endif()

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)

set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
