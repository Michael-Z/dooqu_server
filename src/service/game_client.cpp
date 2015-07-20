#include "game_client.h"

namespace dooqu_server
{
	namespace service
	{
		game_client::game_client(io_service& ios) :
			tcp_client(ios),
			game_info_(NULL),
			cmd_dispatcher_(NULL),
			error_code_(0)
		{
			this->id_[0] = 0;
			this->name_[0] = 0;
			this->retry_update_times_ = UP_RE_TIMES;
			this->active();
			this->message_monitor_.init(5, 4000);
			this->active_monitor_.init(30, 60 * 1000);
		}


		game_client::~game_client()
		{
			printf("game_client destroyed.\n");
		}


		void game_client::on_data_received(const boost::system::error_code& error, size_t bytes_received)
		{
			{
				//receive_handle要在工作者现成上执行；
				//在其他工作者线程上，同一时间不肯定不会有receive_handle的调用，因为receive本质是一个串行的动作；
				//但是! 可能因为逻辑需要、在其他工作者线程上调用disconnect函数，所以必须要同步status；
				//防止在这个代码片段的中间被其他线程disconnect掉

				if (!error)
				{
					if (this->cmd_dispatcher_ != NULL)
					{
						this->cmd_dispatcher_->action(this, bytes_received);

						tcp_client::on_data_received(error, bytes_received);
					}
#ifdef __DEBUG__
					else
					{
						printf("game_client's cmd_dispatcher is NULL.\n");
					}
#endif
				}
				else
				{

					thread_status::log("start->game_client::on_date_received.status_lock_");
					boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);
					thread_status::log("end->game_client::on_date_received.status_lock_");

					printf("client.receive canceled:{%s}\n", error.message().c_str());
					this->available_ = false;
					this->on_error(service_error::CLIENT_NET_ERROR);
				}

			}//end lock;
		}


		//on_error的主要功能是将用户的离开和逻辑错误动作传递给command_dispatcher对象进行依次处理。
		void game_client::on_error(const int error)
		{
			//error_code_的初始默认值为0、即CLIENT_NET_ERROR；
			//如果这个值被改动，说明在on_error之前、调用过disconnect、并传递过断开的原因；
			//这样在tcp_client的断开处理中、即使传递0，也不会被赋值；
			if (this->error_code_ == service_error::CLIENT_NET_ERROR)
			{
				this->error_code_ = error;
			}

			printf("game_client{%s}.on_error:{%d}\n", this->id(), this->error_code_);

			if (this->cmd_dispatcher_ != NULL)
			{
				this->cmd_dispatcher_->dispatch_bye(this);
			}
		}


		void game_client::fill(char* id, char* name, char* profile)
		{
			int id_len = std::strlen(id);
			int name_len = std::strlen(name);
			int pro_len = (profile == NULL) ? 0 : std::strlen(profile);

			int min_id_len = std::min((ID_LEN - 1), id_len);
			int min_name_len = std::min((NAME_LEN - 1), name_len);

			strncpy(this->id_, id, min_id_len);
			strncpy(this->name_, name, min_name_len);

			this->id_[min_id_len] = 0;
			this->name_[min_name_len] = 0;
		}


		void game_client::simulate_on_command(char* command_data, bool is_const_string = true)
		{

			if (this->available() == false || this->cmd_dispatcher_ == NULL)
				return;

			char* command_data_clone = NULL;

			//如果是传入的是一个常量串，那么我们要重新生成一个可写的缓冲区
			if (is_const_string)
			{
				//生成可写的缓冲区
				command_data_clone = new char[std::strlen(command_data) + 1];
				std::strcpy(command_data_clone, command_data);
			}
			else
			{
				command_data_clone = command_data;
			}

			//加锁
			this->cmd_dispatcher_->simulate_client_data(this, command_data_clone);

			if (is_const_string)
			{
				delete[] command_data_clone;
			}
		}


		void game_client::set_command_dispatcher(command_dispatcher* dispatcher)
		{
			this->cmd_dispatcher_ = dispatcher;
		}


		//disconnect(int reason_code)的主要工作:
		//1、判断客户端是否还在可用状态
		//2、如果可用将bye命令发给客户端，code为断开的原因
		//3、tcp_client::disconnect只会在client receive动作已经投递后，才能触发error； 
		//4、so：disconnect和receive_handle可能运行在两个并发线程上。有机会在receive_handle中，还没
		//来得及投递下一次receive，就被close掉，这就导致on_error不会触发，so so:加锁
		void game_client::disconnect(int code)
		{
			if (this->available())
			{
				//设定错误号
				this->error_code_ = code;

				this->write("ERR %d%c", this->error_code_, NULL);
				this->disconnect_when_io_end();
			}
		}
	}
}
