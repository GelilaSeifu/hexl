# Copyright (C) 2020-2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set(NATIVE_SRC
    eltwise/eltwise-mult-mod.cpp
    eltwise/eltwise-reduce-mod.cpp
    eltwise/eltwise-sub-mod.cpp
    eltwise/eltwise-add-mod.cpp
    eltwise/eltwise-fma-mod.cpp
    eltwise/eltwise-cmp-add.cpp
    eltwise/eltwise-cmp-sub-mod.cpp
    ntt/ntt-internal.cpp
    number-theory/number-theory.cpp
)

if (HEXL_HAS_AVX512DQ)
    set(AVX512_SRC
        eltwise/eltwise-mult-mod-avx512.cpp
        eltwise/eltwise-reduce-mod-avx512.cpp
        eltwise/eltwise-add-mod-avx512.cpp
        eltwise/eltwise-cmp-sub-mod-avx512.cpp
        eltwise/eltwise-cmp-add-avx512.cpp
        eltwise/eltwise-sub-mod-avx512.cpp
        eltwise/eltwise-fma-mod-avx512.cpp
        ntt/fwd-ntt-avx512.cpp
        ntt/inv-ntt-avx512.cpp
    )
endif()

set(HEXL_SRC "${NATIVE_SRC};${AVX512_SRC}")

if (HEXL_DEBUG)
    list(APPEND HEXL_SRC logging/logging.cpp)
endif()

if (HEXL_SHARED_LIB)
    add_library(hexl SHARED ${HEXL_SRC})
else()
     add_library(hexl STATIC ${HEXL_SRC})
endif()
add_library(HEXL::hexl ALIAS hexl)

hexl_add_asan_flag(hexl)

set(HEXL_DEFINES_IN_FILENAME ${CMAKE_CURRENT_SOURCE_DIR}/include/hexl/util/defines.hpp.in)
set(HEXL_DEFINES_FILENAME ${CMAKE_CURRENT_SOURCE_DIR}/include/hexl/util/defines.hpp)
configure_file(${HEXL_DEFINES_IN_FILENAME} ${HEXL_DEFINES_FILENAME})

set_target_properties(hexl PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(hexl PROPERTIES VERSION ${HEXL_VERSION})
if (HEXL_DEBUG)
    set_target_properties(hexl PROPERTIES OUTPUT_NAME "hexl_debug")
endif()

target_include_directories(hexl
    PRIVATE ${HEXL_SRC_ROOT_DIR}                               # Private headers
    PUBLIC  $<BUILD_INTERFACE:${HEXL_INC_ROOT_DIR}>            # Public headers
    PUBLIC  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>   # Public headers
)
if(CpuFeatures_FOUND)
    target_include_directories(hexl PUBLIC ${CpuFeatures_INCLUDE_DIR}) # Public headers
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hexl PRIVATE -Wall -Wconversion -Wshadow -pedantic -Wextra
        -Wno-unknown-pragmas -march=native -O3 -fomit-frame-pointer
    )
    # Avoid 3rd-party dependency warnings when including HEXL as a dependency
    target_compile_options(hexl PUBLIC
        -Wno-sign-conversion         # avoid warnings in gflags headers
        -Wno-implicit-int-conversion # avoid warnings in gflags headers
        -Wno-unknown-warning
        -Wno-unknown-warning-option
    )

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # Inling causes some tests to fail on MSVC with AVX512 in Release mode, HEXL_DEBUG=OFF,
    # so we disable it here
    target_compile_options(hexl PRIVATE /Wall /W4 /Zc:preprocessor /Ob0
        /wd4127 # warning C4127: conditional expression is constant; C++11 doesn't support constexpr
     )
    target_compile_definitions(hexl PRIVATE -D_CRT_SECURE_NO_WARNINGS)
endif()

install(DIRECTORY ${HEXL_INC_ROOT_DIR}/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/
        FILES_MATCHING
        PATTERN "*.hpp"
        PATTERN "*.h")

if (HEXL_SHARED_LIB)
    target_link_libraries(hexl PRIVATE cpu_features)
    if (HEXL_DEBUG)
        target_link_libraries(hexl PUBLIC easyloggingpp)
        # Manually add logging include directory
        target_include_directories(hexl
            PUBLIC $<BUILD_INTERFACE:$<TARGET_PROPERTY:easyloggingpp,INCLUDE_DIRECTORIES>>
        )
        target_link_libraries(hexl PRIVATE gflags)
    endif()
