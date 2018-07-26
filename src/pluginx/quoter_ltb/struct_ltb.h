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

#ifndef QUOTER_LTB_STRUCT_LTB_H
#define QUOTER_LTB_STRUCT_LTB_H

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

#include "define_ltb.h"

typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> service_ptr;

#define SNAPSHOT_STOCK_LTB_V1
#define SNAPSHOT_INDEX_LTB_V1
#define TRANSACTION_LTB_V1

#ifdef SNAPSHOT_STOCK_LTB_V1
    #define SNAPSHOT_STOCK_LTB_VERSION '1'
#endif
#ifdef SNAPSHOT_STOCK_LTB_V2
    #define SNAPSHOT_STOCK_LTB_VERSION '2'
#endif

#ifdef SNAPSHOT_INDEX_LTB_V1
    #define SNAPSHOT_INDEX_LTB_VERSION '1'
#endif
#ifdef SNAPSHOT_INDEX_LTB_V2
    #define SNAPSHOT_INDEX_LTB_VERSION '2'
#endif

#ifdef TRANSACTION_LTB_V1
    #define TRANSACTION_LTB_VERSION '1'
#endif
#ifdef TRANSACTION_LTB_V2
    #define TRANSACTION_LTB_VERSION '2'
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

struct Contract // 保证均被赋值
{
	std::string m_code;
	std::string m_type;
	std::string m_market;

	Contract( std::string code, std::string type, std::string market )
		: m_code( code )
		, m_type( type )
		, m_market( market ) {
	}
};

#ifdef SNAPSHOT_STOCK_LTB_V1
struct SnapshotStock_LTB // 所有变量均会被赋值
{
	char m_code[9]; // 证券代码
	char m_name[24]; // 证券名称 // 无
	char m_type[6]; // 证券类型 // 配置指定，否则就为空
	char m_market[6]; // 证券市场 // SSE，SZE
	char m_status[2]; // 证券状态 // 'N'
	uint32_t m_last; // 最新价 // 10000
	uint32_t m_open; // 开盘价 // 10000
	uint32_t m_high; // 最高价 // 10000
	uint32_t m_low; // 最低价 // 10000
	uint32_t m_close; // 收盘价 // 10000
	uint32_t m_pre_close; // 昨收价 // 10000 // 无
	int64_t m_volume; // 成交量
	int64_t m_turnover; // 成交额 // 10000
	uint32_t m_ask_price[10]; // 申卖价 // 10000
	int32_t m_ask_volume[10]; // 申卖量
	uint32_t m_bid_price[10]; // 申买价 // 10000
	int32_t m_bid_volume[10]; // 申买量
	uint32_t m_high_limit; // 涨停价 // 10000 // 无
	uint32_t m_low_limit; // 跌停价 // 10000 // 无
	int64_t m_open_interest; // 持仓量
	int32_t m_iopv; // 基金净值 // 10000
	int32_t m_trade_count; // 成交笔数
	uint32_t m_yield_to_maturity; // 到期收益率 // 10000
	uint32_t m_auction_price; // 动态参考价格 // 10000
	int32_t m_bid_price_level; // 买价深度
	int32_t m_offer_price_level; // 卖价深度
	int32_t m_total_bid_volume; // 申买总量
	int32_t m_total_offer_volume; // 申卖总量
	uint32_t m_weighted_avg_bid_price; // 申买加权均价 // 10000
	uint32_t m_weighted_avg_offer_price; // 申卖加权均价 // 10000
	uint32_t m_alt_weighted_avg_bid_price; // 债券申买加权均价 // 10000
	uint32_t m_alt_weighted_avg_offer_price; // 债券申卖加权均价 // 10000
	char m_trading_phase[2]; // 交易阶段
	char m_open_restriction[2]; // 开仓限制
	int32_t	m_quote_time; // 行情时间 // HHMMSSmmm 精度：毫秒
	int32_t	m_local_time; // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_local_index; //本地序号
	// double BidCount1; // 申买笔数一
	// double OfferCount1; // 申卖笔数一
	// double BidCount2; // 申买笔数二
	// double OfferCount2; // 申卖笔数二
	// double BidCount3; // 申买笔数三
	// double OfferCount3; // 申卖笔数三
	// double BidCount4; // 申买笔数四
	// double OfferCount4; // 申卖笔数四
	// double BidCount5; // 申买笔数五
	// double OfferCount5; // 申卖笔数五
	// double BidCount6; // 申买笔数六
	// double OfferCount6; // 申卖笔数六
	// double BidCount7; // 申买笔数七
	// double OfferCount7; // 申卖笔数七
	// double BidCount8; // 申买笔数八
	// double OfferCount8; // 申卖笔数八
	// double BidCount9; // 申买笔数九
	// double OfferCount9; // 申卖笔数九
	// double BidCountA; // 申买笔数十
	// double OfferCountA; // 申卖笔数十
};
#endif

