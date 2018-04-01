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

#include <windows.h>

#include <chrono>
#include <thread>
#include <nb30.h> // 获取 Mac 地址
#include <stdlib.h>
#include <iostream>
#include <shellapi.h> // NOTIFYICONDATA

#pragma comment( lib, "shell32.lib" ) // Shell_NotifyIcon
#pragma comment( lib, "netapi32.lib" ) // 获取 Mac 地址

#include <common/common.h>
#include <syslog/syslog.h>
#include <syscfg/syscfg.h>
#include <plugins/plugins.h>

#include "global/define.h"

#include "quotex.h"

bool g_command_model = false; // 标记进入命令行模式
bool g_block_auto_info = false; // 阻止自动显示信息
void* g_single_mutex = NULL; // 单例限制
NOTIFYICONDATA g_nid_tray_icon; // 系统托盘

struct ASTAT // 获取 Mac 地址
{
	ADAPTER_STATUS adapter_status;
	NAME_BUFFER name_buffer[30];
};

const unsigned int g_wm_taskbar_created = ::RegisterWindowMessage( L"TaskBarCreated" ); // 桌面重启后更新托盘图标

void SystemUninitialize() { // 在控制台事件和单例限制退出时调用会异常
	try {
		basicx::Plugins* plugins = basicx::Plugins::GetInstance(); // 2
		if( plugins != nullptr ) {
			plugins->~Plugins();
		}

		// basicx::SysLog_S* syslog = basicx::SysLog_S::GetInstance(); // 1
		// if( syslog != nullptr ) {
		//     syslog->~SysLog_S();
		// }
	} // try
	catch( ... ) {}
}

int __stdcall ConsoleHandler( unsigned long event ) { // 控制台事件检测
	std::string log_cate = "<SYSTEM_EVENT>";
	basicx::SysLog_S* syslog = basicx::SysLog_S::GetInstance();

	switch( event ) {
	case CTRL_C_EVENT: // 用户按下 "Ctrl + C" 键
		g_command_model = true;
		syslog->LogWrite( basicx::syslog_level::c_info, log_cate, std::string( "用户按下 Ctrl + C 键。" ) );
		break;
	case CTRL_BREAK_EVENT: // 用户按下 "Ctrl + Break" 键
		syslog->LogWrite( basicx::syslog_level::c_info, log_cate, std::string( "用户按下 Ctrl + Break 键。" ) );
		break;
	case CTRL_CLOSE_EVENT: // 关闭控制台窗口 // 可清理延时时间 5 秒
		syslog->LogWrite( basicx::syslog_level::c_warn, log_cate, std::string( "控制台窗口被强制关闭，系统结束运行！\r\n" ) );
		Shell_NotifyIcon( NIM_DELETE, &g_nid_tray_icon ); // 删除托盘图标
		SystemUninitialize(); // 这里一般来不及清理
		break;
	case CTRL_LOGOFF_EVENT: // 用户注销 // 可清理延时时间 20 秒
		syslog->LogWrite( basicx::syslog_level::c_warn, log_cate, std::string( "用户注销，系统结束运行！\r\n" ) );
		break;
	case CTRL_SHUTDOWN_EVENT: // 用户关机 // 可清理延时时间 20 秒
		syslog->LogWrite( basicx::syslog_level::c_warn, log_cate, std::string( "用户关机，系统结束运行！\r\n" ) );
		break;
	}

	return 1;
}

void ConsoleEventsSet() {
	if( !SetConsoleCtrlHandler( (PHANDLER_ROUTINE)ConsoleHandler, TRUE ) ) { // 安装事件处理，用于检测控制台关闭及按键等
		std::string log_cate = "<SYSTEM_MAIN>";
		basicx::SysLog_S* syslog = basicx::SysLog_S::GetInstance();
		syslog->LogWrite( basicx::syslog_level::c_error, log_cate, std::string( "无法安装控制台事件检测句柄！" ) );
	}
}

void StringReplace( std::string& str_str, const std::string& str_src, const std::string& str_dst ) {
	std::string::size_type pos = 0;
	std::string::size_type src_len = str_src.size();
	std::string::size_type dst_len = str_dst.size();
	while( ( pos = str_str.find( str_src, pos ) ) != std::string::npos ) {
		str_str.replace( pos, src_len, str_dst );
		pos += dst_len;
	}
}

