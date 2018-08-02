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

#ifndef QUOTER_HGT_STRUCT_HGT_H
#define QUOTER_HGT_STRUCT_HGT_H

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

#include "define_hgt.h"

typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> service_ptr;

#define SNAPSHOT_STOCK_HGT_V1

#ifdef SNAPSHOT_STOCK_HGT_V1
    #define SNAPSHOT_STOCK_HGT_VERSION '1'
#endif
#ifdef SNAPSHOT_STOCK_HGT_V2
    #define SNAPSHOT_STOCK_HGT_VERSION '2'
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
	std::string m_market_data_folder;
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

#ifdef SNAPSHOT_STOCK_HGT_V1
struct SnapshotStock_HGT // 所有变量均会被赋值
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

	//int32_t m_Timestamp;        // 行情时间               C12    // MD401、MD404、MD405 // HHMMSS000 精度：秒

	int32_t m_QuoteTime;        // 行情时间 // HHMMSS000 精度：秒
	int32_t m_LocalTime;        // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_LocalIndex;      // 本地序号
};
#endif

#ifdef SNAPSHOT_STOCK_HGT_V2
struct SnapshotStock_HGT // 所有变量均会被赋值
{
};
#endif

struct Define_Item
{
	size_t m_len;
	int32_t m_pos;
	char* m_txt;

	Define_Item()
		: m_len( 0 )
		, m_pos( 0 )
		, m_txt( nullptr ) {
	}

	~Define_Item() {
		if( m_txt != nullptr ) {
			delete[] m_txt;
		}
	}

	void Txt( size_t size ) {
		try {
			m_txt = new char[size];
			memset( m_txt, 0, size );
		}
		catch( ... ) {
		}
	}
};

struct Define_Item_W
{
	size_t m_len;
	int32_t m_pos;
	char16_t* m_txt; //

	Define_Item_W()
		: m_len( 0 )
		, m_pos( 0 )
		, m_txt( nullptr ) {
	}

	~Define_Item_W() {
		if( m_txt != nullptr ) {
			delete[] m_txt;
		}
	}

	void Txt( size_t size ) {
		try {
			m_txt = new char16_t[size];
			memset( m_txt, 0, size * 2 ); // UTF-16 LE
		}
		catch( ... ) {
		}
	}
};

struct Define_Head
{
	Define_Item m_item_01;
	Define_Item m_item_02;
	Define_Item m_item_03;
	Define_Item m_item_04;
	Define_Item m_item_05;
	Define_Item m_item_06;
	Define_Item m_item_07;
	Define_Item m_item_08;
	Define_Item m_item_09;
	int32_t m_pos_end;

	int32_t m_line_size;
	char* m_line_buffer;

	std::string m_BeginString;
	std::string m_Version;
	int32_t m_BodyLength;
	int32_t m_TotNumTradeReports;
	int32_t m_MDReportID;
	std::string m_SenderCompID;
	std::string m_MDTime;
	int32_t m_MDUpdateType;
	int32_t m_MktStatus;

	int32_t m_md_year;
	int32_t m_md_month;
	int32_t m_md_day;
	int32_t m_md_hour;
	int32_t m_md_minute;
	int32_t m_md_second;
	int64_t m_market_data_time;

	Define_Head() {
		m_item_01.m_len = 6;
		m_item_02.m_len = 8;
		m_item_03.m_len = 10;
		m_item_04.m_len = 5;
		m_item_05.m_len = 8;
		m_item_06.m_len = 6;
		m_item_07.m_len = 21;
		m_item_08.m_len = 1;
		m_item_09.m_len = 8;
		// 位置计算 + 1 是分隔符 '|' 占位
		m_item_01.m_pos = 0;                                     // 起始标识符   C6
		m_item_02.m_pos = m_item_01.m_pos + m_item_01.m_len + 1; // 版本         C8
		m_item_03.m_pos = m_item_02.m_pos + m_item_02.m_len + 1; // 数据长度     N10
		m_item_04.m_pos = m_item_03.m_pos + m_item_03.m_len + 1; // 文件体记录数 N5
		m_item_05.m_pos = m_item_04.m_pos + m_item_04.m_len + 1; // 行情序号     N8
		m_item_06.m_pos = m_item_05.m_pos + m_item_05.m_len + 1; // 发送方       C6
		m_item_07.m_pos = m_item_06.m_pos + m_item_06.m_len + 1; // 行情文件时间 C21
		m_item_08.m_pos = m_item_07.m_pos + m_item_07.m_len + 1; // 发送方式     N1
		m_item_09.m_pos = m_item_08.m_pos + m_item_08.m_len + 1; // 市场状态     C8
		m_pos_end = m_item_09.m_pos + m_item_09.m_len + 1;       // 换行符
		m_item_01.Txt( m_item_01.m_len + 1 );
		m_item_02.Txt( m_item_02.m_len + 1 );
		m_item_03.Txt( m_item_03.m_len + 1 );
		m_item_04.Txt( m_item_04.m_len + 1 );
		m_item_05.Txt( m_item_05.m_len + 1 );
		m_item_06.Txt( m_item_06.m_len + 1 );
		m_item_07.Txt( m_item_07.m_len + 1 );
		m_item_08.Txt( m_item_08.m_len + 1 );
		m_item_09.Txt( m_item_09.m_len + 1 );
		m_line_size = m_pos_end;
		m_line_buffer = nullptr;
		try {
			m_line_buffer = new char[m_line_size];
			memset( m_line_buffer, 0, m_line_size );
		}
		catch( ... ) {
		}
		m_BeginString = "";
		m_Version = "";
		m_BodyLength = 0;
		m_TotNumTradeReports = 0;
		m_MDReportID = 0;
		m_SenderCompID = "";
		m_MDTime = "";
		m_MDUpdateType = 0;
		m_MktStatus = 0;
		m_md_year = 0;
		m_md_month = 0;
		m_md_day = 0;
		m_md_hour = 0;
		m_md_minute = 0;
		m_md_second = 0;
		m_market_data_time = 0;
	}

