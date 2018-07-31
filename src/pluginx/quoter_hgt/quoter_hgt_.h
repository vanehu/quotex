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

#ifndef QUOTER_HGT_QUOTER_HGT_P_H
#define QUOTER_HGT_QUOTER_HGT_P_H

#include "struct_hgt.h"
#include "quoter_hgt.h"

typedef boost::shared_ptr<basicx::NetServer> net_server_ptr;

class basicx::SysLog_S;
class basicx::SysCfg_S;
class basicx::SysRtm_S;
class basicx::Plugins;

class QuoterHGT_P : public basicx::NetServer_X
{
public:
	QuoterHGT_P();
	~QuoterHGT_P();

public:
	void SetGlobalPath();
	bool ReadConfig( std::string file_path );
	void LogPrint( basicx::syslog_level log_level, std::string& log_info, int32_t log_show = 0 );

public:
	bool Initialize();
	bool InitializeExt();
	bool StartPlugin();
	bool IsPluginRun();
	bool StopPlugin();
	bool UninitializeExt();
	bool Uninitialize();
	bool AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data );

	void CreateService();
	void HandleTaskMsg();

public:
	Config m_configs;
	std::string m_location;
	std::string m_info_file_path;
	std::string m_cfg_file_path;

	service_ptr m_service;
	bool m_service_running;
	thread_ptr m_thread_service;
	int32_t m_work_thread_number;
	std::vector<thread_ptr> m_vec_work_thread;

	int32_t m_task_id;
	boost::mutex m_task_list_lock;
	std::list<TaskItem> m_list_task;

	bool m_plugin_running;

	std::string m_log_cate;
	basicx::SysLog_S* m_syslog;
	basicx::SysCfg_S* m_syscfg;
	basicx::SysRtm_S* m_sysrtm;
	basicx::Plugins* m_plugins;

// 自定义成员函数和变量
public:
	bool CheckSingleMutex(); // 单例限制检测
	void OnTimer();
	bool CreateDumpFolder();
	void InitQuoteDataFile();

	void MS_AddData_SnapshotStock( SnapshotStock_HGT& snapshot_stock_temp );

	void DumpSnapshotStock();

	void StartNetServer();
	void OnNetServerInfo( basicx::NetServerInfo& net_server_info_temp ) override;
	void OnNetServerData( basicx::NetServerData& net_server_data_temp ) override;
	int32_t gzCompress( Bytef* data_in, uLong size_in, Bytef* data_out, uLong* size_out, int32_t level );

	void HandleUserRequest();
	std::string OnUserAddSub( Request* request );
	std::string OnUserDelSub( Request* request );
	void AddConSubAll( ConSubMan& csm_con_sub_man, int32_t identity, basicx::ConnectInfo* connect_info );
	void DelConSubAll( ConSubMan& csm_con_sub_man, int32_t identity );
	bool IsConSubAll( ConSubMan& csm_con_sub_man, int32_t identity ); // 是否已全市场订阅
	void AddConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity, basicx::ConnectInfo* connect_info );
	void DelConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity );
	void ClearConSubOne( ConSubMan& csm_con_sub_man, int32_t identity );
	void ClearUnavailableConSub( ConSubMan& csm_con_sub_man, bool is_idle_time ); // 一般在空闲时进行
	void CommitResult( int32_t task_id, int32_t identity, int32_t code, std::string& data );
	std::string OnErrorResult( int32_t ret_func, int32_t ret_code, std::string ret_info, int32_t encode );

	void HandleMarketData();

public:
	HANDLE m_single_mutex; // 单例限制检测
	thread_ptr m_thread_on_timer;
	thread_ptr m_thread_handle_market_data;
	std::vector<std::string> m_vec_contract;
	int32_t m_subscribe_count;
	
	QuoteCache<SnapshotStock_HGT> m_cache_snapshot_stock;

	int32_t m_data_encode;
	int32_t m_data_compress;
	net_server_ptr m_net_server_broad;
	net_server_ptr m_net_server_query;
	uint32_t m_output_buf_len_snapshot_stock;
	unsigned char* m_output_buf_snapshot_stock;

	thread_ptr m_thread_user_request;
	boost::mutex m_user_request_list_lock;
	std::list<Request> m_list_user_request;

	ConSubMan m_csm_snapshot_stock;

	Json::CharReader* m_json_reader;
	Json::CharReaderBuilder m_json_reader_builder;
	Json::StreamWriterBuilder m_json_writer;

	std::string m_market_data_file_path;
};

#endif // QUOTER_HGT_QUOTER_HGT_P_H
