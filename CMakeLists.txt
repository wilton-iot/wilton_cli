# Copyright 2017, alex at staticlibs.net
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required ( VERSION 2.8.12 )

# project
project ( wilton_cli CXX )

# dependencies
if ( STATICLIB_TOOLCHAIN MATCHES "(windows|macosx)_.+" )
    staticlib_add_subdirectory ( ${STATICLIB_DEPS}/external_popt )
endif ( )

set ( ${PROJECT_NAME}_DEPS
        staticlib_jni
        staticlib_json
        staticlib_utils
        staticlib_tinydir
        staticlib_unzip
        popt )
        
staticlib_pkg_check_modules ( ${PROJECT_NAME}_DEPS_PC REQUIRED ${PROJECT_NAME}_DEPS )

# executable
set ( ${PROJECT_NAME}_RESFILE )
if ( STATICLIB_TOOLCHAIN MATCHES "windows_.+" )
    configure_file ( ${CMAKE_CURRENT_LIST_DIR}/resources/${PROJECT_NAME}.rc
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc )
    set ( ${PROJECT_NAME}_RESFILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc )
endif ( )

add_executable ( ${PROJECT_NAME}
        ${CMAKE_CURRENT_LIST_DIR}/src/cli.cpp
        ${${PROJECT_NAME}_RESFILE} )

target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${WILTON_DIR}/core/include
        ${WILTON_DIR}/modules/wilton_loader/include
        ${WILTON_DIR}/modules/wilton_signal/include
        ${${PROJECT_NAME}_DEPS_PC_INCLUDE_DIRS} )

target_link_libraries ( ${PROJECT_NAME} PRIVATE
        wilton_signal
        wilton_core
        wilton_logging
        wilton_loader
        wilton_zip
        ${${PROJECT_NAME}_DEPS_PC_LIBRARIES} )

# platform-specific link options
if ( STATICLIB_TOOLCHAIN MATCHES "windows_.+" )
    target_link_libraries ( ${PROJECT_NAME} PRIVATE wtsapi32 )
    set_property ( TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS "/manifest:no" )
    if ( STATICLIB_TOOLCHAIN MATCHES "windows_amd64_.+" )
        # no chakra on winxp
        target_compile_definitions ( ${PROJECT_NAME} PRIVATE
                -DWILTON_DEFAULT_SCRIPT_ENGINE=chakra )
    endif ( )
endif ( )

# debuginfo
staticlib_extract_debuginfo_executable ( ${PROJECT_NAME} )

if ( STATICLIB_TOOLCHAIN MATCHES "(linux|macosx)_.+" )
    target_compile_definitions ( ${PROJECT_NAME} PRIVATE
            -DWILTON_DEFAULT_SCRIPT_ENGINE=jsc )
endif ( )
