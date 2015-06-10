#include "tcp_server.h"

namespace dooqu_server
{
	namespace net
	{
		tcp_server::tcp_server(io_service& ios, unsigned int port) : ios(ios), acceptor(ios, tcp::endpoint(tcp::v4(), port)), port(port)
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
			if(this->is_running == true)
				return;

			this->on_start();

			this->is_running = true;

			//投递 {MAX_ACCEPTION_NUM} 个accept动作
			for(int i = 0; i < MAX_ACCEPTION_NUM; i++)
			{
				this->start_accept();
			}

			//开始工作者线程
			for(int i = 0; i < boost::thread::hardware_concurrency(); i++)
			{
				boost::thread t(boost::bind(&io_service::run, &this->ios)); 
			}
			printf("\nstart %d worker thread.", boost::thread::hardware_concurrency());
			this->on_started();
		}
		

		void tcp_server::stop()
		{
			if(this->is_running == false)
				return;

			this->is_running = false;

			//工作者线程停止，所有异步的调用都停止		
			this->ios.stop();

			boost::system::error_code error;
			//不接受新的client

			this->acceptor.cancel();

			this->acceptor.close(error);	

			//acceptor.close后，这里要线程等一会，目的是让acceptor中在另外的线程中提前投递的game_client能先销毁
			boost::this_thread::sleep(boost::posix_time::milliseconds(100));
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
			if(!error && this->is_running)
			{
				this->on_client_join(client);
			}
			else
			{
				delete client;
			}

			if(this->is_running)
			{
				start_accept();
			}
		}

		tcp_server::~tcp_server()
		{
			printf("\n~tcp_server");
			//this->acceptor.close();
		}
	}
}