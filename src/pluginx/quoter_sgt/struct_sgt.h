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

#ifndef QUOTER_SGT_STRUCT_SGT_H
#define QUOTER_SGT_STRUCT_SGT_H

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

#include "define_sgt.h"

typedef boost::shared_ptr<boost::thread> thread_ptr;
typedef boost::shared_ptr<boost::asio::io_service> service_ptr;

#define SNAPSHOT_STOCK_SGT_V1

#ifdef SNAPSHOT_STOCK_SGT_V1
    #define SNAPSHOT_STOCK_SGT_VERSION '1'
#endif
#ifdef SNAPSHOT_STOCK_SGT_V2
    #define SNAPSHOT_STOCK_SGT_VERSION '2'
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
	std::string m_sender_id;
	int32_t m_timeout;
	int32_t m_need_dump;
	std::string m_dump_path;
	int32_t m_data_compress;
	int32_t m_data_encode;
	int32_t m_dump_time;
	int32_t m_init_time;
	int32_t m_source_time_start;
	int32_t m_source_time_stop;
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

struct MDGW_Header
{
	uint32_t m_MsgType; // 消息类型
	uint32_t m_BodyLength; // 消息体长度
};

struct MDGW_Tailer
{
	uint32_t m_Checksum; // 校验和
};

struct MDGW_Logon
{
	MDGW_Header m_Header; // MsgType = 1
	char m_SenderCompID[20]; // 发送方代码
	char m_TargetCompID[20]; // 接收方代码
	int32_t m_HeartBtInt; // 心跳间隔，单位是秒，用户行情系统登陆时提供给行情网关
	char m_Password[16]; // 密码
	char m_DefaultApplVerID[32]; // 二进制协议版本
	MDGW_Tailer mdgw_tailer;
};

struct MDGW_Logon_Body
{
	char m_SenderCompID[20]; // 发送方代码
	char m_TargetCompID[20]; // 接收方代码
	int32_t m_HeartBtInt; // 心跳间隔，单位是秒，用户行情系统登陆时提供给行情网关
	char m_Password[16]; // 密码
	char m_DefaultApplVerID[32]; // 二进制协议版本
};

struct MDGW_Logout
{
	MDGW_Header m_Header; // MsgType = 2
	int32_t m_SessionStatus; // 退出时的会话状态
	char m_Text[200]; // 文本，注销原因的进一步补充说明
	MDGW_Tailer mdgw_tailer;
};

struct MDGW_Logout_Body
{
	int32_t m_SessionStatus; // 退出时的会话状态
	char m_Text[200]; // 文本，注销原因的进一步补充说明
};

struct MDGW_Heartbeat
{
	MDGW_Header m_Header; // MsgType = 3，BodyLength = 0
	MDGW_Tailer mdgw_tailer;
};

struct MDGW_Snapshot_Body // 快照行情消息定义
{
	int64_t m_OrigTime; // 数据生成时间 Int64 YYYYMMDDHHMMSSsss
	uint16_t m_ChannelNo; // 频道代码 uInt16
	char m_MDStreamID[3]; // 行情类别 char[3]
	char m_SecurityID[8]; // 证券代码 char[8]
	char m_SecurityIDSource[4]; // 证券代码源 char[4] 102深交所 103港交所
	char m_TradingPhaseCode[8]; // 产品所处的交易阶段代码 char[8]
	int64_t m_PrevClosePx; // 昨收价 Int64 N13(4)
	int64_t m_NumTrades; // 成交笔数 Int64
	int64_t m_TotalVolumeTrade; // 成交总量 Int64 N15(2)
	int64_t m_TotalValueTrade; // 成交总金额 Int64 N18(4)
};

struct MDGW_HK_MDEntries // 港股实时行情快照扩展字段 - 行情条目
{
	char m_MDEntryType[2]; // 行情条目类别 char[2]
	int64_t m_MDEntryPx; // 价格 Int64 N18(6)
	int64_t m_MDEntrySize; // 数量 Int64 N15(2)
	uint16_t m_MDPriceLevel; // 买卖盘档位 uInt16
};

struct MDGW_HK_ComplexEventTimes // 港股实时行情快照扩展字段 - VCM 冷静期
{
	int64_t m_ComplexEventStartTime; // 冷静期开始时间 Int64 YYYYMMDDHHMMSSsss
	int64_t m_ComplexEventEndTime; // 冷静期结束时间 Int64 YYYYMMDDHHMMSSsss
};

#define DEF_MDGW_MSG_TYPE_LOGON 1
#define DEF_MDGW_MSG_TYPE_LOGOUT 2
#define DEF_MDGW_MSG_TYPE_HEARTBEAT 3
#define DEF_MDGW_MSG_TYPE_SNAPSHOT_STOCK_SGT 306311 // 港股实时行情

uint32_t g_size_mdgw_header = sizeof( MDGW_Header ); // 8 字节
uint32_t g_size_mdgw_tailer = sizeof( MDGW_Tailer ); // 4 字节
uint32_t g_size_mdgw_logon_body = sizeof( MDGW_Logon_Body ); // 92 字节
uint32_t g_size_mdgw_logout_body = sizeof( MDGW_Logout_Body ); // 204 字节
uint32_t g_size_mdgw_heartbeat_body = 0; // 空结构体长度为 1 字节

