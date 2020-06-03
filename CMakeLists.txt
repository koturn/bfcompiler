cmake_minimum_required(VERSION 3.3)
project(bfcompiler
  VERSION "1.0.0.0"
  LANGUAGES C CXX)

set(BUILD_TARGET ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_USE_RELATIVE_PATHS ON)


# C Compiler Version Requirements
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS 4.4)
    message(FATAL_ERROR "GCC version must be at least 4.4!")
  endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_C_COMPILER_VERSION VERSION_LESS 3.1)
    message(FATAL_ERROR "Clang version must be at least 3.1!")
  endif()
endif()

# C++ Compiler Version Requirements
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4)
    message(FATAL_ERROR "GCC version must be at least 4.4!")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.1)
    message(FATAL_ERROR "Clang version must be at least 3.1!")
  endif()
endif()


set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


if(CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*")
  set(SYSTEM_PROCESSOR_IS_X86 TRUE)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "amd64.*|x86_64.*|AMD64.*")
  set(SYSTEM_PROCESSOR_IS_X64 TRUE)
endif()


if(MSVC OR CYGWIN OR MINGW OR MSYS)
  list(APPEND DEFINES "WIN32_LEAN_AND_MEAN" "NOMINMAX")
endif()
if(MSVC)
  list(APPEND DEFINES "_USE_MATH_DEFINES" "_CRT_NONSTDC_NO_WARNINGS" "_CRT_SECURE_NO_WARNINGS")
  list(APPEND DEFINES_DEBUG "_DEBUG")
  list(APPEND DEFINES_RELWITHDEBINFO "_DEBUG")
else()
  list(APPEND DEFINES_RELEASE "NDEBUG")
  list(APPEND DEFINES_MINSIZEREL "NDEBUG")
  list(APPEND DEFINES_DEBUG "_FORTIFY_SOURCE=2" "_GLIBCXX_DEBUG")
endif()

