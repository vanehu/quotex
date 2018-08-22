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

#include <fstream>
#include <share.h> // _fsopen
#include <algorithm> // transform

#include <common/winver.h>
#include <common/common.h>
#include <syslog/syslog.h>
#include <syscfg/syscfg.h>
#include <sysrtm/sysrtm.h>
#include <plugins/plugins.h>

#include "define_sgt.h"
#include "quoter_sgt.h"
#include "quoter_sgt_.h"

#define FILE_LOG_ONLY 1

#define NET_CLIENT_USE_ASIO
//#define NET_CLIENT_USE_SOCK

QuoterSGT the_app; // 必须

// 预设 QuoterSGT_P 函数实现

QuoterSGT_P::QuoterSGT_P()
	: NetServer_X()
	, m_location( "" )
	, m_info_file_path( "" )
	, m_cfg_file_path( "" )
	, m_service_running( false )
	, m_work_thread_number( CFG_WORK_THREAD_NUM )
	, m_plugin_running( false )
	, m_task_id( 0 )
	, m_app_ver_id( "1.02" ) // 深交所行情文档版本号
	, m_connect_ok( false )
	, m_logon_ok( false )
	, m_logout_ok( false )
	, m_connect_quote( nullptr )
	, m_mdgw_data_cache( nullptr )
	, m_mdgw_data_cache_size( 0 )
	, m_log_cate( "<Quoter_SGT>" ) {
	m_json_reader_builder["collectComments"] = false;
	m_json_reader = m_json_reader_builder.newCharReader();
	m_syslog = basicx::SysLog_S::GetInstance();
	m_syscfg = basicx::SysCfg_S::GetInstance();
	m_sysrtm = basicx::SysRtm_S::GetInstance();
	m_plugins = basicx::Plugins::GetInstance();
}

QuoterSGT_P::~QuoterSGT_P() {
	if( m_output_buf_snapshot_stock != nullptr ) {
		delete[] m_output_buf_snapshot_stock;
		m_output_buf_snapshot_stock = nullptr;
	}

	for( auto it = m_csm_snapshot_stock.m_map_con_sub_one.begin(); it != m_csm_snapshot_stock.m_map_con_sub_one.end(); it++ ) {
		if( it->second != nullptr ) {
			delete it->second;
		}
	}
	m_csm_snapshot_stock.m_map_con_sub_one.clear();

	for( auto it = m_csm_snapshot_stock.m_list_one_sub_con_del.begin(); it != m_csm_snapshot_stock.m_list_one_sub_con_del.end(); it++ ) {
		if( (*it) != nullptr ) {
			delete (*it);
		}
	}
	m_csm_snapshot_stock.m_list_one_sub_con_del.clear();

	if( m_mdgw_data_cache != nullptr ) {
		delete[] m_mdgw_data_cache;
		m_mdgw_data_cache = nullptr;
	}
}

void QuoterSGT_P::SetGlobalPath() {
	m_location = m_plugins->GetPluginLocationByName( PLUGIN_NAME );
	m_cfg_file_path = m_plugins->GetPluginCfgFilePathByName( PLUGIN_NAME );
	m_info_file_path = m_plugins->GetPluginInfoFilePathByName( PLUGIN_NAME );
}

bool QuoterSGT_P::ReadConfig( std::string file_path ) {
	std::string log_info;

	pugi::xml_document document;
	if( !document.load_file( file_path.c_str() ) ) {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "打开插件参数配置信息文件失败！{0}", file_path );
		LogPrint( basicx::syslog_level::c_error, log_info );
		return false;
	}

	pugi::xml_node node_plugin = document.document_element();
	if( node_plugin.empty() ) {
		log_info = "获取插件参数配置信息 根节点 失败！";
		LogPrint( basicx::syslog_level::c_error, log_info );
		return false;
	}
	
	m_configs.m_address = node_plugin.child_value( "Address" );
	m_configs.m_port = atoi( node_plugin.child_value( "Port" ) );
	m_configs.m_username = node_plugin.child_value( "Username" );
	m_configs.m_password = node_plugin.child_value( "Password" );
	m_configs.m_sender_id = node_plugin.child_value( "SenderID" );
	m_configs.m_timeout = atoi( node_plugin.child_value( "Timeout" ) );
	m_configs.m_need_dump = atoi( node_plugin.child_value( "NeedDump" ) );
	m_configs.m_dump_path = node_plugin.child_value( "DumpPath" );
	m_configs.m_data_compress = atoi( node_plugin.child_value( "DataCompress" ) );
	m_configs.m_data_encode = atoi( node_plugin.child_value( "DataEncode" ) );
	m_configs.m_dump_time = atoi( node_plugin.child_value( "DumpTime" ) );
	m_configs.m_init_time = atoi( node_plugin.child_value( "InitTime" ) );

	//FormatLibrary::StandardLibrary::FormatTo( log_info, "{0} {1} {2} {3} {4} {5}", m_configs.m_address, m_configs.m_port, 
	// m_configs.m_username, m_configs.m_password, m_configs.m_sender_id, m_configs.m_timeout );
	//LogPrint( basicx::syslog_level::c_debug, log_info );

	log_info = "插件参数配置信息读取完成。";
	LogPrint( basicx::syslog_level::c_info, log_info );

	return true;
}

void QuoterSGT_P::LogPrint( basicx::syslog_level log_level, std::string& log_info, int32_t log_show/* = 0*/ ) {
	if( 0 == log_show ) {
		m_syslog->LogPrint( log_level, m_log_cate, "LOG>: SGT " + log_info ); // 控制台
		m_sysrtm->LogTrans( log_level, m_log_cate, log_info ); // 远程监控
	}
	m_syslog->LogWrite( log_level, m_log_cate, log_info );
}

bool QuoterSGT_P::Initialize() {
	SetGlobalPath();
	ReadConfig( m_cfg_file_path );

	m_thread_service = boost::make_shared<boost::thread>( boost::bind( &QuoterSGT_P::CreateService, this ) );
	m_thread_user_request = boost::make_shared<boost::thread>( boost::bind( &QuoterSGT_P::HandleUserRequest, this ) );

	return true;
}

bool QuoterSGT_P::InitializeExt() {
	return true;
}

bool QuoterSGT_P::StartPlugin() {
	std::string log_info;

	try {
		log_info = "开始启用插件 ....";
		LogPrint( basicx::syslog_level::c_info, log_info );

		if( CheckSingleMutex() == false ) { // 单例限制检测
		    return false;
		}

		StartNetServer();
		m_thread_on_timer = boost::make_shared<boost::thread>( boost::bind( &QuoterSGT_P::OnTimer, this ) );

		// TODO：添加更多初始化任务

		log_info = "插件启用完成。";
		LogPrint( basicx::syslog_level::c_info, log_info );

		m_plugin_running = true; // 需要在创建线程前赋值为真

		return true;
	} // try
	catch( ... ) {
		log_info = "插件启用时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}

	return false;
}

bool QuoterSGT_P::IsPluginRun() {
	return m_plugin_running;
}

bool QuoterSGT_P::StopPlugin() {
	std::string log_info;

	try {
		log_info = "开始停止插件 ....";
		LogPrint( basicx::syslog_level::c_info, log_info );

		// TODO：添加更多反初始化任务

		log_info = "插件停止完成。";
		LogPrint( basicx::syslog_level::c_info, log_info );

		m_plugin_running = false;

		return true;
	} // try
	catch( ... ) {
		log_info = "插件停止时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}

	return false;
}

bool QuoterSGT_P::UninitializeExt() {
	return true;
}

bool QuoterSGT_P::Uninitialize() {
	if( true == m_service_running ) {
		m_service_running = false;
		m_service->stop();
	}
	
	return true;
}

bool QuoterSGT_P::AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
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
			m_service->post( boost::bind( &QuoterSGT_P::HandleTaskMsg, this ) );
		}

		return true;
	}
	catch( std::exception& ex ) {
		std::string log_info;
		FormatLibrary::StandardLibrary::FormatTo( log_info, "添加 TaskItem 消息 异常：{0}", ex.what() );
		LogPrint( basicx::syslog_level::c_error, log_info );
	}

	return false;
}

void QuoterSGT_P::CreateService() {
	std::string log_info;

	log_info = "创建输入输出服务线程完成, 开始进行输入输出服务 ...";
	LogPrint( basicx::syslog_level::c_info, log_info );

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
			LogPrint( basicx::syslog_level::c_error, log_info );
		}
	} // try
	catch( ... ) {
		log_info = "输入输出服务线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, log_info );
	}

	if( true == m_service_running ) {
		m_service_running = false;
		m_service->stop();
	}

	log_info = "输入输出服务线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

