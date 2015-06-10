#include "tcp_server.h"

namespace dooqu_server
{
	namespace net
	{
		tcp_server::tcp_server(unsigned int port) :
			acceptor(io_service_, tcp::endpoint(tcp::v4(), port)),
			port(port),
			is_running_(false),
			is_accepting_(false)
		{
			this->work_mode_ = new io_service::work(this->io_service_);
		}


		void tcp_server::create_worker_thread()
		{
			boost::thread* worker_thread = new boost::thread(boost::bind(&io_service::run, &this->get_io_service()));

			this->threads_status_[worker_thread->get_id()] = new tick_count_t();

			worker_threads_.push_back(worker_thread);

			printf("create worker thread,id={%d}\n", worker_thread->get_id());
		}


		void tcp_server::start_accept()
		{
			//提前把game_client变量准备好。该game_client实例如果监听失败，在on_accept的error分支中delete。
			tcp_client* client = this->on_create_client();

			//投递accept监听
			this->acceptor.async_accept(client->socket(), boost::bind(&tcp_server::accept_handle, this, boost::asio::placeholders::error, client));

			this->is_accepting_ = true;
		}


		void tcp_server::start()
		{
			if (this->is_running_ == false)
			{
				this->on_init();

				this->is_running_ = true;

				//投递 {MAX_ACCEPTION_NUM} 个accept动作
				for (int i = 0; i < MAX_ACCEPTION_NUM; i++)
				{
					this->start_accept();
				}

				//开始工作者线程
				for (int i = 0; i < boost::thread::hardware_concurrency() + 1; i++)
				{
					this->create_worker_thread();
				}

				this->on_start();
			}
		}


		void tcp_server::stop_accept()
		{
			if (this->is_accepting_ == true)
			{
				boost::system::error_code error;
				this->acceptor.cancel(error);
				this->acceptor.close(error);

				this->is_accepting_ = false;
			}
		}


		void tcp_server::stop()
		{
			if (this->is_running_ == true)
			{
				this->is_running_ = false;

				//不接受新的client
				this->stop_accept();

				this->on_stop();

				//让所有工作者线程不再空等
				delete this->work_mode_;

				for (int i = 0; i < this->worker_threads_.size(); i++)
				{
					printf("waiting for thread exit: %d\n", this->worker_threads_.at(i)->get_id());
					this->worker_threads_.at(i)->join();
				}

				this->io_service_.stop();

				printf("server has been stoped successfully.\n");
			}
		}


		void tcp_server::on_init()
		{
		}


		void tcp_server::on_start()
		{
		}


		void tcp_server::on_stop()
		{
		}


		void tcp_server::accept_handle(const boost::system::error_code& error, tcp_client* client)
		{
			if (!error)
			{
				if (this->is_running_)
				{
					//处理新加入的game_client对象，这个时候game_client的available已经是true;
					this->on_client_join(client);

					//处理当前的game_client对象，继续投递一个新的接收请求；
					start_accept();
				}
				else
				{
					//ios is running
					this->on_destroy_client(client);
				}
				return;
			}
			else
			{
				//如果走到这个分支，说明game_server已经调用了stop，停止了服务；
				//但因为start_accept，已经提前投递了N个game_client
				//所以这里返回要对game_client进行销毁处理
				printf("tcp_server.accept_handle canceled:%s\n", error.message().c_str());

				this->on_destroy_client(client);
			}
		}


		tcp_server::~tcp_server()
		{
			for (int i = 0; i < worker_threads_.size(); i++)
			{
				delete worker_threads_.at(i);
			}
		}
	}
}
