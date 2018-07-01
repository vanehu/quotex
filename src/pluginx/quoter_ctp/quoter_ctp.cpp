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

#include <fstream>
#include <algorithm> // transform

#define OTL_ODBC // 使用 SQL Server

#include <common/assist.h>
#include <common/winver.h>
#include <common/common.h>
#include <syslog/syslog.h>
#include <syscfg/syscfg.h>
#include <sysrtm/sysrtm.h>
#include <sysdbi_s/sysdbi_s.h>
#include <plugins/plugins.h>

#include "define_ctp.h"
#include "quoter_ctp.h"
#include "quoter_ctp_.h"

// 在 <boost/asio.hpp> 之后包含，不然会报：WinSock.h has already been included
#import "C:\Program Files (x86)\Common Files\System\ado\msado20.tlb" named_guids rename("EOF", "adoEOF") // SQL Server

#define FILE_LOG_ONLY 1

QuoterCTP the_app; // 必须

// 预设 QuoterCTP_P 函数实现

QuoterCTP_P::QuoterCTP_P()
	: NetServer_X()
	, m_location( "" )
	, m_info_file_path( "" )
	, m_cfg_file_path( "" )
	, m_service_running( false )
	, m_work_thread_number( CFG_WORK_THREAD_NUM )
	, m_plugin_running( false )
	, m_task_id( 0 )
	, m_user_api( nullptr )
	, m_user_spi( nullptr )
	, m_subscribe_ok( false )
	, m_log_cate( "<Quoter_CTP>" ) {
	m_syslog = basicx::SysLog_S::GetInstance();
	m_syscfg = basicx::SysCfg_S::GetInstance();
	m_sysrtm = basicx::SysRtm_S::GetInstance();
	m_sysdbi_s = basicx::SysDBI_S::GetInstance();
	m_plugins = basicx::Plugins::GetInstance();
}

QuoterCTP_P::~QuoterCTP_P() {
	if( m_output_buf_snapshot_future != nullptr ) {
		delete[] m_output_buf_snapshot_future;
		m_output_buf_snapshot_future = nullptr;
	}

	for( auto it = m_csm_snapshot_future.m_map_con_sub_one.begin(); it != m_csm_snapshot_future.m_map_con_sub_one.end(); it++ ) {
		if( it->second != nullptr ) {
			delete it->second;
		}
	}
	m_csm_snapshot_future.m_map_con_sub_one.clear();

	for( auto it = m_csm_snapshot_future.m_list_one_sub_con_del.begin(); it != m_csm_snapshot_future.m_list_one_sub_con_del.end(); it++ ) {
		if( (*it) != nullptr ) {
			delete (*it);
		}
	}
	m_csm_snapshot_future.m_list_one_sub_con_del.clear();
}

void QuoterCTP_P::SetGlobalPath()
{
	m_location = m_plugins->GetPluginLocationByName( PLUGIN_NAME );
	m_cfg_file_path = m_plugins->GetPluginCfgFilePathByName( PLUGIN_NAME );
	m_info_file_path = m_plugins->GetPluginInfoFilePathByName( PLUGIN_NAME );
}

bool QuoterCTP_P::ReadConfig( std::string file_path )
{
	std::string log_info;

	pugi::xml_document document;
	if( !document.load_file( file_path.c_str() ) ) {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "打开插件参数配置信息文件失败！{0}", file_path );
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		return false;
	}

	pugi::xml_node node_plugin = document.document_element();
	if( node_plugin.empty() ) {
		log_info = "获取插件参数配置信息 根节点 失败！";
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		return false;
	}
	
	m_configs.m_address = node_plugin.child_value( "Address" );
	m_configs.m_broker_id = node_plugin.child_value( "BrokerID" );
	m_configs.m_username = node_plugin.child_value( "Username" );
	m_configs.m_password = node_plugin.child_value( "Password" );
	m_configs.m_sub_list_from = node_plugin.child_value( "SubListFrom" );

	if( "USER" == m_configs.m_sub_list_from ) { // 从本地配置文件获取订阅合约列表
		for( pugi::xml_node child_node_plugin = node_plugin.first_child(); !child_node_plugin.empty(); child_node_plugin = child_node_plugin.next_sibling() ) {
			if( "SubList" == std::string( child_node_plugin.name() ) ) {
				std::string contract = child_node_plugin.attribute( "Contract" ).value();
				m_vec_contract.push_back( contract );
				FormatLibrary::StandardLibrary::FormatTo( log_info, "合约代码：{0}", contract );
				LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info, FILE_LOG_ONLY );
			}
		}
	}
	else if( "JYDB" == m_configs.m_sub_list_from ) { // 从聚源数据库获取订阅合约列表
		m_configs.m_name_odbc = node_plugin.child_value( "NameODBC" );
		m_configs.m_db_address = node_plugin.child_value( "DB_Addr" );
		m_configs.m_db_port = atoi( node_plugin.child_value( "DB_Port" ) );
		m_configs.m_db_username = node_plugin.child_value( "DB_Username" );
		m_configs.m_db_password = node_plugin.child_value( "DB_Password" );
		m_configs.m_db_database = node_plugin.child_value( "DB_DataBase" );
	}
	else if( "CTP" == m_configs.m_sub_list_from ) { // 从上期平台获取订阅合约列表
		m_configs.m_qc_address = node_plugin.child_value( "QC_Address" );
		m_configs.m_qc_broker_id = node_plugin.child_value( "QC_BrokerID" );
		m_configs.m_qc_username = node_plugin.child_value( "QC_Username" );
		m_configs.m_qc_password = node_plugin.child_value( "QC_Password" );
	}

	// 托管时保护账户信息
	if( "0" == m_configs.m_username ) {
		//m_configs.m_username = "12345678";
		//m_configs.m_password = "123456";
	}
	if( "0" == m_configs.m_qc_username ) {
		//m_configs.m_qc_username = "12345678";
		//m_configs.m_qc_password = "123456";
	}

	m_configs.m_need_dump = atoi( node_plugin.child_value( "NeedDump" ) );
	m_configs.m_dump_path = node_plugin.child_value( "DumpPath" );
	m_configs.m_data_compress = atoi( node_plugin.child_value( "DataCompress" ) );
	m_configs.m_data_encode = atoi( node_plugin.child_value( "DataEncode" ) );
	m_configs.m_dump_time = atoi( node_plugin.child_value( "DumpTime" ) );
	m_configs.m_init_time = atoi( node_plugin.child_value( "InitTime" ) );
	m_configs.m_night_time = atoi( node_plugin.child_value( "NightTime" ) );

	//FormatLibrary::StandardLibrary::FormatTo( log_info, "{0} {1} {2} {3} {4} {5} {6} {7}", m_configs.m_address, m_configs.m_broker_id, m_configs.m_username, m_configs.m_password, 
	//	m_configs.m_sub_list_from, m_configs.m_dump_time, m_configs.m_init_time, m_configs.m_night_time );
	//LogPrint( basicx::syslog_level::c_debug, m_log_cate, log_info );

	log_info = "插件参数配置信息读取完成。";
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

	return true;
}

void QuoterCTP_P::LogPrint( basicx::syslog_level log_level, std::string& log_cate, std::string& log_info, int32_t log_show/* = 0*/ ) {
	if( 0 == log_show ) {
		m_syslog->LogPrint( log_level, log_cate, "LOG>: CTP " + log_info ); // 控制台
		m_sysrtm->LogTrans( log_level, log_cate, log_info ); // 远程监控
	}
	m_syslog->LogWrite( log_level, log_cate, log_info );
}

bool QuoterCTP_P::Initialize() {
	SetGlobalPath();
	ReadConfig( m_cfg_file_path );

	m_thread_service = boost::make_shared<boost::thread>( boost::bind( &QuoterCTP_P::CreateService, this ) );
	m_thread_user_request = boost::make_shared<boost::thread>( boost::bind( &QuoterCTP_P::HandleUserRequest, this ) );

	return true;
}

bool QuoterCTP_P::InitializeExt() {
	return true;
}

bool QuoterCTP_P::StartPlugin() {
	std::string log_info;

	try {
		log_info = "开始启用插件 ....";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		if( CheckSingleMutex() == false ) { // 单例限制检测
		    return false;
		}

		StartNetServer();
		m_thread_on_timer = boost::make_shared<boost::thread>( boost::bind( &QuoterCTP_P::OnTimer, this ) );

		// TODO：添加更多初始化任务

		log_info = "插件启用完成。";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		m_plugin_running = true; // 需要在创建线程前赋值为真

		return true;
	} // try
	catch( ... ) {
		log_info = "插件启用时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}

	return false;
}

bool QuoterCTP_P::IsPluginRun() {
	return m_plugin_running;
}

bool QuoterCTP_P::StopPlugin() {
	std::string log_info;

	try {
		log_info = "开始停止插件 ....";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		// TODO：添加更多反初始化任务

		log_info = "插件停止完成。";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		m_plugin_running = false;

		return true;
	} // try
	catch( ... ) {
		log_info = "插件停止时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}

	return false;
}

bool QuoterCTP_P::UninitializeExt() {
	return true;
}

bool QuoterCTP_P::Uninitialize() {
	if( true == m_service_running ) {
		m_service_running = false;
		m_service->stop();
	}
	
	return true;
}

bool QuoterCTP_P::AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
	try {
		m_task_list_lock.lock();
		bool write_in_progress = !m_list_task.empty();
		TaskItem task_item_temp;
		m_list_task.push_back( task_item_temp );
		TaskItem& task_item_ref = m_list_task.back();
		task_item_ref.m_task_id = task_id;
		task_item_ref.m_identity = identity;
		task_item_ref.m_code = code;
		task_item_ref.m_data = data;
		m_task_list_lock.unlock();
		
		if( !write_in_progress && true == m_service_running ) {
			m_service->post( boost::bind( &QuoterCTP_P::HandleTaskMsg, this ) );
		}

		return true;
	}
	catch( std::exception& ex ) {
		std::string log_info;
		FormatLibrary::StandardLibrary::FormatTo( log_info, "添加 TaskItem 消息 异常：{0}", ex.what() );
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}

	return false;
}

void QuoterCTP_P::CreateService()
{
	std::string log_info;

	log_info = "创建输入输出服务线程完成, 开始进行输入输出服务 ...";
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

	try {
		try {
			m_service = boost::make_shared<boost::asio::io_service>();
			boost::asio::io_service::work work( *m_service );

			for( size_t i = 0; i < (size_t)m_work_thread_number; i++ ) {
				thread_ptr thread_service( new boost::thread( boost::bind( &boost::asio::io_service::run, m_service ) ) );
				m_vec_work_thread.push_back( thread_service );
			}

			m_service_running = true;

			for( size_t i = 0; i < m_vec_work_thread.size(); i++ ) { // 等待所有线程退出
				m_vec_work_thread[i]->join();
			}
		}
		catch( std::exception& ex ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "输入输出服务 初始化 异常：{0}", ex.what() );
			LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		}
	} // try
	catch( ... ) {
		log_info = "输入输出服务线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, m_log_cate, log_info );
	}

	if( true == m_service_running ) {
		m_service_running = false;
		m_service->stop();
	}

	log_info = "输入输出服务线程退出！";
	LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );
}

