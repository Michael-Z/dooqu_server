#ifndef __COMMAND_DISPATCHER_H__
#define __COMMAND_DISPATCHER_H__

#include <map>
#include <cstring>
#include "command.h"
#include "service_error.h"


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
					return std::strcmp(c1, c2) < 0;
				}
			};
		public:
			typedef void (command_dispatcher::*command_handler)(game_client* client, command* cmd);

		protected:
			std::map<char*, command_handler, pchar_key_cmp> handles;

			virtual void on_client_data_received(game_client* client, size_t bytes_received) ;
			virtual void on_client_data(game_client*, char* data) ;
			virtual void on_client_command(game_client*, command* command);
			//
		public:
			virtual void simulate_client_data(game_client*, char* data);
			bool regist_handle(char* cmd_name, command_handler handler);
			void action(game_client* client, size_t bytes_received);
			void unregist(char* cmd_name);
			void remove_all_handles();
			virtual void dispatch_bye(game_client*) = 0;
			virtual ~command_dispatcher(){};
		};

#define make_handler(_handler) (command_dispatcher::command_handler)(&_handler)
	}
}

#endif