	~Define_Head() {
		if( m_line_buffer != nullptr ) {
			delete[] m_line_buffer;
		}
	}

	void FillData( FILE* market_data_file ) {
		if( m_line_buffer != nullptr ) {
			try { // 防 m_txt 空
				fread( m_line_buffer, m_line_size, 1, market_data_file );
				// 这里 memcpy 的都是定长 txt 最后一位已被 memset 为 0 且不变
				memcpy( m_item_01.m_txt, &m_line_buffer[m_item_01.m_pos], m_item_01.m_len );
				memcpy( m_item_02.m_txt, &m_line_buffer[m_item_02.m_pos], m_item_02.m_len );
				memcpy( m_item_03.m_txt, &m_line_buffer[m_item_03.m_pos], m_item_03.m_len );
				memcpy( m_item_04.m_txt, &m_line_buffer[m_item_04.m_pos], m_item_04.m_len );
				memcpy( m_item_05.m_txt, &m_line_buffer[m_item_05.m_pos], m_item_05.m_len );
				memcpy( m_item_06.m_txt, &m_line_buffer[m_item_06.m_pos], m_item_06.m_len );
				memcpy( m_item_07.m_txt, &m_line_buffer[m_item_07.m_pos], m_item_07.m_len );
				memcpy( m_item_08.m_txt, &m_line_buffer[m_item_08.m_pos], m_item_08.m_len );
				memcpy( m_item_09.m_txt, &m_line_buffer[m_item_09.m_pos], m_item_09.m_len );
				m_BeginString = m_item_01.m_txt;
				m_Version = basicx::StringTrim( m_item_02.m_txt, " " );
				m_BodyLength = atoi( m_item_03.m_txt );
				m_TotNumTradeReports = atoi( m_item_04.m_txt );
				m_MDReportID = atoi( m_item_05.m_txt );
				m_SenderCompID = basicx::StringTrim( m_item_06.m_txt, " " );
				m_MDTime = m_item_07.m_txt;
				m_MDUpdateType = atoi( m_item_08.m_txt );
				m_MktStatus = atoi( m_item_09.m_txt ); // std::string -> int32_t
				if( m_MDTime != "" ) { // YYYYMMDD-HH:MM:SS.000
					m_md_year = atoi( m_MDTime.substr( 0, 4 ).c_str() );
					m_md_month = atoi( m_MDTime.substr( 4, 2 ).c_str() );
					m_md_day = atoi( m_MDTime.substr( 6, 2 ).c_str() );
					m_md_hour = atoi( m_MDTime.substr( 9, 2 ).c_str() );
					m_md_minute = atoi( m_MDTime.substr( 12, 2 ).c_str() );
					m_md_second = atoi( m_MDTime.substr( 15, 2 ).c_str() );
					m_market_data_time = m_md_year * 10000000000 + m_md_month * 100000000 + m_md_day * 1000000 + m_md_hour * 10000 + m_md_minute * 100 + m_md_second;
				}
			}
			catch( ... ) {
			}
		}
	}

	void Print() {
		std::cout << m_BeginString << "|";
		std::cout << m_Version << "|";
		std::cout << m_BodyLength << "|";
		std::cout << m_TotNumTradeReports << "|";
		std::cout << m_MDReportID << "|";
		std::cout << m_SenderCompID << "|";
		std::cout << m_MDTime << "|";
		std::cout << m_MDUpdateType << "|";
		std::cout << m_MktStatus << "|";
		std::cout << "<" << m_md_year << "-" << m_md_month << "-" << m_md_day << " " << m_md_hour << ":" << m_md_minute << ":" << m_md_second << ">" << std::endl;
	}
};

struct Define_Type
{
	int32_t m_line_size;
	char* m_line_buffer;

	std::string m_MDStreamID;

	Define_Type() {
		m_line_size = 5; // 行情数据类型 C5
		m_line_buffer = nullptr;
		try {
			m_line_buffer = new char[m_line_size + 1];
			memset( m_line_buffer, 0, m_line_size + 1 );
		}
		catch( ... ) {
		}
		m_MDStreamID = "";
	}

	~Define_Type() {
		if( m_line_buffer != nullptr ) {
			delete[] m_line_buffer;
		}
	}

	void FillData( FILE* market_data_file ) {
		if( m_line_buffer != nullptr ) {
			// 这里 fread 的是定长 m_line_buffer 最后一位已被 memset 为 0 且不变
			fread( m_line_buffer, m_line_size, 1, market_data_file );
			m_MDStreamID = m_line_buffer;
		}
	}

