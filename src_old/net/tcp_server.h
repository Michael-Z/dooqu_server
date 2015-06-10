#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include "tcp_client.h"

using namespace boost::asio::ip;

namespace dooqu_server
{
	namespace net
	{
		class tcp_server : boost::noncopyable
		{
		protected:
			enum{ MAX_ACCEPTION_NUM = 2 };
			io_service& ios;
			tcp::acceptor acceptor;
			unsigned int port;
			bool is_running;
			void start_accept();
			void accept_handle(const boost::system::error_code& error, tcp_client* client);
			virtual tcp_client* on_create_client() = 0;
			virtual void on_client_join(tcp_client* client) = 0;
			virtual void on_start();
			virtual void on_started();
			virtual void on_stop();

		public:
			tcp_server(io_service& ios, unsigned int port);
			io_service& get_io_service(){ return this->ios;};
			void start();
			void stop();
			virtual ~tcp_server();

		};
	}
}
#endif
