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
			if(this->contains(cmd_name) == false)
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
			if (cmd != NULL && cmd->is_correct())
			{
				this->on_command(client, cmd);
				return true;
			}
			return false;
		}

		void command_dispatcher::on_command(game_client* client, command* command)
		{
			if(this->contains(command->name()))
			{
				command_handler handle = this->handles[command->name()];

				(this->*handle)(client, command);
			}
		}

		bool command_dispatcher::contains(char* cmd_name)
		{
			std::map<char*, command_handler, pchar_key_cmp>::iterator e;

			e = this->handles.find(cmd_name);

			return e != this->handles.end();
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