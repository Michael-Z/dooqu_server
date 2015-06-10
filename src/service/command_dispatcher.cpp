#include "command_dispatcher.h"
#include <iostream>
#include <cstring>

#include "game_client.h"

namespace dooqu_server
{
	namespace service
	{
		bool command_dispatcher::regist_handle(char* cmd_name, command_handler handler)
		{
			std::map<char*, command_handler, pchar_key_cmp>::iterator handle_pair = this->handles.find(cmd_name);

			if (handle_pair == this->handles.end())
			{
				this->handles[cmd_name] = handler;
				return true;
			}
			else
			{
				return false;
			}
		}


		bool command_dispatcher::action(game_client* client, command* cmd)
		{
			if (client->can_active() == false)
			{
				client->disconnect(service_error::CONSTANT_REQUEST);
				return false;
			}

			if (cmd != NULL && cmd->is_correct())
			{
				client->active();
				this->on_command(client, cmd);
				return true;
			}
			return false;
		}


		void command_dispatcher::on_command(game_client* client, command* command)
		{
			std::map<char*, command_handler, pchar_key_cmp>::iterator handle_pair = this->handles.find(command->name());

			if (handle_pair != this->handles.end())
			{
				command_handler* handle = &handle_pair->second;
				(this->**handle)(client, command);
			}
		}


		void command_dispatcher::unregist(char* cmd_name)
		{
			this->handles.erase(cmd_name);
		}

		void command_dispatcher::remove_all_handles()
		{
			this->handles.clear();
		}
	}
}