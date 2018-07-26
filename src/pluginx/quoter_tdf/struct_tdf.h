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

#ifndef QUOTER_TDF_STRUCT_TDF_H
#define QUOTER_TDF_STRUCT_TDF_H

#include <map>
#include <list>
#include <vector>
#include <atomic>

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

#include <network/server.h>

#include "define_tdf.h"

#include "TDFApiHelper.h"

typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> service_ptr;

#define SNAPSHOT_STOCK_TDF_V1
#define SNAPSHOT_INDEX_TDF_V1
#define TRANSACTION_TDF_V1
#define ORDER_TDF_V1

#ifdef SNAPSHOT_STOCK_TDF_V1
    #define SNAPSHOT_STOCK_TDF_VERSION '1'
#endif
#ifdef SNAPSHOT_STOCK_TDF_V2
    #define SNAPSHOT_STOCK_TDF_VERSION '2'
#endif

#ifdef SNAPSHOT_INDEX_TDF_V1
    #define SNAPSHOT_INDEX_TDF_VERSION '1'
#endif
#ifdef SNAPSHOT_INDEX_TDF_V2
    #define SNAPSHOT_INDEX_TDF_VERSION '2'
#endif

#ifdef TRANSACTION_TDF_V1
    #define TRANSACTION_TDF_VERSION '1'
#endif
#ifdef TRANSACTION_TDF_V2
    #define TRANSACTION_TDF_VERSION '2'
#endif

#ifdef ORDER_TDF_V1
    #define ORDER_TDF_VERSION '1'
#endif
#ifdef ORDER_TDF_V2
    #define ORDER_TDF_VERSION '2'
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
	std::string m_strUsername;
	std::string m_strPassword;
	int32_t m_need_index;
	int32_t m_need_trans;
	int32_t m_need_order;
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

struct MarketInfo
{
	int32_t m_item_number;
	int32_t m_can_hold_item_number;
	std::string m_market;
};

struct SecurityInfo
{
	std::string m_market;
	TDFDefine_SecurityCode m_security_code;
};

#ifdef SNAPSHOT_STOCK_TDF_V1
struct SnapshotStock_TDF // 所有变量均会被赋值
{
	char m_code[8]; // 证券代码
	char m_name[24]; // 证券名称
	char m_type[6]; // 证券类型
	char m_market[6]; // 证券市场
	char m_status[2]; // 证券状态
	uint32_t m_last; // 最新价 // 10000
	uint32_t m_open; // 开盘价 // 10000
	uint32_t m_high; // 最高价 // 10000
	uint32_t m_low; // 最低价 // 10000
	uint32_t m_close; // 收盘价 // 10000
	uint32_t m_pre_close; // 昨收价 // 10000
	int64_t m_volume; // 成交量
	int64_t m_turnover; // 成交额 // 10000
	uint32_t m_ask_price[10]; // 申卖价 // 10000
	int32_t m_ask_volume[10]; // 申卖量
	uint32_t m_bid_price[10]; // 申买价 // 10000
	int32_t m_bid_volume[10]; // 申买量
	uint32_t m_high_limit; // 涨停价 // 10000
	uint32_t m_low_limit; // 跌停价 // 10000
	int64_t m_total_bid_vol; // 总买量
	int64_t m_total_ask_vol; // 总卖量
	uint32_t m_weighted_avg_bid_price; // 加权平均委买价格 // 10000
	uint32_t m_weighted_avg_ask_price; // 加权平均委卖价格 // 10000
	int32_t m_trade_count; // 成交笔数
	int32_t m_iopv; // IOPV净值估值 // 10000
	int32_t m_yield_rate; // 到期收益率 // 10000
	int32_t m_pe_rate_1; // 市盈率 1 // 10000
	int32_t m_pe_rate_2; // 市盈率 2 // 10000
	int32_t m_units; // 计价单位
	int32_t	m_quote_time; // 行情时间 // HHMMSSmmm 精度：毫秒
	int32_t	m_local_time; // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_local_index; // 本地序号
	// 本日编号 nIdnum
	// 证券信息前缀 chPrefix[4]
	// 升跌2 nSD2
};
#endif

#ifdef SNAPSHOT_STOCK_TDF_V2
struct SnapshotStock_TDF // 所有变量均会被赋值
{
};
#endif

#ifdef SNAPSHOT_INDEX_TDF_V1
struct SnapshotIndex_TDF // 所有变量均会被赋值
{
	char m_code[8]; // 指数代码
	char m_name[24]; // 指数名称
	char m_type[6]; // 指数类型
	char m_market[6]; // 指数市场
	char m_status[2]; // 指数状态
	int32_t m_last; // 最新指数 // 10000
	int32_t m_open; // 开盘指数 // 10000
	int32_t m_high; // 最高指数 // 10000
	int32_t m_low; // 最低指数 // 10000
	int32_t m_close; // 收盘指数 // 10000
	int32_t m_pre_close; // 昨收指数 // 10000
	int64_t	m_volume; // 成交量
	int64_t m_turnover; // 成交额 // 10000
	int32_t	m_quote_time; // 行情时间 // HHMMSSmmm 精度：毫秒
	int32_t	m_local_time; // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_local_index; // 本地序号
	// 本日编号 nIdnum
};
#endif

#ifdef SNAPSHOT_INDEX_TDF_V2
struct SnapshotIndex_TDF // 所有变量均会被赋值
{
};
#endif

#ifdef TRANSACTION_TDF_V1
struct Transaction_TDF // 所有变量均会被赋值
{
	char m_code[8]; // 证券代码
	char m_name[24]; // 证券名称
	char m_type[6]; // 证券类型
	char m_market[6]; // 证券市场
	int32_t m_index; // 成交编号
	uint32_t m_price; // 成交价 // 10000
	int32_t m_volume; // 成交量
	int64_t m_turnover; // 成交额 // 10000
	int32_t	m_trans_time; // 成交时间 // HHMMSSmmm 精度：毫秒
	int32_t	m_local_time; // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_local_index; // 本地序号
};
#endif

#ifdef TRANSACTION_TDF_V2
struct Transaction_TDF // 所有变量均会被赋值
{
};
#endif

#ifdef ORDER_TDF_V1
struct Order_TDF // 所有变量均会被赋值
{
	char m_code[8]; // 证券代码
	char m_name[24]; // 证券名称
	char m_type[6]; // 证券类型
	char m_market[6]; // 证券市场
	int32_t m_index; // 委托编号
	uint32_t m_price; // 委托价格 // 10000
	int32_t m_volume; // 委托数量
	char m_order_kind[2]; // 委托类别
	char m_bs_flag[2]; // 委托买卖类型('B','S','C')
	int32_t	m_order_time; // 委托时间 // HHMMSSmmm 精度：毫秒
	int32_t	m_local_time; // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_local_index; // 本地序号
	// 保留 chResv[2]
};
#endif

#ifdef ORDER_TDF_V2
struct Order_TDF // 所有变量均会被赋值
{
};
#endif

#pragma pack( pop )

#endif // QUOTER_TDF_STRUCT_TDF_H
