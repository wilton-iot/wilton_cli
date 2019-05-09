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
        staticlib_ranges
        popt )

staticlib_pkg_check_modules ( ${PROJECT_NAME}_DEPS_PC REQUIRED ${PROJECT_NAME}_DEPS )

# executable
set ( ${PROJECT_NAME}_PLATFORM_LIBS )
set ( ${PROJECT_NAME}_RESFILE )
if ( STATICLIB_TOOLCHAIN MATCHES "windows_.+" )
    configure_file ( ${CMAKE_CURRENT_LIST_DIR}/resources/${PROJECT_NAME}.rc
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc )
    set ( ${PROJECT_NAME}_RESFILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc )
elseif( STATICLIB_TOOLCHAIN MATCHES "linux_.+" )
    list ( APPEND ${PROJECT_NAME}_PLATFORM_LIBS dl )
endif ( )

set ( ${PROJECT_NAME}_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/cli.cpp
        ${${PROJECT_NAME}_RESFILE} )

set ( ${PROJECT_NAME}_INCLUDES
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${WILTON_DIR}/core/include
        ${WILTON_DIR}/modules/wilton_loader/include
        ${WILTON_DIR}/modules/wilton_signal/include
        ${${PROJECT_NAME}_DEPS_PC_INCLUDE_DIRS} )

set ( ${PROJECT_NAME}_LIBS
        wilton_signal
        wilton_core
        wilton_logging
        wilton_loader
        wilton_zip
        ${${PROJECT_NAME}_PLATFORM_LIBS}
        ${${PROJECT_NAME}_DEPS_PC_LIBRARIES} )

add_executable ( ${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES} )
target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE ${${PROJECT_NAME}_INCLUDES} )
target_link_libraries ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_LIBS} )

# platform-specific link options
if ( STATICLIB_TOOLCHAIN MATCHES "windows_.+" )
    # wilton_cli
    target_link_libraries ( ${PROJECT_NAME} PRIVATE wtsapi32 )
    set_property ( TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS "/manifest:no" )
    # wiltonw
    add_executable ( wiltonw ${${PROJECT_NAME}_SOURCES} )
    target_include_directories ( wiltonw BEFORE PRIVATE ${${PROJECT_NAME}_INCLUDES} )
    target_link_libraries ( wiltonw PRIVATE ${${PROJECT_NAME}_LIBS} wtsapi32 )
    set_property ( TARGET wiltonw APPEND_STRING PROPERTY LINK_FLAGS "/manifest:no" )
    get_target_property ( wiltonw_LINK_FLAGS wiltonw LINK_FLAGS )
    if ( STATICLIB_TOOLCHAIN MATCHES "windows_amd64_.+" )
        # no chakra on winxp
        target_compile_definitions ( ${PROJECT_NAME} PRIVATE
                -DWILTON_DEFAULT_SCRIPT_ENGINE=chakra )
        target_compile_definitions ( wiltonw PRIVATE
                -DWILTON_DEFAULT_SCRIPT_ENGINE=chakra )
        set ( wiltonw_LINK_FLAGS "${wiltonw_LINK_FLAGS} /subsystem:windows,6.01 /entry:mainCRTStartup" )
    elseif ( STATICLIB_TOOLCHAIN MATCHES "windows_i386_.+" )
        set ( wiltonw_LINK_FLAGS "${wiltonw_LINK_FLAGS} /subsystem:windows,5.01 /entry:mainCRTStartup" )
    endif ( )
    set_target_properties ( wiltonw PROPERTIES LINK_FLAGS ${wiltonw_LINK_FLAGS} )
endif ( )

# mac dep paths
if ( STATICLIB_TOOLCHAIN MATCHES "macosx_.+" )
	add_custom_command ( TARGET ${PROJECT_NAME} POST_BUILD
			COMMAND ${WILTON_DIR}/resources/scripts/mac-deps.sh 
					${CMAKE_EXECUTABLE_PREFIX}${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}
			WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
			COMMENT "Rewriting dependency paths: [${CMAKE_EXECUTABLE_PREFIX}${PROJECT_NAME}${CMAKE_EXECUTABLE_SUFFIX}]" )
endif ( )

# debuginfo
staticlib_extract_debuginfo_executable ( ${PROJECT_NAME} )

# default engine
if ( STATICLIB_TOOLCHAIN MATCHES "(linux|macosx)_.+" )
    target_compile_definitions ( ${PROJECT_NAME} PRIVATE
            -DWILTON_DEFAULT_SCRIPT_ENGINE=jsc )
endif ( )
