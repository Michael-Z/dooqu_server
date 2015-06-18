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
				boost::bind(&tcp_client::on_data_received, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		}
		


		void tcp_client::write(char* data)
		{
			//Ĭ�ϵ��õ����첽��д
			this->write(true, data);
		}



		void tcp_client::write(bool asynchronized, char* data)
		{

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



		void tcp_client::on_data_received(const boost::system::error_code& error, size_t bytes_received)
		{
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


