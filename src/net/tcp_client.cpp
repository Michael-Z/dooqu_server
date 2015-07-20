#include "tcp_client.h"
#include "tcp_server.h"
#include "threads_lock_status.h"

namespace dooqu_server
{
	namespace net
	{
		bool tcp_client::LOG_IO_DATA = false;
		long tcp_client::CURR_RECE_TOTAL = 0;
		long tcp_client::CURR_SEND_TOTAL = 0;

		tcp_client::tcp_client(io_service& ios)
			: ios(ios),
			t_socket(ios),
			send_buffer_sequence_(3, buffer_stream(MAX_BUFFER_SIZE)),
			read_pos_(-1), write_pos_(0),
			buffer_pos(0),
			available_(false)
		{
			this->p_buffer = &this->buffer[0];
			memset(this->buffer, 0, sizeof(MAX_BUFFER_SIZE));
		}


		void tcp_client::read_from_client()
		{
			//thread_status::log("start:read_from_client");
			//boost::recursive_mutex::scoped_lock lock(this->status_lock_);
			//thread_status::log("end:read_from_client");

			if (this->available() == false)
				return;

			this->t_socket.async_read_some(boost::asio::buffer(this->p_buffer, tcp_client::MAX_BUFFER_SIZE - this->buffer_pos),
				boost::bind(&tcp_client::on_data_received, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}


		bool tcp_client::alloc_available_buffer(buffer_stream** buffer_alloc)
		{
			//如果当前的写入位置的指针指向存在一个可用的send_buffer，那么直接取这个集合；
			if (write_pos_ < this->send_buffer_sequence_.size())
			{
				//得到当前的send_buffer
				*buffer_alloc = &this->send_buffer_sequence_.at(this->write_pos_);
				this->write_pos_++;

				return true;
			}
			else
			{
				//如果指向的位置不存在，需要新申请对象；
				//如果已经存在了超过16个对象，说明网络异常那么断开用户；不再新申请数据;
				if (this->send_buffer_sequence_.size() > MAX_BUFFER_SEQUENCE_SIZE)
				{
					*buffer_alloc = NULL;
					return false;
				}

				//将新申请的消息承载体推入队列
				this->send_buffer_sequence_.push_back(buffer_stream(MAX_BUFFER_SIZE));

				//将写入指针指向下一个预置位置
				this->write_pos_ = this->send_buffer_sequence_.size() - 1;

				//得到当前的send_buffer/
				*buffer_alloc = &this->send_buffer_sequence_.at(this->write_pos_);

				this->write_pos_++;

				return true;
			}
		}


		void tcp_client::write(char* data)
		{
			//默认调用的是异步的写
			this->write("%s%c", data, NULL);
		}


		void tcp_client::write(const char* format, ...)
		{
			thread_status::log("start:tcp_write.status_lock_");
			boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);
			thread_status::log("end:tcp_write.status_lock_");

			if (this->available() == false)
				return;
			thread_status::log("start:tcp_write.send_buffer_lock_");
			boost::recursive_mutex::scoped_lock buffer_lock(this->send_buffer_lock_);
			thread_status::log("start:tcp_write.send_buffer_lock_");

			buffer_stream* curr_buffer = NULL;

			//获取可用的buffer;
			bool ret = this->alloc_available_buffer(&curr_buffer);

			if (ret == false)
			{
				//如果堆积了很多未发送成功的的消息，说明客户端网络状况已经不可到达。
				this->available_ = false;
				this->on_error(service_error::CLIENT_NET_ERROR);
				return;
			}

			//代码到这里 send_buffer已经获取到，下面准备向内填写数据;
			int buff_size = 0;
			int try_count = MAX_BUFFER_SIZE_DOUBLE_TIMES;

			do
			{
				if (try_count < MAX_BUFFER_SIZE_DOUBLE_TIMES)
				{
					curr_buffer->double_size();
				}

				va_list arg_ptr;
				va_start(arg_ptr, format);

				buff_size = curr_buffer->write(format, arg_ptr);

				va_end(arg_ptr);

				if (buff_size == -1)
				{
					printf(format);
				}

			} while ((buff_size == -1 || (curr_buffer->size() == curr_buffer->capacity() && *curr_buffer->at(curr_buffer->size() - 1) != 0)) && try_count-- > 0);


			if (buff_size == -1)
			{
				printf("ERROR: server message queue limited,the message can not be send.\n");
				this->write_pos_--;
				return;
			}
			//assert(buffer_size != -1);

			//如果正在发送的索引为-1，说明空闲
			if (read_pos_ == -1)
			{
				read_pos_++;

				//只要read_pos_ == -1，说明write没有在处理任何数据，说明没有处于发送状态
				boost::asio::async_write(this->t_socket, boost::asio::buffer(curr_buffer->read(), curr_buffer->size()),
					boost::bind(&tcp_client::send_handle, this, boost::asio::placeholders::error));
			}
		}




		void tcp_client::send_handle(const boost::system::error_code& error)
		{
			if (error)
			{
				printf("send_handle error: %s\n", error.message());
				return;
			}

			thread_status::log("start->tcp_client::send_handle.status_lock_");
			boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);
			thread_status::log("end->tcp_client::send_handle.status_lock_");
			//这里考虑下极限情况，如果this已经被销毁？
			if (this->available() == false)
			{
				return;
			}

			thread_status::log("start:tcp_write.send_buffer_lock_");
			boost::recursive_mutex::scoped_lock buffer_lock(this->send_buffer_lock_);
			thread_status::log("end:tcp_write.send_buffer_lock_");

			buffer_stream* curr_buffer = &this->send_buffer_sequence_.at(this->read_pos_);

			if (read_pos_ >= (write_pos_ - 1))
			{
				//全部处理完
				read_pos_ = -1;
				write_pos_ = 0;

				//while (this->send_buffer_sequence_.size() > 5)
				//{
				//	this->send_buffer_sequence_.pop_back();
				//}
			}
			else
			{
				read_pos_++;

				buffer_stream* curr_buffer = &this->send_buffer_sequence_.at(this->read_pos_);

				if (curr_buffer->is_bye_signal())
				{
					this->disconnect();
					return;
				}

				boost::asio::async_write(this->t_socket, boost::asio::buffer(curr_buffer->read(), curr_buffer->size()),
					boost::bind(&tcp_client::send_handle, this, boost::asio::placeholders::error));

			}
		}