void QuoterCTP_P::HandleTaskMsg() {
	std::string log_info;

	try {
		std::string result_data;
		TaskItem* task_item = &m_list_task.front(); // 肯定有

		//FormatLibrary::StandardLibrary::FormatTo( log_info, "开始处理 {0} 号任务 ...", task_item->m_task_id );
		//LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		try {
			Request request_temp;
			request_temp.m_task_id = task_item->m_task_id;
			request_temp.m_identity = task_item->m_identity;
			request_temp.m_code = task_item->m_code;

			int32_t ret_func = 0;
			if( NW_MSG_CODE_JSON == task_item->m_code ) {
				if( m_json_reader.parse( task_item->m_data, request_temp.m_req_json, false ) ) { // 含中文：std::string strUser = StringToGB2312( jsRootR["user"].asString() );
					ret_func = request_temp.m_req_json["function"].asInt();
				}
				else {
					FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时数据 JSON 解析失败！", task_item->m_task_id );
					result_data = OnErrorResult( ret_func, -1, log_info, task_item->m_code );
				}
			}
			else {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时数据编码格式异常！", task_item->m_task_id );
				task_item->m_code = NW_MSG_CODE_JSON; // 编码格式未知时默认使用
				result_data = OnErrorResult( ret_func, -1, log_info, task_item->m_code );
			}

			if( ret_func > 0 ) {
				switch( ret_func ) {
				case TD_FUNC_QUOTE_ADDSUB: // 订阅行情
					m_user_request_list_lock.lock();
					m_list_user_request.push_back( request_temp );
					m_user_request_list_lock.unlock();
					break;
				case TD_FUNC_QUOTE_DELSUB: // 退订行情
					m_user_request_list_lock.lock();
					m_list_user_request.push_back( request_temp );
					m_user_request_list_lock.unlock();
					break;
				default: // 会话自处理请求
					if( "" == result_data ) { // 避免 result_data 覆盖
						FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时功能编号未知！function：{1}", task_item->m_task_id, ret_func );
						result_data = OnErrorResult( ret_func, -1, log_info, request_temp.m_code );
					}
				}
			}
			else {
				if( "" == result_data ) { // 避免 result_data 覆盖
					FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时功能编号异常！function：{1}", task_item->m_task_id, ret_func );
					result_data = OnErrorResult( ret_func, -1, log_info, request_temp.m_code );
				}
			}
		}
		catch( ... ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时发生错误，可能编码格式异常！", task_item->m_task_id );
			task_item->m_code = NW_MSG_CODE_JSON; // 编码格式未知时默认使用
			result_data = OnErrorResult( 0, -1, log_info, task_item->m_code );
		}

		if( result_data != "" ) { // 在任务转发前就发生错误了
			CommitResult( task_item->m_task_id, task_item->m_identity, task_item->m_code, result_data );
		}

		FormatLibrary::StandardLibrary::FormatTo( log_info, "处理 {0} 号任务完成。", task_item->m_task_id );
		//LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		m_task_list_lock.lock();
		m_list_task.pop_front();
		bool write_on_progress = !m_list_task.empty();
		m_task_list_lock.unlock();

		if( write_on_progress && true == m_service_running ) {
			m_service->post( boost::bind( &QuoterCTP_P::HandleTaskMsg, this ) );
		}
	}
	catch( std::exception& ex ) {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "处理 TaskItem 消息 异常：{0}", ex.what() );
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}
}

// 自定义 QuoterCTP_P 函数实现

bool QuoterCTP_P::CheckSingleMutex() { // 单例限制检测
	m_single_mutex = ::CreateMutex( NULL, FALSE, L"quoter_ctp" );
	if( GetLastError() == ERROR_ALREADY_EXISTS ) {
		std::string log_info = "在一台服务器上只允许运行本插件一个实例！";
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		CloseHandle( m_single_mutex );
		return false;
	}
	return true;
}

void QuoterCTP_P::OnTimer() {
	std::string log_info;

	try {
		if( CreateDumpFolder() ) { // 含首次开启时文件创建
			m_thread_init_api_spi = boost::make_shared<boost::thread>( boost::bind( &QuoterCTP_P::InitApiSpi, this ) );

			bool force_dump = false; // 避免瞬间多次调用 Dump 操作
			bool do_resubscribe = false;
			bool do_resubscribe_night = false;
			bool init_quote_data_file = false;
			while( true ) {
				for( size_t i = 0; i < (size_t)m_configs.m_dump_time; i++ ) { // 间隔需小于60秒
					Sleep( 1000 );
					//if( 前置行情服务器启动 )
					//{
					//	DumpSnapshotFuture();
					//	force_dump = true;
					//}
				}
				if( false == force_dump ) {
					DumpSnapshotFuture();
				}
				force_dump = false;

				tm now_time_t = basicx::GetNowTime();
				int32_t now_time = now_time_t.tm_hour * 100 + now_time_t.tm_min;

				char time_temp[32];
				strftime( time_temp, 32, "%H%M%S", &now_time_t);
				FormatLibrary::StandardLibrary::FormatTo( log_info, "{0} SnapshotFuture：R:{1}，W:{2}，C:{3}，S:{4}。", 
					time_temp, m_cache_snapshot_future.m_recv_num.load(), m_cache_snapshot_future.m_dump_num.load(), m_cache_snapshot_future.m_comp_num.load(), m_cache_snapshot_future.m_send_num.load() );
				LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

				m_cache_snapshot_future.m_dump_num = 0;
				m_cache_snapshot_future.m_comp_num = 0;
				m_cache_snapshot_future.m_send_num = 0;

				if( now_time == m_configs.m_init_time && false == do_resubscribe ) {
					DoReSubscribe();
					do_resubscribe = true;
				}

				if( now_time == m_configs.m_night_time && false == do_resubscribe_night ) {
					DoReSubscribe();
					do_resubscribe_night = true;
				}

				if( 600 == now_time && false == init_quote_data_file ) {
					InitQuoteDataFile();
					init_quote_data_file = true;

					ClearUnavailableConSub( m_csm_snapshot_future, true );
					log_info = "清理无效订阅。";
					LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
				}

				if( 500 == now_time ) {
					do_resubscribe = false;
					do_resubscribe_night = false;
					init_quote_data_file = false;
				}
			}
		}
	} // try
	catch( ... ) {
		log_info = "插件定时线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, m_log_cate, log_info );
	}

	log_info = "插件定时线程退出！";
	LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );
}

void QuoterCTP_P::DoReSubscribe() {
	std::string log_info;

	log_info = "开始行情重新初始化 ....";
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

	// 重新初始化时不做退订，订阅在重登成功时更新。

	if( nullptr != m_user_api ) {
		// 这里其实是手动退出 InitApiSpi 线程
		m_user_api->RegisterSpi( nullptr );
		m_user_api->Release();
		m_user_api = nullptr;
	}
	if( nullptr != m_user_spi ) {
		delete m_user_spi;
		m_user_spi = nullptr;
	}

	Sleep( 1000 ); // 这里等待时间跟 InitApiSpi 退出耗时相关

	m_thread_init_api_spi = boost::make_shared<boost::thread>( boost::bind( &QuoterCTP_P::InitApiSpi, this ) );

	log_info = "行情重新初始化完成。";
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
}

bool QuoterCTP_P::CreateDumpFolder() {
	std::string log_info;

	std::string dump_data_folder_path = m_configs.m_dump_path;
	basicx::StringRightTrim( dump_data_folder_path, std::string( "\\" ) );

	WIN32_FIND_DATA find_dump_folder;
	HANDLE handle_dump_folder = FindFirstFile( basicx::StringToWideChar( dump_data_folder_path ).c_str(), &find_dump_folder );
	if( INVALID_HANDLE_VALUE == handle_dump_folder ) {
		FindClose( handle_dump_folder );
		FormatLibrary::StandardLibrary::FormatTo( log_info, "实时行情数据存储文件夹不存在！{0}", dump_data_folder_path );
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		return false;
	}
	FindClose( handle_dump_folder );

	dump_data_folder_path += "\\Future";
	CreateDirectoryA( dump_data_folder_path.c_str(), NULL );

	m_cache_snapshot_future.m_folder_path = dump_data_folder_path + "\\Market";
	CreateDirectoryA( m_cache_snapshot_future.m_folder_path.c_str(), NULL );

	InitQuoteDataFile(); // 首次启动时

	return true;
}

void QuoterCTP_P::InitQuoteDataFile() {
	char date_temp[9];
	tm now_time_t = basicx::GetNowTime();
	strftime( date_temp, 9, "%Y%m%d", &now_time_t);
	FormatLibrary::StandardLibrary::FormatTo( m_cache_snapshot_future.m_file_path, "{0}\\{1}.hq", m_cache_snapshot_future.m_folder_path, date_temp );

	bool is_new_file = false;
	WIN32_FIND_DATA find_file_temp;
	HANDLE handle_find_file = FindFirstFile( basicx::StringToWideChar( m_cache_snapshot_future.m_file_path ).c_str(), &find_file_temp );
	if( INVALID_HANDLE_VALUE == handle_find_file ) { // 数据文件不存在
		is_new_file = true;
	}
	FindClose( handle_find_file );

	if( true == is_new_file ) { // 创建并写入 32 字节文件头部
		std::ofstream file_snapshot_future;
		file_snapshot_future.open( m_cache_snapshot_future.m_file_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app );
		if( file_snapshot_future ) {
			char head_temp[32] = { 0 };
			head_temp[0] = SNAPSHOT_FUTURE_VERSION; // 结构体版本
			file_snapshot_future.write( head_temp, 32 );
			file_snapshot_future.close();
		}
		else {
			std::string log_info;
			FormatLibrary::StandardLibrary::FormatTo( log_info, "SnapshotFuture：创建转储文件失败！{0}", m_cache_snapshot_future.m_file_path );
			LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		}
	}

	m_cache_snapshot_future.m_recv_num = 0;
	m_cache_snapshot_future.m_dump_num = 0;
	m_cache_snapshot_future.m_comp_num = 0;
	m_cache_snapshot_future.m_send_num = 0;
	m_cache_snapshot_future.m_local_index = 0;
}

