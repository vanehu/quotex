
# Copyright (c) 2018-2018 the QuoteX authors
# All rights reserved.
#
# The project sponsor and lead author is Xu Rendong.
# E-mail: xrd@ustc.edu, QQ: 277195007, WeChat: ustc_xrd
# You can get more information at https://xurendong.github.io
# For names of other contributors see the contributors file.
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

PROJECT (quotex CXX)

CMAKE_MINIMUM_REQUIRED (VERSION 2.8)

SET_PROPERTY (GLOBAL PROPERTY USE_FOLDERS ON)

# 考虑接口兼容和平台架构，本项目限制选项 CMAKE_PLATFORM_X64：OFF 且 CMAKE_LINKTYPE_SHARED：ON
OPTION (CMAKE_PLATFORM_X64 "platform x86 or x64." OFF) # 启用则 64 位，禁用则 32 位
OPTION (CMAKE_LINKTYPE_SHARED "link type shared or static." ON) # 启用则动态链接，禁用则静态链接
OPTION (CMAKE_COMPILE_RELEASE "compile release or debug." ON) # 启用则 release 版，禁用则 debug 版

# 目前只允许 link=shared runtime-link=shared (DLL MD/MDd) 和 link=static runtime-link=static (LIB MT/MTd) 两种
IF (CMAKE_PLATFORM_X64)
    SET (PATH_FLAG_PLATFORM "x64")
ELSE ()
    SET (PATH_FLAG_PLATFORM "x86")
ENDIF ()
IF (CMAKE_LINKTYPE_SHARED)
    SET (PATH_FLAG_BINORLIB "bin")
    SET (PATH_FLAG_LINKTYPE "shared")
ELSE ()
    SET (PATH_FLAG_BINORLIB "lib")
    SET (PATH_FLAG_LINKTYPE "static")
ENDIF ()
IF (CMAKE_COMPILE_RELEASE)
    SET (PATH_FLAG_COMPILE "release")
ELSE ()
    SET (PATH_FLAG_COMPILE "debug")
ENDIF ()

SET (SDKDIR_PATH "D:/SDK/GitHub/VS2017" CACHE PATH "sdk dir." FORCE)
SET (SDKDIR_BASICX_INCLUDE "${SDKDIR_PATH}/BasicX/${PATH_FLAG_PLATFORM}/include" CACHE PATH "basicx include dir." FORCE)
SET (SDKDIR_BASICX_LIBRARY_DEBUG "${SDKDIR_PATH}/BasicX/${PATH_FLAG_PLATFORM}/${PATH_FLAG_BINORLIB}/${PATH_FLAG_LINKTYPE}/debug" CACHE PATH "basicx library debug dir." FORCE)
SET (SDKDIR_BASICX_LIBRARY_RELEASE "${SDKDIR_PATH}/BasicX/${PATH_FLAG_PLATFORM}/${PATH_FLAG_BINORLIB}/${PATH_FLAG_LINKTYPE}/release" CACHE PATH "basicx library release dir." FORCE)
SET (SDKDIR_ZLIB_INCLUDE "${SDKDIR_PATH}/ZLib/${PATH_FLAG_PLATFORM}/include" CACHE PATH "zlib include dir." FORCE)
SET (SDKDIR_ZLIB_LIBRARY_DEBUG "${SDKDIR_PATH}/ZLib/${PATH_FLAG_PLATFORM}/${PATH_FLAG_BINORLIB}/${PATH_FLAG_LINKTYPE}/debug" CACHE PATH "zlib library debug dir." FORCE)
SET (SDKDIR_ZLIB_LIBRARY_RELEASE "${SDKDIR_PATH}/ZLib/${PATH_FLAG_PLATFORM}/${PATH_FLAG_BINORLIB}/${PATH_FLAG_LINKTYPE}/release" CACHE PATH "zlib library release dir." FORCE)
SET (SDKDIR_INTERFACE "${SDKDIR_PATH}/Interface/${PATH_FLAG_PLATFORM}" CACHE PATH "interface dir." FORCE)