		void tcp_client::disconnect_when_io_end()
		{
			thread_status::log("start->tcp_client::disconnect_when_io_end.status_lock_");
			boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);
			thread_status::log("end->tcp_client::disconnect_when_io_end.status_lock_");

			if (this->available() == false)
				return;

			thread_status::log("start->tcp_client::disconnect_when_io_end.send_buffer_lock");
			boost::recursive_mutex::scoped_lock buffer_lock(this->send_buffer_lock_);
			thread_status::log("end->tcp_client::disconnect_when_io_end.send_buffer_lock");

			if (this->read_pos_ != -1)
			{
				buffer_stream* curr_buffer = NULL;

				bool alloc_ret = this->alloc_available_buffer(&curr_buffer);

				if (alloc_ret)
				{
					curr_buffer->set_bye_signal();
					return;
				}
			}

			this->disconnect();
		}



		void tcp_client::disconnect()
		{
			thread_status::log("start->tcp_client::disconnect.status_lock_");
			boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);
			thread_status::log("start->tcp_client::disconnect.status_lock_");

			if (this->available_)
			{
				this->available_ = false;

				boost::system::error_code err_code;

				this->t_socket.shutdown(boost::asio::socket_base::shutdown_receive, err_code);
				this->t_socket.close(err_code);
			}
		}


		void tcp_client::on_data_received(const boost::system::error_code& error, size_t bytes_received)
		{
			if (!error && tcp_client::LOG_IO_DATA && bytes_received)
			{
				tcp_client::CURR_RECE_TOTAL += (long)bytes_received;
			}
		}


		tcp_client::~tcp_client()
		{
		}
	}
}


