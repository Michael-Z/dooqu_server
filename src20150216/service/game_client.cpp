#include "game_client.h"

namespace dooqu_server
{
	namespace service
	{
		game_client::game_client(io_service& ios) : tcp_client(ios)
		{
			this->id_[0] = 0;
			this->name_[0] = 0;
			this->game_info_ = NULL;
			this->cmd_dispatcher_ = NULL;
			this->available_ = false;
			this->error_code_ = 0;
			this->active();
			this->message_monitor_.init(5, 4000);
			this->active_monitor_.init(30, 60 * 1000);
		}


		game_client::~game_client()
		{
			if (this->game_info_ != NULL)
			{
				delete this->game_info_;
			}

			printf("~game_client\n");
		}

		void game_client::on_data(char* data)
		{
			//printf("game_client.ondata:%s", data);
			//上锁，保证用户的命令是串行的
			boost::recursive_mutex::scoped_lock lock(this->commander_mutex_);

			this->commander_.reset(data);

			if (this->commander_.is_correct())
			{
				this->on_command(&this->commander_);
			}
		}

		void game_client::on_error(const int error)
		{
			char buffer[16];
			memset(buffer, 0, sizeof(buffer));
			sprintf(buffer, "BYE %d", error);

			if (this->error_code_ == 0)
			{
				this->error_code_ = error;
			}

			printf("{%s} game_client::on_error:{%d}\n", this->id(), this->error_code_);
			this->simulate_on_command(buffer, false);

		}

		void game_client::on_command(command* command)
		{
			if (this->cmd_dispatcher_ != NULL)
			{
				this->cmd_dispatcher_->action(this, command);
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
			boost::recursive_mutex::scoped_lock lock(this->commander_mutex_);

			this->commander_.reset(command_data_clone);
			if (this->commander_.is_correct())
			{
				this->on_command(&this->commander_);
			}

			if (is_const_string)
			{
				delete[] command_data_clone;
			}
		}


		void game_client::set_command_dispatcher(command_dispatcher* dispatcher)
		{
			this->cmd_dispatcher_ = dispatcher;
		}


		//disconnect相比disconnect(int reason_code)的区别
		//disconnect不会发给用户离开原因
		void game_client::disconnect()
		{
			tcp_client::disconnect();
		}


		//disconnect(int reason_code)的主要工作:
		//1、判断客户端是否还在可用状态
		//2、如果可用将bye命令发给客户端，code为断开的原因
		//3、tcp_client::disconnect会调用game_client::on_error
		//4、game_client::on_error会调用simulate_command进行虚拟BYE命令触发
		//5、simluate_command会触发离开的命令
		void game_client::disconnect(int code)
		{
			if (this->available())
			{
				//设定错误号
				this->error_code_ = code;

				//向其回显错误信息
				char buffer[16];
				int n = std::sprintf(buffer, "BYE %d", code);
				buffer[n] = 0;
				this->write(false, buffer);

				//关闭socket
				tcp_client::disconnect();
			}
		}
	}
}
