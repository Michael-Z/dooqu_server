#include "tcp_client.h"
#include "tcp_server.h"

namespace dooqu_server
{
	namespace net
	{
		bool tcp_client::LOG_IO_DATA = false;
		long tcp_client::CURR_RECE_TOTAL = 0;
		long tcp_client::CURR_SEND_TOTAL = 0;

		tcp_client::tcp_client(io_service& ios)
			: ios(ios), t_socket(ios)
		{
			this->p_buffer = &this->buffer[0];
			this->buffer_pos = 0;
			memset(this->buffer, 0, sizeof(this->buffer));
		}



		void tcp_client::read_from_client()
		{
			if (this->available() == false)
				return;

			this->t_socket.async_read_some(boost::asio::buffer(this->p_buffer, tcp_client::MAX_BUFFER_SIZE - this->buffer_pos),
				boost::bind(&tcp_client::receive_handle, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}
		


		void tcp_client::write(char* data)
		{
			//Ĭ�ϵ��õ����첽��д
			this->write(true, data);
		}



		void tcp_client::write(bool asynchronized, char* data)
		{
			//boost::recursive_mutex::scoped_lock lock(this->status_lock_);

			if (this->available() == false)
				return;

			int data_len = std::strlen(data) + 1;

			if (asynchronized)
			{
				//������첽��д�� ʹ��ȫ�ֵĺ�������������������һ�ε����ж����ͳ�ȥ���ڲ����ܻ�ʹ�ö�ε�write_some��
				boost::asio::async_write(this->t_socket, boost::asio::buffer(data, data_len),
					boost::bind(&tcp_client::send_handle, this, boost::asio::placeholders::error));
			}
			else
			{
				boost::system::error_code err_no;
				//�����ͬ����д�� ʹ��ȫ�ֵĺ�������������������һ�ε����ж����ͳ�ȥ���ڲ����ܻ�ʹ�ö�ε�write_some��
				boost::asio::write(this->t_socket, boost::asio::buffer(data, data_len), err_no);
			}

			if (tcp_client::LOG_IO_DATA)
			{
				tcp_client::CURR_SEND_TOTAL += (long)data_len;
			}
		}



		void tcp_client::write(bool asychronized, ...)
		{
			if (this->available() == false)
				return;

			const int buffer_size = 512;
			char buffer[buffer_size] = { 0 };

			char* argc = NULL;
			va_list arg_ptr;
			va_start(arg_ptr, asychronized);

			int curr_pos = 0;
			while ((argc = va_arg(arg_ptr, char*)) != NULL && curr_pos < (buffer_size - 2))
			{
				if (curr_pos != 0)
				{
					buffer[curr_pos++] = ' ';
				}

				while (curr_pos < (buffer_size - 1) && *argc)
				{
					buffer[curr_pos++] = *argc++;
				}
			}

			va_end(arg_ptr);
			buffer[buffer_size - 1] = '\0';

			this->write(asychronized, &buffer[0]);
		}



		void tcp_client::disconnect()
		{
			boost::recursive_mutex::scoped_lock lock(this->status_lock_);

			if (this->available_)
			{
				this->available_ = false;

				boost::system::error_code err_code;

				this->t_socket.shutdown(boost::asio::socket_base::shutdown_receive, err_code);
				this->t_socket.close(err_code);
			}
		}



		void tcp_client::receive_handle(const boost::system::error_code& error, size_t bytes_received)
		{
			{
				//receive_handleҪ�ڹ������ֳ���ִ�У�
				//�������������߳��ϣ�ͬһʱ�䲻�϶�������receive_handle�ĵ��ã���Ϊreceive������һ�����еĶ�����
				//����! ������Ϊ�߼���Ҫ���������������߳��ϵ���disconnect���������Ա���Ҫͬ��status��
				boost::recursive_mutex::scoped_lock lock(this->status_lock_);

				if (!error)
				{
					this->buffer_pos += bytes_received;

					if (this->buffer_pos > tcp_client::MAX_BUFFER_SIZE)
					{
						this->disconnect(service_error::DATA_OUT_OF_BUFFER);

						//�˺�һ��on_error������ӣ���Ϊ�ö�����Ѿ�return�� û�м���recv��Ҳ���޷�����0����
						this->on_error(service_error::DATA_OUT_OF_BUFFER);
						//delete this;
						return;
					}

					for (int i = 0, cmd_start_pos = 0; i < this->buffer_pos; ++i)
					{
						if (this->buffer[i] == 0)
						{
							this->on_data(&this->buffer[cmd_start_pos]);

							//���on_data ������Ҫ���û��������ߴ�����ôֻ�����disconnect
							//��disconnect�У�����available = false�����رս��գ��ر�socket��
							//��ô��֮��ļ���У��ж���Ҫ���û����뿪���д�������on_error�����������뿪���߼�ѭ����
							if (this->available() == false)
							{
								//delete this;
								this->on_error(0);
								return;
							}

							if (i < (this->buffer_pos - 1))
							{
								cmd_start_pos = i + 1;
							}
						}

						if (i == (this->buffer_pos - 1))
						{
							if (this->buffer[i] != 0)
							{
								if ((this->buffer_pos - cmd_start_pos) >= tcp_client::MAX_BUFFER_SIZE)
								{
									this->disconnect(service_error::DATA_OUT_OF_BUFFER);

									//�˺�һ��on_error������ӣ���Ϊ�ö�����Ѿ�return�� û�м���recv��Ҳ���޷�����0����
									this->on_error(service_error::DATA_OUT_OF_BUFFER);
									//delete this;
									return;
								}

								if (cmd_start_pos != 0)
								{
									std::copy(this->buffer + cmd_start_pos, this->buffer + this->buffer_pos, this->buffer);
									this->buffer_pos -= cmd_start_pos;
								}
							}
							else
							{
								//�趨bufferPos = 0;��ʾbuffer���Դ�ͷ�����ˡ�
								this->buffer_pos = 0;
							}
						}
					}

					if (this->available() == false)
					{
						//delete this;
						this->on_error(service_error::CLIENT_NET_ERROR);
						return;
					}

					//�趨�û���������ʼ��������������
					this->p_buffer = &this->buffer[this->buffer_pos];
					this->read_from_client();
				}
				else
				{
					printf("client.receive canceled:{%s}\n", error.message().c_str());
					this->available_ = false;
					this->on_error(0);

					return;
				}
			}//end lock;

			if (tcp_client::LOG_IO_DATA)
			{
				tcp_client::CURR_RECE_TOTAL += (long)bytes_received;
			}
		}



		void tcp_client::send_handle(const boost::system::error_code& error)
		{
		}



		tcp_client::~tcp_client()
		{
			
		}
	}
}