void QuoterCTP_P::InitApiSpi() {
	std::string log_info;

	m_subscribe_count = 0;

	try {
		log_info = "开始初始化接口 ....";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
		std::string flow_path = m_syscfg->GetPath_ExtFolder() + "\\";
		m_user_api = CThostFtdcMdApi::CreateFtdcMdApi( flow_path.c_str() );
		m_user_spi = new CThostFtdcMdSpiImpl( m_user_api, this );
		m_user_api->RegisterSpi( m_user_spi );
		m_user_api->RegisterFront( const_cast<char*>(m_configs.m_address.c_str()) );
		m_user_api->Init();
		log_info = "初始化接口完成。等待连接响应 ....";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
	}
	catch( ... ) {
		log_info = "初始化接口时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}

	if( nullptr != m_user_api ) {
		m_user_api->Join();
	}

	log_info = "接口初始化线程停止阻塞！";
	LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );

	// 目前不做退订操作
	//if( true == m_subscribe_ok ) { // 退订行情
	//	m_subscribe_ok = false;

	//	size_t contract_number = m_vec_contract.size();
	//	char** contract = new char*[contract_number];
	//	for( size_t i = 0; i < contract_number; i++ ) {
	//	    contract[i] = const_cast<char*>(m_vec_contract[i].c_str());
	//	}

	//	int32_t result = m_user_api->UnSubscribeMarketData( contract, contract_number );
	//	if( 0 == result ) {
	//	    log_info = "发送行情 退订 请求成功。";
	//	    LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
	//	}
	//	else {
	//	    log_info = "发送行情 退订 请求失败！";
	//	    LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	//	}

	//	delete[] contract;
	//}

	//Sleep( 2500 ); // 等待退订反馈

	// 目前清理在 DoReSubscribe 执行
	//if( nullptr != m_user_api ) {
	//	m_user_api->RegisterSpi( nullptr );
	//	m_user_api->Release();
	//	m_user_api = nullptr;
	//}
	//if( nullptr != m_user_spi ) {
	//	delete m_user_spi;
	//	m_user_spi = nullptr;
	//}

	log_info = "接口初始化线程退出！";
	LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );
}

void QuoterCTP_P::GetSubListFromDB() {
	std::string log_info;

	m_vec_contract.clear(); // 如果查询失败，则会导致当日无合约行情可订阅

	std::string sql_query;
	FormatLibrary::StandardLibrary::FormatTo( sql_query, "select ContractCode, ExchangeCode from [{0}].[dbo].[Fut_ContractMain] A \
					 where ( A.ExchangeCode = 10 or A.ExchangeCode = 13 or A.ExchangeCode = 15 or A.ExchangeCode = 20 ) \
					 and A.EffectiveDate <= CONVERT( VARCHAR( 10 ), GETDATE(), 120 ) \
					 and A.LastTradingDate >= CONVERT( VARCHAR( 10 ), GETDATE(), 120 ) order by A.LastTradingDate", m_configs.m_db_database );

	basicx::WinVer windows_version;
	LogPrint( basicx::syslog_level::c_info, m_log_cate, windows_version.GetVersion() ); // 打印同时取得版本信息

	if( true == windows_version.m_is_win_nt6 ) { // 使用 ADO 连接
		log_info = "使用 ADO 连接 聚源 数据库。";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		int32_t result = S_OK;
		ADODB::_Connection* connection = nullptr;
		ADODB::_Recordset* recordset = nullptr;
		
		result = m_sysdbi_s->Connect( connection, recordset, m_configs.m_db_address, m_configs.m_db_port, m_configs.m_db_username, m_configs.m_db_password, m_configs.m_db_database );
		if( FAILED( result ) ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "连接数据库失败！{0}", result );
			LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
			return;
		}
		else {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "连接数据库成功。{0}", result );
			LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
		}

		if( true == m_sysdbi_s->Query( connection, recordset, sql_query ) ) {
			int64_t row_number = m_sysdbi_s->GetCount( recordset );
			if( row_number <= 0 ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "查询获得合约代码记录条数异常！{0}", row_number );
				LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
			}
			else {
				while( !m_sysdbi_s->GetEOF( recordset ) ) {
					_variant_t v_contract_code;
					_variant_t v_exchange_code;
					v_contract_code = recordset->GetCollect( "ContractCode" );
					v_exchange_code = recordset->GetCollect( "ExchangeCode" );
					std::string contract_code = _bstr_t( v_contract_code );
					int32_t exchange_code = 0;
					if( v_exchange_code.vt != VT_NULL ) {
						exchange_code = (int32_t)v_exchange_code; // 10-上海期货交易所，13-大连商品交易所，15-郑州商品交易所，20-中国金融期货交易所
					}
					if( contract_code != "" ) {
						// 查询所得均为大写
						// 上期所和大商所品种代码需小写：10、13
						// 中金所和郑商所品种代码需大写：15、20
						if( 10 == exchange_code || 13 == exchange_code ) {
							std::transform( contract_code.begin(), contract_code.end(), contract_code.begin(), tolower );
						}
						m_vec_contract.push_back( contract_code );
						FormatLibrary::StandardLibrary::FormatTo( log_info, "合约代码：{0} {1}", contract_code, exchange_code );
						LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info, FILE_LOG_ONLY );
					}
					m_sysdbi_s->MoveNext( recordset );
				}

				FormatLibrary::StandardLibrary::FormatTo( log_info, "查询获得合约代码记录 {0} {1} 条。", row_number, m_vec_contract.size() );
				LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
			}
		}
		else {
			log_info = "查询合约代码记录失败！";
			LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		}

		m_sysdbi_s->Close( recordset );
		m_sysdbi_s->Release( connection, recordset );
	}
	else { // 使用 OTL 连接
		log_info = "使用 OTL 连接 聚源 数据库。";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		otl_connect connect_otl;
		otl_connect::otl_initialize(); // 初始化 ODBC 环境

		try {
			std::string logon;
			FormatLibrary::StandardLibrary::FormatTo( logon, "{0}/{1}@{2}", m_configs.m_db_username, m_configs.m_db_password, m_configs.m_name_odbc );
			connect_otl.rlogon( logon.c_str() ); // 连接 ODBC

			otl_stream stream_otl( 1, sql_query.c_str(), connect_otl );
			
			int32_t record_number = 0;
			while( !stream_otl.eof() ) {
				char contract_temp[8];
				int32_t exchange_code = 0;
				stream_otl >> contract_temp >> exchange_code; // 10-上海期货交易所，13-大连商品交易所，15-郑州商品交易所，20-中国金融期货交易所
				std::string contract_code = contract_temp;
				if( contract_code != "" ) {
					// 查询所得均为大写
					// 上期所和大商所品种代码需小写：10、13
					// 中金所和郑商所品种代码需大写：15、20
					if( 10 == exchange_code || 13 == exchange_code ) {
						std::transform( contract_code.begin(), contract_code.end(), contract_code.begin(), tolower );
					}
					m_vec_contract.push_back( contract_code );
					FormatLibrary::StandardLibrary::FormatTo( log_info, "合约代码：{0} {1}", contract_code, exchange_code );
					LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info, FILE_LOG_ONLY );
				}
				record_number++;
			}

			FormatLibrary::StandardLibrary::FormatTo( log_info, "查询获得合约代码记录 {0} {1} 条。", record_number, m_vec_contract.size() );
			LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
		}
		catch( otl_exception& ex ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "查询合约代码记录异常！{0} {1} {2} {3}", 
				std::string( (char*)ex.msg, 1000 ), std::string( ex.stm_text, 2048 ), std::string( (char*)ex.sqlstate, 1000 ), std::string( ex.var_info, 256 ) );
			LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		}

		connect_otl.logoff(); // 断开 ODBC 连接
	}

	log_info = "从 数据库 获取合约代码完成。";
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
}