void QuoterSGT_P::HandleTaskMsg() {
	std::string log_info;

	try {
		std::string result_data;
		TaskItem* task_item = &m_list_task.front(); // 肯定有

		//FormatLibrary::StandardLibrary::FormatTo( log_info, "开始处理 {0} 号任务 ...", task_item->m_task_id );
		//LogPrint( basicx::syslog_level::c_info, log_info );

		try {
			Request request_temp;
			request_temp.m_task_id = task_item->m_task_id;
			request_temp.m_identity = task_item->m_identity;
			request_temp.m_code = task_item->m_code;

			int32_t ret_func = 0;
			if( NW_MSG_CODE_JSON == task_item->m_code ) {

				std::string errors_json;
				if( m_json_reader->parse( task_item->m_data.c_str(), task_item->m_data.c_str() + task_item->m_data.length(), &request_temp.m_req_json, &errors_json ) ) { // 含中文：std::string strUser = StringToGB2312( jsRootR["user"].asString() );
					ret_func = request_temp.m_req_json["function"].asInt();
				}
				else {
					FormatLibrary::StandardLibrary::FormatTo( log_info, "处理任务 {0} 时数据 JSON 解析失败！{1}", task_item->m_task_id, errors_json );
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
		//LogPrint( basicx::syslog_level::c_info, log_info );

		m_task_list_lock.lock();
		m_list_task.pop_front();
		bool write_on_progress = !m_list_task.empty();
		m_task_list_lock.unlock();

		if( write_on_progress && true == m_service_running ) {
			m_service->post( boost::bind( &QuoterSGT_P::HandleTaskMsg, this ) );
		}
	}
	catch( std::exception& ex ) {
		FormatLibrary::StandardLibrary::FormatTo( log_info, "处理 TaskItem 消息 异常：{0}", ex.what() );
		LogPrint( basicx::syslog_level::c_error, log_info );
	}
}

// 自定义 QuoterSGT_P 函数实现

bool QuoterSGT_P::CheckSingleMutex() { // 单例限制检测
	m_single_mutex = ::CreateMutex( NULL, FALSE, L"quoter_sgt" );
	if( GetLastError() == ERROR_ALREADY_EXISTS ) {
		std::string log_info = "在一台服务器上只允许运行本插件一个实例！";
		LogPrint( basicx::syslog_level::c_error, log_info );
		CloseHandle( m_single_mutex );
		return false;
	}
	return true;
}

void QuoterSGT_P::OnTimer() {
	std::string log_info;

	try {
		if( CreateDumpFolder() ) { // 含首次开启时文件创建
#ifdef NET_CLIENT_USE_ASIO
			m_thread_handle_market_data = boost::make_shared<boost::thread>( boost::bind( &QuoterSGT_P::HandleMarketData_Asio, this ) );
#endif
#ifdef NET_CLIENT_USE_SOCK
			m_thread_handle_market_data = boost::make_shared<boost::thread>( boost::bind( &QuoterSGT_P::HandleMarketData_Sock, this ) );
#endif

			bool force_dump = false; // 避免瞬间多次调用 Dump 操作
			bool init_quote_receive = false;
			bool init_quote_data_file = false;
			while( true ) {
				if( false == m_connect_ok ) { // 进行自动重连
#ifdef NET_CLIENT_USE_ASIO
					QuoteLogin_Asio();
#endif
#ifdef NET_CLIENT_USE_SOCK
					QuoteLogin_Sock();
#endif
				}

				for( size_t i = 0; i < (size_t)m_configs.m_dump_time; i++ ) { // 间隔需小于60秒
					std::this_thread::sleep_for( std::chrono::milliseconds( 1000 ) );
					//if( 前置行情服务器启动 )
					//{
					//	DumpSnapshotStock();
					//	force_dump = true;
					//}
				}
				if( false == force_dump ) {
					DumpSnapshotStock();
				}
				force_dump = false;

#ifdef NET_CLIENT_USE_ASIO
				SendHeartCheck_Asio();
#endif
#ifdef NET_CLIENT_USE_SOCK
				SendHeartCheck_Sock();
#endif

				tm now_time_t = basicx::GetNowTime();
				int32_t now_time = now_time_t.tm_hour * 100 + now_time_t.tm_min;

				char time_temp[32];
				strftime( time_temp, 32, "%H%M%S", &now_time_t);
				FormatLibrary::StandardLibrary::FormatTo( log_info, "{0} SnapshotStock：R:{1}，W:{2}，C:{3}，S:{4}。", 
					time_temp, m_cache_snapshot_stock.m_recv_num.load(), m_cache_snapshot_stock.m_dump_num.load(), m_cache_snapshot_stock.m_comp_num.load(), m_cache_snapshot_stock.m_send_num.load() );
				LogPrint( basicx::syslog_level::c_info, log_info );

				m_cache_snapshot_stock.m_dump_num = 0;
				m_cache_snapshot_stock.m_comp_num = 0;
				m_cache_snapshot_stock.m_send_num = 0;

				if( now_time == m_configs.m_init_time && false == init_quote_receive ) {
#ifdef NET_CLIENT_USE_ASIO
					QuoteLogout_Asio(); // 重连重登
#endif
#ifdef NET_CLIENT_USE_SOCK
					QuoteLogout_Sock(); // 重连重登
#endif
					init_quote_receive = true;
				}

				if( 600 == now_time && false == init_quote_data_file ) {
					InitQuoteDataFile();
					init_quote_data_file = true;

					ClearUnavailableConSub( m_csm_snapshot_stock, true );
					log_info = "清理无效订阅。";
					LogPrint( basicx::syslog_level::c_info, log_info );
				}

				if( 500 == now_time ) {
					init_quote_receive = false;
					init_quote_data_file = false;
				}
			}
		}
	} // try
	catch( ... ) {
		log_info = "插件定时线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, log_info );
	}

	log_info = "插件定时线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

bool QuoterSGT_P::CreateDumpFolder() {
	std::string log_info;

	std::string dump_data_folder_path = m_configs.m_dump_path;
	basicx::StringRightTrim( dump_data_folder_path, std::string( "\\" ) );

	WIN32_FIND_DATA find_dump_folder;
	HANDLE handle_dump_folder = FindFirstFile( basicx::StringToWideChar( dump_data_folder_path ).c_str(), &find_dump_folder );
	if( INVALID_HANDLE_VALUE == handle_dump_folder ) {
		FindClose( handle_dump_folder );
		FormatLibrary::StandardLibrary::FormatTo( log_info, "实时行情数据存储文件夹不存在！{0}", dump_data_folder_path );
		LogPrint( basicx::syslog_level::c_error, log_info );
		return false;
	}
	FindClose( handle_dump_folder );

	dump_data_folder_path += "\\Stock_SGT";
	CreateDirectoryA( dump_data_folder_path.c_str(), NULL );

	m_cache_snapshot_stock.m_folder_path = dump_data_folder_path + "\\Stock";
	CreateDirectoryA( m_cache_snapshot_stock.m_folder_path.c_str(), NULL );

	InitQuoteDataFile(); // 首次启动时

	return true;
}

void QuoterSGT_P::InitQuoteDataFile() {
	char date_temp[9];
	tm now_time_t = basicx::GetNowTime();
	strftime( date_temp, 9, "%Y%m%d", &now_time_t);
	FormatLibrary::StandardLibrary::FormatTo( m_cache_snapshot_stock.m_file_path, "{0}\\{1}.hq", m_cache_snapshot_stock.m_folder_path, date_temp );

	bool is_new_file = false;
	WIN32_FIND_DATA find_file_temp;
	HANDLE handle_find_file = FindFirstFile( basicx::StringToWideChar( m_cache_snapshot_stock.m_file_path ).c_str(), &find_file_temp );
	if( INVALID_HANDLE_VALUE == handle_find_file ) { // 数据文件不存在
		is_new_file = true;
	}
	FindClose( handle_find_file );

	if( true == is_new_file ) { // 创建并写入 32 字节文件头部
		std::ofstream file_snapshot_stock;
		file_snapshot_stock.open( m_cache_snapshot_stock.m_file_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app );
		if( file_snapshot_stock ) {
			char head_temp[32] = { 0 };
			head_temp[0] = SNAPSHOT_STOCK_SGT_VERSION; // 结构体版本
			file_snapshot_stock.write( head_temp, 32 );
			file_snapshot_stock.close();
		}
		else {
			std::string log_info;
			FormatLibrary::StandardLibrary::FormatTo( log_info, "SnapshotStock：创建转储文件失败！{0}", m_cache_snapshot_stock.m_file_path );
			LogPrint( basicx::syslog_level::c_error, log_info );
		}
	}

	m_cache_snapshot_stock.m_recv_num = 0;
	m_cache_snapshot_stock.m_dump_num = 0;
	m_cache_snapshot_stock.m_comp_num = 0;
	m_cache_snapshot_stock.m_send_num = 0;
	m_cache_snapshot_stock.m_local_index = 0;
}

void QuoterSGT_P::MS_AddData_SnapshotStock( SnapshotStock_SGT& snapshot_stock_temp ) {
	//if( m_net_server_broad->Server_GetConnectCount() > 0 )
	if( m_csm_snapshot_stock.m_map_con_sub_all.size() > 0 || m_csm_snapshot_stock.m_map_con_sub_one.size() > 0 ) { // 全市场和单证券可以分别压缩以减少CPU压力提高效率，但一般还是全市场订阅的多
		int32_t output_buf_len_snapshot_stock = m_output_buf_len_snapshot_stock; // 必须每次重新赋值，否则压缩出错
		memset( m_output_buf_snapshot_stock, 0, m_output_buf_len_snapshot_stock );
		int32_t result = gzCompress( (unsigned char*)&snapshot_stock_temp, sizeof( snapshot_stock_temp ), m_output_buf_snapshot_stock, (uLongf*)&output_buf_len_snapshot_stock, m_data_compress );
		if( Z_OK == result ) { // 数据已压缩
			m_cache_snapshot_stock.m_comp_num++;
			std::string quote_data( (char*)m_output_buf_snapshot_stock, output_buf_len_snapshot_stock );
			quote_data = TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_SGT_HEAD + quote_data;

			// 先给全市场的吧，全市场订阅者可能还要自己进行过滤

			{ // 广播给全市场订阅者
				m_csm_snapshot_stock.m_lock_con_sub_all.lock();
				std::map<int32_t, basicx::ConnectInfo*> map_con_sub_all = m_csm_snapshot_stock.m_map_con_sub_all;
				m_csm_snapshot_stock.m_lock_con_sub_all.unlock();

				for( auto it = map_con_sub_all.begin(); it != map_con_sub_all.end(); it++ ) {
					if( true == m_net_server_broad->IsConnectAvailable( it->second ) ) {
						m_net_server_broad->Server_SendData( it->second, NW_MSG_TYPE_USER_DATA, m_data_encode, quote_data );
						m_cache_snapshot_stock.m_send_num++;
					}
				}
			}

			{ // 广播给单证券订阅者
				m_csm_snapshot_stock.m_lock_con_sub_one.lock();
				std::map<std::string, ConSubOne*> map_con_sub_one = m_csm_snapshot_stock.m_map_con_sub_one;
				m_csm_snapshot_stock.m_lock_con_sub_one.unlock();

				std::map<std::string, ConSubOne*>::iterator it_cso = map_con_sub_one.find( snapshot_stock_temp.m_code );
				if( it_cso != map_con_sub_one.end() ) {
					ConSubOne* con_sub_one = it_cso->second; // 运行期间 con_sub_one 所指对象不会被删除

					con_sub_one->m_lock_con_sub_one.lock();
					std::map<int32_t, basicx::ConnectInfo*> map_identity = con_sub_one->m_map_identity;
					con_sub_one->m_lock_con_sub_one.unlock();

					for( auto it = map_identity.begin(); it != map_identity.end(); it++ ) {
						if( true == m_net_server_broad->IsConnectAvailable( it->second ) ) {
							m_net_server_broad->Server_SendData( it->second, NW_MSG_TYPE_USER_DATA, m_data_encode, quote_data );
							m_cache_snapshot_stock.m_send_num++;
						}
					}
				}
			}
		}
	}

	if( 1 == m_configs.m_need_dump ) {
		m_cache_snapshot_stock.m_lock_cache.lock();
		m_cache_snapshot_stock.m_vec_cache_put->push_back( snapshot_stock_temp );
		m_cache_snapshot_stock.m_lock_cache.unlock();
	}
}

void QuoterSGT_P::DumpSnapshotStock() {
	std::string log_info;

	m_cache_snapshot_stock.m_lock_cache.lock();
	if( m_cache_snapshot_stock.m_vec_cache_put == &m_cache_snapshot_stock.m_vec_cache_01 ) {
		m_cache_snapshot_stock.m_vec_cache_put = &m_cache_snapshot_stock.m_vec_cache_02;
		m_cache_snapshot_stock.m_vec_cache_put->clear();
		m_cache_snapshot_stock.m_vec_cache_out = &m_cache_snapshot_stock.m_vec_cache_01;
	}
	else {
		m_cache_snapshot_stock.m_vec_cache_put = &m_cache_snapshot_stock.m_vec_cache_01;
		m_cache_snapshot_stock.m_vec_cache_put->clear();
		m_cache_snapshot_stock.m_vec_cache_out = &m_cache_snapshot_stock.m_vec_cache_02;
	}
	m_cache_snapshot_stock.m_lock_cache.unlock();

	size_t out_data_number = m_cache_snapshot_stock.m_vec_cache_out->size();
	if( out_data_number > 0 ) { // 避免无新数据时操作文件
		std::ofstream file_snapshot_stock;
		file_snapshot_stock.open( m_cache_snapshot_stock.m_file_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app );
		if( file_snapshot_stock ) {
			for( size_t i = 0; i < out_data_number; ++i ) {
				file_snapshot_stock.write( (const char*)(&(*m_cache_snapshot_stock.m_vec_cache_out)[i]), sizeof( SnapshotStock_SGT ) );
			}
			file_snapshot_stock.close(); // 已含 flush() 动作
			m_cache_snapshot_stock.m_dump_num += out_data_number;
		}
		else {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "SnapshotStock：打开转储文件失败！{0}", m_cache_snapshot_stock.m_file_path );
			LogPrint( basicx::syslog_level::c_error, log_info );
		}
	}
}

void QuoterSGT_P::StartNetServer() {
	m_data_encode = m_configs.m_data_encode;
	m_data_compress = m_configs.m_data_compress;

	m_output_buf_len_snapshot_stock = sizeof( SnapshotStock_SGT ) + sizeof( SnapshotStock_SGT ) / 3 + 128; // > ( nInputLen + 12 ) * 1.001;
	m_output_buf_snapshot_stock = new unsigned char[m_output_buf_len_snapshot_stock]; // 需足够大

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
		if( "broad_sgt" == cfg_basic->m_vec_server_server[i].m_type && 1 == cfg_basic->m_vec_server_server[i].m_work ) {
			m_net_server_broad->Server_AddListen( "0.0.0.0", cfg_basic->m_vec_server_server[i].m_port, cfg_basic->m_vec_server_server[i].m_type ); // 0.0.0.0
		}
		if( "query_sgt" == cfg_basic->m_vec_server_server[i].m_type && 1 == cfg_basic->m_vec_server_server[i].m_work ) {
			m_net_server_query->Server_AddListen( "0.0.0.0", cfg_basic->m_vec_server_server[i].m_port, cfg_basic->m_vec_server_server[i].m_type ); // 0.0.0.0
		}
	}
}

void QuoterSGT_P::OnNetServerInfo( basicx::NetServerInfo& net_server_info_temp ) {
	// TODO：清理全市场订阅和单证券订阅
}

void QuoterSGT_P::OnNetServerData( basicx::NetServerData& net_server_data_temp ) {
	try {
		if( m_task_id >= 214748300/*0*/ ) { // 2 ^ 31 = 2147483648
			m_task_id = 0;
		}
		m_task_id++;

		if( "broad_sgt" == net_server_data_temp.m_node_type ) { // 1 // 节点类型必须和配置文件一致
			AssignTask( m_task_id * 10 + 1, net_server_data_temp.m_identity, net_server_data_temp.m_code, net_server_data_temp.m_data ); // 1
		}
		else if( "query_sgt" == net_server_data_temp.m_node_type ) { // 2 // 节点类型必须和配置文件一致
			AssignTask( m_task_id * 10 + 2, net_server_data_temp.m_identity, net_server_data_temp.m_code, net_server_data_temp.m_data ); // 2
		}
	}
	catch( ... ) {
		std::string log_info = "转发网络数据 发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}
}

int32_t QuoterSGT_P::gzCompress( Bytef* data_in, uLong size_in, Bytef* data_out, uLong* size_out, int32_t level ) {
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

void QuoterSGT_P::HandleUserRequest() {
	std::string log_info;

	log_info = "创建用户请求处理线程完成, 开始进行用户请求处理 ...";
	LogPrint( basicx::syslog_level::c_info, log_info );

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

			std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
		}
	} // try
	catch( ... ) {
		log_info = "用户请求处理线程发生未知错误！";
		LogPrint( basicx::syslog_level::c_fatal, log_info );
	}

	log_info = "用户请求处理线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

std::string QuoterSGT_P::OnUserAddSub( Request* request ) {
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

	if( TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_SGT == quote_type ) {
		if( "" == quote_list ) { // 订阅全市场
			AddConSubAll( m_csm_snapshot_stock, request->m_identity, connect_info );
			ClearConSubOne( m_csm_snapshot_stock, request->m_identity ); // 退订所有单个证券
		}
		else { // 指定证券
			if( IsConSubAll( m_csm_snapshot_stock, request->m_identity ) == false ) { // 未全市场订阅
				std::vector<std::string> vec_filter_temp;
				boost::split( vec_filter_temp, quote_list, boost::is_any_of( ", " ), boost::token_compress_on );
				size_t size_temp = vec_filter_temp.size();
				for( size_t i = 0; i < size_temp; i++ ) {
					if( vec_filter_temp[i] != "" ) {
						AddConSubOne( m_csm_snapshot_stock, vec_filter_temp[i], request->m_identity, connect_info );
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
	LogPrint( basicx::syslog_level::c_info, log_info );

	if( NW_MSG_CODE_JSON == request->m_code ) {
		Json::Value results_json;
		results_json["ret_func"] = TD_FUNC_QUOTE_ADDSUB;
		results_json["ret_code"] = 0;
		results_json["ret_info"] = basicx::StringToUTF8( log_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return Json::writeString( m_json_writer, results_json );
	}

	return "";
}

std::string QuoterSGT_P::OnUserDelSub( Request* request ) {
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

	if( TD_FUNC_QUOTE_DATA_SNAPSHOT_STOCK_SGT == quote_type ) {
		if( "" == quote_list ) { // 退订全市场
			DelConSubAll( m_csm_snapshot_stock, request->m_identity );
			ClearConSubOne( m_csm_snapshot_stock, request->m_identity ); // 退订所有单个证券
		}
		else { // 指定证券
			if( IsConSubAll( m_csm_snapshot_stock, request->m_identity ) == false ) { // 未全市场订阅，已订阅全市场的话不可能还有单证券订阅
				std::vector<std::string> vec_filter_temp;
				boost::split( vec_filter_temp, quote_list, boost::is_any_of( ", " ), boost::token_compress_on );
				size_t size_temp = vec_filter_temp.size();
				for( size_t i = 0; i < size_temp; i++ ) {
					if( vec_filter_temp[i] != "" ) {
						DelConSubOne( m_csm_snapshot_stock, vec_filter_temp[i], request->m_identity );
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
	LogPrint( basicx::syslog_level::c_info, log_info );

	if( NW_MSG_CODE_JSON == request->m_code ) {
		Json::Value results_json;
		results_json["ret_func"] = TD_FUNC_QUOTE_DELSUB;
		results_json["ret_code"] = 0;
		results_json["ret_info"] = basicx::StringToUTF8( log_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return Json::writeString( m_json_writer, results_json );
	}

	return "";
}

void QuoterSGT_P::AddConSubAll( ConSubMan& csm_con_sub_man, int32_t identity, basicx::ConnectInfo* connect_info ) {
	csm_con_sub_man.m_lock_con_sub_all.lock();
	csm_con_sub_man.m_map_con_sub_all[identity] = connect_info;
	csm_con_sub_man.m_lock_con_sub_all.unlock();

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "订阅 全市场：{0}", identity );
	//LogPrint( basicx::syslog_level::c_debug, log_info );
}

void QuoterSGT_P::DelConSubAll( ConSubMan& csm_con_sub_man, int32_t identity ) {
	csm_con_sub_man.m_lock_con_sub_all.lock();
	csm_con_sub_man.m_map_con_sub_all.erase( identity );
	csm_con_sub_man.m_lock_con_sub_all.unlock();

	//std::string log_info;
	//FormatLibrary::StandardLibrary::FormatTo( log_info, "退订 全市场：{0}", identity );
	//LogPrint( basicx::syslog_level::c_debug, log_info );
}

bool QuoterSGT_P::IsConSubAll( ConSubMan& csm_con_sub_man, int32_t identity ) {
	bool is_all_sub_already = false;
	csm_con_sub_man.m_lock_con_sub_all.lock();
	std::map<int32_t, basicx::ConnectInfo*>::iterator it_csa = csm_con_sub_man.m_map_con_sub_all.find( identity );
	if( it_csa != csm_con_sub_man.m_map_con_sub_all.end() ) {
		is_all_sub_already = true;
	}
	csm_con_sub_man.m_lock_con_sub_all.unlock();
	return is_all_sub_already;
}

void QuoterSGT_P::AddConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity, basicx::ConnectInfo* connect_info ) {
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
	//LogPrint( basicx::syslog_level::c_debug, log_info );
}

void QuoterSGT_P::DelConSubOne( ConSubMan& csm_con_sub_man, std::string& code, int32_t identity ) {
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
		//LogPrint( basicx::syslog_level::c_debug, log_info );
	}
	csm_con_sub_man.m_lock_con_sub_one.unlock();
}

void QuoterSGT_P::ClearConSubOne( ConSubMan& csm_con_sub_man, int32_t identity ) {
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

void QuoterSGT_P::ClearUnavailableConSub( ConSubMan& csm_con_sub_man, bool is_idle_time ) { // 清理所有已经失效的连接订阅，一般在空闲时进行
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

void QuoterSGT_P::CommitResult( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
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

std::string QuoterSGT_P::OnErrorResult( int32_t ret_func, int32_t ret_code, std::string ret_info, int32_t encode ) {
	LogPrint( basicx::syslog_level::c_error, ret_info );

	if( NW_MSG_CODE_JSON == encode ) {
		Json::Value results_json;
		results_json["ret_func"] = ret_func;
		results_json["ret_code"] = ret_code;
		results_json["ret_info"] = basicx::StringToUTF8( ret_info ); // 中文 -> 客户端
		results_json["ret_numb"] = 0;
		results_json["ret_data"] = "";
		return Json::writeString( m_json_writer, results_json );
	}

	return "";
}

////////////////////////////// Asio //////////////////////////////

int32_t QuoterSGT_P::QuoteLogin_Asio() {
	std::string log_info;

	QuoteLogout_Asio();

	if( false == m_asio_client.IsNetworkStarted() ) {
		m_asio_client.StartNetwork();
	}

	try {
		log_info = "开始用户登录发送连接请求 ....";
		LogPrint( basicx::syslog_level::c_info, log_info );

		m_connect_quote = m_asio_client.Client_AddConnect( m_configs.m_address, m_configs.m_port, "connect_quote", m_configs.m_timeout );
		if( nullptr == m_connect_quote ) {
			log_info = "用户登录连接失败！";
			LogPrint( basicx::syslog_level::c_error, log_info );
			return -1;
		}
		else {
			log_info = "用户登录连接成功。";
			LogPrint( basicx::syslog_level::c_info, log_info );
		}

		m_logon_ok = false; //

		MDGW_Logon mdgw_logon;
		memset( &mdgw_logon, 0, sizeof( mdgw_logon ) );
		mdgw_logon.m_Header.m_MsgType = htonl( DEF_MDGW_MSG_TYPE_LOGON );
		mdgw_logon.m_Header.m_BodyLength = htonl( g_size_mdgw_logon_body );
		strncpy_s( mdgw_logon.m_SenderCompID, sizeof( mdgw_logon.m_SenderCompID ), m_configs.m_sender_id.c_str(), m_configs.m_sender_id.length() );
		strncpy_s( mdgw_logon.m_TargetCompID, sizeof( mdgw_logon.m_TargetCompID ), m_configs.m_username.c_str(), m_configs.m_username.length() ); // 填登录用户名
		mdgw_logon.m_HeartBtInt = htonl( m_configs.m_dump_time * 2 ); // * 2
		strncpy_s( mdgw_logon.m_Password, sizeof( mdgw_logon.m_Password ), m_configs.m_password.c_str(), m_configs.m_password.length() );
		strncpy_s( mdgw_logon.m_DefaultApplVerID, sizeof( mdgw_logon.m_DefaultApplVerID ), m_app_ver_id.c_str(), m_app_ver_id.length() );
		mdgw_logon.mdgw_tailer.m_Checksum = htonl( MDGW_GenerateCheckSum( (char*)&mdgw_logon, g_size_mdgw_header + g_size_mdgw_logon_body ) );

		try {
			int32_t net_ret = m_asio_client.Client_SendData_Sy( m_connect_quote, (const char*)&mdgw_logon, sizeof( mdgw_logon ) );
			if( net_ret <= 0 ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "发送用户登录请求时读写错误！{0}", net_ret );
				LogPrint( basicx::syslog_level::c_error, log_info );
				throw 1;
			}

			log_info = "用户登录请求发送完成。";
			LogPrint( basicx::syslog_level::c_info, log_info );

			int32_t time_wait = 0;
			while( false == m_logon_ok && time_wait < 5000 ) { // 最多等待 5 秒
				std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
				time_wait += 100;
			}
			if( true == m_logon_ok ) {
				m_connect_ok = true; //
				log_info = "用户登录请求成功。";
				LogPrint( basicx::syslog_level::c_info, log_info );
			}
			else {
				log_info = "用户登录请求超时！";
				LogPrint( basicx::syslog_level::c_error, log_info );
				return -2;
			}
		}
		catch( int32_t error ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "发送用户登录请求时读写异常！{0}", error );
			LogPrint( basicx::syslog_level::c_error, log_info );
			return -3;
		}
		catch( ... ) {
			log_info = "发送用户登录请求时发生未知错误！";
			LogPrint( basicx::syslog_level::c_error, log_info );
			return -4;
		}
	}
	catch( ... ) {
		log_info = "用户登录时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, log_info );
		return -5;
	}

	return 0;
}

void QuoterSGT_P::QuoteLogout_Asio() {
	std::string log_info;

	m_logout_ok = false; //

	if( true == m_connect_ok ) {
		std::string logout_text = "bye_bye";

		MDGW_Logout mdgw_logout;
		memset( &mdgw_logout, 0, sizeof( mdgw_logout ) );
		mdgw_logout.m_Header.m_MsgType = htonl( DEF_MDGW_MSG_TYPE_LOGOUT );
		mdgw_logout.m_Header.m_BodyLength = htonl( g_size_mdgw_logout_body );
		mdgw_logout.m_SessionStatus = htonl( 4 ); // 就用 会话退登完成 吧
		strncpy_s( mdgw_logout.m_Text, sizeof( mdgw_logout.m_Text ), logout_text.c_str(), logout_text.length() );
		mdgw_logout.mdgw_tailer.m_Checksum = htonl( MDGW_GenerateCheckSum( (char*)&mdgw_logout, g_size_mdgw_header + g_size_mdgw_logout_body ) );

		try {
			int32_t net_ret = m_asio_client.Client_SendData_Sy( m_connect_quote, (const char*)&mdgw_logout, sizeof( mdgw_logout ) );
			if( net_ret <= 0 ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "发送用户登出请求时读写错误！{0}", net_ret );
				LogPrint( basicx::syslog_level::c_error, log_info );
				throw 1;
			}
			else {
				log_info = "用户登出请求发送完成。";
				LogPrint( basicx::syslog_level::c_info, log_info );

				// 测试显示，登出请求发出以后，读取立刻报 EOF 错误，接收不到行情源发送的 Logout 应答的
				int32_t time_wait = 0;
				while( false == m_logout_ok && time_wait < 5000 ) { // 最多等待 5 秒
					std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
					time_wait += 100;
				}
				if( true == m_logout_ok ) {
					log_info = "用户登出请求成功。";
					LogPrint( basicx::syslog_level::c_info, log_info );
				}
				else {
					log_info = "用户登出请求超时！";
					LogPrint( basicx::syslog_level::c_error, log_info );
				}
			}
		}
		catch( int32_t error ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "发送用户登出请求时读写异常！{0}", error );
			LogPrint( basicx::syslog_level::c_error, log_info );
		}
		catch( ... ) {
			log_info = "发送用户登出请求时发生未知错误！";
			LogPrint( basicx::syslog_level::c_error, log_info );
		}

		// 不管登出请求是否成功都断开连接
		m_connect_ok = false; // 等待重连重登
		m_asio_client.Client_CloseByUser( m_connect_quote );
		m_connect_quote = nullptr; //

		log_info = "用户登出断开现有连接。";
		LogPrint( basicx::syslog_level::c_info, log_info );
	}
}

void QuoterSGT_P::SendHeartCheck_Asio() {
	std::string log_info;

	if( true == m_connect_ok ) {
		MDGW_Heartbeat mdgw_heartbeat;
		memset( &mdgw_heartbeat, 0, sizeof( mdgw_heartbeat ) );
		mdgw_heartbeat.m_Header.m_MsgType = htonl( DEF_MDGW_MSG_TYPE_HEARTBEAT );
		mdgw_heartbeat.m_Header.m_BodyLength = htonl( g_size_mdgw_heartbeat_body );
		mdgw_heartbeat.mdgw_tailer.m_Checksum = htonl( MDGW_GenerateCheckSum( (char*)&mdgw_heartbeat, g_size_mdgw_header + g_size_mdgw_heartbeat_body ) );

		try {
			int32_t net_ret = m_asio_client.Client_SendData_Sy( m_connect_quote, (const char*)&mdgw_heartbeat, sizeof( mdgw_heartbeat ) );
			if( net_ret <= 0 ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "发送心跳消息时读写错误！{0}", net_ret );
				LogPrint( basicx::syslog_level::c_error, log_info );
				throw 1;
			}
			//else {
			//	log_info = "心跳消息发送完成。";
			//	LogPrint( basicx::syslog_level::c_info, log_info );
			//}
		}
		catch( int32_t error ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "发送心跳消息时读写异常！{0}", error );
			LogPrint( basicx::syslog_level::c_error, log_info );
			QuoteLogout_Asio(); // 标记 m_connect_ok = false 等待重连重登
		}
		catch( ... ) {
			log_info = "发送心跳消息时发生未知错误！";
			LogPrint( basicx::syslog_level::c_error, log_info );
			QuoteLogout_Asio(); // 标记 m_connect_ok = false 等待重连重登
		}
	}
}

void QuoterSGT_P::HandleMarketData_Asio() { // 只管网络读取
	std::string log_info;

	while( true == m_plugin_running ) {
		if( m_connect_quote != nullptr ) {
			try {
				int32_t net_ret = 0;

				MDGW_Header mdgw_header;
				memset( &mdgw_header, 0, sizeof( mdgw_header ) );
				net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, (char*)&mdgw_header, g_size_mdgw_header );
				if( net_ret <= 0 ) {
					FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
					LogPrint( basicx::syslog_level::c_error, log_info );
					throw 1;
				}

				uint32_t msg_type = ntohl( mdgw_header.m_MsgType );
				size_t body_length = ntohl( mdgw_header.m_BodyLength );
				//std::cout << "asio_recv：" << msg_type << " " << body_length << std::endl;

				if( body_length > m_mdgw_data_cache_size ) {
					if( m_mdgw_data_cache != nullptr ) {
						delete[] m_mdgw_data_cache;
						m_mdgw_data_cache = nullptr;
					}
					m_mdgw_data_cache_size = body_length;
					m_mdgw_data_cache = new char[m_mdgw_data_cache_size]; // 可支持消息体尾部扩展
				}
				memset( m_mdgw_data_cache, 0, m_mdgw_data_cache_size ); //

				switch( msg_type ) {
				case DEF_MDGW_MSG_TYPE_HEARTBEAT: { // MDGW_Heartbeat
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, (char*)&mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 2;
					}
					break;
				}
				case DEF_MDGW_MSG_TYPE_LOGON: { // MDGW_Logon
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, m_mdgw_data_cache, body_length );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 3;
					}
					QuotePhrase_Logon( m_mdgw_data_cache ); //
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, (char*)&mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 4;
					}
					break;
				}
				case DEF_MDGW_MSG_TYPE_LOGOUT: { // MDGW_Logout
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, m_mdgw_data_cache, body_length );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 5;
					}
					QuotePhrase_Logout( m_mdgw_data_cache ); //
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, (char*)&mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 6;
					}
					break;
				}
				case DEF_MDGW_MSG_TYPE_SNAPSHOT_STOCK_SGT: { // 港股实时行情
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, m_mdgw_data_cache, body_length );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 7;
					}
					QuotePhrase_SnapshotStock_SGT( m_mdgw_data_cache, body_length ); //
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, (char*)&mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 8;
					}
					break;
				}
				default: { // 其他消息
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, m_mdgw_data_cache, body_length );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 9;
					}
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_asio_client.Client_ReadData_Sy( m_connect_quote, (char*)&mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 10;
					}
					break;
				}
				}
			}
			catch( int32_t error ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写异常！{0}", error );
				LogPrint( basicx::syslog_level::c_error, log_info );
				QuoteLogout_Asio(); // 标记 m_connect_ok = false 等待重连重登
			}
			catch( ... ) {
				log_info = "接收行情网关数据时发生未知错误！";
				LogPrint( basicx::syslog_level::c_error, log_info );
				QuoteLogout_Asio(); // 标记 m_connect_ok = false 等待重连重登
			}
		}
		else {
			std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
		}
	} // while

	log_info = "行情网关数据处理线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