	void Print() {
		std::cout << m_MDStreamID << "|";
	}
};

struct Define_MD401
{
	Define_Item m_item_01;
	Define_Item m_item_02;
	Define_Item_W m_item_03; //
	Define_Item m_item_04;
	Define_Item m_item_05;
	Define_Item m_item_06;
	Define_Item m_item_07;
	Define_Item m_item_08;
	Define_Item m_item_09;
	Define_Item m_item_10;
	Define_Item m_item_11;
	Define_Item m_item_12;
	Define_Item m_item_13;
	Define_Item m_item_14;
	Define_Item m_item_15;
	Define_Item m_item_16;
	Define_Item m_item_17;
	int32_t m_pos_end;

	int32_t m_line_size;
	char* m_line_buffer;

	Define_MD401() {
		m_item_01.m_len = 5;
		m_item_02.m_len = 5;
		m_item_03.m_len = 32;
		m_item_04.m_len = 15;
		m_item_05.m_len = 16;
		m_item_06.m_len = 16;
		m_item_07.m_len = 11;
		m_item_08.m_len = 11;
		m_item_09.m_len = 11;
		m_item_10.m_len = 11;
		m_item_11.m_len = 11;
		m_item_12.m_len = 11;
		m_item_13.m_len = 12;
		m_item_14.m_len = 11;
		m_item_15.m_len = 12;
		m_item_16.m_len = 8;
		m_item_17.m_len = 12;
		// 位置计算 + 1 是分隔符 '|' 占位
		m_item_01.m_pos = 0;                                     // 行情数据类型 C5
		m_item_02.m_pos = 1;                                     // 证券代码     C5 // 从 行情数据类型 字段后的 "|" 开始
		m_item_03.m_pos = m_item_02.m_pos + m_item_02.m_len + 1; // 中文证券简称 C32
		m_item_04.m_pos = m_item_03.m_pos + m_item_03.m_len + 1; // 英文证券简称 C15
		m_item_05.m_pos = m_item_04.m_pos + m_item_04.m_len + 1; // 成交数量     N16
		m_item_06.m_pos = m_item_05.m_pos + m_item_05.m_len + 1; // 成交金额     N16(3)
		m_item_07.m_pos = m_item_06.m_pos + m_item_06.m_len + 1; // 昨日收盘价   N11(3)
		m_item_08.m_pos = m_item_07.m_pos + m_item_07.m_len + 1; // 按盘价       N11(3)
		m_item_09.m_pos = m_item_08.m_pos + m_item_08.m_len + 1; // 最高价       N11(3)
		m_item_10.m_pos = m_item_09.m_pos + m_item_09.m_len + 1; // 最低价       N11(3)
		m_item_11.m_pos = m_item_10.m_pos + m_item_10.m_len + 1; // 最新价       N11(3)
		m_item_12.m_pos = m_item_11.m_pos + m_item_11.m_len + 1; // 申买价一     N11(3)
		m_item_13.m_pos = m_item_12.m_pos + m_item_12.m_len + 1; // 申买量一     N12
		m_item_14.m_pos = m_item_13.m_pos + m_item_13.m_len + 1; // 申卖价一     N11(3)
		m_item_15.m_pos = m_item_14.m_pos + m_item_14.m_len + 1; // 申卖量一     N12
		m_item_16.m_pos = m_item_15.m_pos + m_item_15.m_len + 1; // 证券交易状态 C8
		m_item_17.m_pos = m_item_16.m_pos + m_item_16.m_len + 1; // 行情时间     C12
		m_pos_end = m_item_17.m_pos + m_item_17.m_len + 1;       // 换行符
		m_item_01.Txt( m_item_01.m_len + 1 );
		m_item_02.Txt( m_item_02.m_len + 1 );
		m_item_03.Txt( m_item_03.m_len / 2 + 1 ); // UTF-16 LE
		m_item_04.Txt( m_item_04.m_len + 1 );
		m_item_05.Txt( m_item_05.m_len + 1 );
		m_item_06.Txt( m_item_06.m_len + 1 );
		m_item_07.Txt( m_item_07.m_len + 1 );
		m_item_08.Txt( m_item_08.m_len + 1 );
		m_item_09.Txt( m_item_09.m_len + 1 );
		m_item_10.Txt( m_item_10.m_len + 1 );
		m_item_11.Txt( m_item_11.m_len + 1 );
		m_item_12.Txt( m_item_12.m_len + 1 );
		m_item_13.Txt( m_item_13.m_len + 1 );
		m_item_14.Txt( m_item_14.m_len + 1 );
		m_item_15.Txt( m_item_15.m_len + 1 );
		m_item_16.Txt( m_item_16.m_len + 1 );
		m_item_17.Txt( m_item_17.m_len + 1 );
		m_line_size = m_pos_end;
		m_line_buffer = nullptr;
		try {
			m_line_buffer = new char[m_line_size];
			memset( m_line_buffer, 0, m_line_size );
		}
		catch( ... ) {
		}
	}

	~Define_MD401() {
		if( m_line_buffer != nullptr ) {
			delete[] m_line_buffer;
		}
	}