void QuoterCTP_P::GetSubListFromCTP() { // 实盘测试显示从 CTP 查询得到的合约列表中也没有套利指令合约
	std::string log_info;

	m_vec_contract.clear(); // 如果查询失败，则会导致当日无合约行情可订阅

	int32_t result = 0;
	CThostFtdcTraderApi* user_api = nullptr;
	CThostFtdcTraderSpiImpl* user_spi = nullptr;
	try {
		log_info = "开始初始化 合约查询 接口 ....";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
		std::string flow_path = m_syscfg->GetPath_ExtFolder() + "\\";
		user_api = CThostFtdcTraderApi::CreateFtdcTraderApi( flow_path.c_str() );
		user_spi = new CThostFtdcTraderSpiImpl( user_api, this );
		user_api->RegisterSpi( user_spi );
		user_api->RegisterFront( const_cast<char*>(m_configs.m_qc_address.c_str()) );
		// 不需要订阅流消息
		user_spi->m_connect_ok = false;
		user_api->Init();
		log_info = "初始化 合约查询 接口完成。等待连接响应 ....";
		LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		int32_t time_wait = 0;
		while( false == user_spi->m_connect_ok && time_wait < 5000 ) { // 最多等待 5 秒
			Sleep( 100 );
			time_wait += 100;
		}
		if( false == user_spi->m_connect_ok ) {
			log_info = "交易前置 连接超时！";
			LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		}
		else {
			log_info = "交易前置 连接完成。";
			LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

			// 登录操作
			result = user_spi->ReqUserLogin( m_configs.m_qc_broker_id, m_configs.m_qc_username, m_configs.m_qc_password );
			if( result != 0 ) { // -1、-2、-3
				FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登录时 登录请求失败！{0}", result );
				LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
			}
			else {
				log_info = "用户登录 提交成功。";
				LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

				int32_t time_wait = 0;
				while( false == user_spi->m_login_ok && time_wait < 5000 ) { // 最多等待 5 秒
					if( true == user_spi->m_last_rsp_is_error ) {
						break;
					}
					Sleep( 100 );
					time_wait += 100;
				}
			}
			if( false == user_spi->m_login_ok ) {
				std::string error_msg = user_spi->GetLastErrorMsg();
				FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登录时 登录失败！{0}", error_msg );
				LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
			}
			else {
				log_info = "交易柜台 登录完成。";
				LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

				// 查询操作
				result = user_spi->ReqQryInstrument();
				if( result != 0 ) { // -1、-2、-3
					FormatLibrary::StandardLibrary::FormatTo( log_info, "合约查询 提交失败！{0}", result );
					LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
				}
				else {
					log_info = "合约查询 提交成功。";
					LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

					int32_t time_wait = 0;
					while( false == user_spi->m_qry_instrument && time_wait < 5000 ) { // 最多等待 5 秒
						Sleep( 100 );
						time_wait += 100;
					}
				}
				if( false == user_spi->m_qry_instrument ) {
					log_info = "合约查询超时！";
					LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
				}
				else {
					FormatLibrary::StandardLibrary::FormatTo( log_info, "合约查询成功。共计 {0} 个。", m_vec_contract.size() );
					LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
				}

				// 登出操作 // 可以不做，执行 ReqUserLogout 操作会显示会话断开和前置连接中断(00001001)，但如果不 Release 依然会发心跳消息
				//result = user_spi->ReqUserLogout( m_configs.m_qc_broker_id, m_configs.m_qc_username );
				//if( result != 0 ) { //-1、-2、-3
				//	FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登出时 登出请求失败！{0}", result );
				//	LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
				//}
				//else {
				//	log_info = "用户登出 提交成功。";
				//	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

				//	int32_t time_wait = 0;
				//	while( false == user_spi->m_logout_ok && time_wait < 5000 ) { // 最多等待 5 秒
				//		if( true == user_spi->m_last_rsp_is_error ) {
				//		    break;
				//	    }
				//		Sleep( 100 );
				//		time_wait += 100;
				//	}
				//}
				//if( false == user_spi->m_logout_ok ) {
				//	std::string error_msg = user_spi->GetLastErrorMsg();
				//	FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登出时 登出失败！{0}", error_msg );
				//	LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
				//}
				//else {
				//	log_info = "上期柜台 登出完成。";
				//	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
				//}
			}
		}

		//user_api->Join();
	}
	catch( ... ) {
		log_info = "从上期平台获取合约代码时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}

	if( user_api != nullptr ) {
		user_api->Release();
	} // 只有 Release 以后才会停止发送心跳消息，如果之前已执行 ReqUserLogout 操作，这里还会新建一次会话，之后再次显示会话断开和前置连接中断(00000000)
	if( user_spi != nullptr ) {
		delete user_spi;
	}

	log_info = "从 上期平台 获取合约代码完成。";
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
}

void QuoterCTP_P::MS_AddData_SnapshotFuture( SnapshotFuture& snapshot_future_temp ) {
	//if( m_net_server_broad->Server_GetConnectCount() > 0 )
	if( m_csm_snapshot_future.m_map_con_sub_all.size() > 0 || m_csm_snapshot_future.m_map_con_sub_one.size() > 0 ) { // 全市场和单证券可以分别压缩以减少CPU压力提高效率，但一般还是全市场订阅的多
		int32_t output_buf_len_snapshot_future = m_output_buf_len_snapshot_future; // 必须每次重新赋值，否则压缩出错
		memset( m_output_buf_snapshot_future, 0, m_output_buf_len_snapshot_future );
		int32_t result = gzCompress( (unsigned char*)&snapshot_future_temp, sizeof( snapshot_future_temp ), m_output_buf_snapshot_future, (uLongf*)&output_buf_len_snapshot_future, m_data_compress );
		if( Z_OK == result ) { // 数据已压缩
			m_cache_snapshot_future.m_comp_num++;
			std::string quote_data( (char*)m_output_buf_snapshot_future, output_buf_len_snapshot_future );
			quote_data = TD_FUNC_QUOTE_DATA_MARKET_FUTURE_NP_HEAD + quote_data;

			// 先给全市场的吧，全市场订阅者可能还要自己进行过滤

			{ // 广播给全市场订阅者
				m_csm_snapshot_future.m_lock_con_sub_all.lock();
				std::map<int32_t, basicx::ConnectInfo*> map_con_sub_all = m_csm_snapshot_future.m_map_con_sub_all;
				m_csm_snapshot_future.m_lock_con_sub_all.unlock();

				for( auto it = map_con_sub_all.begin(); it != map_con_sub_all.end(); it++ ) {
					if( true == m_net_server_broad->IsConnectAvailable( it->second ) ) {
						m_net_server_broad->Server_SendData( it->second, NW_MSG_TYPE_USER_DATA, m_data_encode, quote_data );
						m_cache_snapshot_future.m_send_num++;
					}
				}
			}

			{ // 广播给单证券订阅者
				m_csm_snapshot_future.m_lock_con_sub_one.lock();
				std::map<std::string, ConSubOne*> map_con_sub_one = m_csm_snapshot_future.m_map_con_sub_one;
				m_csm_snapshot_future.m_lock_con_sub_one.unlock();

				std::map<std::string, ConSubOne*>::iterator it_cso = map_con_sub_one.find( snapshot_future_temp.m_code );
				if( it_cso != map_con_sub_one.end() ) {
					ConSubOne* con_sub_one = it_cso->second; // 运行期间 con_sub_one 所指对象不会被删除

					con_sub_one->m_lock_con_sub_one.lock();
					std::map<int32_t, basicx::ConnectInfo*> map_identity = con_sub_one->m_map_identity;
					con_sub_one->m_lock_con_sub_one.unlock();

					for( auto it = map_identity.begin(); it != map_identity.end(); it++ ) {
						if( true == m_net_server_broad->IsConnectAvailable( it->second ) ) {
							m_net_server_broad->Server_SendData( it->second, NW_MSG_TYPE_USER_DATA, m_data_encode, quote_data );
							m_cache_snapshot_future.m_send_num++;
						}
					}
				}
			}
		}
	}

	if( 1 == m_configs.m_need_dump ) {
		m_cache_snapshot_future.m_lock_cache.lock();
		m_cache_snapshot_future.m_vec_cache_put->push_back( snapshot_future_temp );
		m_cache_snapshot_future.m_lock_cache.unlock();
	}
}

void QuoterCTP_P::DumpSnapshotFuture() {
	std::string log_info;

	m_cache_snapshot_future.m_lock_cache.lock();
	if( m_cache_snapshot_future.m_vec_cache_put == &m_cache_snapshot_future.m_vec_cache_01 ) {
		m_cache_snapshot_future.m_vec_cache_put = &m_cache_snapshot_future.m_vec_cache_02;
		m_cache_snapshot_future.m_vec_cache_put->clear();
		m_cache_snapshot_future.m_vec_cache_out = &m_cache_snapshot_future.m_vec_cache_01;
	}
	else {
		m_cache_snapshot_future.m_vec_cache_put = &m_cache_snapshot_future.m_vec_cache_01;
		m_cache_snapshot_future.m_vec_cache_put->clear();
		m_cache_snapshot_future.m_vec_cache_out = &m_cache_snapshot_future.m_vec_cache_02;
	}
	m_cache_snapshot_future.m_lock_cache.unlock();

	size_t out_data_number = m_cache_snapshot_future.m_vec_cache_out->size();
	if( out_data_number > 0 ) { // 避免无新数据时操作文件
		std::ofstream file_snapshot_future;
		file_snapshot_future.open( m_cache_snapshot_future.m_file_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app );
		if( file_snapshot_future ) {
			for( size_t i = 0; i < out_data_number; ++i ) {
				file_snapshot_future.write( (const char*)(&(*m_cache_snapshot_future.m_vec_cache_out)[i]), sizeof( SnapshotFuture ) );
			}
			file_snapshot_future.close(); // 已含 flush() 动作
			m_cache_snapshot_future.m_dump_num += out_data_number;
		}
		else {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "SnapshotFuture：打开转储文件失败！{0}", m_cache_snapshot_future.m_file_path );
			LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		}
	}
}

void QuoterCTP_P::StartNetServer() {
	m_data_encode = m_configs.m_data_encode;
	m_data_compress = m_configs.m_data_compress;

	m_output_buf_len_snapshot_future = sizeof( SnapshotFuture ) + sizeof( SnapshotFuture ) / 3 + 128; // > ( nInputLen + 12 ) * 1.001;
	m_output_buf_snapshot_future = new unsigned char[m_output_buf_len_snapshot_future]; // 需足够大

	basicx::CfgBasic* cfg_basic = m_syscfg->GetCfgBasic();

	m_net_server_broad = boost::make_shared<basicx::NetServer>();
	m_net_server_query = boost::make_shared<basicx::NetServer>();
	m_net_server_broad->ComponentInstance( this );
	m_net_server_query->ComponentInstance( this );
	basicx::NetServerCfg net_server_cfg_temp;
	net_server_cfg_temp.m_log_test = cfg_basic->m_debug_infos_server;
	net_server_cfg_temp.m_heart_check_time = cfg_basic->m_heart_check_server;
	net_server_cfg_temp.m_max_msg_cache_number = cfg_basic->m_max_msg_cache_server;
	net_server_cfg_temp.m_io_work_thread_number = cfg_basic->m_work_thread_server;
	net_server_cfg_temp.m_client_connect_timeout = cfg_basic->m_con_time_out_server;
	net_server_cfg_temp.m_max_connect_total_s = cfg_basic->m_con_max_server_server;
	net_server_cfg_temp.m_max_data_length_s = cfg_basic->m_data_length_server;
	m_net_server_broad->StartNetwork( net_server_cfg_temp );
	m_net_server_query->StartNetwork( net_server_cfg_temp );

	for( size_t i = 0; i < cfg_basic->m_vec_server_server.size(); i++ ) {
		if( "broad_ctp" == cfg_basic->m_vec_server_server[i].m_type && 1 == cfg_basic->m_vec_server_server[i].m_work ) {
			m_net_server_broad->Server_AddListen( "0.0.0.0", cfg_basic->m_vec_server_server[i].m_port, cfg_basic->m_vec_server_server[i].m_type ); // 0.0.0.0
		}
		if( "query_ctp" == cfg_basic->m_vec_server_server[i].m_type && 1 == cfg_basic->m_vec_server_server[i].m_work ) {
			m_net_server_query->Server_AddListen( "0.0.0.0", cfg_basic->m_vec_server_server[i].m_port, cfg_basic->m_vec_server_server[i].m_type ); // 0.0.0.0
		}
	}
}

