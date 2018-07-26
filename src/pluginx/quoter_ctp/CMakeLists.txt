
# Copyright (c) 2018-2018 the QuoteX authors
# All rights reserved.
#
# The project sponsor and lead author is Xu Rendong.
# E-mail: xrd@ustc.edu, QQ: 277195007, WeChat: ustc_xrd
# See the contributors file for names of other contributors.
#
# Commercial use of this code in source and binary forms is
# governed by a LGPL v3 license. You may get a copy from the
# root directory. Or else you should get a specific written 
# permission from the project author.
#
# Individual and educational use of this code in source and
# binary forms is governed by a 3-clause BSD license. You may
# get a copy from the root directory. Certainly welcome you
# to contribute code of all sorts.
#
# Be sure to retain the above copyright notice and conditions.

INCLUDE_DIRECTORIES (${Boost_INCLUDE_DIRS} "${PROJECT_SOURCE_DIR}" "${SDKDIR_BASICX_INCLUDE}" "${SDKDIR_ZLIB_INCLUDE}" "${SDKDIR_INTERFACE}/CTP_F")

# Boost 的附加库目录会自动添加
IF (CMAKE_LINKTYPE_SHARED) # 插件仅限动态链接模式
    IF (MSVC)
        SET (CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /LIBPATH:\"${PROJECT_BINARY_DIR}/archive/Debug\"")
        SET (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LIBPATH:\"${PROJECT_BINARY_DIR}/archive/Release\"")
		SET (CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /LIBPATH:\"${SDKDIR_BASICX_LIBRARY_DEBUG}\"")
        SET (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LIBPATH:\"${SDKDIR_BASICX_LIBRARY_RELEASE}\"")
		SET (CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /LIBPATH:\"${SDKDIR_ZLIB_LIBRARY_DEBUG}\"")
        SET (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LIBPATH:\"${SDKDIR_ZLIB_LIBRARY_RELEASE}\"")
		SET (CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /LIBPATH:\"${SDKDIR_INTERFACE}/CTP_F\"")
        SET (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LIBPATH:\"${SDKDIR_INTERFACE}/CTP_F\"")
    ELSE ()
		LINK_DIRECTORIES (${PROJECT_BINARY_DIR}/archive) # need check
        LINK_DIRECTORIES (${PROJECT_BINARY_DIR}/archive) # need check
		LINK_DIRECTORIES (${PROJECT_BINARY_DIR}/archive) # need check
		LINK_DIRECTORIES (${PROJECT_BINARY_DIR}/archive) # need check
    ENDIF ()
ENDIF ()

FILE (GLOB PLUGIN_HEADER_FILES "./*.h")
FILE (GLOB PLUGIN_SOURCE_FILES "./*.cpp")

SOURCE_GROUP ("Header Files" FILES ${PLUGIN_HEADER_FILES})
SOURCE_GROUP ("Source Files" FILES ${PLUGIN_SOURCE_FILES})

IF (CMAKE_LINKTYPE_SHARED) # 插件仅限动态链接模式
    ADD_LIBRARY ("quoter_ctp" SHARED ${PLUGIN_HEADER_FILES} ${PLUGIN_SOURCE_FILES})
    IF (WIN32)
        TARGET_LINK_LIBRARIES ("quoter_ctp" common.lib syslog.lib syscfg.lib sysrtm.lib sysdbi_s.lib network.lib plugins.lib zlibwapi.lib odbc32.lib thostmduserapi.lib thosttraderapi.lib)
    ELSEIF (UNIX)
        TARGET_LINK_LIBRARIES ("quoter_ctp" common.a syslog.a syscfg.a sysrtm.a sysdbi_s.a network.a plugins.a zlibwapi.a thostmduserapi.a thosttraderapi.a) # odbc32
    ELSEIF (APPLE)
        TARGET_LINK_LIBRARIES ("quoter_ctp" common.a syslog.a syscfg.a sysrtm.a sysdbi_s.a network.a plugins.a zlibwapi.a thostmduserapi.a thosttraderapi.a) # odbc32
    ENDIF ()
ENDIF ()