	void FillData( FILE* market_data_file ) {
		if( m_line_buffer != nullptr ) {
			try { // 防 m_txt 空
				fread( m_line_buffer, m_line_size, 1, market_data_file );
				// 这里 memcpy 的都是定长 txt 最后一位已被 memset 为 0 且不变
				// memcpy( m_item_01.m_txt, &m_line_buffer[m_item_01.m_pos], m_item_01.m_len );
				memcpy( m_item_02.m_txt, &m_line_buffer[m_item_02.m_pos], m_item_02.m_len );
				memcpy( m_item_03.m_txt, &m_line_buffer[m_item_03.m_pos], m_item_03.m_len );
				memcpy( m_item_04.m_txt, &m_line_buffer[m_item_04.m_pos], m_item_04.m_len );
				memcpy( m_item_05.m_txt, &m_line_buffer[m_item_05.m_pos], m_item_05.m_len );
				memcpy( m_item_06.m_txt, &m_line_buffer[m_item_06.m_pos], m_item_06.m_len );
				memcpy( m_item_07.m_txt, &m_line_buffer[m_item_07.m_pos], m_item_07.m_len );
				memcpy( m_item_08.m_txt, &m_line_buffer[m_item_08.m_pos], m_item_08.m_len );
				memcpy( m_item_09.m_txt, &m_line_buffer[m_item_09.m_pos], m_item_09.m_len );
				memcpy( m_item_10.m_txt, &m_line_buffer[m_item_10.m_pos], m_item_10.m_len );
				memcpy( m_item_11.m_txt, &m_line_buffer[m_item_11.m_pos], m_item_11.m_len );
				memcpy( m_item_12.m_txt, &m_line_buffer[m_item_12.m_pos], m_item_12.m_len );
				memcpy( m_item_13.m_txt, &m_line_buffer[m_item_13.m_pos], m_item_13.m_len );
				memcpy( m_item_14.m_txt, &m_line_buffer[m_item_14.m_pos], m_item_14.m_len );
				memcpy( m_item_15.m_txt, &m_line_buffer[m_item_15.m_pos], m_item_15.m_len );
				memcpy( m_item_16.m_txt, &m_line_buffer[m_item_16.m_pos], m_item_16.m_len );
				memcpy( m_item_17.m_txt, &m_line_buffer[m_item_17.m_pos], m_item_17.m_len );
			}
			catch( ... ) {
			}
		}
	}
};

struct Define_MD404
{
	Define_Item m_item_01;
	Define_Item m_item_02;
	Define_Item_W m_item_03; //
	Define_Item m_item_04;
	Define_Item m_item_05;
	Define_Item m_item_06;
	Define_Item m_item_07;
	Define_Item m_item_08;
	Define_Item m_item_09;
	Define_Item m_item_10;
	int32_t m_pos_end;

	int32_t m_line_size;
	char* m_line_buffer;

	Define_MD404() {
		m_item_01.m_len = 5;
		m_item_02.m_len = 5;
		m_item_03.m_len = 32;
		m_item_04.m_len = 16;
		m_item_05.m_len = 8;
		m_item_06.m_len = 8;
		m_item_07.m_len = 11;
		m_item_08.m_len = 11;
		m_item_09.m_len = 11;
		m_item_10.m_len = 12;
		// 位置计算 + 1 是分隔符 '|' 占位
		m_item_01.m_pos = 0;                                     // 行情数据类型     C5
		m_item_02.m_pos = 1;                                     // 证券代码         C5 // 从 行情数据类型 字段后的 "|" 开始
		m_item_03.m_pos = m_item_02.m_pos + m_item_02.m_len + 1; // 中文证券简称     C32
		m_item_04.m_pos = m_item_03.m_pos + m_item_03.m_len + 1; // 英文证券简称     C16
		m_item_05.m_pos = m_item_04.m_pos + m_item_04.m_len + 1; // 市调机制开始时间 C8
		m_item_06.m_pos = m_item_05.m_pos + m_item_05.m_len + 1; // 市调机制结束时间 C8
		m_item_07.m_pos = m_item_06.m_pos + m_item_06.m_len + 1; // 市调机制参考价   N11(3)
		m_item_08.m_pos = m_item_07.m_pos + m_item_07.m_len + 1; // 市调机制下限价   N11(3)
		m_item_09.m_pos = m_item_08.m_pos + m_item_08.m_len + 1; // 市调机制上限价   N11(3)
		m_item_10.m_pos = m_item_09.m_pos + m_item_09.m_len + 1; // 行情时间         C12
		m_pos_end = m_item_10.m_pos + m_item_10.m_len + 1;       // 换行符
		m_item_01.Txt( m_item_01.m_len + 1 );
		m_item_02.Txt( m_item_02.m_len + 1 );
		m_item_03.Txt( m_item_03.m_len / 2 + 1 ); // UTF-16 LE
		m_item_04.Txt( m_item_04.m_len + 1 );
		m_item_05.Txt( m_item_05.m_len + 1 );
		m_item_06.Txt( m_item_06.m_len + 1 );
		m_item_07.Txt( m_item_07.m_len + 1 );
		m_item_08.Txt( m_item_08.m_len + 1 );
		m_item_09.Txt( m_item_09.m_len + 1 );
		m_item_10.Txt( m_item_10.m_len + 1 );
		m_line_size = m_pos_end;
		m_line_buffer = nullptr;
		try {
			m_line_buffer = new char[m_line_size];
			memset( m_line_buffer, 0, m_line_size );
		}
		catch( ... ) {
		}
	}