void QuoterCTP_P::OnNetServerInfo( basicx::NetServerInfo& net_server_info_temp ) {
	// TODO：清理全市场订阅和单证券订阅
}

void QuoterCTP_P::OnNetServerData( basicx::NetServerData& net_server_data_temp ) {
	try {
		if( m_task_id >= 214748300/*0*/ ) { // 2 ^ 31 = 2147483648
			m_task_id = 0;
		}
		m_task_id++;

		if( "broad_ctp" == net_server_data_temp.m_node_type ) { // 1 // 节点类型必须和配置文件一致
			AssignTask( m_task_id * 10 + 1, net_server_data_temp.m_identity, net_server_data_temp.m_code, net_server_data_temp.m_data ); // 1
		}
		else if( "query_ctp" == net_server_data_temp.m_node_type ) { // 2 // 节点类型必须和配置文件一致
			AssignTask( m_task_id * 10 + 2, net_server_data_temp.m_identity, net_server_data_temp.m_code, net_server_data_temp.m_data ); // 2
		}
	}
	catch( ... ) {
		std::string log_info = "转发网络数据 发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}
}

int32_t QuoterCTP_P::gzCompress( Bytef* data_in, uLong size_in, Bytef* data_out, uLong* size_out, int32_t level ) {
	int32_t error = 0;
	z_stream c_stream;

	if( data_in && size_in > 0 ) {
		c_stream.opaque = (voidpf)0;
		c_stream.zfree = (free_func)0;
		c_stream.zalloc = (alloc_func)0;

		// 默认 level 为 Z_DEFAULT_COMPRESSION，windowBits 须为 MAX_WBITS+16 而不是 -MAX_WBITS，否则压缩的数据 WinRAR 解不开
		if( deflateInit2( &c_stream, level, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY ) != Z_OK ) {
			return -1;
		}

		c_stream.next_in = data_in;
		c_stream.avail_in = size_in;
		c_stream.next_out = data_out;
		c_stream.avail_out = *size_out;

		while( c_stream.avail_in != 0 && c_stream.total_out < *size_out ) {
			if( deflate( &c_stream, Z_NO_FLUSH ) != Z_OK ) {
				return -1;
			}
		}

		if( c_stream.avail_in != 0 ) {
			return c_stream.avail_in;
		}

		for( ; ; ) {
			if( ( error = deflate( &c_stream, Z_FINISH ) ) == Z_STREAM_END ) {
				break;
			}
			if( error != Z_OK ) {
				return -1;
			}
		}

		if( deflateEnd( &c_stream ) != Z_OK ) {
			return -1;
		}

		*size_out = c_stream.total_out;

		return 0;
	}

	return -1;
}

void QuoterCTP_P::HandleUserRequest() {
	std::string log_info;

	log_info = "创建用户请求处理线程完成, 开始进行用户请求处理 ...";
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

	try {
		while( 1 ) {
			if( !m_list_user_request.empty() ) {
				std::string result_data = "";
				Request* request = &m_list_user_request.front();

				try {
					int32_t ret_func = 0;
					if( NW_MSG_CODE_JSON == request->m_code ) {
						ret_func = request->m_req_json["function"].asInt();
					}

					// 只可能为 TD_FUNC_QUOTE_ADDSUB、TD_FUNC_QUOTE_DELSUB、
					switch( ret_func ) {
					case TD_FUNC_QUOTE_ADDSUB: // 订阅行情
						result_data = OnUserAddSub( request );
						break;
					case TD_FUNC_QUOTE_DELSUB: // 退订行情
						result_data = OnUserDelSub( request );
						break;
					}
				}
				catch( ... ) {
					FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 用户请求时发生未知错误！", request->m_task_id );
					result_data = OnErrorResult( 0, -1, log_info, request->m_code );
				}

				CommitResult( request->m_task_id, request->m_identity, request->m_code, result_data );

				m_user_request_list_lock.lock();
				m_list_user_request.pop_front();
				m_user_request_list_lock.unlock();
			}

			Sleep( 1 );
		}
	} // try
	catch( ... ) {
		log_info = "用户请求处理线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, m_log_cate, log_info );
	}

	log_info = "用户请求处理线程退出！";
	LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );
}

std::string QuoterCTP_P::OnUserAddSub( Request* request ) {
	std::string log_info;

	int32_t quote_type = 0;
	std::string quote_list = "";
	if( NW_MSG_CODE_JSON == request->m_code ) {
		quote_type = request->m_req_json["quote_type"].asInt();
		quote_list = request->m_req_json["quote_list"].asString();
	}
	if( 0 == quote_type ) {
		log_info = "行情订阅时 行情类型 异常！";
		return OnErrorResult( TD_FUNC_QUOTE_ADDSUB, -1, log_info, request->m_code );
	}

	basicx::ConnectInfo* connect_info = m_net_server_broad->Server_GetConnect( request->m_identity );
	if( nullptr == connect_info ) {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "行情订阅时 获取 {0} 连接信息 异常！", request->m_identity );
		return OnErrorResult( TD_FUNC_QUOTE_ADDSUB, -1, log_info, request->m_code );
	}

	if( TD_FUNC_QUOTE_DATA_MARKET_FUTURE_NP == quote_type ) {
		if( "" == quote_list ) { // 订阅全市场
			AddConSubAll( m_csm_snapshot_future, request->m_identity, connect_info );
			ClearConSubOne( m_csm_snapshot_future, request->m_identity ); // 退订所有单个证券
		}
		else { // 指定证券
			if( IsConSubAll( m_csm_snapshot_future, request->m_identity ) == false ) { // 未全市场订阅
				std::vector<std::string> vec_filter_temp;
				boost::split( vec_filter_temp, quote_list, boost::is_any_of( ", " ), boost::token_compress_on );
				size_t size_temp = vec_filter_temp.size();
				for( size_t i = 0; i < size_temp; i++ ) {
					if( vec_filter_temp[i] != "" ) {
						AddConSubOne( m_csm_snapshot_future, vec_filter_temp[i], request->m_identity, connect_info );
					}
				}
			}
		}
	}
	else {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "行情订阅时 行情类型 未知！{0}", quote_type );
		return OnErrorResult( TD_FUNC_QUOTE_ADDSUB, -1, log_info, request->m_code );
	}

	FormatLibrary::StandardLibrary::FormatTo( log_info, "行情订阅成功。{0}", quote_type );
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

	if( NW_MSG_CODE_JSON == request->m_code ) {
		Json::Value results_json;
		results_json["ret_func"] = TD_FUNC_QUOTE_ADDSUB;
		results_json["ret_code"] = 0;
		results_json["ret_info"] = basicx::StringToUTF8( log_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return m_json_writer.write( results_json );
	}

	return "";
}

std::string QuoterCTP_P::OnUserDelSub( Request* request ) {
	std::string log_info;

	int32_t quote_type = 0;
	std::string quote_list = "";
	if( NW_MSG_CODE_JSON == request->m_code ) {
		quote_type = request->m_req_json["quote_type"].asInt();
		quote_list = request->m_req_json["quote_list"].asString();
	}
	if( 0 == quote_type ) {
		log_info = "行情退订时 行情类型 异常！";
		return OnErrorResult( TD_FUNC_QUOTE_DELSUB, -1, log_info, request->m_code );
	}

	if( TD_FUNC_QUOTE_DATA_MARKET_FUTURE_NP == quote_type ) {
		if( "" == quote_list ) { // 退订全市场
			DelConSubAll( m_csm_snapshot_future, request->m_identity );
			ClearConSubOne( m_csm_snapshot_future, request->m_identity ); // 退订所有单个证券
		}
		else { // 指定证券
			if( IsConSubAll( m_csm_snapshot_future, request->m_identity ) == false ) { // 未全市场订阅，已订阅全市场的话不可能还有单证券订阅
				std::vector<std::string> vec_filter_temp;
				boost::split( vec_filter_temp, quote_list, boost::is_any_of( ", " ), boost::token_compress_on );
				size_t size_temp = vec_filter_temp.size();
				for( size_t i = 0; i < size_temp; i++ ) {
					if( vec_filter_temp[i] != "" ) {
						DelConSubOne( m_csm_snapshot_future, vec_filter_temp[i], request->m_identity );
					}
				}
			}
		}
	}
	else {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "行情退订时 行情类型 未知！{0}", quote_type );
		return OnErrorResult( TD_FUNC_QUOTE_DELSUB, -1, log_info, request->m_code );
	}

	FormatLibrary::StandardLibrary::FormatTo( log_info, "行情退订成功。{0}", quote_type );
	LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

	if( NW_MSG_CODE_JSON == request->m_code ) {
		Json::Value results_json;
		results_json["ret_func"] = TD_FUNC_QUOTE_DELSUB;
		results_json["ret_code"] = 0;
		results_json["ret_info"] = basicx::StringToUTF8( log_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return m_json_writer.write( results_json );
	}

	return "";
}

void QuoterCTP_P::AddConSubAll( ConSubMan& csm_con_sub_man, int32_t identity, basicx::ConnectInfo* connect_info ) {
	csm_con_sub_man.m_lock_con_sub_all.lock();
	csm_con_sub_man.m_map_con_sub_all[identity] = connect_info;
	csm_con_sub_man.m_lock_con_sub_all.unlock();

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "订阅 全市场：{0}", identity );
	//LogPrint( basicx::syslog_level::c_debug, m_log_cate, log_info );
}

void QuoterCTP_P::DelConSubAll( ConSubMan& csm_con_sub_man, int32_t identity ) {
	csm_con_sub_man.m_lock_con_sub_all.lock();
	csm_con_sub_man.m_map_con_sub_all.erase( identity );
	csm_con_sub_man.m_lock_con_sub_all.unlock();

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "退订 全市场：{0}", identity );
	//LogPrint( basicx::syslog_level::c_debug, m_log_cate, log_info );
}