SET (QUOTEX_VERSION_MAJOR 0)
SET (QUOTEX_VERSION_MINOR 1)
SET (QUOTEX_VERSION_PATCH 0)

# SET (CMAKE_C_COMPILER "D:/VS 2017/VC/bin/x86_amd64")
# SET (CMAKE_CXX_COMPILER "D:/VS 2017/VC/bin/x86_amd64")

INCLUDE (CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG ("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG ("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
IF (COMPILER_SUPPORTS_CXX11)
    IF (CMAKE_COMPILER_IS_GNUCXX)
        SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    ELSE ()
        SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    ENDIF ()
ELSEIF (COMPILER_SUPPORTS_CXX0X)
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
ELSE ()
    # MSVC, ON by default, if available
ENDIF ()

MESSAGE (STATUS ${CMAKE_SYSTEM_NAME} " " ${CMAKE_SYSTEM_VERSION} " " ${CMAKE_SYSTEM_PROCESSOR})

IF (CMAKE_COMPILE_RELEASE)
    SET (CMAKE_CONFIGURATION_TYPES "Release;" CACHE STRING "project type." FORCE) # Debug、Release、RelWithDebInfo、MinSizeRel
ELSE ()
    SET (CMAKE_CONFIGURATION_TYPES "Debug;" CACHE STRING "project type." FORCE) # Debug、Release、RelWithDebInfo、MinSizeRel
ENDIF ()

IF (WIN32)
    SET (CMAKE_INSTALL_PREFIX "C:/Users/xrd/Desktop/quotex" CACHE PATH "install prefix." FORCE)
	IF (CMAKE_PLATFORM_X64)
    	SET (CMAKE_BOOST_ROOT "${SDKDIR_PATH}/Boost/1.65/x64" CACHE PATH "boost root." FORCE)
	ELSE ()
    	SET (CMAKE_BOOST_ROOT "${SDKDIR_PATH}/Boost/1.65/x86" CACHE PATH "boost root." FORCE)
	ENDIF ()
ELSEIF ((UNIX) OR (APPLE))
    SET (CMAKE_INSTALL_PREFIX "/usr/local/quotex" CACHE PATH "install prefix." FORCE)
	IF (CMAKE_PLATFORM_X64)
    	SET (CMAKE_BOOST_ROOT "/usr/local/boost/1.65/x64" CACHE PATH "boost root." FORCE)
	ELSE ()
    	SET (CMAKE_BOOST_ROOT "/usr/local/boost/1.65/x86" CACHE PATH "boost root." FORCE)
	ENDIF ()
ENDIF ()
SET (CMAKE_BOOST_VERSION "1.65.1" CACHE STRING "boost version." FORCE)
SET (BOOST_ROOT ${CMAKE_BOOST_ROOT})

IF (MSVC)
    # https://msdn.microsoft.com/en-us/library/fwkeyyhe.aspx
    IF (CMAKE_LINKTYPE_SHARED)
        SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd") # /MDd、/MTd
    ELSE ()
        SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd") # /MDd、/MTd
	ENDIF ()
	SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Ob0") # /Ob0、Ob1、Ob2
	SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Gm") # /Gm、/Gm-
	SET (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D \"_DEBUG\"") #
    IF (CMAKE_LINKTYPE_SHARED)
        SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD") # /MD、/MT
    ELSE ()
        SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT") # /MD、/MT
	ENDIF ()
	SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2") # /Od、/O1、/O2、/Ox
	SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2") # /Ob0、/Ob1、/Ob2
	SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi") # /Oi
	SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ot") # /Os、/Ot
	SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oy-") # /Oy、/Oy-
	SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL") # /GL
	SET (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Gy") # /Gy、/Gy-
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:wchar_t") # /Zc:wchar_t、/Zc:wchar_t-
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3") # /W0、/W1、/W2、/W3、/W4、/Wall
    SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi") # /Z7、/Zi、/ZI
	SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /sdl") # /sdl、/sdl-
	SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D \"_CONSOLE\" /D \"_LIB\" /D \"_UNICODE\" /D \"UNICODE\"") #
	# SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:wchar_t /W3")
	# SET_TARGET_PROPERTIES (${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/Zc:wchar_t /W3")
	# ADD_DEFINITIONS ("/Zc:wchar_t /W3")
	SET (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF")
	SET (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:ICF")
	SET (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
	SET (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCLUDE:\"__tcmalloc\"")
ELSE ()
	# TODO:
ENDIF ()

# SEND_ERROR、STATUS、FATAL_ERROR
MESSAGE (STATUS "project binary dir: " ${PROJECT_BINARY_DIR})
MESSAGE (STATUS "project source dir: " ${PROJECT_SOURCE_DIR})

CONFIGURE_FILE (
    "${PROJECT_SOURCE_DIR}/config.h.in"
	"${PROJECT_BINARY_DIR}/config.h"
    )

# SET (Boost_DEBUG 1)
SET (Boost_COMPILER "-vc141")
IF (CMAKE_PLATFORM_X64)
	# SET (BOOST_ROOT "${SDKDIR_PATH}/Boost/1.65/x64") # 1
	# SET (BOOST_INCLUDEDIR "${SDKDIR_PATH}/Boost/1.65/x64") # 1
	# SET (BOOST_LIBRARYDIR "${SDKDIR_PATH}/Boost/1.65/x64/stage/lib") # 1
ELSE ()
	# SET (BOOST_ROOT "${SDKDIR_PATH}/Boost/1.65/x86") # 1
	# SET (BOOST_INCLUDEDIR "${SDKDIR_PATH}/Boost/1.65/x86") # 1
	# SET (BOOST_LIBRARYDIR "${SDKDIR_PATH}/Boost/1.65/x86/stage/lib") # 1
ENDIF ()
SET (Boost_USE_STATIC_LIBS ON) # 2
SET (Boost_USE_MULTITHREADED ON) # 2
SET (Boost_USE_STATIC_RUNTIME OFF) # 2
FIND_PACKAGE (Boost ${CMAKE_BOOST_VERSION} REQUIRED system random) # 3
MESSAGE (STATUS "boost include path: " ${Boost_INCLUDE_DIRS})
MESSAGE (STATUS "boost library path: " ${Boost_LIBRARY_DIRS})
INCLUDE_DIRECTORIES (${Boost_INCLUDE_DIRS})
LINK_DIRECTORIES (${Boost_LIBRARY_DIRS})

FILE (MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/archive" "${PROJECT_BINARY_DIR}/library" "${PROJECT_BINARY_DIR}/runtime")
SET (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/archive")
SET (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/library")
SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime")

FILE (COPY "${SDKDIR_BASICX_LIBRARY_RELEASE}/libtcmalloc.lib" DESTINATION ${PROJECT_BINARY_DIR})

FILE (GLOB QUOTEX_HEADER_FILES "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/*.h")
FILE (GLOB QUOTEX_SOURCE_FILES "${PROJECT_SOURCE_DIR}/${PROJECT_NAME}/*.cpp")
SOURCE_GROUP ("Header Files" FILES ${QUOTEX_HEADER_FILES})
SOURCE_GROUP ("Source Files" FILES ${QUOTEX_SOURCE_FILES})

ADD_SUBDIRECTORY ("${PROJECT_SOURCE_DIR}/global")
ADD_SUBDIRECTORY ("${PROJECT_SOURCE_DIR}/shares")
ADD_SUBDIRECTORY ("${PROJECT_SOURCE_DIR}/pluginx")

INCLUDE_DIRECTORIES ("${PROJECT_SOURCE_DIR}" "${SDKDIR_BASICX_INCLUDE}")

LINK_DIRECTORIES ("${PROJECT_BINARY_DIR}/archive")
IF (MSVC)
    SET (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /LIBPATH:\"${SDKDIR_BASICX_LIBRARY_DEBUG}\"")
    SET (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LIBPATH:\"${SDKDIR_BASICX_LIBRARY_RELEASE}\"")
ELSE ()
    LINK_DIRECTORIES (${PROJECT_BINARY_DIR}/archive) # need check
    LINK_DIRECTORIES (${PROJECT_BINARY_DIR}/archive) # need check
ENDIF ()

ADD_EXECUTABLE (${PROJECT_NAME} ${QUOTEX_HEADER_FILES} ${QUOTEX_SOURCE_FILES})

IF (WIN32)
    TARGET_LINK_LIBRARIES (${PROJECT_NAME} libtcmalloc.lib global.lib shares.lib common.lib syslog.lib syscfg.lib sysrtm.lib sysdbi_s.lib plugins.lib)
ELSEIF (UNIX)
    TARGET_LINK_LIBRARIES (${PROJECT_NAME} libtcmalloc.a global.a shares.a common.a syslog.a syscfg.a sysrtm.a sysdbi_s.a plugins.a)
ELSEIF (APPLE)
    TARGET_LINK_LIBRARIES (${PROJECT_NAME} libtcmalloc.a global.a shares.a common.a syslog.a syscfg.a sysrtm.a sysdbi_s.a plugins.a)
ENDIF ()

IF (WIN32)
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/configs")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/extdlls")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ctp")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ltb")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ltp")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_tdf")
	FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/doc")
	FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/etc")
	FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib")
    INSTALL (
	    TARGETS ${PROJECT_NAME} 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
	)
    INSTALL (
	    PROGRAMS "${SDKDIR_BASICX_LIBRARY_RELEASE}/libtcmalloc.dll" 
                 "${SDKDIR_ZLIB_LIBRARY_RELEASE}/zlibwapi.dll" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
	)
    INSTALL (
	    FILES "${SDKDIR_BASICX_LIBRARY_RELEASE}/syslog.dll" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/syscfg.dll" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/sysrtm.dll" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/sysdbi_s.dll" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/network.dll" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/plugins.dll" 
		      "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/global.dll" 
		      "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/shares.dll" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
	)
    INSTALL (
	    FILES "${PROJECT_SOURCE_DIR}/libinfo.txt" 
		      "${PROJECT_SOURCE_DIR}/quotex/logo.ico" 
		      "${SDKDIR_INTERFACE}/CTP_F/thostmduserapi.dll" 
		      "${SDKDIR_INTERFACE}/CTP_F/thosttraderapi.dll" 
		      "${SDKDIR_INTERFACE}/LTS/L2mduserapi.dll" 
		      "${SDKDIR_INTERFACE}/LTS/securityqueryapi.dll" 
		      "${SDKDIR_INTERFACE}/TDF/CCECore.dll" 
		      "${SDKDIR_INTERFACE}/TDF/TDFAPI.dll" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/extdlls"
	)
    INSTALL (
	    FILES "${PROJECT_SOURCE_DIR}/../changes.md" 
	          "${PROJECT_SOURCE_DIR}/../library.txt" 
	          "${PROJECT_SOURCE_DIR}/../license.txt" 
	          "${PROJECT_SOURCE_DIR}/../license_bsd.txt" 
	          "${PROJECT_SOURCE_DIR}/../license_lgpl_chs.txt" 
	          "${PROJECT_SOURCE_DIR}/../license_lgpl_eng.txt" 
	          "${PROJECT_SOURCE_DIR}/../readme.md" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/doc"
	)
    INSTALL (
	    FILES "${PROJECT_SOURCE_DIR}/../etc/cfg_basic.ini" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/configs"
	)
    INSTALL (
		FILES "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/quoter_ctp.dll" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ctp/quoter_ctp.ini" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ctp/quoter_ctp.xml" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ctp"
	)
    #INSTALL (
	#	FILES "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/quoter_ltb.dll" 
	#		  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ltb/quoter_ltb.ini" 
	#		  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ltb/quoter_ltb.xml" 
	#    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ltb"
	#)
    #INSTALL (
	#	FILES "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/quoter_ltp.dll" 
	#		  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ltp/quoter_ltp.ini" 
	#		  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ltp/quoter_ltp.xml" 
	#    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ltp"
	#)
    #INSTALL (
	#	FILES "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/quoter_tdf.dll" 
	#		  "${PROJECT_SOURCE_DIR}/pluginx/quoter_tdf/quoter_tdf.ini" 
	#		  "${PROJECT_SOURCE_DIR}/pluginx/quoter_tdf/quoter_tdf.xml" 
	#    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_tdf"
	#)
ELSEIF ((UNIX) OR (APPLE))
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/configs")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/extdlls")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ctp")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ltb")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ltp")
    FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_tdf")
	FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/doc")
	FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/etc")
	FILE (MAKE_DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib")
    INSTALL (
	    TARGETS ${PROJECT_NAME} 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
	)
    INSTALL (
	    PROGRAMS "${SDKDIR_BASICX_LIBRARY_RELEASE}/libtcmalloc.so" 
                 "${SDKDIR_ZLIB_LIBRARY_RELEASE}/zlibwapi.so" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" 
		# PERMISSIONS OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE # 755
	)
    INSTALL (
	    FILES "${SDKDIR_BASICX_LIBRARY_RELEASE}/syslog.so" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/syscfg.so" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/sysrtm.so" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/sysdbi_s.so" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/network.so" 
		      "${SDKDIR_BASICX_LIBRARY_RELEASE}/plugins.so" 
		      "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/global.so" 
		      "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/shares.so" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
	    # PERMISSIONS OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE # 755
	)
    INSTALL (
	    FILES "${PROJECT_SOURCE_DIR}/libinfo.txt" 
		      "${PROJECT_SOURCE_DIR}/quotex/logo.ico" 
		      "${SDKDIR_INTERFACE}/CTP_F/thostmduserapi.so" 
		      "${SDKDIR_INTERFACE}/CTP_F/thosttraderapi.so" 
		      "${SDKDIR_INTERFACE}/LTS/L2mduserapi.so" 
		      "${SDKDIR_INTERFACE}/LTS/securityqueryapi.so" 
		      "${SDKDIR_INTERFACE}/TDF/CCECore.so" 
		      "${SDKDIR_INTERFACE}/TDF/TDFAPI.so" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/extdlls"
		# PERMISSIONS OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE # 755
	)
    INSTALL (
	    FILES "${PROJECT_SOURCE_DIR}/../changes.md" 
	          "${PROJECT_SOURCE_DIR}/../library.txt" 
	          "${PROJECT_SOURCE_DIR}/../license.txt" 
	          "${PROJECT_SOURCE_DIR}/../license_bsd.txt" 
	          "${PROJECT_SOURCE_DIR}/../license_lgpl_chs.txt" 
	          "${PROJECT_SOURCE_DIR}/../license_lgpl_eng.txt" 
	          "${PROJECT_SOURCE_DIR}/../readme.md" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/doc" 
		# PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ # 644
	)
    INSTALL (
	    FILES "${PROJECT_SOURCE_DIR}/../etc/cfg_basic.ini" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/configs" 
		# PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ # 644
	)
    INSTALL (
		FILES "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/quoter_ctp.so" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ctp/quoter_ctp.ini" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ctp/quoter_ctp.xml" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ctp" 
		# PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ # 644
	)
    INSTALL (
		FILES "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/quoter_ltb.so" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ltb/quoter_ltb.ini" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ltb/quoter_ltb.xml" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ltb" 
		# PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ # 644
	)
    INSTALL (
		FILES "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/quoter_ltp.so" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ltp/quoter_ltp.ini" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_ltp/quoter_ltp.xml" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_ltp" 
		# PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ # 644
	)
    INSTALL (
		FILES "${PROJECT_BINARY_DIR}/runtime/${PATH_FLAG_COMPILE}/quoter_tdf.so" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_tdf/quoter_tdf.ini" 
			  "${PROJECT_SOURCE_DIR}/pluginx/quoter_tdf/quoter_tdf.xml" 
	    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/plugins/quoter_tdf" 
		# PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ # 644
	)
ENDIF ()