bool IsMacAddrMatch( int32_t lana_num, std::string mac_address ) {
	std::string log_info;
	std::string log_cate = "<SYSTEM_MAIN>";
	basicx::SysLog_S* syslog = basicx::SysLog_S::GetInstance();

	NCB ncb;
	unsigned char ret_code;

	memset( &ncb, 0, sizeof( ncb ) );
	ncb.ncb_command = NCBRESET;
	ncb.ncb_lana_num = lana_num; // 指定网卡号

	ret_code = Netbios( &ncb ); // 首先对选定的网卡发送一个 NCBRESET 命令，以便进行初始化
	// FormatLibrary::StandardLibrary::FormatTo( log_info, "1 - IsMacAddrMatch NCBENUM：0x{0:x}", ret_code );
	// syslog->LogWrite( basicx::syslog_level::c_debug, log_cate, log_info );

	ASTAT mac_adapter; // 获取 Mac 地址

	memset( &ncb, 0, sizeof( ncb ) );
	ncb.ncb_command = NCBASTAT;
	ncb.ncb_lana_num = lana_num; // 指定网卡号
	strcpy_s( (char*)ncb.ncb_callname, sizeof( ncb.ncb_callname ), "* " );
	ncb.ncb_buffer = (unsigned char*)&mac_adapter;
	ncb.ncb_length = sizeof( mac_adapter ); // 指定返回的信息存放的变量

	ret_code = Netbios( &ncb ); // 发送 NCBASTAT 命令以获取网卡的信息
	// FormatLibrary::StandardLibrary::FormatTo( log_info, "2 - IsMacAddrMatch NCBENUM：0x{0:x}", ret_code );
	// syslog->LogWrite( basicx::syslog_level::c_debug, log_cate, log_info );
	if( 0 == ret_code ) {
		std::string mac_addr_lana;
		FormatLibrary::StandardLibrary::FormatTo( mac_addr_lana, "{0,2:x}-{1,2:x}-{2,2:x}-{3,2:x}-{4,2:x}-{5,2:x}", 
			mac_adapter.adapter_status.adapter_address[0],
			mac_adapter.adapter_status.adapter_address[1],
			mac_adapter.adapter_status.adapter_address[2],
			mac_adapter.adapter_status.adapter_address[3],
			mac_adapter.adapter_status.adapter_address[4],
			mac_adapter.adapter_status.adapter_address[5] );
		StringReplace( mac_addr_lana, " ", "0" );
		// FormatLibrary::StandardLibrary::FormatTo( log_info, "Mac Address：{0}", mac_addr_lana );
		// syslog->LogWrite( basicx::syslog_level::c_debug, log_cate, log_info );
		if( mac_address == mac_addr_lana ) {
			return true;
		}
	}

	return false;
}

void CheckHostMacAddr() { // 主机限制检测
	std::string log_info;
	std::string log_cate = "<SYSTEM_MAIN>";
	basicx::SysLog_S* syslog = basicx::SysLog_S::GetInstance();

	bool mac_addr_match = false;
	std::string mac_address = "90-61-AE-C0-9E-0F"; // "24-BE-05-A5-25-C0";

	NCB ncb;
	memset( &ncb, 0, sizeof( ncb ) );
	LANA_ENUM lana_enum;
	ncb.ncb_command = NCBENUM;
	ncb.ncb_buffer = (unsigned char*)&lana_enum;
	ncb.ncb_length = sizeof( lana_enum );

	unsigned char ret_code = Netbios( &ncb ); // 向网卡发送 NCBENUM 命令，以获取当前机器的网卡信息，如存在多少个网卡、每张网卡的编号等
	// FormatLibrary::StandardLibrary::FormatTo( log_info, "CheckHostMacAddr NCBENUM：0x{0:x}", ret_code );
	// syslog->LogWrite( basicx::syslog_level::c_debug, log_cate, log_info );
	if( 0 == ret_code ) {
		// FormatLibrary::StandardLibrary::FormatTo( log_info, "CheckHostMacAddr Ethernet：{0}", lana_enum.length );
		// syslog->LogWrite( basicx::syslog_level::c_debug, log_cate, log_info );
		for( size_t i = 0; i < lana_enum.length; i++ ) {
			if( true == IsMacAddrMatch( lana_enum.lana[i], mac_address ) ) {
				mac_addr_match = true;
			}
		}
	}

	if( false == mac_addr_match ) {
		syslog->LogWrite( basicx::syslog_level::c_warn, log_cate, std::string( "程序运行主机 Mac 地址不匹配！" ) );
		std::this_thread::sleep_for( std::chrono::seconds( 1 ) ); // 留点时间输出日志
		SystemUninitialize(); // 这里能完整清理
		exit( 0 );
	}
}