bool QuoterCTP_P::IsConSubAll( ConSubMan& csm_con_sub_man, int32_t identity ) {
	bool is_all_sub_already = false;
	csm_con_sub_man.m_lock_con_sub_all.lock();
	std::map<int32_t, basicx::ConnectInfo*>::iterator it_csa = csm_con_sub_man.m_map_con_sub_all.find( identity );
	if( it_csa != csm_con_sub_man.m_map_con_sub_all.end() ) {
		is_all_sub_already = true;
	}
	csm_con_sub_man.m_lock_con_sub_all.unlock();
	return is_all_sub_already;
}

void QuoterCTP_P::AddConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity, basicx::ConnectInfo* connect_info ) {
	ConSubOne* con_sub_one = nullptr;
	csm_con_sub_man.m_lock_con_sub_one.lock();
	std::map<std::string, ConSubOne*>::iterator it_cso = csm_con_sub_man.m_map_con_sub_one.find( code );
	if( it_cso != csm_con_sub_man.m_map_con_sub_one.end() ) {
		con_sub_one = it_cso->second;
	}
	else {
		con_sub_one = new ConSubOne;
		csm_con_sub_man.m_map_con_sub_one[code] = con_sub_one;
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();

	con_sub_one->m_lock_con_sub_one.lock();
	con_sub_one->m_map_identity[identity] = connect_info;
	con_sub_one->m_lock_con_sub_one.unlock();

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "订阅 单证券：{0} {1}", code, identity );
	//LogPrint( basicx::syslog_level::c_debug, m_log_cate, log_info );
}

void QuoterCTP_P::DelConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity ) {
	ConSubOne* con_sub_one = nullptr;
	csm_con_sub_man.m_lock_con_sub_one.lock();
	std::map<std::string, ConSubOne*>::iterator it_cso = csm_con_sub_man.m_map_con_sub_one.find( code );
	if( it_cso != csm_con_sub_man.m_map_con_sub_one.end() ) {
		con_sub_one = it_cso->second;
		con_sub_one->m_lock_con_sub_one.lock();
		con_sub_one->m_map_identity.erase( identity );
		con_sub_one->m_lock_con_sub_one.unlock();
		if( con_sub_one->m_map_identity.empty() ) {
			csm_con_sub_man.m_map_con_sub_one.erase( code );
			csm_con_sub_man.m_lock_one_sub_con_del.lock();
			csm_con_sub_man.m_list_one_sub_con_del.push_back( con_sub_one ); // 目前不进行删除
			csm_con_sub_man.m_lock_one_sub_con_del.unlock();
		}

		//std::string log_info;
		//FormatLibrary::StandardLibrary::FormatTo( log_info, "退订 单证券：{0} {1}", code, identity );
		//LogPrint( basicx::syslog_level::c_debug, m_log_cate, log_info );
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();
}

void QuoterCTP_P::ClearConSubOne( ConSubMan& csm_con_sub_man, int32_t identity ) {
	std::vector<std::string> vec_del_key_temp;
	ConSubOne* con_sub_one = nullptr;
	csm_con_sub_man.m_lock_con_sub_one.lock();
	for( auto it = csm_con_sub_man.m_map_con_sub_one.begin(); it != csm_con_sub_man.m_map_con_sub_one.end(); it++ ) {
		con_sub_one = it->second;
		con_sub_one->m_lock_con_sub_one.lock();
		con_sub_one->m_map_identity.erase( identity );
		con_sub_one->m_lock_con_sub_one.unlock();
		if( con_sub_one->m_map_identity.empty() ) {
			vec_del_key_temp.push_back( it->first );
			csm_con_sub_man.m_lock_one_sub_con_del.lock();
			csm_con_sub_man.m_list_one_sub_con_del.push_back( con_sub_one ); // 目前不进行删除
			csm_con_sub_man.m_lock_one_sub_con_del.unlock();
		}
	}
	for( size_t i = 0; i < vec_del_key_temp.size(); i++ ) {
		csm_con_sub_man.m_map_con_sub_one.erase( vec_del_key_temp[i] );
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();
}

void QuoterCTP_P::ClearUnavailableConSub( ConSubMan& csm_con_sub_man, bool is_idle_time ) { // 清理所有已经失效的连接订阅，一般在空闲时进行
	std::vector<int32_t> vec_del_key_csa;
	csm_con_sub_man.m_lock_con_sub_all.lock();
	for( auto it = csm_con_sub_man.m_map_con_sub_all.begin(); it != csm_con_sub_man.m_map_con_sub_all.end(); it++ ) {
		if( false == m_net_server_broad->IsConnectAvailable( it->second ) ) { // 已失效
			vec_del_key_csa.push_back( it->first );
		}
	}
	for( size_t i = 0; i < vec_del_key_csa.size(); i++ ) {
		csm_con_sub_man.m_map_con_sub_all.erase( vec_del_key_csa[i] );
	}
	csm_con_sub_man.m_lock_con_sub_all.unlock();

	std::vector<std::string> vec_del_key_cso;
	ConSubOne* con_sub_one = nullptr;
	csm_con_sub_man.m_lock_con_sub_one.lock();
	for( auto it_cso = csm_con_sub_man.m_map_con_sub_one.begin(); it_cso != csm_con_sub_man.m_map_con_sub_one.end(); it_cso++ ) {
		con_sub_one = it_cso->second;
		std::vector<int32_t> vec_del_key_id;
		con_sub_one->m_lock_con_sub_one.lock();
		for( auto it_id = con_sub_one->m_map_identity.begin(); it_id != con_sub_one->m_map_identity.end(); it_id++ ) {
			if( false == m_net_server_broad->IsConnectAvailable( it_id->second ) ) { // 已失效
				vec_del_key_id.push_back( it_id->first );
			}
		}
		for( size_t i = 0; i < vec_del_key_id.size(); i++ ) {
			con_sub_one->m_map_identity.erase( vec_del_key_id[i] );
		}
		con_sub_one->m_lock_con_sub_one.unlock();
		if( con_sub_one->m_map_identity.empty() ) {
			vec_del_key_cso.push_back( it_cso->first );
			csm_con_sub_man.m_lock_one_sub_con_del.lock();
			csm_con_sub_man.m_list_one_sub_con_del.push_back( con_sub_one ); // 目前不进行删除
			csm_con_sub_man.m_lock_one_sub_con_del.unlock();
		}
	}
	for( size_t i = 0; i < vec_del_key_cso.size(); i++ ) {
		csm_con_sub_man.m_map_con_sub_one.erase( vec_del_key_cso[i] );
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();

	if( true == is_idle_time ) { // 如果确定是在空闲时执行，则 m_list_one_sub_con_del 也可以清理
		csm_con_sub_man.m_lock_one_sub_con_del.lock();
		for( auto it = csm_con_sub_man.m_list_one_sub_con_del.begin(); it != csm_con_sub_man.m_list_one_sub_con_del.end(); it++ ) {
			if( (*it) != nullptr ) {
				delete (*it);
			}
		}
		csm_con_sub_man.m_list_one_sub_con_del.clear();
		csm_con_sub_man.m_lock_one_sub_con_del.unlock();
	}
}

void QuoterCTP_P::CommitResult( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
	int32_t net_flag = task_id % 10;
	if( 1 == net_flag ) { // broad：1
		basicx::ConnectInfo* connect_info = m_net_server_broad->Server_GetConnect( identity );
		if( connect_info != nullptr ) {
			m_net_server_broad->Server_SendData( connect_info, NW_MSG_TYPE_USER_DATA, code, data );
		}
	}
	else if( 2 == net_flag ) { // query：2
		basicx::ConnectInfo* connect_info = m_net_server_query->Server_GetConnect( identity );
		if( connect_info != nullptr ) {
			m_net_server_query->Server_SendData( connect_info, NW_MSG_TYPE_USER_DATA, code, data );
		}
	}
}

std::string QuoterCTP_P::OnErrorResult( int32_t ret_func, int32_t ret_code, std::string ret_info, int32_t encode ) {
	LogPrint( basicx::syslog_level::c_error, m_log_cate, ret_info );

	if( NW_MSG_CODE_JSON == encode ) {
		Json::Value results_json;
		results_json["ret_func"] = ret_func;
		results_json["ret_code"] = ret_code;
		results_json["ret_info"] = basicx::StringToUTF8( ret_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return m_json_writer.write( results_json );
	}

	return "";
}

// 自定义 QuoterCTP 函数实现

CThostFtdcMdSpiImpl::CThostFtdcMdSpiImpl( CThostFtdcMdApi* user_api, QuoterCTP_P* quoter_ctp_p )
	: m_user_api( user_api )
	, m_quoter_ctp_p( quoter_ctp_p )
	, m_log_cate( "<Quoter_CTP>" ) {
}

CThostFtdcMdSpiImpl::~CThostFtdcMdSpiImpl() {
}

void CThostFtdcMdSpiImpl::OnFrontConnected() {
	std::string log_info;

	log_info = "连接成功。尝试行情登录 ....";
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

	CThostFtdcReqUserLoginField req_user_login;
	memset( &req_user_login, 0, sizeof( req_user_login ) );
	strcpy_s( req_user_login.BrokerID, m_quoter_ctp_p->m_configs.m_broker_id.c_str() );
	strcpy_s( req_user_login.UserID, m_quoter_ctp_p->m_configs.m_username.c_str() );
	strcpy_s( req_user_login.Password, m_quoter_ctp_p->m_configs.m_password.c_str() );
	int32_t result = m_user_api->ReqUserLogin( &req_user_login, 0 );
	if( 0 == result ) {
		log_info = "发送行情 登录 请求成功。";
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
	}
	else {
		log_info = "发送行情 登录 请求失败！";
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}
}

void CThostFtdcMdSpiImpl::OnFrontDisconnected( int nReason ) {
	// 当发生这个情况后，API会自动重新连接，客户端可不做处理
	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "连接断开：Reason:{0}", nReason );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );
}

void CThostFtdcMdSpiImpl::OnHeartBeatWarning( int nTimeLapse ) {
	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "心跳警告：TimeLapse:{0}", nTimeLapse );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
}

void CThostFtdcMdSpiImpl::OnRspUserLogin( CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) {
	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "登录反馈：ErrorID:{0} ErrorMsg:{1} RequestID:{2} Chain:{3}", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

	if( pRspInfo->ErrorID != 0 ) { // 这里是否隔几秒以后发起重登？
		log_info = "行情登录失败！";
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}
	else {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "行情登录成功。当前交易日：{0}，尝试行情订阅 ....", pRspUserLogin->TradingDay );
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );

		// 指定 JYDB 或 CTP 会将 m_vec_contract 刷新，指定 USER 则 m_vec_contract 不变
		if( "JYDB" == m_quoter_ctp_p->m_configs.m_sub_list_from ) {
			m_quoter_ctp_p->GetSubListFromDB();
		}
		else if( "CTP" == m_quoter_ctp_p->m_configs.m_sub_list_from ) {
			m_quoter_ctp_p->GetSubListFromCTP();
		}

		if( 0 == m_quoter_ctp_p->m_vec_contract.size() ) {
			log_info = "需要订阅行情的合约个数为零！";
			m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
			return;
		}

		// 这里就不先退订了
		size_t contract_number = m_quoter_ctp_p->m_vec_contract.size();
		char** contract = new char*[contract_number];
		for( size_t i = 0; i < contract_number; i++ ) {
			contract[i] = const_cast<char*>(m_quoter_ctp_p->m_vec_contract[i].c_str());
		}

		int32_t result = m_user_api->SubscribeMarketData( contract, contract_number );
		if( 0 == result ) {
			m_quoter_ctp_p->m_subscribe_ok = true;
			log_info = "发送行情 订阅 请求成功。";
			m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
		}
		else {
			m_quoter_ctp_p->m_subscribe_ok = false;
			log_info = "发送行情 订阅 请求失败！";
			m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
		}

		delete[] contract;
	}
}

