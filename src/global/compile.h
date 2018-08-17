/*
* Copyright (c) 2018-2018 the QuoteX authors
* All rights reserved.
*
* The project sponsor and lead author is Xu Rendong.
* E-mail: xrd@ustc.edu, QQ: 277195007, WeChat: ustc_xrd
* See the contributors file for names of other contributors.
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

#ifndef QUOTEX_GLOBAL_COMPILE_H
#define QUOTEX_GLOBAL_COMPILE_H

//---------- 基础组件 ----------//

#define QUOTEX_GLOBAL_EXP
//#define QUOTEX_GLOBAL_IMP

#define QUOTEX_SHARES_EXP
//#define QUOTEX_SHARES_IMP

//---------- 各类插件 ----------//

#define QUOTER_CTP_EXP
//#define QUOTER_CTP_IMP

#define QUOTER_HGT_EXP
//#define QUOTER_HGT_IMP

#define QUOTER_LTB_EXP
//#define QUOTER_LTB_IMP

#define QUOTER_LTP_EXP
//#define QUOTER_LTP_IMP

#define QUOTER_SGT_EXP
//#define QUOTER_SGT_IMP

#define QUOTER_TDF_EXP
//#define QUOTER_TDF_IMP

//---------- 设置结束 ----------//

#ifdef QUOTEX_GLOBAL_EXP
    #define QUOTEX_GLOBAL_EXPIMP __declspec(dllexport)
#endif

#ifdef QUOTEX_GLOBAL_IMP
    #define QUOTEX_GLOBAL_EXPIMP __declspec(dllimport)
#endif

//------------------------------//

#ifdef QUOTEX_SHARES_EXP
    #define QUOTEX_SHARES_EXPIMP __declspec(dllexport)
#endif

#ifdef QUOTEX_SHARES_IMP
    #define QUOTEX_SHARES_EXPIMP __declspec(dllimport)
#endif

//------------------------------//

#ifdef QUOTER_CTP_EXP
    #define QUOTER_CTP_EXPIMP __declspec(dllexport)
#endif

#ifdef QUOTER_CTP_IMP
    #define QUOTER_CTP_EXPIMP __declspec(dllimport)
#endif

//------------------------------//

#ifdef QUOTER_HGT_EXP
    #define QUOTER_HGT_EXPIMP __declspec(dllexport)
#endif

#ifdef QUOTER_HGT_IMP
    #define QUOTER_HGT_EXPIMP __declspec(dllimport)
#endif

//------------------------------//

#ifdef QUOTER_LTB_EXP
    #define QUOTER_LTB_EXPIMP __declspec(dllexport)
#endif

#ifdef QUOTER_LTB_IMP
    #define QUOTER_LTB_EXPIMP __declspec(dllimport)
#endif

//------------------------------//

#ifdef QUOTER_LTP_EXP
    #define QUOTER_LTP_EXPIMP __declspec(dllexport)
#endif

#ifdef QUOTER_LTP_IMP
    #define QUOTER_LTP_EXPIMP __declspec(dllimport)
#endif

//------------------------------//

#ifdef QUOTER_SGT_EXP
#define QUOTER_SGT_EXPIMP __declspec(dllexport)
#endif

#ifdef QUOTER_SGT_IMP
#define QUOTER_SGT_EXPIMP __declspec(dllimport)
#endif

//------------------------------//

#ifdef QUOTER_TDF_EXP
    #define QUOTER_TDF_EXPIMP __declspec(dllexport)
#endif

#ifdef QUOTER_TDF_IMP
    #define QUOTER_TDF_EXPIMP __declspec(dllimport)
#endif

//------------------------------//

#endif // QUOTEX_GLOBAL_COMPILE_H
