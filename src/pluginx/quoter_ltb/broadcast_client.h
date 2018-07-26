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

#ifndef QUOTER_LTB_BROADCAST_CLIENT_H
#define QUOTER_LTB_BROADCAST_CLIENT_H

#include <iostream>
#include <winsock2.h>

class BroadcastClient
{
public:
	BroadcastClient();
	~BroadcastClient();

public:
	int32_t InitBroadcast( std::string address, int32_t port );
	int32_t Read( void* buffer, int32_t need_length );
	int32_t RecvFrom( void* buffer, int32_t need_length );
	bool IsReadable( int32_t wait_time = 0 );
	void Close();

public:
	uint32_t m_socket_c;
	int32_t m_time_out;
};

#endif // QUOTER_LTB_BROADCAST_CLIENT_H
