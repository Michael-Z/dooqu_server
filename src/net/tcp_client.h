#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

/*
对于用户离开的处理：

1、当用户网络错误、直接关闭客户的情况：
a.receive_handle会直接进入error的分支
b.在error分支中将tcp_client.availible_置成 false,标识socket不可用
c.在error分支处理on_error.
d.在game_client.on_error中进行simulate_receive.BYE，将离开通知逻辑处理
e.on_error处理完后，delete this.

2、当用户输入非法内容、例如发送超长:
a.在receive_handle中有两处进行了超长的检查，会先调用game_client.on_error，并传入error号
b.在game_client.on_error中进行simulate_receive.BYE，将用户的离开通知逻辑处理
c.在game_service.on_client_leave中将错误的原因通知用户，但在这里并不关闭socket
d.逻辑返回到receive_handle的delete this. 销毁实例

3、在用户输入BYE，申请离开
a.在receive_handle中的on_data中进行处理
b.on_data将BYE送给逻辑处理，并最后调用game_service.on_client_leave， 由改方法调用client.disconnect将availiable置false，并关闭socket
c.返回到receive_handle继续语句处理，后续语句发现经过on_data后，availible已经置false,说明用户已经不可用，delete this.

4、异步的检查，发现用户超时等
a.如不回显，可直接调用game_client.disconnect，将availible置false，并关闭socket.
b.关闭socket后， receive_handle会理解返回，并进入error分支.
c.error分支分支处理on_error
d.在game_client.on_error中进行simulate_receive.BYE，将离开通知逻辑处理
e.on_error处理完后，delete this.

*/

#include <cstdarg>
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

			boost::recursive_mutex status_lock_;


			//接收数据缓冲区的大小
			


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

		public:
			tcp_client(io_service& ios);


			tcp::socket& socket(){ return this->t_socket; }


			inline virtual bool available(){ return this->available_ && this->t_socket.is_open(); }


			void write(char* data);


			inline void write(bool asynchronized, char* data);


			void write(bool asynchronized, ...);


			virtual void disconnect();


			virtual void disconnect(int error_code) = 0;


			virtual ~tcp_client();


			static long CURR_RECE_TOTAL;


			static long CURR_SEND_TOTAL;


			static bool LOG_IO_DATA;

		};
	}
}



#endif