#ifdef SNAPSHOT_STOCK_LTB_V2
struct SnapshotStock_LTB // 所有变量均会被赋值
{
};
#endif

#ifdef SNAPSHOT_INDEX_LTB_V1
struct SnapshotIndex_LTB // 所有变量均会被赋值
{
	char m_code[9]; // 指数代码
	char m_name[24]; // 指数名称 // 无
	char m_type[6]; // 指数类型 // 配置指定，否则就为空
	char m_market[6]; // 指数市场 // SSE，SZE
	char m_status[2]; // 指数状态 // 'N'
	int32_t m_last; // 最新指数 // 10000
	int32_t m_open; // 开盘指数 // 10000
	int32_t m_high; // 最高指数 // 10000
	int32_t m_low; // 最低指数 // 10000
	int32_t m_close; // 收盘指数 // 10000
	int32_t m_pre_close; // 昨收指数 // 10000 // 无
	int64_t	m_volume; // 成交量
	int64_t m_turnover; // 成交额 // 10000
	int32_t	m_quote_time; // 行情时间 // HHMMSSmmm 精度：毫秒
	int32_t	m_local_time; // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_local_index; // 本地序号
};
#endif

#ifdef SNAPSHOT_INDEX_LTB_V2
struct SnapshotIndex_LTB // 所有变量均会被赋值
{
};
#endif

#ifdef TRANSACTION_LTB_V1
struct Transaction_LTB // 所有变量均会被赋值
{
	char m_code[9]; // 证券代码
	char m_name[24]; // 证券名称 // 无
	char m_type[6]; // 证券类型 // 配置指定，否则就为空
	char m_market[6]; // 证券市场 // SSE，SZE
	int32_t m_index; // 成交编号
	uint32_t m_price; // 成交价 // 10000
	int32_t m_volume; // 成交量
	int64_t m_turnover; // 成交额 // 10000
	int32_t m_trade_group_id; // 成交组
	int32_t m_buy_index; // 买方委托序号
	int32_t m_sell_index; // 卖方委托序号
	char m_order_kind[2]; // 报单类型
	char m_function_code[2]; // 功能码
	int32_t	m_trans_time; // 成交时间 // HHMMSSmmm 精度：毫秒
	int32_t	m_local_time; // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_local_index; // 本地序号
};
#endif

#ifdef TRANSACTION_LTB_V2
struct Transaction_LTB // 所有变量均会被赋值
{
};
#endif