////////////////////////////// Asio //////////////////////////////

////////////////////////////// Sock //////////////////////////////

int32_t QuoterSGT_P::QuoteLogin_Sock() {
	std::string log_info;

	QuoteLogout_Sock();

	try {
		log_info = "开始用户登录发送连接请求 ....";
		LogPrint( basicx::syslog_level::c_info, log_info );

		int32_t con_ret = m_sock_client.Connect( m_configs.m_address, m_configs.m_port, 1, 1000 );
		if( con_ret != 0 ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "用户登录连接失败！{0}", con_ret );
			LogPrint( basicx::syslog_level::c_error, log_info );
			return -1;
		}
		else {
			log_info = "用户登录连接成功。";
			LogPrint( basicx::syslog_level::c_info, log_info );
		}

		m_logon_ok = false; //

		MDGW_Logon mdgw_logon;
		memset( &mdgw_logon, 0, sizeof( mdgw_logon ) );
		mdgw_logon.m_Header.m_MsgType = htonl( DEF_MDGW_MSG_TYPE_LOGON );
		mdgw_logon.m_Header.m_BodyLength = htonl( g_size_mdgw_logon_body );
		strncpy_s( mdgw_logon.m_SenderCompID, sizeof( mdgw_logon.m_SenderCompID ), m_configs.m_sender_id.c_str(), m_configs.m_sender_id.length() );
		strncpy_s( mdgw_logon.m_TargetCompID, sizeof( mdgw_logon.m_TargetCompID ), m_configs.m_username.c_str(), m_configs.m_username.length() ); // 填登录用户名
		mdgw_logon.m_HeartBtInt = htonl( m_configs.m_dump_time * 2 ); // * 2
		strncpy_s( mdgw_logon.m_Password, sizeof( mdgw_logon.m_Password ), m_configs.m_password.c_str(), m_configs.m_password.length() );
		strncpy_s( mdgw_logon.m_DefaultApplVerID, sizeof( mdgw_logon.m_DefaultApplVerID ), m_app_ver_id.c_str(), m_app_ver_id.length() );
		mdgw_logon.mdgw_tailer.m_Checksum = htonl( MDGW_GenerateCheckSum( (char*)&mdgw_logon, g_size_mdgw_header + g_size_mdgw_logon_body ) );

		try {
			int32_t net_ret = m_sock_client.Write( &mdgw_logon, sizeof( mdgw_logon ) );
			if( net_ret <= 0 ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "发送用户登录请求时读写错误！{0}", net_ret );
				LogPrint( basicx::syslog_level::c_error, log_info );
				throw 1;
			}

			log_info = "用户登录请求发送完成。";
			LogPrint( basicx::syslog_level::c_info, log_info );

			int32_t time_wait = 0;
			while( false == m_logon_ok && time_wait < 5000 ) { // 最多等待 5 秒
				std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
				time_wait += 100;
			}
			if( true == m_logon_ok ) {
				m_connect_ok = true; //
				log_info = "用户登录请求成功。";
				LogPrint( basicx::syslog_level::c_info, log_info );
			}
			else {
				log_info = "用户登录请求超时！";
				LogPrint( basicx::syslog_level::c_error, log_info );
				return -2;
			}
		}
		catch( int32_t error ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "发送用户登录请求时读写异常！{0}", error );
			LogPrint( basicx::syslog_level::c_error, log_info );
			return -3;
		}
		catch( ... ) {
			log_info = "发送用户登录请求时发生未知错误！";
			LogPrint( basicx::syslog_level::c_error, log_info );
			return -4;
		}
	}
	catch( ... ) {
		log_info = "用户登录时发生未知错误！";
		LogPrint( basicx::syslog_level::c_error, log_info );
		return -5;
	}

	return 0;
}

