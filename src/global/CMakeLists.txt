
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

INCLUDE_DIRECTORIES ("${PROJECT_SOURCE_DIR}")

FILE (GLOB GLOBAL_HEADER_FILES "./*.h")
FILE (GLOB GLOBAL_SOURCE_FILES "./*.cpp")

SOURCE_GROUP ("Header Files" FILES ${GLOBAL_HEADER_FILES})
SOURCE_GROUP ("Source Files" FILES ${GLOBAL_SOURCE_FILES})

IF (CMAKE_LINKTYPE_SHARED)
    ADD_LIBRARY ("global" SHARED ${GLOBAL_HEADER_FILES} ${GLOBAL_SOURCE_FILES})
ELSE ()
    ADD_LIBRARY ("global" ${GLOBAL_HEADER_FILES} ${GLOBAL_SOURCE_FILES})
ENDIF ()