void CThostFtdcMdSpiImpl::OnRspUserLogout( CThostFtdcUserLogoutField* pUserLogout, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) {
	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "登出反馈：ErrorID:{0} ErrorMsg:{1} RequestID:{2} Chain:{3}", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
}

void CThostFtdcMdSpiImpl::OnRspError( CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) {
	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "异常反馈：ErrorID:{0} ErrorMsg:{1} RequestID:{2} Chain:{3}", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );
}

void CThostFtdcMdSpiImpl::OnRspSubMarketData( CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) {
	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "订阅反馈：ErrorID:{0} ErrorMsg:{1} RequestID:{2} Chain:{3}", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info, FILE_LOG_ONLY );
	m_quoter_ctp_p->m_subscribe_count++; // 这里可以区分下是否订阅成功
}

void CThostFtdcMdSpiImpl::OnRspUnSubMarketData( CThostFtdcSpecificInstrumentField* pSpecificInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) {
	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "退订反馈：ErrorID:{0} ErrorMsg:{1} RequestID:{2} Chain:{3}", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info, FILE_LOG_ONLY );
	m_quoter_ctp_p->m_subscribe_count--; // 这里可以区分下是否退订成功
}

void CThostFtdcMdSpiImpl::OnRtnDepthMarketData( CThostFtdcDepthMarketDataField* pDepthMarketData ) {
	SYSTEMTIME sys_time;
	GetLocalTime( &sys_time );
	tm now_time_t = basicx::GetNowTime();
	char now_date_temp[9];
	strftime( now_date_temp, 9, "%Y%m%d", &now_time_t);

	SnapshotFuture snapshot_future_temp;
	memset( &snapshot_future_temp, 0, sizeof( SnapshotFuture ) );

	strcpy_s( snapshot_future_temp.m_code, pDepthMarketData->InstrumentID ); // 合约代码
	strcpy_s( snapshot_future_temp.m_name, "" ); // 合约名称 // 无
	strcpy_s( snapshot_future_temp.m_type, "F" ); // 合约类型
	strcpy_s( snapshot_future_temp.m_market, pDepthMarketData->ExchangeID ); // 合约市场
	strcpy_s( snapshot_future_temp.m_status, "N" ); // 合约状态：正常
	snapshot_future_temp.m_last = (uint32_t)(pDepthMarketData->LastPrice * 10000); // 最新价 // 10000
	snapshot_future_temp.m_open = (uint32_t)(pDepthMarketData->OpenPrice * 10000); // 开盘价 // 10000
	snapshot_future_temp.m_high = (uint32_t)(pDepthMarketData->HighestPrice * 10000); // 最高价 // 10000
	snapshot_future_temp.m_low = (uint32_t)(pDepthMarketData->LowestPrice * 10000); // 最低价 // 10000
	snapshot_future_temp.m_close = (uint32_t)(pDepthMarketData->ClosePrice * 10000); // 收盘价 // 10000
	snapshot_future_temp.m_pre_close = (uint32_t)(pDepthMarketData->PreClosePrice * 10000); // 昨收价 // 10000
	snapshot_future_temp.m_volume = pDepthMarketData->Volume; // 成交量
	snapshot_future_temp.m_turnover = (int64_t)(pDepthMarketData->Turnover * 10000); // 成交额 // 10000
	snapshot_future_temp.m_ask_price[0] = (uint32_t)(pDepthMarketData->AskPrice1 * 10000); // 卖价 1 // 10000
	snapshot_future_temp.m_ask_volume[0] = pDepthMarketData->AskVolume1; // 卖量 1
	snapshot_future_temp.m_ask_price[1] = (uint32_t)(pDepthMarketData->AskPrice2 * 10000); // 卖价 2 // 10000
	snapshot_future_temp.m_ask_volume[1] = pDepthMarketData->AskVolume2; // 卖量 2
	snapshot_future_temp.m_ask_price[2] = (uint32_t)(pDepthMarketData->AskPrice3 * 10000); // 卖价 3 // 10000
	snapshot_future_temp.m_ask_volume[2] = pDepthMarketData->AskVolume3; // 卖量 3
	snapshot_future_temp.m_ask_price[3] = (uint32_t)(pDepthMarketData->AskPrice4 * 10000); // 卖价 4 // 10000
	snapshot_future_temp.m_ask_volume[3] = pDepthMarketData->AskVolume4; // 卖量 4
	snapshot_future_temp.m_ask_price[4] = (uint32_t)(pDepthMarketData->AskPrice5 * 10000); // 卖价 5 // 10000
	snapshot_future_temp.m_ask_volume[4] = pDepthMarketData->AskVolume5; // 卖量 5
	snapshot_future_temp.m_bid_price[0] = (uint32_t)(pDepthMarketData->BidPrice1 * 10000); // 买价 1 // 10000
	snapshot_future_temp.m_bid_volume[0] = pDepthMarketData->BidVolume1; // 买量 1
	snapshot_future_temp.m_bid_price[1] = (uint32_t)(pDepthMarketData->BidPrice2 * 10000); // 买价 2 // 10000
	snapshot_future_temp.m_bid_volume[1] = pDepthMarketData->BidVolume2; // 买量 2
	snapshot_future_temp.m_bid_price[2] = (uint32_t)(pDepthMarketData->BidPrice3 * 10000); // 买价 3 // 10000
	snapshot_future_temp.m_bid_volume[2] = pDepthMarketData->BidVolume3; // 买量 3
	snapshot_future_temp.m_bid_price[3] = (uint32_t)(pDepthMarketData->BidPrice4 * 10000); // 买价 4 // 10000
	snapshot_future_temp.m_bid_volume[3] = pDepthMarketData->BidVolume4; // 买量 4
	snapshot_future_temp.m_bid_price[4] = (uint32_t)(pDepthMarketData->BidPrice5 * 10000); // 买价 5 // 10000
	snapshot_future_temp.m_bid_volume[4] = pDepthMarketData->BidVolume5; // 买量 5
	snapshot_future_temp.m_high_limit = (uint32_t)(pDepthMarketData->UpperLimitPrice * 10000); // 涨停价 // 10000
	snapshot_future_temp.m_low_limit = (uint32_t)(pDepthMarketData->LowerLimitPrice * 10000); // 跌停价 // 10000
	snapshot_future_temp.m_settle = (uint32_t)(pDepthMarketData->SettlementPrice * 10000); // 今日结算价 // 10000
	snapshot_future_temp.m_pre_settle = (uint32_t)(pDepthMarketData->PreSettlementPrice * 10000); // 昨日结算价 // 10000
	snapshot_future_temp.m_position = (int32_t)pDepthMarketData->OpenInterest; // 今日持仓量
	snapshot_future_temp.m_pre_position = (int32_t)pDepthMarketData->PreOpenInterest; // 昨日持仓量
	snapshot_future_temp.m_average = (uint32_t)(pDepthMarketData->AveragePrice * 10000); // 均价 // 10000
	snapshot_future_temp.m_up_down = 0; // 涨跌 // 10000 // 无
	snapshot_future_temp.m_up_down_rate = 0; // 涨跌幅度 // 10000 // 无
	snapshot_future_temp.m_swing = 0; // 振幅 // 10000 // 无
	snapshot_future_temp.m_delta = (int32_t)(pDepthMarketData->CurrDelta * 10000); // 今日虚实度 // 10000
	snapshot_future_temp.m_pre_delta = (int32_t)(pDepthMarketData->PreDelta * 10000); // 昨日虚实度 // 10000
	snapshot_future_temp.m_quote_date = atoi( pDepthMarketData->TradingDay ); // 行情日期 // YYYYMMDD
	char c_quote_time[12] = { 0 };
	sprintf( c_quote_time, "%s%03d", pDepthMarketData->UpdateTime, pDepthMarketData->UpdateMillisec ); // HH:MM:SSmmm
	std::string s_quote_time( c_quote_time );
	basicx::StringReplace( s_quote_time, ":", "" );
	snapshot_future_temp.m_quote_time = atoi( s_quote_time.c_str() ); // 行情时间 // HHMMSSmmm 精度：毫秒
	snapshot_future_temp.m_local_date = ( now_time_t.tm_year + 1900 ) * 10000 + ( now_time_t.tm_mon + 1 ) * 100 + now_time_t.tm_mday; // 本地日期 // YYYYMMDD
	snapshot_future_temp.m_local_time = now_time_t.tm_hour * 10000000 + now_time_t.tm_min * 100000 + now_time_t.tm_sec * 1000 + sys_time.wMilliseconds; // 本地时间 // HHMMSSmmm 精度：毫秒
	m_quoter_ctp_p->m_cache_snapshot_future.m_local_index++;
	snapshot_future_temp.m_local_index = m_quoter_ctp_p->m_cache_snapshot_future.m_local_index; // 本地序号

	m_quoter_ctp_p->m_cache_snapshot_future.m_recv_num++;

	m_quoter_ctp_p->MS_AddData_SnapshotFuture( snapshot_future_temp );

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "行情反馈：合约:{0} 市场:{1} 现价:{2} 最高价:{3} 最低价:{4} 卖一价:{5} 卖一量:{6} 买一价:{7} 买一量:{8}", 
	//	pDepthMarketData->InstrumentID, pDepthMarketData->ExchangeID, pDepthMarketData->LastPrice, pDepthMarketData->HighestPrice, pDepthMarketData->LowestPrice, 
	//	pDepthMarketData->AskPrice1, pDepthMarketData->AskVolume1, pDepthMarketData->BidPrice1, pDepthMarketData->BidVolume1 );
	//m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_debug, m_log_cate, log_info );
}