void QuoterSGT_P::QuoteLogout_Sock() {
	std::string log_info;

	m_logout_ok = false; //

	if( true == m_connect_ok ) {
		std::string logout_text = "bye_bye";

		MDGW_Logout mdgw_logout;
		memset( &mdgw_logout, 0, sizeof( mdgw_logout ) );
		mdgw_logout.m_Header.m_MsgType = htonl( DEF_MDGW_MSG_TYPE_LOGOUT );
		mdgw_logout.m_Header.m_BodyLength = htonl( g_size_mdgw_logout_body );
		mdgw_logout.m_SessionStatus = htonl( 4 ); // 就用 会话退登完成 吧
		strncpy_s( mdgw_logout.m_Text, sizeof( mdgw_logout.m_Text ), logout_text.c_str(), logout_text.length() );
		mdgw_logout.mdgw_tailer.m_Checksum = htonl( MDGW_GenerateCheckSum( (char*)&mdgw_logout, g_size_mdgw_header + g_size_mdgw_logout_body ) );

		try {
			int32_t net_ret = m_sock_client.Write( &mdgw_logout, sizeof( mdgw_logout ) );
			if( net_ret <= 0 ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "发送用户登出请求时读写错误！{0}", net_ret );
				LogPrint( basicx::syslog_level::c_error, log_info );
				throw 1;
			}
			else {
				log_info = "用户登出请求发送完成。";
				LogPrint( basicx::syslog_level::c_info, log_info );

				// 测试显示，登出请求发出以后，读取立刻报 EOF 错误，接收不到行情源发送的 Logout 应答的
				int32_t time_wait = 0;
				while( false == m_logout_ok && time_wait < 5000 ) { // 最多等待 5 秒
					std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
					time_wait += 100;
				}
				if( true == m_logout_ok ) {
					log_info = "用户登出请求成功。";
					LogPrint( basicx::syslog_level::c_info, log_info );
				}
				else {
					log_info = "用户登出请求超时！";
					LogPrint( basicx::syslog_level::c_error, log_info );
				}
			}
		}
		catch( int32_t error ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "发送用户登出请求时读写异常！{0}", error );
			LogPrint( basicx::syslog_level::c_error, log_info );
		}
		catch( ... ) {
			log_info = "发送用户登出请求时发生未知错误！";
			LogPrint( basicx::syslog_level::c_error, log_info );
		}

		// 不管登出请求是否成功都断开连接
		m_connect_ok = false; // 等待重连重登
		if( true == m_sock_client.IsConnectValid() ) {
			m_sock_client.Close();
		}

		log_info = "用户登出断开现有连接。";
		LogPrint( basicx::syslog_level::c_info, log_info );
	}
}