	~Define_MD404() {
		if( m_line_buffer != nullptr ) {
			delete[] m_line_buffer;
		}
	}

	void FillData( FILE* market_data_file ) {
		if( m_line_buffer != nullptr ) {
			try { // 防 m_txt 空
				fread( m_line_buffer, m_line_size, 1, market_data_file );
				// 这里 memcpy 的都是定长 txt 最后一位已被 memset 为 0 且不变
				// memcpy( m_item_01.m_txt, &m_line_buffer[m_item_01.m_pos], m_item_01.m_len );
				memcpy( m_item_02.m_txt, &m_line_buffer[m_item_02.m_pos], m_item_02.m_len );
				memcpy( m_item_03.m_txt, &m_line_buffer[m_item_03.m_pos], m_item_03.m_len );
				memcpy( m_item_04.m_txt, &m_line_buffer[m_item_04.m_pos], m_item_04.m_len );
				memcpy( m_item_05.m_txt, &m_line_buffer[m_item_05.m_pos], m_item_05.m_len );
				memcpy( m_item_06.m_txt, &m_line_buffer[m_item_06.m_pos], m_item_06.m_len );
				memcpy( m_item_07.m_txt, &m_line_buffer[m_item_07.m_pos], m_item_07.m_len );
				memcpy( m_item_08.m_txt, &m_line_buffer[m_item_08.m_pos], m_item_08.m_len );
				memcpy( m_item_09.m_txt, &m_line_buffer[m_item_09.m_pos], m_item_09.m_len );
				memcpy( m_item_10.m_txt, &m_line_buffer[m_item_10.m_pos], m_item_10.m_len );
			}
			catch( ... ) {
			}
		}
	}
};

struct Define_MD405
{
	Define_Item m_item_01;
	Define_Item m_item_02;
	Define_Item_W m_item_03; //
	Define_Item m_item_04;
	Define_Item m_item_05;
	Define_Item m_item_06;
	Define_Item m_item_07;
	Define_Item m_item_08;
	Define_Item m_item_09;
	Define_Item m_item_10;
	int32_t m_pos_end;

	int32_t m_line_size;
	char* m_line_buffer;

	Define_MD405() {
		m_item_01.m_len = 5;
		m_item_02.m_len = 5;
		m_item_03.m_len = 32;
		m_item_04.m_len = 16;
		m_item_05.m_len = 11;
		m_item_06.m_len = 11;
		m_item_07.m_len = 11;
		m_item_08.m_len = 1;
		m_item_09.m_len = 12;
		m_item_10.m_len = 12;
		// 位置计算 + 1 是分隔符 '|' 占位
		m_item_01.m_pos = 0;                                     // 行情数据类型           C5
		m_item_02.m_pos = 1;                                     // 证券代码               C5 // 从 行情数据类型 字段后的 "|" 开始
		m_item_03.m_pos = m_item_02.m_pos + m_item_02.m_len + 1; // 中文证券简称           C32
		m_item_04.m_pos = m_item_03.m_pos + m_item_03.m_len + 1; // 英文证券简称           C16
		m_item_05.m_pos = m_item_04.m_pos + m_item_04.m_len + 1; // 收盘集合竞价时段参考价 N11(3)
		m_item_06.m_pos = m_item_05.m_pos + m_item_05.m_len + 1; // 收盘集合竞价时段下限价 N11(3)
		m_item_07.m_pos = m_item_06.m_pos + m_item_06.m_len + 1; // 收盘集合竞价时段上限价 N11(3)
		m_item_08.m_pos = m_item_07.m_pos + m_item_07.m_len + 1; // 不能配对买卖盘方向     C1
		m_item_09.m_pos = m_item_08.m_pos + m_item_08.m_len + 1; // 不能配对买卖盘量       N12
		m_item_10.m_pos = m_item_09.m_pos + m_item_09.m_len + 1; // 行情时间               C12
		m_pos_end = m_item_10.m_pos + m_item_10.m_len + 1;       // 换行符
		m_item_01.Txt( m_item_01.m_len + 1 );
		m_item_02.Txt( m_item_02.m_len + 1 );
		m_item_03.Txt( m_item_03.m_len / 2 + 1 ); // UTF-16 LE
		m_item_04.Txt( m_item_04.m_len + 1 );
		m_item_05.Txt( m_item_05.m_len + 1 );
		m_item_06.Txt( m_item_06.m_len + 1 );
		m_item_07.Txt( m_item_07.m_len + 1 );
		m_item_08.Txt( m_item_08.m_len + 1 );
		m_item_09.Txt( m_item_09.m_len + 1 );
		m_item_10.Txt( m_item_10.m_len + 1 );
		m_line_size = m_pos_end;
		m_line_buffer = nullptr;
		try {
			m_line_buffer = new char[m_line_size];
			memset( m_line_buffer, 0, m_line_size );
		}
		catch( ... ) {
		}
	}

