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
			//默认调用的是异步的写
			this->write(true, data);
		}

		void tcp_client::write(bool asynchronized, char* data)
		{
			if (this->available() == false)
				return;

			int data_len = std::strlen(data) + 1;

			if (asynchronized)
			{
				//如果是异步的写， 使用全局的函数，保障所有数据在一次调用中都发送出去，内部可能会使用多次的write_some；
				boost::asio::async_write(this->t_socket, boost::asio::buffer(data, data_len),
					boost::bind(&tcp_client::send_handle, this, boost::asio::placeholders::error));
			}
			else
			{
				boost::system::error_code err_no;
				//如果是同步的写， 使用全局的函数，保障所有数据在一次调用中都发送出去，内部可能会使用多次的write_some；
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
			if (this->available_)
			{
				this->available_ = false;

				boost::system::error_code err_code;

				//if (this->t_socket.is_open())
				{
					this->t_socket.shutdown(boost::asio::socket_base::shutdown_both, err_code);
					this->t_socket.close(err_code);
				}

			}
		}

		void tcp_client::receive_handle(const boost::system::error_code& error, size_t bytes_received)
		{
			{// start lock;
				boost::mutex::scoped_lock lock(this->status_lock_);

				if (!error)
				{
					this->buffer_pos += bytes_received;
					if (this->buffer_pos > tcp_client::MAX_BUFFER_SIZE)
					{
						this->disconnect(service_error::DATA_OUT_OF_BUFFER);
						this->on_error(service_error::DATA_OUT_OF_BUFFER);
						//delete this;
						return;
					}

					for (int i = 0, cmd_start_pos = 0; i < this->buffer_pos; i++)
					{
						if (this->buffer[i] == 0)
						{
							this->on_data(&this->buffer[cmd_start_pos]);

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
								//设定bufferPos = 0;表示buffer可以从头开用了。
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

					//设定好缓冲区的起始，继续接收数据
					this->p_buffer = &this->buffer[this->buffer_pos];
					this->read_from_client();
				}
				else
				{
					printf("网络层错误 error_message=%s\n", error.message().c_str());
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
			this->available_ = false;
			printf("~tcp_client\n");
		}
	}
}


