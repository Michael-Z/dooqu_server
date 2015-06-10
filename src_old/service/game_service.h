#ifndef __GAME_SERVICE_H__
#define __GAME_SERVICE_H__

#include <vector>
#include <set>
#include <boost/thread/mutex.hpp>
#include <cstdarg>
#include <boost/asio.hpp>
#include "../net/tcp_server.h"
#include "../service/command_dispatcher.h"
#include "game_client.h"
#include "game_plugin.h"
#include "char_key_op.h"
#include "timer.h"

using namespace boost::asio;
using namespace dooqu_server::net;

namespace dooqu_server
{
	namespace service
	{
		struct auth_request_data
		{
			game_plugin* plugin;
			game_client* client;
		};

		class game_zone;

		class game_service :  public command_dispatcher, public tcp_server
		{
			typedef std::map<char*, game_plugin*, char_key_op> game_plugin_map;
			typedef std::map<char*, game_zone*, char_key_op> game_zone_map;
			typedef std::set<game_client*> game_client_map;

		protected:

			game_plugin_map plugins_;
			game_zone_map zones_;
			game_client_map clients_;
			boost::mutex clients_mutex_;
			boost::mutex plugins_mutex_;
			boost::mutex zones_mutex_;
			timer update_timer_;
			boost::asio::deadline_timer check_timeout_timer;

			virtual void on_start();
			virtual void on_started();
			virtual void on_stop();
			virtual void on_command(game_client* client, command* command);
			virtual tcp_client* on_create_client();
			virtual void on_client_join(tcp_client* client);
			virtual void on_client_leave(game_client* client, int code);
			virtual void on_check_timeout_clients(const boost::system::error_code &error);
			void begin_auth(game_plugin* plugin, game_client* client, command* cmd);
			void end_auth(const boost::system::error_code&, void*, const std::string&);

			virtual void on_client_login_handle(game_client* client, command* command);
			virtual void on_robot_login_handle(game_client* client, command* command);
			void thread_for_update();
		public:
			game_service(io_service& ios, unsigned int port);
			game_plugin* create_game(char* game_id, char* title, char* zoneid, int frequence, ...);
			game_plugin* create_plugin(game_plugin* game_plugin, char* zone_id);
			io_service& get_io_service(){return this->ios;}
			virtual ~game_service();

		};
	}
}
#endif
