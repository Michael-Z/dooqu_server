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
			//boost::recursive_mutex::scoped_lock lock(this->status_lock_);

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
				//receive_handle要在工作者现成上执行；
				//在其他工作者线程上，同一时间不肯定不会有receive_handle的调用，因为receive本质是一个串行的动作；
				//但是! 可能因为逻辑需要、在其他工作者线程上调用disconnect函数，所以必须要同步status；
				boost::recursive_mutex::scoped_lock lock(this->status_lock_);

				if (!error)
				{
					this->buffer_pos += bytes_received;

					if (this->buffer_pos > tcp_client::MAX_BUFFER_SIZE)
					{
						this->disconnect(service_error::DATA_OUT_OF_BUFFER);

						//此后一句on_error必须添加，因为该段最后已经return， 没有继续recv，也就无法触发0接收
						this->on_error(service_error::DATA_OUT_OF_BUFFER);
						//delete this;
						return;
					}

					for (int i = 0, cmd_start_pos = 0; i < this->buffer_pos; ++i)
					{
						if (this->buffer[i] == 0)
						{
							this->on_data(&this->buffer[cmd_start_pos]);

							//如果on_data 中如需要对用户进行离线处理，那么只需调用disconnect
							//在disconnect中，设置available = false，并关闭接收，关闭socket。
							//那么在之后的检查中，判断需要对用户的离开进行处理，调用on_error进行清理，并离开大逻辑循环。
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

									//此后一句on_error必须添加，因为该段最后已经return， 没有继续recv，也就无法触发0接收
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