void CheckSingleMutex() { // 单例限制检测
	g_single_mutex = ::CreateMutex( NULL, FALSE, TRAY_POP_START );
	if( GetLastError() == ERROR_ALREADY_EXISTS ) {
		basicx::SysLog_S* syslog = basicx::SysLog_S::GetInstance();
		syslog->LogWrite( basicx::syslog_level::c_warn, std::string( "<SYSTEM_MAIN>" ), std::string( "已有另外一个实例在运行！" ) );
		std::this_thread::sleep_for( std::chrono::seconds( 1 ) ); // 留点时间输出日志
		CloseHandle( g_single_mutex );
		SystemUninitialize(); // 这里能完整清理
		exit( 0 );
	}
}

void SetConsoleWindow() {
	basicx::SysCfg_S* syscfg = basicx::SysCfg_S::GetInstance();

	// 获取窗口句柄
	wchar_t title[255];
	GetConsoleTitle( title, 255 );
	HWND h_wnd = FindWindow( L"ConsoleWindowClass", title );
	void* h_out = GetStdHandle( STD_OUTPUT_HANDLE );

	// 设置大小位置
	// 如果行数超过屏幕高度而出现纵向滚动条，则宽度必须 <= 80 才不会出现横向滚动条
	// 也就是一旦窗口宽度或高度超过屏幕宽度或高度，窗口就会被自动调整为 { 80, 25 } 且带滚动条的形式
	COORD wnd_size = { 80, 100 };
	SetConsoleScreenBufferSize( h_out, wnd_size );
	SMALL_RECT wnd_rect = { 0, 0, 80 - 1, 100 - 1 };
	SetConsoleWindowInfo( h_out, TRUE, &wnd_rect );
	// ShowWindow( h_wnd, g_wm_taskbar_created );

	// 更改窗体图标
	HICON h_icon = (HICON)LoadImageA( NULL, std::string( syscfg->GetPath_ExtFolder() + "\\logo.ico" ).c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE ); // 从 ico 文件读取图标
	SendMessage( h_wnd, WM_SETICON, ICON_SMALL, (LPARAM)h_icon ); // 设置窗体图标

	// 更改窗体标题
	std::string console_title;
	FormatLibrary::StandardLibrary::FormatTo( console_title, "{0} {1}", DEF_APP_NAME, DEF_APP_VERSION );
	int32_t number = MultiByteToWideChar( 0, 0, console_title.c_str(), -1, NULL, 0 );
	wchar_t* temp_console_title = new wchar_t[number];
	MultiByteToWideChar( 0, 0, console_title.c_str(), -1, temp_console_title, number );
	SetConsoleTitle( temp_console_title ); // 修改 Console 窗口标题
	delete[] temp_console_title;
}

