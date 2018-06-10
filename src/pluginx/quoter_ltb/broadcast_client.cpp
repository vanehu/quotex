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

#include <common/sysdef.h>

#include "broadcast_client.h"

BroadcastClient::BroadcastClient()
	: m_time_out( 10 )
	, m_socket_c( INVALID_SOCKET ) {
}

BroadcastClient::~BroadcastClient() {
    Close();
}

int32_t BroadcastClient::InitBroadcast( std::string address, int32_t port ) {
	int32_t flag_reuse_addr = 1;
	int32_t flag_recv_buf_size = 1024 * 1024;
#ifdef __OS_WINDOWS__
	unsigned long flag_fionbio = 1;
	char flag_broadcast = 1;
#else
	int32_t flag_fionbio = 1;
	int32_t flag_broadcast = 1;
#endif

#ifdef __OS_WINDOWS__
	WSADATA init_data = { 0 };
	if( 0 != WSAStartup( MAKEWORD( 2, 2 ), &init_data ) ) {
		return -1; // WSAGetLastError()
	}
#endif

	if( INVALID_SOCKET == m_socket_c ) {
		m_socket_c = socket( AF_INET, SOCK_DGRAM, 0 );
		if( INVALID_SOCKET == m_socket_c ) {
			return -2;
		}
	}

	if( 0 != setsockopt( m_socket_c, SOL_SOCKET, SO_REUSEADDR, (char*)&flag_reuse_addr, sizeof( flag_reuse_addr ) ) ) {
		return -3;
	}

#ifdef __OS_WINDOWS__
	if( ioctlsocket( m_socket_c, FIONBIO, &flag_fionbio ) < 0 ) {
#else
	if( ioctl( m_socket_c, FIONBIO, &flag_fionbio ) < 0 ) {
#endif
		return -4;
	}

	if( 0 != setsockopt( m_socket_c, SOL_SOCKET, SO_RCVBUF, (const char*)&flag_recv_buf_size, sizeof( flag_recv_buf_size ) ) ) {
		return -5;
	}

	SOCKADDR_IN	sock_addr_l;
	memset( &sock_addr_l, 0, sizeof( sock_addr_l ) );
	sock_addr_l.sin_family = AF_INET;
	sock_addr_l.sin_port = htons( port );
#ifdef __OS_WINDOWS__
	sock_addr_l.sin_addr.s_addr = htonl( INADDR_ANY );
#else
	sock_addr_l.sin_addr.s_addr = inet_addr( address );
#endif

	if( 0 != bind( m_socket_c, (sockaddr*)&sock_addr_l, sizeof( sock_addr_l ) ) ) {
		return -6;
	}

	if( 0 != setsockopt( m_socket_c, SOL_SOCKET, SO_BROADCAST, &flag_broadcast, sizeof( flag_broadcast ) ) ) {
		return -7;
	}

	return 0;
}

int32_t BroadcastClient::Read( void* buffer, int32_t need_length ) {
	if( INVALID_SOCKET == m_socket_c ) {
		return -1;
	}

	int32_t recv_length = 0;
	int32_t read_length = 0;

	fd_set fd_set_temp;
	timeval	time_out;
	time_out.tv_sec = m_time_out;
	time_out.tv_usec = 0;

	while( recv_length < need_length ) {
		FD_ZERO( &fd_set_temp );
		FD_SET( m_socket_c, &fd_set_temp );
		if( select( m_socket_c, &fd_set_temp, nullptr, nullptr, &time_out ) == 1 ) {
			read_length = recv( m_socket_c, ((char*)buffer ) + recv_length, need_length - recv_length, 0 );
			if( read_length <= 0 ) {
				Close();
				return -2; // 读数据出错：连接已经断开
			}
			recv_length += read_length;
		}
		else {
			Close();
			return -3; // 读数据出错：接收数据超时
		}
	}

	return recv_length;
}

int32_t BroadcastClient::RecvFrom( void* buffer, int32_t need_length ) {
	if( INVALID_SOCKET == m_socket_c ) {
		return -1;
	}

	int32_t recv_length = 0;

	sockaddr_in sock_addr;
#ifdef __OS_WINDOWS__
	int32_t addr_length;
#else
	socklen_t addr_length;
#endif

	fd_set fd_set_temp;
	timeval	time_out;
	time_out.tv_sec = m_time_out;
	time_out.tv_usec = 0;

	FD_ZERO( &fd_set_temp );
	FD_SET( m_socket_c, &fd_set_temp );
	if( select( m_socket_c, &fd_set_temp, nullptr, nullptr, &time_out ) == 1 ) {
		addr_length = sizeof( sockaddr_in );
		recv_length = recvfrom( m_socket_c, (char*)buffer, need_length, 0, (sockaddr*)&sock_addr, &addr_length );
		if( recv_length <= 0 ) {
			Close();
			return -2; // 读数据出错：连接已经断开
		}
	}
	else {
		Close();
		return -3; // 读数据出错：接收数据超时
	}

	return recv_length;
}

bool BroadcastClient::IsReadable( int32_t wait_time ) {
	if( INVALID_SOCKET == m_socket_c ) {
		return false;
	}

	timeval	time_out;
	time_out.tv_sec = 0;
	time_out.tv_usec = wait_time;
	fd_set fd_set_temp;
	FD_ZERO( &fd_set_temp );
	FD_SET( m_socket_c, &fd_set_temp );

	if( select( m_socket_c, &fd_set_temp, nullptr, nullptr, &time_out ) == 1 ) {
		return true;
	}

	return false;
}

void BroadcastClient::Close() {
	if( m_socket_c != INVALID_SOCKET ) {
		closesocket( m_socket_c );
		m_socket_c = INVALID_SOCKET;
	}

#ifdef __OS_WINDOWS__
	WSACleanup();
#endif
}
