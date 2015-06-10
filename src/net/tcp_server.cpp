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
			//��ǰ��game_client����׼���á���game_clientʵ���������ʧ�ܣ���on_accept��error��֧��delete��
			tcp_client* client = this->on_create_client();

			//Ͷ��accept����
			this->acceptor.async_accept(client->socket(), boost::bind(&tcp_server::accept_handle, this, boost::asio::placeholders::error, client));

			this->is_accepting_ = true;
		}


		void tcp_server::start()
		{
			if (this->is_running_ == false)
			{
				this->on_init();

				this->is_running_ = true;

				//Ͷ�� {MAX_ACCEPTION_NUM} ��accept����
				for (int i = 0; i < MAX_ACCEPTION_NUM; i++)
				{
					this->start_accept();
				}

				//��ʼ�������߳�
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

				//�������µ�client
				this->stop_accept();

				this->on_stop();

				//�����й������̲߳��ٿյ�
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
					//�����¼����game_client�������ʱ��game_client��available�Ѿ���true;
					this->on_client_join(client);

					//����ǰ��game_client���󣬼���Ͷ��һ���µĽ�������
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
				//����ߵ������֧��˵��game_server�Ѿ�������stop��ֹͣ�˷���
				//����Ϊstart_accept���Ѿ���ǰͶ����N��game_client
				//�������ﷵ��Ҫ��game_client�������ٴ���
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