void SetSystemTrayIcon() {
	basicx::SysCfg_S* syscfg = basicx::SysCfg_S::GetInstance();

	// 获取窗口句柄
	wchar_t title[255];
	GetConsoleTitle( title, 255 );
	HWND h_wnd = FindWindow( L"ConsoleWindowClass", title );

	// 设置托盘 NOTIFYICONDATA 结构体
	// g_nid_tray_icon.cbSize = sizeof( NOTIFYICONDATA ); // 控制台模式下使用 VS2008 或更高版本时在 XP 系统上无法弹出气泡提示
	g_nid_tray_icon.cbSize = NOTIFYICONDATA_V2_SIZE;
	g_nid_tray_icon.hIcon = (HICON)LoadImageA( NULL, std::string( syscfg->GetPath_ExtFolder() + "\\logo.ico" ).c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE ); // 从 ico 文件读取图标
	g_nid_tray_icon.hWnd = h_wnd;
	g_nid_tray_icon.uCallbackMessage = NULL;
	g_nid_tray_icon.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE | NIF_INFO; // 打开相应功能
	g_nid_tray_icon.uID = NULL;
	wcscpy_s( g_nid_tray_icon.szTip, 128, TRAY_POP_START ); // 托盘图标提示字符串
	wcscpy_s( g_nid_tray_icon.szInfoTitle, 64, TRAY_POP_TITLE ); // 气球型提示标题
	g_nid_tray_icon.dwInfoFlags = NIIF_INFO;
	wcscpy_s( g_nid_tray_icon.szInfo, 256, TRAY_POP_START ); // 气球型提示字符串
	g_nid_tray_icon.uTimeout = 500; // 提示时间
	// 添加托盘图标：Shell_NotifyIcon( DWORD dwMessage, PNOTIFYICONDATA lpData )
	Shell_NotifyIcon( NIM_ADD, &g_nid_tray_icon );
	// 更改托盘图标
	// g_nid_tray_icon.hIcon = LoadIcon( NULL, IDI_QUESTION );
	// strcpy( g_nid_tray_icon.szTip, "fhsdjdhggggg" );
	// Shell_NotifyIcon( NIM_MODIFY, &g_nid_tray_icon );
	// 删除托盘图标
	// Shell_NotifyIcon( NIM_DELETE, &g_nid_tray_icon );
}

bool SystemInitialize() {
	std::string log_info;
	std::string log_cate = "<SYSTEM_INIT>";
	basicx::SysLog_S* syslog = basicx::SysLog_S::GetInstance();
	basicx::SysCfg_S* syscfg = basicx::SysCfg_S::GetInstance();
	basicx::Plugins* plugins = basicx::Plugins::GetInstance();

	syslog->LogWrite( basicx::syslog_level::c_info, log_cate, std::string( "开始系统初始化 ...\r\n" ) );
	syslog->LogPrint( basicx::syslog_level::c_info, log_cate, std::string( "LOG>: 开始系统初始化 ...." ) );

	try {
		syslog->LogPrint( basicx::syslog_level::c_info, log_cate, std::string( "LOG>: 读取 系统参数配置 ...." ) );
		syscfg->ReadCfgBasic( syscfg->GetPath_CfgBasic() );
		basicx::CfgBasic* cfg_basic = syscfg->GetCfgBasic();

		// syslog->LogPrint( basicx::syslog_level::c_info, log_cate, std::string( "LOG>: 开启 运行监控服务 ...." ) );
		// sysrtm->StartNetServer();

		syslog->LogPrint( basicx::syslog_level::c_info, log_cate, std::string( "LOG>: 启动 插件管理服务 ...." ) );
		plugins->StartPlugins();

		syslog->LogPrint( basicx::syslog_level::c_info, log_cate, std::string( "LOG>: 加载 各类应用插件 ...." ) );
		plugins->LoadAll( syscfg->GetPath_PluFolder() );
		for( size_t i = 0; i < cfg_basic->m_vec_plugin.size(); i++ ) {
			if( 1 == cfg_basic->m_vec_plugin[i].m_work ) {
				FormatLibrary::StandardLibrary::FormatTo( log_info, "LOG>: 启用 插件 {0} ....", cfg_basic->m_vec_plugin[i].m_name );
				syslog->LogPrint( basicx::syslog_level::c_info, log_cate, log_info );
				basicx::Plugins_X* plugins_x = nullptr;
				plugins_x = plugins->GetPluginsX( cfg_basic->m_vec_plugin[i].m_name );
				if( plugins_x != nullptr ) {
					plugins_x->StartPlugin();
				}
			}
		}

		// TODO：添加更多初始化任务

	} // try
	catch( ... ) {
		syslog->LogWrite( basicx::syslog_level::c_fatal, log_cate, std::string( "系统初始化时发生未知错误！\r\n" ) );
	}

	syslog->LogWrite( basicx::syslog_level::c_info, log_cate, std::string( "系统初始化完成。\r\n" ) );
	syslog->LogPrint( basicx::syslog_level::c_info, log_cate, std::string( "LOG>: 系统初始化完成。" ) );

	return true;
}