else ()
    # For static library, we include all the dependencies for Intel HEXL in
    # the libhexl.a.
    # For proper export of HEXLConfig.cmake / HEXLTargets.cmake,
    # we avoid explicitly linking dependencies via target_link_libraries, since
    # this would add dependencies to the exported hexl target.

    if (CpuFeatures_FOUND)
      target_link_libraries(hexl PRIVATE cpu_features)
    else()
      add_dependencies(hexl cpu_features)
    endif()

    if (HEXL_DEBUG)
        add_dependencies(hexl gflags)
        add_dependencies(hexl easyloggingpp)

        # Manually add logging include directory
        target_include_directories(hexl
            PUBLIC $<BUILD_INTERFACE:$<TARGET_PROPERTY:easyloggingpp,INCLUDE_DIRECTORIES>>
        )

        # Manually add gflags include directory
        target_include_directories(hexl
            PRIVATE $<TARGET_PROPERTY:gflags,INTERFACE_INCLUDE_DIRECTORIES>)
    endif()

    # Manually add cpu_features include directory
    target_include_directories(hexl
      PRIVATE $<TARGET_PROPERTY:cpu_features,INTERFACE_INCLUDE_DIRECTORIES>)

    # Export logging only if we are debugging
    if (${HEXL_DEBUG})
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            if(CpuFeatures_FOUND)
                add_custom_command(TARGET hexl POST_BUILD
                    COMMAND ar -x $<TARGET_FILE:hexl>
                    COMMAND ar -x $<TARGET_FILE:gflags>
                    COMMAND ar -x $<TARGET_FILE:easyloggingpp>
                    COMMAND ar -qcs $<TARGET_FILE:hexl> *.o
                    COMMAND rm -f *.o
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS hexl gflags easyloggingpp
                )
            else()
                add_custom_command(TARGET hexl POST_BUILD
                    COMMAND ar -x $<TARGET_FILE:hexl>
                    COMMAND ar -x $<TARGET_FILE:cpu_features>
                    COMMAND ar -x $<TARGET_FILE:gflags>
                    COMMAND ar -x $<TARGET_FILE:easyloggingpp>
                    COMMAND ar -qcs $<TARGET_FILE:hexl> *.o
                    COMMAND rm -f *.o
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS hexl cpu_features gflags easyloggingpp
            )
            endif()
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            if(CpuFeatures_FOUND)
                add_custom_command(TARGET hexl POST_BUILD
                    COMMAND lib.exe /OUT:$<TARGET_FILE:hexl>
                        $<TARGET_FILE:hexl>
                        $<TARGET_FILE:gflags>
                        $<TARGET_FILE:easyloggingpp>
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS hexl gflags easyloggingpp
                )
            else()
                add_custom_command(TARGET hexl POST_BUILD
                    COMMAND lib.exe /OUT:$<TARGET_FILE:hexl>
                        $<TARGET_FILE:hexl>
                        $<TARGET_FILE:cpu_features>
                        $<TARGET_FILE:gflags>
                        $<TARGET_FILE:easyloggingpp>
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS hexl cpu_features gflags easyloggingpp
             )
            endif()
        endif()
    else()
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            if(CpuFeatures_FOUND)
                add_custom_command(TARGET hexl POST_BUILD
                    COMMAND ar -x $<TARGET_FILE:hexl>
                    COMMAND ar -qcs $<TARGET_FILE:hexl> *.o
                    COMMAND rm -f *.o
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS hexl

            )
            else()
                add_custom_command(TARGET hexl POST_BUILD
                    COMMAND ar -x $<TARGET_FILE:hexl>
                    COMMAND ar -x $<TARGET_FILE:cpu_features>
                    COMMAND ar -qcs $<TARGET_FILE:hexl> *.o
                    COMMAND rm -f *.o
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS hexl cpu_features
        )
            endif()
        elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            if(CpuFeatures_FOUND)
                add_custom_command(TARGET hexl POST_BUILD
                    COMMAND lib.exe /OUT:$<TARGET_FILE:hexl>
                        $<TARGET_FILE:hexl>
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS hexl

                )
            else()
                add_custom_command(TARGET hexl POST_BUILD
                    COMMAND lib.exe /OUT:$<TARGET_FILE:hexl>
                        $<TARGET_FILE:hexl>
                        $<TARGET_FILE:cpu_features>
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS hexl cpu_features
            )
            endif()
      endif()
    endif()
endif()

install(TARGETS hexl DESTINATION ${CMAKE_INSTALL_LIBDIR})

#------------------------------------------------------------------------------
# Config export...
#------------------------------------------------------------------------------

# Config filenames
set(HEXL_TARGET_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/cmake/hexl-${HEXL_VERSION}/HEXLTargets.cmake)
set(HEXL_CONFIG_IN_FILENAME ${HEXL_CMAKE_PATH}/HEXLConfig.cmake.in)
set(HEXL_CONFIG_FILENAME ${HEXL_ROOT_DIR}/cmake/hexl-${HEXL_VERSION}/HEXLConfig.cmake)
set(HEXL_CONFIG_VERSION_FILENAME ${CMAKE_CURRENT_BINARY_DIR}/cmake/hexl-${HEXL_VERSION}/HEXLConfigVersion.cmake)
set(HEXL_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/hexl-${HEXL_VERSION}/)

# Create and install the CMake config and target file
install(
    EXPORT HEXLTargets
    NAMESPACE HEXL::
    DESTINATION ${HEXL_CONFIG_INSTALL_DIR}
)

# Export version
write_basic_package_version_file(
  ${HEXL_CONFIG_VERSION_FILENAME}
  VERSION ${HEXL_VERSION}
  COMPATIBILITY ExactVersion)

include(CMakePackageConfigHelpers)
    configure_package_config_file(
        ${HEXL_CONFIG_IN_FILENAME} ${HEXL_CONFIG_FILENAME}
        INSTALL_DESTINATION ${HEXL_CONFIG_INSTALL_DIR}
    )

install(
    TARGETS hexl
    EXPORT HEXLTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

install(FILES ${HEXL_CONFIG_FILENAME}
              ${HEXL_CONFIG_VERSION_FILENAME}
        DESTINATION ${HEXL_CONFIG_INSTALL_DIR})

export(EXPORT HEXLTargets
        FILE ${HEXL_TARGET_FILENAME})