struct CL2FAST_MD // 接口定义
{
	double  LastPrice;                // 最新价
	double  OpenPrice;                // 开盘价
	double  ClosePrice;               // 收盘价
	double  HighPrice;                // 最高价
	double  LowPrice;                 // 最低价
	double  TotalTradeVolume;         // 成交数量
	double  TotalTradeValue;          // 成交金额
	double  TradeCount;               // 成交笔数
	double  OpenInterest;             // 持仓量
	double  IOPV;                     // 基金净值
	double  YieldToMaturity;          // 到期收益率
	double  AuctionPrice;             // 动态参考价格
	double  TotalBidVolume;           // 申买总量
	double  WeightedAvgBidPrice;      // 申买加权均价
	double  AltWeightedAvgBidPrice;   // 债券申买加权均价
	double  TotalOfferVolume;         // 申卖总量
	double  WeightedAvgOfferPrice;    // 申卖加权均价
	double  AltWeightedAvgOfferPrice; // 债券申卖加权均价
	int32_t BidPriceLevel;            // 买价深度
	int32_t OfferPriceLevel;          // 卖价深度
	double  BidPrice1;                // 申买价一
	double  BidVolume1;               // 申买量一
	double  BidCount1;                // 申买笔数一
	double  OfferPrice1;              // 申卖价一
	double  OfferVolume1;             // 申卖量一
	double  OfferCount1;              // 申卖笔数一
	double  BidPrice2;                // 申买价二
	double  BidVolume2;               // 申买量二
	double  BidCount2;                // 申买笔数二
	double  OfferPrice2;              // 申卖价二
	double  OfferVolume2;             // 申卖量二
	double  OfferCount2;              // 申卖笔数二
	double  BidPrice3;                // 申买价三
	double  BidVolume3;               // 申买量三
	double  BidCount3;                // 申买笔数三
	double  OfferPrice3;              // 申卖价三
	double  OfferVolume3;             // 申卖量三
	double  OfferCount3;              // 申卖笔数三
	double  BidPrice4;                // 申买价四
	double  BidVolume4;               // 申买量四
	double  BidCount4;                // 申买笔数四
	double  OfferPrice4;              // 申卖价四
	double  OfferVolume4;             // 申卖量四
	double  OfferCount4;              // 申卖笔数四
	double  BidPrice5;                // 申买价五
	double  BidVolume5;               // 申买量五
	double  BidCount5;                // 申买笔数五
	double  OfferPrice5;              // 申卖价五
	double  OfferVolume5;             // 申卖量五
	double  OfferCount5;              // 申卖笔数五
	double  BidPrice6;                // 申买价六
	double  BidVolume6;               // 申买量六
	double  BidCount6;                // 申买笔数六
	double  OfferPrice6;              // 申卖价六
	double  OfferVolume6;             // 申卖量六
	double  OfferCount6;              // 申卖笔数六
	double  BidPrice7;                // 申买价七
	double  BidVolume7;               // 申买量七
	double  BidCount7;                // 申买笔数七
	double  OfferPrice7;              // 申卖价七
	double  OfferVolume7;             // 申卖量七
	double  OfferCount7;              // 申卖笔数七
	double  BidPrice8;                // 申买价八
	double  BidVolume8;               // 申买量八
	double  BidCount8;                // 申买笔数八
	double  OfferPrice8;              // 申卖价八
	double  OfferVolume8;             // 申卖量八
	double  OfferCount8;              // 申卖笔数八
	double  BidPrice9;                // 申买价九
	double  BidVolume9;               // 申买量九
	double  BidCount9;                // 申买笔数九
	double  OfferPrice9;              // 申卖价九
	double  OfferVolume9;             // 申卖量九
	double  OfferCount9;              // 申卖笔数九
	double  BidPriceA;                // 申买价十
	double  BidVolumeA;               // 申买量十
	double  BidCountA;                // 申买笔数十
	double  OfferPriceA;              // 申卖价十
	double  OfferVolumeA;             // 申卖量十
	double  OfferCountA;              // 申卖笔数十
	char    ExchangeID[4];            // 交易所代码
	char    InstrumentID[9];          // 证券代码
	char    TimeStamp[13];            // 时间戳
	char    TradingPhase;             // 交易阶段
	char    OpenRestriction;          // 开仓限制
};

struct CL2FAST_INDEX // 接口定义
{
	double  LastIndex;                // 最新指数
	double  OpenIndex;                // 开盘指数
	double  CloseIndex;               // 收盘指数
	double  HighIndex;                // 最高指数
	double  LowIndex;                 // 最低指数
	double  TurnOver;                 // 成交金额
	double  TotalVolume;              // 成交数量
	char    ExchangeID[4];            // 交易所代码
	char    InstrumentID[9];          // 指数代码
	char    TimeStamp[13];            // 时间戳
};

struct CL2FAST_ORDER // 接口定义
{
	double  Price;                    // 委托价格
	double  Volume;                   // 委托数量
	int32_t OrderGroupID;             // 委托组
	int32_t OrderIndex;               // 委托序号
	char    OrderKind;                // 报单类型
	char    FunctionCode;             // 功能码
	char    ExchangeID[4];            // 交易所代码
	char    InstrumentID[9];          // 合约代码
	char    OrderTime[13];            // 时间戳
};

struct CL2FAST_TRADE // 接口定义
{
	double  Price;                    // 成交价格
	double  Volume;                   // 成交数量
	int32_t TradeGroupID;             // 成交组
	int32_t TradeIndex;               // 成交序号
	int32_t BuyIndex;                 // 买方委托序号
	int32_t SellIndex;                // 卖方委托序号
	char    OrderKind;                // 报单类型
	char    FunctionCode;             // 功能码
	char    ExchangeID[4];            // 交易所代码
	char    InstrumentID[9];          // 合约代码
	char    TradeTime[13];            // 时间戳
};

#pragma pack( pop )

#endif // QUOTER_LTB_STRUCT_LTB_H
