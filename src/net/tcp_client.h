﻿#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

 
/*
status_lock_
command_lock_
data_sequence_lock_

receive_from_client
{
}

on_receive_data
{
lock(status_lock_);
lock(command_lock);
}

send_data
{
lock(status_lock_);
lock(data_sequence_lock_);
}

send_handle
{
lock(status_lock_);
lock(data_sequence_lock_);
}

disconnect()
{
lock(status_lock_);
}


disconnect_when_io_end()
{
lock(status_lock_);
lock(data_sequence_lock_);
}

*/

#include <cstdarg>
#include <string>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/pool/singleton_pool.hpp>
#include <boost/noncopyable.hpp>
#include "service_error.h"

namespace dooqu_server
{
	namespace net
	{
		using namespace boost::asio;
		using namespace boost::asio::ip;

		class tcp_server;

		class tcp_client : boost::noncopyable
		{
		public:
			enum{ MAX_BUFFER_SIZE = 65 };
		protected:
			//状态锁

			boost::recursive_mutex status_lock_;

			//发送数据缓冲区		
			std::vector< std::vector<char> > send_buffer_sequence_;		

			//数据锁
			boost::mutex send_buffer_lock_;

			//正在处理的位置，这个位置是发送函数正在读取的位置
			int read_pos_;

			//write_pos_永远指向该写入的位置
			int write_pos_;


			//接收数据缓冲区
			char buffer[MAX_BUFFER_SIZE];


			//指向缓冲区的指针，使用asio::buffer会丢失位置
			char* p_buffer;


			//当前缓冲区的位置
			unsigned int buffer_pos;


			//io_service对象
			io_service& ios;
			  

			//包含的socket对象
			tcp::socket t_socket;


			//用来标识socket对象是否可用
			bool available_;


			//开始从客户端读取数据
			void read_from_client();


			//用来接收数据的回调方法
			virtual void on_data_received(const boost::system::error_code& error, size_t bytes_received);


			//发送数据的回调方法
			void send_handle(const boost::system::error_code& error);


			//当有数据到达时会被调用，该方法为虚方法，由子类进行具体逻辑的处理和运用
			//inline virtual void on_data(char* client_data) = 0;


			//当客户端出现规则错误时进行调用， 该方法为虚方法，由子类进行具体的逻辑的实现。
			virtual void on_error(const int error) = 0;

			inline bool alloc_available_buffer(std::vector<char>** buffer_alloc);

		public:
			tcp_client(io_service& ios);


			tcp::socket& socket(){ return this->t_socket; }


			inline virtual bool available(){ return this->available_ && this->t_socket.is_open(); }


			void write(char* data);


			void write(const char* format, ...);


			void disconnect_when_io_end();


			void disconnect();


			virtual void disconnect(int error_code) = 0;


			virtual ~tcp_client();


			static long CURR_RECE_TOTAL;


			static long CURR_SEND_TOTAL;


			static bool LOG_IO_DATA;

		};
	}
}



#endif