	~Define_MD405() {
		if( m_line_buffer != nullptr ) {
			delete[] m_line_buffer;
		}
	}

	void FillData( FILE* market_data_file ) {
		if( m_line_buffer != nullptr ) {
			try { // 防 m_txt 空
				fread( m_line_buffer, m_line_size, 1, market_data_file );
				// 这里 memcpy 的都是定长 txt 最后一位已被 memset 为 0 且不变
				// memcpy( m_item_01.m_txt, &m_line_buffer[m_item_01.m_pos], m_item_01.m_len );
				memcpy( m_item_02.m_txt, &m_line_buffer[m_item_02.m_pos], m_item_02.m_len );
				memcpy( m_item_03.m_txt, &m_line_buffer[m_item_03.m_pos], m_item_03.m_len );
				memcpy( m_item_04.m_txt, &m_line_buffer[m_item_04.m_pos], m_item_04.m_len );
				memcpy( m_item_05.m_txt, &m_line_buffer[m_item_05.m_pos], m_item_05.m_len );
				memcpy( m_item_06.m_txt, &m_line_buffer[m_item_06.m_pos], m_item_06.m_len );
				memcpy( m_item_07.m_txt, &m_line_buffer[m_item_07.m_pos], m_item_07.m_len );
				memcpy( m_item_08.m_txt, &m_line_buffer[m_item_08.m_pos], m_item_08.m_len );
				memcpy( m_item_09.m_txt, &m_line_buffer[m_item_09.m_pos], m_item_09.m_len );
				memcpy( m_item_10.m_txt, &m_line_buffer[m_item_10.m_pos], m_item_10.m_len );
			}
			catch( ... ) {
			}
		}
	}
};

struct Define_Tail
{
	Define_Item m_item_01;
	Define_Item m_item_02;
	int32_t m_pos_end;

	int32_t m_line_size;
	char* m_line_buffer;

	std::string m_EndString;
	std::string m_CheckSum;

	Define_Tail() {
		m_item_01.m_len = 7;
		m_item_02.m_len = 3;
		// 位置计算 + 1 是分隔符 '|' 占位
		m_item_01.m_pos = 0;                                     // 结束标识符 C7
		m_item_02.m_pos = m_item_01.m_pos + m_item_01.m_len + 1; // 校验和     C3
		m_pos_end = m_item_02.m_pos + m_item_02.m_len + 1;       // 换行符
		m_item_01.Txt( m_item_01.m_len + 1 );
		m_item_02.Txt( m_item_02.m_len + 1 );
		m_line_size = m_pos_end;
		m_line_buffer = nullptr;
		try {
			m_line_buffer = new char[m_line_size];
			memset( m_line_buffer, 0, m_line_size );
		}
		catch( ... ) {
		}
		m_EndString = "";
		m_CheckSum = "";
	}

	~Define_Tail() {
		if( m_line_buffer != nullptr ) {
			delete[] m_line_buffer;
		}
	}

	void FillData( FILE* market_data_file ) {
		if( m_line_buffer != nullptr ) {
			try { // 防 m_txt 空
				fread( m_line_buffer, m_line_size, 1, market_data_file );
				// 这里 memcpy 的都是定长 txt 最后一位已被 memset 为 0 且不变
				memcpy( m_item_01.m_txt, &m_line_buffer[m_item_01.m_pos], m_item_01.m_len );
				memcpy( m_item_02.m_txt, &m_line_buffer[m_item_02.m_pos], m_item_02.m_len );
				m_EndString = m_item_01.m_txt;
				m_CheckSum = m_item_02.m_txt;
			}
			catch( ... ) {
			}
		}
	}

	void Print() {
		std::cout << m_EndString << "|";
		std::cout << m_CheckSum << "|" << std::endl;
	}
};

struct Result_MD401
{
	// std::string m_MDStreamID;
	std::string m_SecurityID;
	std::string m_Symbol;
	std::string m_SymbolEn;
	int64_t m_TradeVolume;
	double m_TotalValueTraded;
	double m_PreClosePx;
	double m_NominalPrice;
	double m_HighPrice;
	double m_LowPrice;
	double m_TradePrice;
	double m_BuyPrice1;
	int32_t m_BuyVolume1;
	double m_SellPrice1;
	int32_t m_SellVolume1;
	int32_t m_SecTradingStatus;
	std::string m_Timestamp;

	int32_t m_md_hour;
	int32_t m_md_minute;
	int32_t m_md_second;

	Result_MD401() {
		// m_MDStreamID = "";
		m_SecurityID = "";
		m_Symbol = "";
		m_SymbolEn = "";
		m_TradeVolume = 0;
		m_TotalValueTraded = 0.0;
		m_PreClosePx = 0.0;
		m_NominalPrice = 0.0;
		m_HighPrice = 0.0;
		m_LowPrice = 0.0;
		m_TradePrice = 0.0;
		m_BuyPrice1 = 0.0;
		m_BuyVolume1 = 0;
		m_SellPrice1 = 0.0;
		m_SellVolume1 = 0;
		m_SecTradingStatus = 0;
		m_Timestamp = "";

		m_md_hour = 0;
		m_md_minute = 0;
		m_md_second = 0;
	}

