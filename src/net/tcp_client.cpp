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
			: ios(ios),
			t_socket(ios),
			send_buffer_sequence_(1, buffer_stream(MAX_BUFFER_SIZE)),
			read_pos_(-1), write_pos_(0), available_(false),
			buffer_pos(0)
		{
			this->p_buffer = &this->buffer[0];
			memset(this->buffer, 0, sizeof(MAX_BUFFER_SIZE));
		}


		void tcp_client::read_from_client()
		{
			boost::recursive_mutex::scoped_lock lock(this->status_lock_);

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
				if (this->send_buffer_sequence_.size() > 16)
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
			boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);

			if (this->available() == false)
				return;

			boost::mutex::scoped_lock buffer_lock(this->send_buffer_lock_);

			buffer_stream* curr_buffer = NULL;

			//获取可用的buffer;
			bool ret = this->alloc_available_buffer(&curr_buffer);

			if (ret == false)
			{
				this->disconnect();
				return;
			}

			//代码到这里 send_buffer已经获取到，下面准备向内填写数据;
			int buff_size = 0;
			int try_count = 5;

			do
			{
				if (try_count < 5)
				{
					curr_buffer->double_size();
				}

				va_list arg_ptr;
				va_start(arg_ptr, format);

				buff_size = curr_buffer->write(format, arg_ptr);

				va_end(arg_ptr);

			} while ((buff_size == -1 || (curr_buffer->size() == curr_buffer->capacity() && *curr_buffer->at(curr_buffer->size() - 1) != 0)) && try_count-- > 0);


			if (buff_size == -1)
			{
				printf("server message error.\n");
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


		void tcp_client::disconnect_when_io_end()
		{
			boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);

			if (this->available() == false)
				return;

			boost::mutex::scoped_lock buffer_lock(this->send_buffer_lock_);


			if (this->read_pos_ != -1)
			{
				buffer_stream* curr_buffer = NULL;

				bool alloc_ret = this->alloc_available_buffer(&curr_buffer);

				if (alloc_ret)
				{
					curr_buffer->zero();
					return;
				}
			}

			this->disconnect();
		}


		void tcp_client::send_handle(const boost::system::error_code& error)
		{
			if (error)
			{
				printf("send_handle error: %s\n", error.message());
				return;
			}


			boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);

			//这里考虑下极限情况，如果this已经被销毁？
			if (this->available() == false)
			{
				return;
			}

			boost::mutex::scoped_lock buffer_lock(this->send_buffer_lock_);

			buffer_stream* curr_buffer = &this->send_buffer_sequence_.at(this->read_pos_);

			if (read_pos_ >= (write_pos_ - 1))
			{
				//全部处理完
				read_pos_ = -1;
				write_pos_ = 0;

				while (this->send_buffer_sequence_.size() > 5)
				{
					this->send_buffer_sequence_.pop_back();
				}
			}
			else
			{
				read_pos_++;

				buffer_stream* curr_buffer = &this->send_buffer_sequence_.at(this->read_pos_);

				if (curr_buffer->size() == 0)
				{
					this->disconnect();
					return;
				}

				boost::asio::async_write(this->t_socket, boost::asio::buffer(curr_buffer->read(), curr_buffer->size()),
					boost::bind(&tcp_client::send_handle, this, boost::asio::placeholders::error));

			}
		}


		void tcp_client::disconnect()
		{
			boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);

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
			if (tcp_client::LOG_IO_DATA)
			{
				tcp_client::CURR_RECE_TOTAL += (long)bytes_received;
			}
		}


		tcp_client::~tcp_client()
		{

		}
	}
}