void QuoterSGT_P::SendHeartCheck_Sock() {
	std::string log_info;

	if( true == m_connect_ok ) {
		MDGW_Heartbeat mdgw_heartbeat;
		memset( &mdgw_heartbeat, 0, sizeof( mdgw_heartbeat ) );
		mdgw_heartbeat.m_Header.m_MsgType = htonl( DEF_MDGW_MSG_TYPE_HEARTBEAT );
		mdgw_heartbeat.m_Header.m_BodyLength = htonl( g_size_mdgw_heartbeat_body );
		mdgw_heartbeat.mdgw_tailer.m_Checksum = htonl( MDGW_GenerateCheckSum( (char*)&mdgw_heartbeat, g_size_mdgw_header + g_size_mdgw_heartbeat_body ) );

		try {
			int32_t net_ret = m_sock_client.Write( &mdgw_heartbeat, sizeof( mdgw_heartbeat ) );
			if( net_ret <= 0 ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "发送心跳消息时读写错误！{0}", net_ret );
				LogPrint( basicx::syslog_level::c_error, log_info );
				throw 1;
			}
			//else {
			//	log_info = "心跳消息发送完成。";
			//	LogPrint( basicx::syslog_level::c_info, log_info );
			//}
		}
		catch( int32_t error ) {
			FormatLibrary::StandardLibrary::FormatTo( log_info, "发送心跳消息时读写异常！{0}", error );
			LogPrint( basicx::syslog_level::c_error, log_info );
			QuoteLogout_Sock(); // 标记 m_connect_ok = false 等待重连重登
		}
		catch( ... ) {
			log_info = "发送心跳消息时发生未知错误！";
			LogPrint( basicx::syslog_level::c_error, log_info );
			QuoteLogout_Sock(); // 标记 m_connect_ok = false 等待重连重登
		}
	}
}

