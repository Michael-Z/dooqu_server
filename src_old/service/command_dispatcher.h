#ifndef __COMMAND_DISPATCHER_H__
#define __COMMAND_DISPATCHER_H__

#include <map>
#include "command.h"
#include <cstring>

namespace dooqu_server
{
	namespace service
	{

		//预先先game_client声明在这， 先不引用game_client.h，容易造成交叉引用
		class game_client;

		class command_dispatcher
		{
			class pchar_key_cmp
			{
			public:
				bool operator()(const char* c1, const char* c2)
				{
					//return std::strcmp(c1, c2) < 0;
					return std::strcmp(c1, c2) < 0;
				}
			};
		public:
			typedef void (command_dispatcher::*command_handler)(game_client* client, command* cmd);

		protected:
			std::map<char*, command_handler, pchar_key_cmp> handles;
			virtual void on_command(game_client* client, command* command);
		public:

			bool regist(char* cmd_name, command_handler handler);
			bool action(game_client* client, command* cmd);
			bool contains(char* cmd_name);
			void unregist(char* cmd_name);
			void clear();
		};

#define make_handler(_handler) (command_dispatcher::command_handler)(&_handler)
	}
}

#endif
