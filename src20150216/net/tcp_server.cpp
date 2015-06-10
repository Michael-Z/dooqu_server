#include "tcp_server.h"

namespace dooqu_server
{
	namespace net
	{
		tcp_server::tcp_server(unsigned int port) :
			work_(ios),
			acceptor(ios, tcp::endpoint(tcp::v4(), port)),
			port(port),
			is_running_(false)
		{

		}

		void tcp_server::start_accept()
		{
			//提前把game_client变量准备好。该game_client实例如果监听失败，在on_accept的error分支中delete。
			tcp_client* client = this->on_create_client();

			//投递accept监听
			this->acceptor.async_accept(client->socket(), boost::bind(&tcp_server::accept_handle, this, boost::asio::placeholders::error, client));
		}


		void tcp_server::start()
		{
			if (this->is_running_ == true)
			{
				return;
			}

			this->on_start();

			this->is_running_ = true;

			//投递 {MAX_ACCEPTION_NUM} 个accept动作
			for (int i = 0; i < MAX_ACCEPTION_NUM; i++)
			{
				this->start_accept();
			}

			//开始工作者线程
			for (int i = 0; i < boost::thread::hardware_concurrency() + 1; i++)
			{
				boost::thread t(boost::bind(&io_service::run, &this->ios));
				this->threads_status_[t.get_id()] = new tick_count_t();

				printf("create thread's id=%d\n", t.get_id());
				t.detach();
			}

			printf("start %d worker thread.\n", boost::thread::hardware_concurrency() + 1);
			this->on_started();
		}


		void tcp_server::stop()
		{
			if (this->is_running_ == false)
			{
				return;
			}
			this->is_running_ = false;

			//不接受新的client
			boost::system::error_code error;
			this->acceptor.cancel(error);
			this->acceptor.close(error);



			//停止所有异步事件的获取
			this->ios.stop();

			//小等100毫秒，让在ios.stop之前就执行在线程的事件执行完成。
			boost::this_thread::sleep(boost::posix_time::milliseconds(200));

			//资源清理
			this->on_stop();
		}


		void tcp_server::on_start()
		{
		}


		void tcp_server::on_started()
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
					client->disconnect(service_error::SERVER_CLOSEING);
				}
				return;
			}



			//如果走到这个分支，说明game_server已经调用了stop，停止了服务；
			//但因为start_accept，已经提前投递了N个game_client
			//所以这里返回要对game_client进行销毁处理
			printf("%s\n", error.message().c_str());
			this->on_destroy_client(client);
		}

		tcp_server::~tcp_server()
		{
			printf("~tcp_server\n");
		}
	}
}