void QuoterSGT_P::HandleMarketData_Sock() { // 只管网络读取
	std::string log_info;

	while( true == m_plugin_running ) {
		if( m_sock_client.IsReadable( 1 ) ) {
			try {
				int32_t net_ret = 0;

				MDGW_Header mdgw_header;
				memset( &mdgw_header, 0, sizeof( mdgw_header ) );
				net_ret = m_sock_client.Read( &mdgw_header, g_size_mdgw_header );
				if( net_ret <= 0 ) {
					FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
					LogPrint( basicx::syslog_level::c_error, log_info );
					throw 1;
				}

				uint32_t msg_type = ntohl( mdgw_header.m_MsgType );
				size_t body_length = ntohl( mdgw_header.m_BodyLength );
				//std::cout << "sock_recv：" << msg_type << " " << body_length << std::endl;

				if( body_length > m_mdgw_data_cache_size ) {
					if( m_mdgw_data_cache != nullptr ) {
						delete[] m_mdgw_data_cache;
						m_mdgw_data_cache = nullptr;
					}
					m_mdgw_data_cache_size = body_length;
					m_mdgw_data_cache = new char[m_mdgw_data_cache_size]; // 可支持消息体尾部扩展
				}
				memset( m_mdgw_data_cache, 0, m_mdgw_data_cache_size ); //

				switch( msg_type ) {
				case DEF_MDGW_MSG_TYPE_HEARTBEAT: { // MDGW_Heartbeat
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_sock_client.Read( &mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 2;
					}
					break;
				}
				case DEF_MDGW_MSG_TYPE_LOGON: { // MDGW_Logon
					net_ret = m_sock_client.Read( m_mdgw_data_cache, body_length );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 3;
					}
					QuotePhrase_Logon( m_mdgw_data_cache ); //
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_sock_client.Read( &mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 4;
					}
					break;
				}
				case DEF_MDGW_MSG_TYPE_LOGOUT: { // MDGW_Logout
					net_ret = m_sock_client.Read( m_mdgw_data_cache, body_length );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 5;
					}
					QuotePhrase_Logout( m_mdgw_data_cache ); //
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_sock_client.Read( &mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 6;
					}
					break;
				}
				case DEF_MDGW_MSG_TYPE_SNAPSHOT_STOCK_SGT: { // 港股实时行情
					net_ret = m_sock_client.Read( m_mdgw_data_cache, body_length );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 7;
					}
					QuotePhrase_SnapshotStock_SGT( m_mdgw_data_cache, body_length ); //
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_sock_client.Read( &mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 8;
					}
					break;
				}
				default: { // 其他消息
					net_ret = m_sock_client.Read( m_mdgw_data_cache, body_length );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 9;
					}
					MDGW_Tailer mdgw_tailer;
					memset( &mdgw_tailer, 0, sizeof( mdgw_tailer ) );
					net_ret = m_sock_client.Read( &mdgw_tailer, g_size_mdgw_tailer );
					if( net_ret <= 0 ) {
						FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写错误！{0}", net_ret );
						LogPrint( basicx::syslog_level::c_error, log_info );
						throw 10;
					}
					break;
				}
				}
			}
			catch( int32_t error ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "接收行情网关数据时读写异常！{0}", error );
				LogPrint( basicx::syslog_level::c_error, log_info );
				QuoteLogout_Sock(); // 标记 m_connect_ok = false 等待重连重登
			}
			catch( ... ) {
				log_info = "接收行情网关数据时发生未知错误！";
				LogPrint( basicx::syslog_level::c_error, log_info );
				QuoteLogout_Sock(); // 标记 m_connect_ok = false 等待重连重登
			}
		}
	} // while

	log_info = "行情网关数据处理线程退出！";
	LogPrint( basicx::syslog_level::c_warn, log_info );
}

