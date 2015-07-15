#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include <map>
#include <vector>
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
		typedef std::vector<boost::thread*> worker_threads;



		class tcp_server : boost::noncopyable
		{
		protected:
			friend class tcp_client;

			enum{ MAX_ACCEPTION_NUM = 2 };
			//io_service ios;
			io_service io_service_;
			io_service io_service_for_unload_;
			io_service::work* work_mode_;
			tcp::acceptor acceptor;
			unsigned int port;
			bool is_running_;
			bool is_accepting_;
			thread_status_map threads_status_;
			worker_threads worker_threads_;


			void create_worker_thread();
			void start_accept();
			void accept_handle(const boost::system::error_code& error, tcp_client* client);

			virtual tcp_client* on_create_client() = 0;
			virtual void on_client_join(tcp_client* client) = 0;
			virtual void on_destroy_client(tcp_client*) = 0;
			virtual void on_init();
			virtual void on_start();
			virtual void on_stop();

		public:

			tcp_server(unsigned int port);
			io_service& get_io_service(){ return this->io_service_; };
			io_service& get_io_service_for_unload(){ return this->io_service_for_unload_; }
			thread_status_map* threads_status(){ return &this->threads_status_; }
			void tick_count_threads(){
 for (int i = 0; i < this->worker_threads_.size();i++)
 {
	 this->io_service_.post(boost::bind(&tcp_server::update_curr_thread, this));
 }
			}

			void update_curr_thread()
			{
				thread_status_map::iterator curr_thread_pair = this->threads_status()->find(boost::this_thread::get_id());

				if (curr_thread_pair != this->threads_status()->end())
				{
					curr_thread_pair->second->restart();

					printf("post update at thread: %d\n", boost::this_thread::get_id());
				}
			}
			void start();
			void stop();
			void stop_accept();
			inline bool is_running(){ return this->is_running_; }
			virtual ~tcp_server();

		};
	}
}
#endif