	void FillData( Define_MD401& define_md ) {
		try { // 防 m_txt 空
			  // m_MDStreamID = define_md.m_item_01.m_txt;
			m_SecurityID = define_md.m_item_02.m_txt;
			m_Symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( define_md.m_item_03.m_txt ) ), "　" ) ); // "　"
			m_SymbolEn = basicx::StringTrim( define_md.m_item_04.m_txt, " " );
			m_TradeVolume = atoll( define_md.m_item_05.m_txt );
			m_TotalValueTraded = atof( define_md.m_item_06.m_txt );
			m_PreClosePx = atof( define_md.m_item_07.m_txt );
			m_NominalPrice = atof( define_md.m_item_08.m_txt );
			m_HighPrice = atof( define_md.m_item_09.m_txt );
			m_LowPrice = atof( define_md.m_item_10.m_txt );
			m_TradePrice = atof( define_md.m_item_11.m_txt );
			m_BuyPrice1 = atof( define_md.m_item_12.m_txt );
			m_BuyVolume1 = atoi( define_md.m_item_13.m_txt );
			m_SellPrice1 = atof( define_md.m_item_14.m_txt );
			m_SellVolume1 = atoi( define_md.m_item_15.m_txt );
			m_SecTradingStatus = atoi( define_md.m_item_16.m_txt ); // std::string -> int32_t
			m_Timestamp = define_md.m_item_17.m_txt;
			if( m_Timestamp != "" ) { // HH:MM:SS.000
				m_md_hour = atoi( m_Timestamp.substr( 0, 2 ).c_str() );
				m_md_minute = atoi( m_Timestamp.substr( 3, 2 ).c_str() );
				m_md_second = atoi( m_Timestamp.substr( 6, 2 ).c_str() );
			}
		}
		catch( ... ) {
		}
	}

	void Print() {
		// std::cout << m_MDStreamID << "|";
		std::cout << m_SecurityID << "|";
		std::cout << basicx::StringToGB2312( m_Symbol ) << "|";
		std::cout << m_SymbolEn << "|";
		std::cout << m_TradeVolume << "|";
		std::cout << m_TotalValueTraded << "|";
		std::cout << m_PreClosePx << "|";
		std::cout << m_NominalPrice << "|";
		std::cout << m_HighPrice << "|";
		std::cout << m_LowPrice << "|";
		std::cout << m_TradePrice << "|";
		std::cout << m_BuyPrice1 << "|";
		std::cout << m_BuyVolume1 << "|";
		std::cout << m_SellPrice1 << "|";
		std::cout << m_SellVolume1 << "|";
		std::cout << m_SecTradingStatus << "|";
		std::cout << m_Timestamp << "|";
		std::cout << "<" << m_md_hour << ":" << m_md_minute << ":" << m_md_second << ">" << std::endl;
	}
};

struct Result_MD404
{
	// std::string m_MDStreamID;
	std::string m_SecurityID;
	std::string m_Symbol;
	std::string m_SymbolEn;
	std::string m_VCMStartTime;
	std::string m_VCMEndTime;
	double m_VCMRefPrice;
	double m_VCMLowerPrice;
	double m_VCMUpperPrice;
	std::string m_Timestamp;

	int32_t m_vcm_start_hour;
	int32_t m_vcm_start_minute;
	int32_t m_vcm_start_second;
	int32_t m_vcm_end_hour;
	int32_t m_vcm_end_minute;
	int32_t m_vcm_end_second;
	int32_t m_md_hour;
	int32_t m_md_minute;
	int32_t m_md_second;

	Result_MD404() {
		// m_MDStreamID = "";
		m_SecurityID = "";
		m_Symbol = "";
		m_SymbolEn = "";
		m_VCMStartTime = "";
		m_VCMEndTime = "";
		m_VCMRefPrice = 0.0;
		m_VCMLowerPrice = 0.0;
		m_VCMUpperPrice = 0.0;
		m_Timestamp = "";

		m_vcm_start_hour = 0;
		m_vcm_start_minute = 0;
		m_vcm_start_second = 0;
		m_vcm_end_hour = 0;
		m_vcm_end_minute = 0;
		m_vcm_end_second = 0;
		m_md_hour = 0;
		m_md_minute = 0;
		m_md_second = 0;
	}

