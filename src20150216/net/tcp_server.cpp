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
			//��ǰ��game_client����׼���á���game_clientʵ���������ʧ�ܣ���on_accept��error��֧��delete��
			tcp_client* client = this->on_create_client();

			//Ͷ��accept����
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

			//Ͷ�� {MAX_ACCEPTION_NUM} ��accept����
			for (int i = 0; i < MAX_ACCEPTION_NUM; i++)
			{
				this->start_accept();
			}

			//��ʼ�������߳�
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

			//�������µ�client
			boost::system::error_code error;
			this->acceptor.cancel(error);
			this->acceptor.close(error);



			//ֹͣ�����첽�¼��Ļ�ȡ
			this->ios.stop();

			//С��100���룬����ios.stop֮ǰ��ִ�����̵߳��¼�ִ����ɡ�
			boost::this_thread::sleep(boost::posix_time::milliseconds(200));

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
			if (!error)
			{
				if (this->is_running_)
				{
					//�����¼����game_client�������ʱ��game_client��available�Ѿ���true;
					this->on_client_join(client);

					//����ǰ��game_client���󣬼���Ͷ��һ���µĽ�������
					start_accept();
				}
				else
				{
					//ios is running
					client->disconnect(service_error::SERVER_CLOSEING);
				}
				return;
			}



			//����ߵ������֧��˵��game_server�Ѿ�������stop��ֹͣ�˷���
			//����Ϊstart_accept���Ѿ���ǰͶ����N��game_client
			//�������ﷵ��Ҫ��game_client�������ٴ���
			printf("%s\n", error.message().c_str());
			this->on_destroy_client(client);
		}

		tcp_server::~tcp_server()
		{
			printf("~tcp_server\n");
		}
	}
}
