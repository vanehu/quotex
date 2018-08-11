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

#ifndef QUOTER_SZB_STRUCT_SZB_H
#define QUOTER_SZB_STRUCT_SZB_H

#include <map>
#include <list>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#define ZLIB_WINAPI // 必须，否则报：无法解析的外部符号

#include "zlib.h"

#include <common/assist.h>
#include <network/server.h>

#include "define_szb.h"

typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> service_ptr;

#define SNAPSHOT_STOCK_SZB_V1

#ifdef SNAPSHOT_STOCK_SZB_V1
    #define SNAPSHOT_STOCK_SZB_VERSION '1'
#endif
#ifdef SNAPSHOT_STOCK_SZB_V2
    #define SNAPSHOT_STOCK_SZB_VERSION '2'
#endif

#pragma pack( push )
#pragma pack( 1 )

struct TaskItem // 保证均被赋值
{
	int32_t m_task_id;
	int32_t m_identity;
	int32_t m_code;
	std::string m_data;
};

struct Request // 保证均被赋值
{
	int32_t m_task_id;
	int32_t m_identity;
	int32_t m_code;
	Json::Value m_req_json;
	// 添加其他序列化对象
};

struct ConSubOne
{
	boost::mutex m_lock_con_sub_one; // 不能拷贝
	std::map<int32_t, basicx::ConnectInfo*> m_map_identity; // 不会产生重复而推送多次
};

struct ConSubMan
{
	boost::mutex m_lock_con_sub_all;
	std::map<int32_t, basicx::ConnectInfo*> m_map_con_sub_all;
	boost::mutex m_lock_con_sub_one;
	std::map<std::string, ConSubOne*> m_map_con_sub_one;
	boost::mutex m_lock_one_sub_con_del;
	std::list<ConSubOne*> m_list_one_sub_con_del;
};

struct Config // 保证均被赋值
{
	std::string m_address;
	int32_t m_port;
	std::string m_username;
	std::string m_password;
	int32_t m_timeout;
	int32_t m_need_dump;
	std::string m_dump_path;
	int32_t m_data_compress;
	int32_t m_data_encode;
	int32_t m_dump_time;
	int32_t m_init_time;
};

template<typename T>
class QuoteCache
{
public:
	QuoteCache()
		: m_recv_num( 0 )
		, m_dump_num( 0 )
		, m_comp_num( 0 )
		, m_send_num( 0 )
		, m_local_index( 0 )
		, m_folder_path( "" )
		, m_file_path( "" ) {
		m_vec_cache_put = &m_vec_cache_01;
		m_vec_cache_out = &m_vec_cache_02;
	}

public:
	std::atomic<uint32_t> m_recv_num;
	std::atomic<uint32_t> m_dump_num;
	std::atomic<uint32_t> m_comp_num;
	std::atomic<uint32_t> m_send_num;
	std::atomic<uint32_t> m_local_index;
	std::string m_folder_path;
	std::string m_file_path;
	boost::mutex m_lock_cache;
	std::vector<T> m_vec_cache_01;
	std::vector<T> m_vec_cache_02;
	std::vector<T>* m_vec_cache_put;
	std::vector<T>* m_vec_cache_out;
};

#ifdef SNAPSHOT_STOCK_SZB_V1
struct SnapshotStock_SZB // 所有变量均会被赋值
{
	char m_MDStreamID[6];       // 行情数据类型           C5     // MD401、MD404、MD405
	char m_SecurityID[6];       // 证券代码               C5     // MD401、MD404、MD405
	char m_Symbol[33];          // 中文证券简称           C32    // MD401、MD404、MD405
	char m_SymbolEn[17];        // 英文证券简称           C15/16 // MD401、MD404、MD405

	int64_t m_TradeVolume;      // 成交数量               N16    // MD401
	int64_t m_TotalValueTraded; // 成交金额               N16(3) // MD401 // 10000
	uint32_t m_PreClosePx;      // 昨日收盘价             N11(3) // MD401 // 10000
	uint32_t m_NominalPrice;    // 按盘价                 N11(3) // MD401 // 10000
	uint32_t m_HighPrice;       // 最高价                 N11(3) // MD401 // 10000
	uint32_t m_LowPrice;        // 最低价                 N11(3) // MD401 // 10000
	uint32_t m_TradePrice;      // 最新价                 N11(3) // MD401 // 10000
	uint32_t m_BuyPrice1;       // 申买价一               N11(3) // MD401 // 10000
	int32_t m_BuyVolume1;       // 申买量一               N12    // MD401
	uint32_t m_SellPrice1;      // 申卖价一               N11(3) // MD401 // 10000
	int32_t m_SellVolume1;      // 申卖量一               N12    // MD401
	int32_t m_SecTradingStatus; // 证券交易状态           C8     // MD401

	int32_t m_VCMStartTime;     // 市调机制开始时间       C8     // MD404 // HHMMSS000 精度：秒
	int32_t m_VCMEndTime;       // 市调机制结束时间       C8     // MD404 // HHMMSS000 精度：秒
	uint32_t m_VCMRefPrice;     // 市调机制参考价         N11(3) // MD404 // 10000
	uint32_t m_VCMLowerPrice;   // 市调机制下限价         N11(3) // MD404 // 10000
	uint32_t m_VCMUpperPrice;   // 市调机制上限价         N11(3) // MD404 // 10000

	uint32_t m_CASRefPrice;     // 收盘集合竞价时段参考价 N11(3) // MD405 // 10000
	uint32_t m_CASLowerPrice;   // 收盘集合竞价时段下限价 N11(3) // MD405 // 10000
	uint32_t m_CASUpperPrice;   // 收盘集合竞价时段上限价 N11(3) // MD405 // 10000
	char m_OrdImbDirection[2];  // 不能配对买卖盘方向     C1     // MD405
	int32_t m_OrdImbQty;        // 不能配对买卖盘量       N12    // MD405

	int32_t m_QuoteTime;        // 行情时间 // HHMMSS000 精度：秒
	int32_t m_LocalTime;        // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_LocalIndex;      // 本地序号
};
#endif

#ifdef SNAPSHOT_STOCK_SZB_V2
struct SnapshotStock_SZB // 所有变量均会被赋值
{
};
#endif

#pragma pack( pop )

#endif // QUOTER_SZB_STRUCT_SZB_H
