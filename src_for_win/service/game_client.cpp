#include "game_client.h"

namespace dooqu_server
{
	namespace service
	{
		game_client::game_client(io_service& ios) : tcp_client(ios)
		{
			this->id_ = NULL;
			this->name_ = NULL;
			this->game_info_ = NULL;
			this->cmd_dispatcher_ = NULL;
			this->available_ = true;
			this->error_code_ = 0;
			this->active();
		}


		game_client::~game_client()
		{
			if(this->game_info_ != NULL)
			{
				delete this->game_info_;
			}

			if(this->id_ != NULL)
			{
				delete [] this->id_;
			}

			if(this->name_ != NULL)
			{
				delete [] this->name_;
			}

			printf("\n~game_client");
		}

		void game_client::on_data(char* data)
		{
			//上锁
			boost::recursive_mutex::scoped_lock lock(this->commander_mutex_);

			this->commander_.reset(data);

			if(this->commander_.is_ok())
			{
				this->on_command(&this->commander_);
			}
		}

		void game_client::on_error(const int error)
		{
			printf("\ngame_client::on_error:%s", this->id());
			char buffer[16];
			memset(buffer, 0 , sizeof(buffer));
			sprintf(buffer, "BYE %d", error);

			if(this->error_code_ == 0)
			{
				this->error_code_ = error;
			}
			this->simulate_on_command(buffer, false);
		}

		void game_client::on_command(command* command)
		{
			if(this->cmd_dispatcher_ != NULL)
			{
				this->cmd_dispatcher_->action(this, command);
			}
		}

		void game_client::fill(char* id, char* name, char* profile)
		{
			if(this->id_ != NULL)
			{
				delete [] this->id_;
			}

			this->id_ = new char[std::strlen(id) + 1];
			std::memset(this->id_, 0, std::strlen(id) + 1);
			std::strcpy(this->id_, id);

			if(this->name_ != NULL)
			{
				delete [] this->name_;
			}

			this->name_ = new char[std::strlen(name) + 1];
			std::memset(this->name_, 0, std::strlen(name) + 1);
			std::strcpy(this->name_, name);
		}

		//void game_client::simulate_on_command(char* command_data)
		//{

		//}

		void game_client::simulate_on_command(char* command_data, bool is_const_string = true)
		{
			char* command_data_clone = NULL;

			//如果是传入的是一个常量串，那么我们要重新生成一个可写的缓冲区
			if(is_const_string)
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
			if(this->commander_.is_ok())
			{
				this->on_command(&this->commander_);
			}

			if(is_const_string)
			{
				delete [] command_data_clone;
			}
		}

		void game_client::set_command_dispatcher(command_dispatcher* dispatcher)
		{
			this->cmd_dispatcher_ = dispatcher;
		}

		void game_client::disconnect()
		{
			tcp_client::disconnect();
		}

		void game_client::disconnect(int code)
		{
			if(this->available())
			{
				char buffer[16];
				int n = std::sprintf(buffer, "BYE %d", code);
				buffer[n] = 0;
				this->error_code_ = code;
				this->write(false, buffer);

				tcp_client::disconnect();

				this->available_ = false;
			}
		}
	}
}