CThostFtdcTraderSpiImpl::CThostFtdcTraderSpiImpl( CThostFtdcTraderApi* user_api, QuoterCTP_P* quoter_ctp_p )
	: m_user_api( user_api )
	, m_quoter_ctp_p( quoter_ctp_p )
	, m_log_cate( "<Quoter_CTP>" )
	, m_last_rsp_is_error( false )
	, m_last_error_msg( "无错误信息。" )
	, m_request_id( 0 )
	, m_connect_ok( false )
	, m_login_ok( false )
	, m_qry_instrument( false )
	, m_logout_ok( false )
	, m_front_id( 0 )
	, m_session_id( 0 ) {
}

CThostFtdcTraderSpiImpl::~CThostFtdcTraderSpiImpl() {
}

void CThostFtdcTraderSpiImpl::OnFrontConnected() {
	m_connect_ok = true;

	std::string log_info;
	log_info = "交易前置连接成功。";
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
}

void CThostFtdcTraderSpiImpl::OnFrontDisconnected( int nReason ) { // CTP的API会自动重新连接
	m_connect_ok = false;

	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "交易前置连接中断！{0}", nReason );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );
}

void CThostFtdcTraderSpiImpl::OnHeartBeatWarning( int nTimeLapse ) {
	std::string log_info;
	FormatLibrary::StandardLibrary::FormatTo( log_info, "心跳超时警告！{0}", nTimeLapse );
	m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_warn, m_log_cate, log_info );
}

int32_t CThostFtdcTraderSpiImpl::ReqUserLogin( std::string broker_id, std::string user_id, std::string password ) {
	CThostFtdcReqUserLoginField req;
	memset( &req, 0, sizeof( req ) );
	strcpy_s( req.BrokerID, const_cast<char*>(broker_id.c_str()) ); // 经纪公司代码 char 11
	strcpy_s( req.UserID, const_cast<char*>(user_id.c_str()) ); // 用户代码 char 16
	strcpy_s( req.Password, const_cast<char*>(password.c_str()) ); // 密码 char 41
	//req.TradingDay; // 交易日 char 9
	//req.UserProductInfo; // 用户端产品信息 char 11
	//req.InterfaceProductInfo; // 接口端产品信息 char 11
	//req.ProtocolInfo; // 协议信息 char 11
	//req.MacAddress; // Mac地址 char 21
	//req.OneTimePassword; // 动态密码 char 41
	//req.ClientIPAddress; // 终端IP地址 char 16
	m_login_ok = false; // 只用于首次登录成功标记
	m_last_rsp_is_error = false;
	int32_t request_id = GetRequestID();
	return m_user_api->ReqUserLogin( &req, request_id );
}

void CThostFtdcTraderSpiImpl::OnRspUserLogin( CThostFtdcRspUserLoginField* pRspUserLogin, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) {
	std::string log_info;
	if( ( pRspInfo && pRspInfo->ErrorID != 0 ) || !pRspUserLogin ) {
		m_last_error_msg = pRspInfo->ErrorMsg;
		m_last_rsp_is_error = true;
		FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登录失败！{0}", m_last_error_msg );
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}
	else {
		m_login_ok = true; // 只用于首次登录成功标记
		m_front_id = pRspUserLogin->FrontID; // 前置编号 int
		m_session_id = pRspUserLogin->SessionID; // 会话编号 int
		//m_trader_ctp_p->m_order_ref = atoi( pRspUserLogin->MaxOrderRef ); // 最大报单引用 char 13 // 每次新建会话 CTP 都会重置 OrderRef 为1，所以这里以自己计数为准
		//pRspUserLogin->TradingDay; // 交易日 char 9
		//pRspUserLogin->LoginTime; // 登录成功时间 char 9
		//pRspUserLogin->BrokerID; // 经纪公司代码 char 11
		//pRspUserLogin->UserID; // 用户代码 char 16
		//pRspUserLogin->SystemName; // 交易系统名称 char 41
		//pRspUserLogin->SHFETime; // 上期所时间 char 9
		//pRspUserLogin->DCETime; // 大商所时间 char 9
		//pRspUserLogin->CZCETime; // 郑商所时间 char 9
		//pRspUserLogin->FFEXTime; // 中金所时间 char 9
		FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登录成功。当前交易日：{0}", pRspUserLogin->TradingDay );
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
	}
}

int32_t CThostFtdcTraderSpiImpl::ReqUserLogout( std::string broker_id, std::string user_id ) {
	CThostFtdcUserLogoutField req;
	memset( &req, 0, sizeof( req ) );
	strcpy_s( req.BrokerID, const_cast<char*>(broker_id.c_str()) ); // 经纪公司代码 char 11
	strcpy_s( req.UserID, const_cast<char*>(user_id.c_str()) ); // 用户代码 char 16
	m_logout_ok = false; // 只用于末次登出成功标记
	m_last_rsp_is_error = false;
	int32_t request_id = GetRequestID();
	return m_user_api->ReqUserLogout( &req, request_id );
}

void CThostFtdcTraderSpiImpl::OnRspUserLogout( CThostFtdcUserLogoutField* pUserLogout, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) {
	std::string log_info;
	if( ( pRspInfo && pRspInfo->ErrorID != 0 ) || !pUserLogout ) {
		m_last_error_msg = pRspInfo->ErrorMsg;
		m_last_rsp_is_error = true;
		FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登出失败！{0}", m_last_error_msg );
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}
	else {
		m_logout_ok = true; // 只用于末次登出成功标记
		//pUserLogout->BrokerID; // 经纪公司代码 char 11
		//pUserLogout->UserID; // 用户代码 char 16
		FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登出成功。{0} {1}", pUserLogout->BrokerID, pUserLogout->UserID );
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info );
	}
}

int32_t CThostFtdcTraderSpiImpl::ReqQryInstrument() {
	CThostFtdcQryInstrumentField req;
	memset( &req, 0, sizeof( req ) );
	//req.InstrumentID; // 合约代码 char 31 // 为空表示查询所有合约
	//req.ExchangeID; // 交易所代码 char 9
	//req.ExchangeInstID; // 合约在交易所的代码 char 31
	//req.ProductID; // 产品代码 char 31
	m_qry_instrument = false; //
	int32_t request_id = GetRequestID();
	return m_user_api->ReqQryInstrument( &req, request_id );
}

void CThostFtdcTraderSpiImpl::OnRspQryInstrument( CThostFtdcInstrumentField* pInstrument, CThostFtdcRspInfoField* pRspInfo, int nRequestID, bool bIsLast ) {
	std::string log_info;
	
	if( ( pRspInfo && pRspInfo->ErrorID != 0 ) || !pInstrument ) {
		if( pRspInfo ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "期货合约查询提交失败！{0}", pRspInfo->ErrorMsg );
		}
		else {
			log_info = "期货合约查询提交失败！原因未知！";
		}
		m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_error, m_log_cate, log_info );
	}
	else {
		if( true == bIsLast ) { //
			m_qry_instrument = true;
		}

		std::string contract_code = pInstrument->InstrumentID; // 合约代码 char 31
		std::string exchange_id = pInstrument->ExchangeID; // 交易所代码 char 9
		if( contract_code != "" && contract_code.length() < 8 ) { // 有些测试环境(永安期货)会查询到类似 IO1402-C-2550 这样的代码导致行情接收时代码字段溢出崩溃
			m_quoter_ctp_p->m_vec_contract.push_back( contract_code );
			FormatLibrary::StandardLibrary::FormatTo( log_info, "合约代码：{0} {1}", contract_code, exchange_id );
			m_quoter_ctp_p->LogPrint( basicx::syslog_level::c_info, m_log_cate, log_info, FILE_LOG_ONLY );
		}
	}
}

int32_t CThostFtdcTraderSpiImpl::GetRequestID() {
	m_request_id++;
	if( m_request_id > 2147483600 ) { // 2147483648
		m_request_id = 1;
	}
	return m_request_id;
}

std::string CThostFtdcTraderSpiImpl::GetLastErrorMsg() {
	std::string last_error_msg = m_last_error_msg;
	m_last_error_msg = "无错误信息。";
	return last_error_msg;
}

// 预设 QuoterCTP 函数实现

QuoterCTP::QuoterCTP()
	: basicx::Plugins_X( PLUGIN_NAME )
	, m_quoter_ctp_p( nullptr ) {
	try {
		m_quoter_ctp_p = new QuoterCTP_P();
	}
	catch( ... ) {
	}
}

QuoterCTP::~QuoterCTP() {
	if( m_quoter_ctp_p != nullptr ) {
		delete m_quoter_ctp_p;
		m_quoter_ctp_p = nullptr;
	}
}

bool QuoterCTP::Initialize() {
	return m_quoter_ctp_p->Initialize();
}

bool QuoterCTP::InitializeExt() {
	return m_quoter_ctp_p->InitializeExt();
}

bool QuoterCTP::StartPlugin() {
	return m_quoter_ctp_p->StartPlugin();
}

bool QuoterCTP::IsPluginRun() {
	return m_quoter_ctp_p->IsPluginRun();
}

bool QuoterCTP::StopPlugin() {
	return m_quoter_ctp_p->StopPlugin();
}

bool QuoterCTP::UninitializeExt() {
	return m_quoter_ctp_p->UninitializeExt();
}

bool QuoterCTP::Uninitialize() {
	return m_quoter_ctp_p->Uninitialize();
}

bool QuoterCTP::AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
	return m_quoter_ctp_p->AssignTask( task_id, identity, code, data );
}

// 自定义 QuoterCTP 函数实现
