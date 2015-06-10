#ifndef __GAME_CLIENT_H__
#define __GAME_CLIENT_H__

#include <iostream>
#include <boost\thread\recursive_mutex.hpp>
#include "..\net\tcp_client.h"
#include "game_info.h"
#include "command.h"
#include "command_dispatcher.h"
#include "timer.h"


using namespace dooqu_server::net;
using namespace boost::asio;

namespace dooqu_server
{
	namespace service
	{
		class game_client : public dooqu_server::net::tcp_client
		{
			friend class game_service;
			friend class game_plugin;
		private:
			int error_code_;

		protected:
			game_info* game_info_;
			char* id_;
			char* name_;
			command_dispatcher* cmd_dispatcher_;
			command commander_;
			timer actived_timer;
			boost::recursive_mutex commander_mutex_;

			virtual void on_command(command* command);
			virtual void on_data(char* data);
			virtual void on_error(const int error);

			void fill(char* id, char* name, char* profile);		
		public:
			game_client(io_service& ios);
			virtual ~game_client();
			char* id(){ return this->id_;}
			char* name(){ return this->name_;}
			//void simulate_on_command(char* command_data);
			void simulate_on_command(char* command_data, bool is_const_string);
			void set_command_dispatcher(command_dispatcher* dispather);
			void active(){this->actived_timer.restart();}
			long long get_actived(){return this->actived_timer.elapsed();}
			int error_code(){return this->error_code_;}
			game_info* get_game_info(){ return this->game_info_;}
			void set_game_info(game_info* info){this->game_info_ = info;}
			virtual void disconnect();
			virtual void disconnect(int code);
		};
	}
}

#endif