int main( int argc, char* argv[] ) {
	std::string log_info;
	std::string log_cate = "<SYSTEM_MAIN>";
	basicx::SysLog_S syslog_s( DEF_APP_NAME ); // 唯一实例 // 01
	basicx::SysLog_S* syslog = basicx::SysLog_S::GetInstance();
	syslog->SetThreadSafe( false );
	syslog->SetLocalCache( true );
	syslog->SetActiveFlush( false );
	syslog->SetActiveSync( false );
	syslog->SetWorkThreads( 1 );
	syslog->SetFileStreamBuffer( DEF_SYSLOG_FSBM_NONE );
	syslog->InitSysLog( DEF_APP_NAME, DEF_APP_VERSION, DEF_APP_COMPANY, DEF_APP_COPYRIGHT );
	syslog->PrintSysInfo();
	syslog->WriteSysInfo();
	basicx::SysCfg_S syscfg_s; // 唯一实例 // 02
	basicx::SysCfg_S* syscfg = basicx::SysCfg_S::GetInstance();
	syscfg->SetGlobalPath( "cfg_basic.ini" );
	basicx::Plugins plugins; // 唯一实例 // 03

	// syslog->ClearScreen( 0, 0, true, 3000 ); // 等待 3 秒清屏

	try {
		ConsoleEventsSet();  // 01
		// CheckHostMacAddr();  // 02
		// CheckSingleMutex();  // 03
		SetConsoleWindow();  // 04
		SetSystemTrayIcon(); // 05
		SystemInitialize();  // 06

		while( 1 ) {
			std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

			if( true == g_command_model ) {
				if( false == g_block_auto_info ) {
					g_block_auto_info = true;
					syslog->LogWrite( basicx::syslog_level::c_info, log_cate, std::string( "用户 进入 命令行模式。" ) );

					// 更改托盘图标
					g_nid_tray_icon.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE | NIF_INFO;
					wcscpy_s( g_nid_tray_icon.szInfoTitle, 64, TRAY_POP_TITLE ); // 气球型提示标题
					g_nid_tray_icon.dwInfoFlags = NIIF_INFO;
					wcscpy_s( g_nid_tray_icon.szInfo, 256, L"进入 命令行模式" ); // 气球型提示字符串
					g_nid_tray_icon.uTimeout = 500; // 提示时间
					// g_nid_tray_icon.hIcon = g_tray_icon_02;
					Shell_NotifyIcon( NIM_MODIFY, &g_nid_tray_icon );

					char user_input[CFG_MAX_PATH_LEN] = { 0 }; // 缓存输入
					syslog->LogPrint( basicx::syslog_level::c_info, std::string( "<USER_COMMAND>" ), std::string( "CMD>: " ) ); // 最好不换行
					gets_s( user_input, CFG_MAX_PATH_LEN );
					if( strlen( user_input ) == 0 ) {
						Beep( 1000, 100 ); // 频率赫兹，时间毫秒
						syslog->LogWrite( basicx::syslog_level::c_info, std::string( "<USER_COMMAND>" ), std::string( "用户输入命令为空。" ) );
						syslog->LogPrint( basicx::syslog_level::c_error, std::string( "<USER_COMMAND>" ), std::string( "ERR>: 命令不能为空！" ) ); // 最好不换行  // 输出 "\nCMD>: " 时换行
					}
					size_t length = strlen( user_input );
					for( size_t i = 0; i < length; i++ ) { // 全部转换为小写字母
						if( user_input[i] >= 'A' && user_input[i] <= 'Z' ) {
							user_input[i] += 'a' - 'A';
						}
					}
					std::string command = user_input;
					FormatLibrary::StandardLibrary::FormatTo( log_info, "用户输入命令：{0}", user_input );
					syslog->LogWrite( basicx::syslog_level::c_info, std::string( "<USER_COMMAND>" ), log_info );

					while( command != "work;" && command != "01" ) { //
						bool need_cout_cmd = true;
						if( command == "info;" || command == "02" ) { // 查看系统信息
							// syskit->SystemInfo();
							syslog->LogPrint( basicx::syslog_level::c_info, std::string( "<USER_COMMAND>" ), std::string( "SystemInfo" ) );
						}
						else if( command == "help;" || command == "03" ) { // 查看系统信息
							// syskit->SystemHelp();
							syslog->LogPrint( basicx::syslog_level::c_info, std::string( "<USER_COMMAND>" ), std::string( "SystemHelp" ) );
						}
						else if( command == "exit;" || command == "04" ) { // 用户退出系统
							// if( syskit->SystemExit() ) {
							if( true ) {
								Shell_NotifyIcon( NIM_DELETE, &g_nid_tray_icon ); // 删除托盘图标
								SystemUninitialize(); // 这里交给主线程退出时清理
								exit( 0 );
							}
						}
						else {
							if( command != "" ) {
								FormatLibrary::StandardLibrary::FormatTo( log_info, "ERR>: 未知命令：{0}", command );
								syslog->LogPrint( basicx::syslog_level::c_error, std::string( "<USER_COMMAND>" ), log_info ); // 最好不换行 // 输出 "\nCMD>: " 时换行
							}
						}

						// user_input[CFG_MAX_PATH_LEN] = { 0 }; // 清空
						if( true == need_cout_cmd ) { // 为 false 的均由线程执行结束后输出 "\nCMD>: "
							syslog->LogPrint( basicx::syslog_level::c_info, std::string( "<USER_COMMAND>" ), std::string( "\nCMD>: " ) ); // 最好不换行
						}
						gets_s( user_input, CFG_MAX_PATH_LEN );
						if( strlen( user_input ) == 0 ) {
							Beep( 1000, 100 ); // 频率赫兹，时间毫秒
							syslog->LogWrite( basicx::syslog_level::c_info, std::string( "<USER_COMMAND>" ), std::string( "用户输入命令为空。" ) );
							syslog->LogPrint( basicx::syslog_level::c_error, std::string( "<USER_COMMAND>" ), std::string( "ERR>: 命令不能为空！" ) ); // 最好不换行 // 输出 "\nCMD>: " 时换行
						}
						size_t length = strlen( user_input );
						for( size_t i = 0; i < length; i++ ) { // 全部转换为小写字母
							if( user_input[i] >= 'A' && user_input[i] <= 'Z' ) {
								user_input[i] += 'a' - 'A';
							}
						}
						command = user_input;
						FormatLibrary::StandardLibrary::FormatTo( log_info, "用户输入命令：{0}", user_input );
						syslog->LogWrite( basicx::syslog_level::c_info, std::string( "<USER_COMMAND>" ), log_info );
					}
				}

				if( true == g_block_auto_info ) {
					g_block_auto_info = false;
					syslog->LogWrite( basicx::syslog_level::c_info, log_cate, std::string( "用户 退出 命令行模式。" ) );

					// 更改托盘图标
					g_nid_tray_icon.uFlags = NIF_TIP | NIF_ICON | NIF_MESSAGE | NIF_INFO;
					wcscpy_s( g_nid_tray_icon.szInfoTitle, 64, TRAY_POP_TITLE ); // 气球型提示标题
					g_nid_tray_icon.dwInfoFlags = NIIF_INFO;
					wcscpy_s( g_nid_tray_icon.szInfo, 256, L"退出 命令行模式" ); // 气球型提示字符串
					g_nid_tray_icon.uTimeout = 500; // 提示时间
					// g_nid_tray_icon.hIcon = g_tray_icon_02;
					Shell_NotifyIcon( NIM_MODIFY, &g_nid_tray_icon );

					g_command_model = false;

					syslog->ClearScreen( 0, 0, true ); // 清屏
				}
			}
		} // while( 1 )
	} // try
	catch( ... ) {
		syslog->LogWrite( basicx::syslog_level::c_fatal, log_cate, std::string( "系统主线程发生未知错误！" ) );
	}

	syslog->LogWrite( basicx::syslog_level::c_warn, log_cate, std::string( "系统主线程退出！" ) );

	return 0;
}

namespace quotex {

} // namespace quotex
