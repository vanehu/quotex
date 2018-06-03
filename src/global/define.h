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
#define DEF_APP_VERSION "V0.1.0-Beta Build 20180601" // 系统版本号
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

// 网络数据包类型定义
#define NW_MSG_ATOM_TYPE_MIN 0 // 网络通信 元操作 类型起始
#define NW_MSG_TYPE_HEART_CHECK  NW_MSG_ATOM_TYPE_MIN + 0 // 连接心跳检测消息
#define NW_MSG_ATOM_TYPE_MAX 7 // 网络通信 元操作 类型终止

#define NW_MSG_USER_TYPE_MIN 8 // 网络通信 自定义 类型起始
#define NW_MSG_TYPE_USER_DATA    NW_MSG_USER_TYPE_MIN + 0 // 用户数据处理消息
#define NW_MSG_USER_TYPE_MAX 15 // 网络通信 自定义 类型终止

// 网络数据包编码定义
#define NW_MSG_CODE_TYPE_MIN 0 // 网络通信 编码 类型起始
#define NW_MSG_CODE_NONE         NW_MSG_CODE_TYPE_MIN + 0 // 适用于元操作
#define NW_MSG_CODE_STRING       NW_MSG_CODE_TYPE_MIN + 1 // 直接字符串
#define NW_MSG_CODE_JSON         NW_MSG_CODE_TYPE_MIN + 2 // Json格式
#define NW_MSG_CODE_BASE64       NW_MSG_CODE_TYPE_MIN + 3 // Base64格式
#define NW_MSG_CODE_PROTOBUF     NW_MSG_CODE_TYPE_MIN + 4 // ProtoBuf格式
#define NW_MSG_CODE_ZLIB         NW_MSG_CODE_TYPE_MIN + 5 // ZLib格式
#define NW_MSG_CODE_MSGPACK      NW_MSG_CODE_TYPE_MIN + 6 // MsgPack格式
#define NW_MSG_CODE_TYPE_MAX 15 // 网络通信 编码 类型终止

// 行情功能编号定义
#define TD_FUNC_QUOTE_ADDSUB                       100001 // 订阅行情
#define TD_FUNC_QUOTE_DELSUB                       100002 // 退订行情
////////////////////////////////////////////////////////////////////////////
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_TDF      910001 // TDF个股快照
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_INDEX_TDF      910002 // TDF指数快照
#define TD_FUNC_QUOTE_DATA_TRANSACTION_TDF         910003 // TDF逐笔成交
#define TD_FUNC_QUOTE_DATA_ORDER_TDF               910004 // TDF逐笔委托
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_LTB      910006 // LTB个股快照(Level2广播)
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_INDEX_LTB      910007 // LTB指数快照(Level2广播)
#define TD_FUNC_QUOTE_DATA_TRANSACTION_LTB         910008 // LTB逐笔成交(Level2广播)
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_LTP      910011 // LTP个股快照(Level2推送)
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_INDEX_LTP      910012 // LTP指数快照(Level2推送)
#define TD_FUNC_QUOTE_DATA_TRANSACTION_LTP         910013 // LTP逐笔成交(Level2推送)
#define TD_FUNC_QUOTE_DATA_MARKET_FUTURE_NP        920001 // 期货内盘市场快照
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_TDF_HEAD "91000101" // V1
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_INDEX_TDF_HEAD "91000201" // V1
#define TD_FUNC_QUOTE_DATA_TRANSACTION_TDF_HEAD    "91000301" // V1
#define TD_FUNC_QUOTE_DATA_ORDER_TDF_HEAD          "91000401" // V1
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_LTB_HEAD "91000601" // V1
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_INDEX_LTB_HEAD "91000701" // V1
#define TD_FUNC_QUOTE_DATA_TRANSACTION_LTB_HEAD    "91000801" // V1
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_LTP_HEAD "91001101" // V1
#define TD_FUNC_QUOTE_DATA_SNAPSHOT_INDEX_LTP_HEAD "91001201" // V1
#define TD_FUNC_QUOTE_DATA_TRANSACTION_LTP_HEAD    "91001301" // V1
#define TD_FUNC_QUOTE_DATA_MARKET_FUTURE_NP_HEAD   "92000101" // V1

#endif // QUOTEX_GLOBAL_DEFINE_H