uint32_t MDGW_GenerateCheckSum( char* buffer, uint32_t bufer_length ) {
	uint32_t check_sum = 0;
	for( size_t i = 0; i < bufer_length; ++i ) {
		check_sum += (uint32_t)buffer[i];
	}
	return ( check_sum % 256 );
}

std::string MDGW_SessionStatus( int32_t status ) {
	std::string result;
	switch( status ) {
	case 0: return result = "会话活跃";
	case 1: return result = "会话口令已更改";
	case 2: return result = "将过期的会话口令";
	case 3: return result = "新会话口令不符合规范";
	case 4: return result = "会话退登完成";
	case 5: return result = "不合法的用户名或口令";
	case 6: return result = "账户锁定";
	case 7: return result = "当前时间不允许登录";
	case 8: return result = "口令过期";
	case 9: return result = "收到的 MsgSeqNum(34) 太小";
	case 10: return result = "收到的 NextExpectedMsgSeqNum(789) 太大";
	case 101: return result = "其他";
	case 102: return result = "无效消息";
	default: return result = "未知状态";
	}
}

#ifdef SNAPSHOT_STOCK_SGT_V1
struct SnapshotStock_SGT // 所有变量均会被赋值
{
	char m_code[8]; // 证券代码
	char m_name[33]; // 证券名称 // 中文名称 // 无
	char m_type[6]; // 证券类型 // "H"
	char m_market[6]; // 证券市场 // "HK"
	char m_status[2]; // 证券状态 // "N"、"S"、"X"
	uint32_t m_last; // 最新价 // 10000
	uint32_t m_open; // 开盘价 // 10000 // 无
	uint32_t m_high; // 最高价 // 10000
	uint32_t m_low; // 最低价 // 10000
	uint32_t m_close; // 收盘价 // 10000 // 按盘价
	uint32_t m_pre_close; // 昨收价 // 10000
	int64_t m_volume; // 成交量
	int64_t m_turnover; // 成交额 // 10000
	uint32_t m_ask_price[10]; // 申卖价 // 10000
	int32_t m_ask_volume[10]; // 申卖量
	uint32_t m_bid_price[10]; // 申买价 // 10000
	int32_t m_bid_volume[10]; // 申买量
	uint32_t m_high_limit; // 涨停价 // 10000
	uint32_t m_low_limit; // 跌停价 // 10000
	int32_t m_trade_count; // 成交笔数
	int32_t m_vcm_start_time; // 冷静期开始时间 // HHMMSSmmm 精度：秒
	int32_t m_vcm_end_time; // 冷静期结束时间 // HHMMSSmmm 精度：秒
	int32_t m_quote_time; // 行情时间 // HHMMSSmmm 精度：秒
	int32_t m_local_time; // 本地时间 // HHMMSSmmm 精度：毫秒
	uint32_t m_local_index; // 本地序号

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Standard Header           消息头                  MsgType = 306311

	//OrigTime                  数据生成时间            Int64 YYYYMMDDHHMMSSsss
	//ChannelNo                 频道代码                uInt16
	//MDStreamID                行情类别                char[3] = 630
	//SecurityID                证券代码                char[8]
	//SecurityIDSource          证券代码源              char[4] 102深交所 103港交所
	//TradingPhaseCode          产品所处的交易阶段代码  char[8]
	//    第 0 位：S = 启动( 开市前 )、O = 开盘集合竞价、T = 连续竞价、B = 休市、C = 收盘集合竞价、E = 已闭市、H = 临时停牌、A = 盘后交易、V = 波动性中断
	//    第 1 位：0 = 正常状态、1 = 全天停牌
	//PrevClosePx               昨收价                  Int64 N13(4)
	//NumTrades                 成交笔数                Int64
	//TotalVolumeTrade          成交总量                Int64 N15(2)
	//TotalValueTrade           成交总金额              Int64 N18(4)

	//NoMDEntries               行情条目个数            uInt32
	//    MDEntryType           行情条目类别            char[2]
	//        0 = 买入、1 = 卖出、2 = 最近价、7 = 最高价、8 = 最低价、xe = 涨停价、xf = 跌停价、xh = 按盘价（收盘后为收盘价格）、xi = 参考价
	//    MDEntryPx             价格                    Int64 N18(6)
	//    MDEntrySize           数量                    Int64 N15(2)
	//    MDPriceLevel          买卖盘档位              uInt16
	//NoComplexEventTimes       冷静期个数              uInt32
	//    0 或 1，为 1 表示当前处于触发 VCM 的冷静期，下面的时间是冷静期的开始结束时间
	//    ComplexEventStartTime 冷静期开始时间          Int64 YYYYMMDDHHMMSSsss
	//    ComplexEventEndTime   冷静期结束时间          Int64 YYYYMMDDHHMMSSsss
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
};
#endif

#ifdef SNAPSHOT_STOCK_SGT_V2
struct SnapshotStock_SGT // 所有变量均会被赋值
{
};
#endif

#pragma pack( pop )

#endif // QUOTER_SGT_STRUCT_SGT_H