////////////////////////////// Sock //////////////////////////////

void QuoterSGT_P::QuotePhrase_Logon( char* mdgw_data_cache ) {
	std::string log_info;
	MDGW_Logon_Body* mdgw_logon_body = (MDGW_Logon_Body*)mdgw_data_cache;
	std::string sender_comp_id = basicx::StringTrim( std::string( mdgw_logon_body->m_SenderCompID, sizeof( mdgw_logon_body->m_SenderCompID ) ), " " );
	std::string target_comp_id = basicx::StringTrim( std::string( mdgw_logon_body->m_TargetCompID, sizeof( mdgw_logon_body->m_TargetCompID ) ), " " );
	m_logon_ok = true; //
	FormatLibrary::StandardLibrary::FormatTo( log_info, "收到用户登录应答。{0} {1}", sender_comp_id, target_comp_id );
	LogPrint( basicx::syslog_level::c_info, log_info );
}

void QuoterSGT_P::QuotePhrase_Logout( char* mdgw_data_cache ) {
	std::string log_info;
	MDGW_Logout_Body* mdgw_logout_body = (MDGW_Logout_Body*)mdgw_data_cache;
	std::string session_status = MDGW_SessionStatus( ntohl( mdgw_logout_body->m_SessionStatus ) );
	std::string text = basicx::StringTrim( std::string( mdgw_logout_body->m_Text, sizeof( mdgw_logout_body->m_Text ) ), " " );
	m_logout_ok = true; //
	FormatLibrary::StandardLibrary::FormatTo( log_info, "收到用户登出应答。{0} {1}", session_status, text );
	LogPrint( basicx::syslog_level::c_info, log_info );
}

