#ifndef __GAME_PLUGIN_H__
#define __GAME_PLUGIN_H__

#include <iostream>
#include <map>
#include <boost\thread\recursive_mutex.hpp>
#include <boost\asio\deadline_timer.hpp>
#include <boost\noncopyable.hpp>
#include "command_dispatcher.h"
#include "game_client.h"
#include "char_key_op.h"
#include "game_zone.h"
#include "timer.h"

namespace dooqu_server
{
	namespace service
	{
		class game_service;
		
		class game_plugin : public command_dispatcher, boost::noncopyable
		{			
			typedef std::map<char*, game_client*, char_key_op> game_client_map;

			friend class game_service;
			friend class game_zone;

		private:
			void load();
			void unload();
			int auto_client(game_client* client, std::string&);

		protected:
			char* gameid_;
			char* title_;
			double frequence_;			
			double last_run_;
			bool is_onlined_;
			timer timer_;
			game_service* game_service_;
			game_zone* zone_;
			game_client_map clients_;
			boost::asio::deadline_timer update_timer_;

			virtual void on_load();
			virtual void on_unload();
			virtual void on_update();
			virtual void on_run();
			virtual int on_auth_client(game_client* client, std::string&);
			virtual int on_befor_client_join(game_client* client);
			virtual void on_client_join(game_client* client);
			virtual void on_client_leave(game_client* client, int code);
			virtual void on_command(game_client* client, command* command);
			virtual void broadcast(char* message);
			virtual game_zone* on_create_zone(char* zone_id);
			
		public:
			boost::recursive_mutex clients_lock_;
			game_plugin(game_service* game_service, char* gameid, char* title, double frequence = 2000);
			char* game_id();
			char* title();
			bool is_onlined();
			const game_client_map* clients(){ return &this->clients_;};
			int join_client(game_client* client);
			void remove_client(game_client* client);
			virtual ~game_plugin();
			void test(void* argument){printf("%s", argument);}
		};
	}
}

#endif