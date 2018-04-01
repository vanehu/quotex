/*
* Copyright (c) 2018-2018 the QuoteX authors
* All rights reserved.
*
* The project sponsor and lead author is Xu Rendong.
* E-mail: xrd@ustc.edu, QQ: 277195007, WeChat: ustc_xrd
* You can get more information at https://xurendong.github.io
* For names of other contributors see the contributors file.
*
* Commercial use of this code in source and binary forms is
* governed by a LGPL v3 license. You may get a copy from the
* root directory. Or else you should get a specific written
* permission from the project author.
*
* Individual and educational use of this code in source and
* binary forms is governed by a 3-clause BSD license. You may
* get a copy from the root directory. Certainly welcome you
* to contribute code of all sorts.
*
* Be sure to retain the above copyright notice and conditions.
*/

#ifndef QUOTEX_GLOBAL_DEFINE_H
#define QUOTEX_GLOBAL_DEFINE_H

// 软件信息
#define DEF_APP_NAME "QuoteX" // 系统英文名称
#define DEF_APP_NAME_CN "证 券 行 情 服 务 器" // 系统中文名称
#define DEF_APP_VERSION "V0.1.0-Beta Build 20180401" // 系统版本号
#define DEF_APP_DEVELOPER "Developed by the X-Lab." // 开发者声明
#define DEF_APP_COMPANY "X-Lab (Shanghai) Co., Ltd." // 公司声明
#define DEF_APP_COPYRIGHT "Copyright 2018-2018 X-Lab All Rights Reserved." // 版权声明
#define DEF_APP_HOMEURL "http://www.xlab.com" // 主页链接

#define TRAY_POP_TITLE L"系统消息：" // 托盘气球型提示标题
#define TRAY_POP_START L"QuoteX" // 托盘气球型提示启动

// 配置定义
#define CFG_MAX_WORD_LEN 64 // 字串最大字符数
#define CFG_MAX_PATH_LEN 256 // 路径最大字符数

#define NON_WARNING( n ) __pragma( warning( push ) ) __pragma( warning( disable : n ) )
#define USE_WARNING( n ) __pragma( warning( default : n ) ) __pragma( warning( pop ) )

#endif // QUOTEX_GLOBAL_DEFINE_H