void QuoterSGT_P::QuotePhrase_SnapshotStock_SGT( char* mdgw_data_cache, size_t body_length ) {
	try {
		size_t cache_offset = 0; // 缓存位置偏移
		if( body_length > sizeof( MDGW_Snapshot_Body ) ) {
			SYSTEMTIME sys_time;
			GetLocalTime( &sys_time );
			tm now_time_t = basicx::GetNowTime();
			//char now_date_temp[9];
			//strftime( now_date_temp, 9, "%Y%m%d", &now_time_t);

			// 直接 ( ( now_time_t.tm_year + 1900 ) * 10000 + ( now_time_t.tm_mon + 1 ) * 100 + now_time_t.tm_mday ) * 1000000000 会产生溢出
			int64_t now_date = ( now_time_t.tm_year + 1900 ) * 10000 + ( now_time_t.tm_mon + 1 ) * 100 + now_time_t.tm_mday; // YYYYMMDD
			int64_t date_for_mod = now_date * 1000000000; // YYYYMMDD000000000

			SnapshotStock_SGT snapshot_stock_temp;
			memset( &snapshot_stock_temp, 0, sizeof( SnapshotStock_SGT ) );

			MDGW_Snapshot_Body* mdgw_snapshot_body = (MDGW_Snapshot_Body*)&mdgw_data_cache[cache_offset];
			cache_offset += sizeof( MDGW_Snapshot_Body ); //

			std::string code = basicx::StringTrim( std::string( mdgw_snapshot_body->m_SecurityID, 8 ), " " );
			strcpy_s( snapshot_stock_temp.m_code, code.c_str() ); // 证券代码
			//snapshot_stock_temp.m_name[33]; // 证券名称 // 无
			strcpy_s( snapshot_stock_temp.m_type, "H" ); // 证券类型
			strcpy_s( snapshot_stock_temp.m_market, "HK" ); // 证券市场
			if( '1' == mdgw_snapshot_body->m_TradingPhaseCode[1] || 'H' == mdgw_snapshot_body->m_TradingPhaseCode[0] ) { // 停牌
				strcpy_s( snapshot_stock_temp.m_status, "S" ); // 证券状态 // "N"、"S"、"X"
			}
			else { // 正常
				strcpy_s( snapshot_stock_temp.m_status, "N" ); // 证券状态 // "N"、"S"、"X"
			}
			snapshot_stock_temp.m_pre_close = (uint32_t)( ntohll( mdgw_snapshot_body->m_PrevClosePx ) ); // 昨收价 // 10000 已一万倍
			snapshot_stock_temp.m_volume = ntohll( mdgw_snapshot_body->m_TotalVolumeTrade ) / 100; // 成交量 // 已百倍
			snapshot_stock_temp.m_turnover = ntohll( mdgw_snapshot_body->m_TotalValueTrade ); // 成交额 // 10000 已一万倍
			snapshot_stock_temp.m_trade_count = (int32_t)( ntohll( mdgw_snapshot_body->m_NumTrades ) ); // 成交笔数 // 貌似一直为零
			snapshot_stock_temp.m_quote_time = (int32_t)( ntohll( mdgw_snapshot_body->m_OrigTime ) % date_for_mod ); // 行情时间 // HHMMSSmmm 精度：秒

			uint32_t no_md_entries = 0;
			if( ( body_length - cache_offset ) > sizeof( uint32_t ) ) {
				no_md_entries = ntohl( *( (uint32_t*)&mdgw_data_cache[cache_offset] ) );
				cache_offset += sizeof( uint32_t ); //
				for( size_t i = 0; i < no_md_entries; ++i ) {
					if( ( body_length - cache_offset ) > sizeof( MDGW_HK_MDEntries ) ) {
						MDGW_HK_MDEntries* mdgw_hk_md_entries = (MDGW_HK_MDEntries*)&mdgw_data_cache[cache_offset];
						cache_offset += sizeof( MDGW_HK_MDEntries ); //
						char type_0 = mdgw_hk_md_entries->m_MDEntryType[0];
						char type_1 = mdgw_hk_md_entries->m_MDEntryType[1];
						if( '0' == type_0 ) { // 买入
							uint16_t md_prive_level = ntohs( mdgw_hk_md_entries->m_MDPriceLevel ); // 从 1 开始
							snapshot_stock_temp.m_bid_price[md_prive_level - 1] = (uint32_t)( ntohll( mdgw_hk_md_entries->m_MDEntryPx ) / 100 ); // 申买价 // 10000 已百万倍
							snapshot_stock_temp.m_bid_volume[md_prive_level - 1] = (int32_t)( ntohll( mdgw_hk_md_entries->m_MDEntrySize ) / 100 ); // 申买量 // 已百倍
						}
						else if( '1' == type_0 ) { // 卖出
							uint16_t md_prive_level = ntohs( mdgw_hk_md_entries->m_MDPriceLevel ); // 从 1 开始
							snapshot_stock_temp.m_ask_price[md_prive_level - 1] = (uint32_t)( ntohll( mdgw_hk_md_entries->m_MDEntryPx ) / 100 ); // 申买价 // 10000 已百万倍
							snapshot_stock_temp.m_ask_volume[md_prive_level - 1] = (int32_t)( ntohll( mdgw_hk_md_entries->m_MDEntrySize ) / 100 ); // 申买量 // 已百倍
						}
						//snapshot_stock_temp.m_open; // 开盘价 // 10000 // 无
						else if( '2' == type_0 ) { // 最近价
							snapshot_stock_temp.m_last = (uint32_t)( ntohll( mdgw_hk_md_entries->m_MDEntryPx ) / 100 ); // 最新价 // 10000 已百万倍
						}
						else if( '7' == type_0 ) { // 最高价
							snapshot_stock_temp.m_high = (uint32_t)( ntohll( mdgw_hk_md_entries->m_MDEntryPx ) / 100 ); // 最高价 // 10000 已百万倍
						}
						else if( '8' == type_0 ) { // 最低价
							snapshot_stock_temp.m_low = (uint32_t)( ntohll( mdgw_hk_md_entries->m_MDEntryPx ) / 100 ); // 最低价 // 10000 已百万倍
						}
						else if( 'x' == type_0 ) {
							if( 'e' == type_1 ) { // 涨停价 // 目前没有收到过
								// 取值为 999999999.9999 表示无涨停价格限制
								int64_t md_entry_px = ntohll( mdgw_hk_md_entries->m_MDEntryPx );
								if( md_entry_px > 999999999 ) {
									snapshot_stock_temp.m_high_limit = 999999999; // 涨停价 // 10000 // 2^32 = 0 ~ 4294967296
								}
								else {
									snapshot_stock_temp.m_high_limit = (uint32_t)( md_entry_px / 100 ); // 涨停价 // 10000 已百万倍
								}
							}
							else if( 'f' == type_1 ) { // 跌停价 // 目前没有收到过
								// 对于价格可以为负数的证券，取值为 -999999999.9999 表示无跌停价格限制
								// 对于价格不可以为负数的证券，取值为证券的价格档位表示无跌停价格限制，比如对于股票填写 0.01
								int64_t md_entry_px = ntohll( mdgw_hk_md_entries->m_MDEntryPx );
								if( md_entry_px < -999999999 ) {
									snapshot_stock_temp.m_low_limit = 0; // 跌停价 // 10000 // 2^32 = 0 ~ 4294967296
								}
								else {
									snapshot_stock_temp.m_low_limit = (uint32_t)( md_entry_px / 100 ); // 跌停价 // 10000 已百万倍
								}
							}
							else if( 'h' == type_1 ) { // 按盘价（收盘后为收盘价格）
								snapshot_stock_temp.m_close = (uint32_t)( ntohll( mdgw_hk_md_entries->m_MDEntryPx ) / 100 ); // 收盘价 // 10000 已百万倍// 按盘价
							}
							else if( 'i' == type_1 ) { // 参考价 // 目前没有收到过
								snapshot_stock_temp.m_bid_price[0] = (uint32_t)( ntohll( mdgw_hk_md_entries->m_MDEntryPx ) / 100 ); // 申买价 // 10000 已百万倍
								snapshot_stock_temp.m_ask_price[0] = (uint32_t)( ntohll( mdgw_hk_md_entries->m_MDEntryPx ) / 100 ); // 申买价 // 10000 已百万倍
							}
						}
					}
					else {
						// 长度不够了！
						break; // for
					}
				}

				uint32_t no_complex_event_times = 0; // 之前一定有 NoMDEntries 字段
				if( ( body_length - cache_offset ) >= sizeof( uint32_t ) ) { // >=
					no_complex_event_times = ntohl( *( (uint32_t*)&mdgw_data_cache[cache_offset] ) );
					cache_offset += sizeof( uint32_t ); //
					if( 1 == no_complex_event_times ) { // 当前处于触发 VCM 的冷静期
						if( ( body_length - cache_offset ) >= sizeof( MDGW_HK_ComplexEventTimes ) ) { // >=
							MDGW_HK_ComplexEventTimes* mdgw_hk_complex_event_times = (MDGW_HK_ComplexEventTimes*)&mdgw_data_cache[cache_offset];
							cache_offset += sizeof( MDGW_HK_ComplexEventTimes ); //
							snapshot_stock_temp.m_vcm_start_time = (int32_t)( ntohll( mdgw_hk_complex_event_times->m_ComplexEventStartTime ) % date_for_mod ); // 冷静期开始时间 // HHMMSSmmm 精度：秒
							snapshot_stock_temp.m_vcm_end_time = (int32_t)( ntohll( mdgw_hk_complex_event_times->m_ComplexEventEndTime ) % date_for_mod ); // 冷静期结束时间 // HHMMSSmmm 精度：秒
						}
					}
				}
			}

			snapshot_stock_temp.m_local_time = now_time_t.tm_hour * 10000000 + now_time_t.tm_min * 100000 + now_time_t.tm_sec * 1000 + sys_time.wMilliseconds; // 本地时间 // HHMMSSmmm 精度：毫秒
			m_cache_snapshot_stock.m_local_index++;
			snapshot_stock_temp.m_local_index = m_cache_snapshot_stock.m_local_index; // 本地序号

			m_cache_snapshot_stock.m_recv_num++;

			MS_AddData_SnapshotStock( snapshot_stock_temp );

			//std::string log_info;
			//FormatLibrary::StandardLibrary::FormatTo( log_info, "行情反馈：代码:{0} 名称:{1} 最新:{2} 昨收:{3} 时间:{4}", 
			//	snapshot_stock_temp.m_code, snapshot_stock_temp.m_name, snapshot_stock_temp.m_last, snapshot_stock_temp.m_pre_close, snapshot_stock_temp.m_quote_time );
			//LogPrint( basicx::syslog_level::c_debug, log_info );
		}
	}
	catch( ... ) {
		std::string log_info = "行情数据处理发生未知异常！";
		LogPrint( basicx::syslog_level::c_error, log_info );
	}
}

// 预设 QuoterSGT 函数实现

QuoterSGT::QuoterSGT()
	: basicx::Plugins_X( PLUGIN_NAME )
	, m_quoter_sgt_p( nullptr ) {
	try {
		m_quoter_sgt_p = new QuoterSGT_P();
	}
	catch( ... ) {
	}
}

QuoterSGT::~QuoterSGT() {
	if( m_quoter_sgt_p != nullptr ) {
		delete m_quoter_sgt_p;
		m_quoter_sgt_p = nullptr;
	}
}

bool QuoterSGT::Initialize() {
	return m_quoter_sgt_p->Initialize();
}

bool QuoterSGT::InitializeExt() {
	return m_quoter_sgt_p->InitializeExt();
}

bool QuoterSGT::StartPlugin() {
	return m_quoter_sgt_p->StartPlugin();
}

bool QuoterSGT::IsPluginRun() {
	return m_quoter_sgt_p->IsPluginRun();
}

bool QuoterSGT::StopPlugin() {
	return m_quoter_sgt_p->StopPlugin();
}

bool QuoterSGT::UninitializeExt() {
	return m_quoter_sgt_p->UninitializeExt();
}

bool QuoterSGT::Uninitialize() {
	return m_quoter_sgt_p->Uninitialize();
}

bool QuoterSGT::AssignTask( int32_t task_id, int32_t identity, int32_t code, std::string& data ) {
	return m_quoter_sgt_p->AssignTask( task_id, identity, code, data );
}

// 自定义 QuoterSGT 函数实现