if(MSVC)
  foreach(TARGET_FLAG
      CMAKE_CXX_FLAGS
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL
      CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    string(REGEX REPLACE "/RTC[^ ]*" "" ${TARGET_FLAG} "${${TARGET_FLAG}}")
    string(REGEX REPLACE "/W[0-4]" "/W4" ${TARGET_FLAG} "${${TARGET_FLAG}}")
  endforeach(TARGET_FLAG)

  list(APPEND CXX_FLAGS_DEBUG "/Oi" "/Oy")
  list(APPEND CXX_FLAGS_RELEASE "/Ob2" "/Oi" "/Ot" "/Ox" "/Oy" "/GL")
  list(APPEND CXX_FLAGS_MINSIZEREL "/Os")

  list(APPEND EXE_LINKER_FLAGS_RELEASE "/LTCG")
else()
  set(GNU_CLANG_COMMON_WARNING_FLAGS
    "-Wall"
    "-Wextra"
    "-Wabi"
    "-Wcast-align"
    "-Wcast-qual"
    "-Wconversion"
    "-Wdisabled-optimization"
    "-Wfloat-equal"
    "-Wformat=2"
    "-Winit-self"
    "-Winvalid-pch"
    "-Wmissing-declarations"
    "-Wpointer-arith"
    "-Wredundant-decls"
    "-Wstack-protector"
    "-Wstrict-aliasing=2"
    "-Wstrict-overflow=5"
    "-Wswitch-enum"
    "-Wswitch-default"
    "-Wunknown-pragmas"
    "-Wunreachable-code"
    "-Wwrite-strings"
    "-pedantic")

  option(ENABLE_ADDITIONAL_WARNING_FLAGS "Enable additional warning flags." OFF)
  if(ENABLE_ADDITIONAL_WARNING_FLAGS)
    list(APPEND GNU_CLANG_COMMON_WARNING_FLAGS "-Winline" "-Wsign-conversion")
  endif()

  set(GNU_COMMON_WARNING_FLAGS "${GNU_CLANG_COMMON_WARNING_FLAGS}" "-Wlogical-op")
  if(ENABLE_ADDITIONAL_WARNING_FLAGS)
    list(APPEND GNU_COMMON_WARNING_FLAGS "-Wunsafe-loop-optimizations")
  endif()

  set(CLANG_COMMON_WARNING_FLAGS "${GNU_CLANG_COMMON_WARNING_FLAGS}" "-Wsign-promo")

  if(CMAKE_COMPILER_IS_GNUCXX)
    set(CXX_WARNING_FLAGS
      "${GNU_COMMON_WARNING_FLAGS}"
      "-Weffc++"
      "-Wctor-dtor-privacy"
      "-Wnon-virtual-dtor"
      "-Wold-style-cast"
      "-Woverloaded-virtual"
      "-Wreorder"
      "-Wsign-promo"
      "-Wstrict-null-sentinel")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.3)
      message(STATUS "Add warning flags implemented in g++ 4.3")
      list(APPEND CXX_WARNING_FLAGS "-Wc++0x-compat")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.6)
      message(STATUS "Add warning flags implemented in g++ 4.6")
      list(APPEND CXX_WARNING_FLAGS
        "-Wdouble-promotion"
        "-Wnoexcept"
        "-Wsuggest-attribute=const"
        "-Wsuggest-attribute=noreturn"
        "-Wsuggest-attribute=pure"
        "-Wtrampolines")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.7)
      message(STATUS "Add warning flags implemented in g++ 4.7")
      string(REGEX REPLACE "-Wc\\+\\+0x-compat" "-Wc++11-compat" CXX_WARNING_FLAGS "${CXX_WARNING_FLAGS}")
      list(APPEND CXX_WARNING_FLAGS
        "-Wdelete-non-virtual-dtor"
        "-Wvector-operation-performance"
        "-Wno-return-local-addr"
        "-Wzero-as-null-pointer-constant")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.8)
      message(STATUS "Add warning flags implemented in g++ 4.8")
      list(APPEND CXX_WARNING_FLAGS
        "-Wsuggest-attribute=format"
        "-Wuseless-cast")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.9)
      message(STATUS "Add warning flags implemented in g++ 4.9")
      list(APPEND CXX_WARNING_FLAGS "-Wconditionally-supported")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 5.1)
      message(STATUS "Add warning flags implemented in g++ 5.1")
      list(APPEND CXX_WARNING_FLAGS
        "-Wc++14-compat"
        "-Wformat-signedness"
        "-Wsuggest-final-methods"
        "-Wsuggest-final-types"
        "-Wsuggest-override")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
      message(STATUS "Add warning flags implemented in g++ 6.0")
      list(APPEND CXX_WARNING_FLAGS
        "-Wduplicated-cond"
        "-Wplacement-new=2"
        "-Wshift-overflow=2")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7.0)
      message(STATUS "Add warning flags implemented in g++ 7.0")
      string(REGEX REPLACE "-Wabi" "-Wabi=11" CXX_WARNING_FLAGS "${CXX_WARNING_FLAGS}")
      list(APPEND CXX_WARNING_FLAGS
        "-Wc++17-compat"
        "-Walloc-zero"
        "-Wduplicated-branches"
        "-Wformat-overflow=2"
        "-Wformat-truncation=2"
        "-Wregister"
        "-Wrestrict"
        "-Wshadow-local"
        "-Wstringop-overflow=4")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
      message(STATUS "Add warning flags implemented in g++ 8.0")
      string(REGEX REPLACE "-Wcast-align" "-Wcast-align=strict" CXX_WARNING_FLAGS "${CXX_WARNING_FLAGS}")
      list(APPEND CXX_WARNING_FLAGS "-Wsuggest-attribute=malloc")
    endif()

    set(CXX_FLAGS "${CMAKE_CXX_FLAGS}" "-pipe" "${CXX_WARNING_FLAGS}")
    set(CXX_FLAGS_DEBUG "-g3" "-O0" "-ftrapv" "-fstack-protector-all")
    set(CXX_FLAGS_RELEASE "-O3")
    set(CXX_FLAGS_MINSIZEREL "-Os")
    if(SYSTEM_PROCESSOR_IS_X86 OR SYSTEM_PROCESSOR_IS_X64)
      list(APPEND CXX_FLAGS_RELEASE "-mtune=native" "-march=native")
      list(APPEND CXX_FLAGS_MINSIZEREL "-mtune=native" "-march=native")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
      set(CXX_FLAGS_RELWITHDEBINFO "-g" "-O2")
    else()
      set(CXX_FLAGS_RELWITHDEBINFO "-g" "-Og")
    endif()
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CXX_WARNING_FLAGS "${CLANG_COMMON_WARNING_FLAGS}")
    list(APPEND CXX_WARNING_FLAGS
      "-Wc++11-compat"
      "-Weffc++"
      "-Wctor-dtor-privacy"
      "-Wdelete-non-virtual-dtor"
      "-Wnon-virtual-dtor"
      "-Wold-style-cast"
      "-Woverloaded-virtual"
      "-Wreorder")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 3.5)
      message(STATUS "Add warning flags implemented in clang++ 3.5")
      list(APPEND CXX_WARNING_FLAGS "-Wc++14-compat")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 3.8)
      message(STATUS "Add warning flags implemented in clang++ 3.8")
      list(APPEND CXX_WARNING_FLAGS
        "-Wc++1z-compat"
        "-Wdouble-promotion")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
      message(STATUS "Add warning flags implemented in clang++ 5.0")
      string(REGEX REPLACE "-Wc\\+\\+1z-compat" "-Wc++17-compat" CXX_WARNING_FLAGS "${CXX_WARNING_FLAGS}")
      list(CXX_WARNING_FLAGS "-Wzero-as-null-pointer-constant")
    endif()

    set(CXX_FLAGS "${CMAKE_CXX_FLAGS}" "-pipe" "${CXX_WARNING_FLAGS}")
    set(CXX_FLAGS_DEBUG "-g3" "-O0" "-ftrapv" "-fstack-protector-all")
    set(CXX_FLAGS_RELEASE "-O3")
    set(CXX_FLAGS_MINSIZEREL "-s")
    if(SYSTEM_PROCESSOR_IS_X86 OR SYSTEM_PROCESSOR_IS_X64)
      list(APPEND CXX_FLAGS_RELEASE "-mtune=native" "-march=native")
      list(APPEND CXX_FLAGS_MINSIZEREL "-mtune=native" "-march=native")
    endif()
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.0)
      set(CXX_FLAGS_RELWITHDEBINFO "-g" "-O2")
    else()
      set(CXX_FLAGS_RELWITHDEBINFO "-g" "-Og")
    endif()
  else()
    set(CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
    set(CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  endif()

  set(EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" "-s" "-Wl,-O1")
  set(EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL}" "-s")
endif()

add_subdirectory(bf2elfx64)
add_subdirectory(bf2elfx86)
add_subdirectory(bf2pex64)
add_subdirectory(bf2pex86)