	void FillData( Define_MD404& define_md ) {
		try { // 防 m_txt 空
			  // m_MDStreamID = define_md.m_item_01.m_txt;
			m_SecurityID = define_md.m_item_02.m_txt;
			m_Symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( define_md.m_item_03.m_txt ) ), "　" ) ); // "　"
			m_SymbolEn = basicx::StringTrim( define_md.m_item_04.m_txt, " " );
			m_VCMStartTime = define_md.m_item_05.m_txt;
			m_VCMEndTime = define_md.m_item_06.m_txt;
			m_VCMRefPrice = atof( define_md.m_item_07.m_txt );
			m_VCMLowerPrice = atof( define_md.m_item_08.m_txt );
			m_VCMUpperPrice = atof( define_md.m_item_09.m_txt );
			m_Timestamp = define_md.m_item_10.m_txt;
			if( m_VCMStartTime != "" ) { // HH:MM:SS
				m_vcm_start_hour = atoi( m_VCMStartTime.substr( 0, 2 ).c_str() );
				m_vcm_start_minute = atoi( m_VCMStartTime.substr( 3, 2 ).c_str() );
				m_vcm_start_second = atoi( m_VCMStartTime.substr( 6, 2 ).c_str() );
			}
			if( m_VCMEndTime != "" ) { // HH:MM:SS
				m_vcm_end_hour = atoi( m_VCMEndTime.substr( 0, 2 ).c_str() );
				m_vcm_end_minute = atoi( m_VCMEndTime.substr( 3, 2 ).c_str() );
				m_vcm_end_second = atoi( m_VCMEndTime.substr( 6, 2 ).c_str() );
			}
			if( m_Timestamp != "" ) { // HH:MM:SS.000
				m_md_hour = atoi( m_Timestamp.substr( 0, 2 ).c_str() );
				m_md_minute = atoi( m_Timestamp.substr( 3, 2 ).c_str() );
				m_md_second = atoi( m_Timestamp.substr( 6, 2 ).c_str() );
			}
		}
		catch( ... ) {
		}
	}

	void Print() {
		// std::cout << m_MDStreamID << "|";
		std::cout << m_SecurityID << "|";
		std::cout << basicx::StringToGB2312( m_Symbol ) << "|";
		std::cout << m_SymbolEn << "|";
		std::cout << m_VCMStartTime << "|";
		std::cout << m_VCMEndTime << "|";
		std::cout << m_VCMRefPrice << "|";
		std::cout << m_VCMLowerPrice << "|";
		std::cout << m_VCMUpperPrice << "|";
		std::cout << m_Timestamp << "|";
		std::cout << "<" << m_vcm_start_hour << ":" << m_vcm_start_minute << ":" << m_vcm_start_second << ">";
		std::cout << "<" << m_vcm_end_hour << ":" << m_vcm_end_minute << ":" << m_vcm_end_second << ">";
		std::cout << "<" << m_md_hour << ":" << m_md_minute << ":" << m_md_second << ">" << std::endl;
	}
};

struct Result_MD405
{
	// std::string m_MDStreamID;
	std::string m_SecurityID;
	std::string m_Symbol;
	std::string m_SymbolEn;
	double m_CASRefPrice;
	double m_CASLowerPrice;
	double m_CASUpperPrice;
	std::string m_OrdImbDirection;
	int32_t m_OrdImbQty;
	std::string m_Timestamp;

	int32_t m_md_hour;
	int32_t m_md_minute;
	int32_t m_md_second;

	Result_MD405() {
		// m_MDStreamID = "";
		m_SecurityID = "";
		m_Symbol = "";
		m_SymbolEn = "";
		m_CASRefPrice = 0.0;
		m_CASLowerPrice = 0.0;
		m_CASUpperPrice = 0.0;
		m_OrdImbDirection = "";
		m_OrdImbQty = 0;
		m_Timestamp = "";

		m_md_hour = 0;
		m_md_minute = 0;
		m_md_second = 0;
	}

	void FillData( Define_MD405& define_md ) {
		try { // 防 m_txt 空
			  // m_MDStreamID = define_md.m_item_01.m_txt;
			m_SecurityID = define_md.m_item_02.m_txt;
			m_Symbol = basicx::StringToUTF8( basicx::StringTrim( basicx::StringToGB2312( basicx::UTF16LE_To_UTF8( define_md.m_item_03.m_txt ) ), "　" ) ); // "　"
			m_SymbolEn = basicx::StringTrim( define_md.m_item_04.m_txt, " " );
			m_CASRefPrice = atof( define_md.m_item_05.m_txt );
			m_CASLowerPrice = atof( define_md.m_item_06.m_txt );
			m_CASUpperPrice = atof( define_md.m_item_07.m_txt );
			m_OrdImbDirection = basicx::StringTrim( define_md.m_item_08.m_txt, " " );
			m_OrdImbQty = atoi( define_md.m_item_09.m_txt );
			m_Timestamp = define_md.m_item_10.m_txt;
			if( m_Timestamp != "" ) { // HH:MM:SS.000
				m_md_hour = atoi( m_Timestamp.substr( 0, 2 ).c_str() );
				m_md_minute = atoi( m_Timestamp.substr( 3, 2 ).c_str() );
				m_md_second = atoi( m_Timestamp.substr( 6, 2 ).c_str() );
			}
		}
		catch( ... ) {
		}
	}

	void Print() {
		// std::cout << m_MDStreamID << "|";
		std::cout << m_SecurityID << "|";
		std::cout << basicx::StringToGB2312( m_Symbol ) << "|";
		std::cout << m_SymbolEn << "|";
		std::cout << m_CASRefPrice << "|";
		std::cout << m_CASLowerPrice << "|";
		std::cout << m_CASUpperPrice << "|";
		std::cout << m_OrdImbDirection << "|";
		std::cout << m_OrdImbQty << "|";
		std::cout << m_Timestamp << "|";
		std::cout << "<" << m_md_hour << ":" << m_md_minute << ":" << m_md_second << ">" << std::endl;
	}
};

#pragma pack( pop )

#endif // QUOTER_HGT_STRUCT_HGT_H
