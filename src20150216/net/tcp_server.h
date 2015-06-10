#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include <map>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/noncopyable.hpp>
#include "tcp_client.h"

namespace dooqu_server
{
	namespace net
	{
		using namespace boost::asio::ip;
		using namespace boost::asio;
		using namespace boost::posix_time;

		class tick_count_t : boost::noncopyable
		{
		protected:
			ptime start_time_;
		public:
			tick_count_t(){ this->restart(); }
			void restart(){ this->start_time_ = microsec_clock::local_time(); }
			void restart(long long millisecs){ this->start_time_ = microsec_clock::local_time(); this->start_time_ + milliseconds(millisecs); }
			long long elapsed(){ return (microsec_clock::local_time() - this->start_time_).total_milliseconds(); }
		};


		typedef std::map<boost::thread::id, tick_count_t*> thread_status_map;


		class tcp_server : boost::noncopyable
		{
		protected:
			friend class tcp_client;

			enum{ MAX_ACCEPTION_NUM = 2 };
			io_service ios;
			io_service::work work_;
			tcp::acceptor acceptor;
			unsigned int port;
			bool is_running_;
			thread_status_map threads_status_;

			void start_accept();
			void accept_handle(const boost::system::error_code& error, tcp_client* client);

			virtual tcp_client* on_create_client() = 0;
			virtual void on_client_join(tcp_client* client) = 0;
			virtual void on_destroy_client(tcp_client*) = 0;
			virtual void on_start();
			virtual void on_started();
			virtual void on_stop();

		public:

			tcp_server(unsigned int port);
			io_service& get_io_service(){ return this->ios; };
			thread_status_map* threads_status(){ return &this->threads_status_; }
			void start();
			void stop();
			inline bool is_running(){ return this->is_running_; }
			virtual ~tcp_server();

		};
	}
}
#endif
