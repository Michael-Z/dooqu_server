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
			//��ǰ��game_client����׼���á���game_clientʵ���������ʧ�ܣ���on_accept��error��֧��delete��
			tcp_client* client = this->on_create_client();

			//Ͷ��accept����
			this->acceptor.async_accept(client->socket(), boost::bind(&tcp_server::accept_handle, this, boost::asio::placeholders::error, client));
		}

		void tcp_server::start()
		{
			if(this->is_running == true)
				return;

			this->on_start();

			this->is_running = true;

			//Ͷ�� {MAX_ACCEPTION_NUM} ��accept����
			for(int i = 0; i < MAX_ACCEPTION_NUM; i++)
			{
				this->start_accept();
			}

			//��ʼ�������߳�
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

			//�������߳�ֹͣ�������첽�ĵ��ö�ֹͣ		
			this->ios.stop();

			boost::system::error_code error;
			//�������µ�client

			this->acceptor.cancel();

			this->acceptor.close(error);	

			//acceptor.close������Ҫ�̵߳�һ�ᣬĿ������acceptor����������߳�����ǰͶ�ݵ�game_client��������
			boost::this_thread::sleep(boost::posix_time::milliseconds(100));
			//��Դ����
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