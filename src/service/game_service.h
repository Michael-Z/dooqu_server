#ifndef __GAME_SERVICE_H__
#define __GAME_SERVICE_H__


#include <set>
#include <queue>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/asio.hpp>
#include <boost/pool/singleton_pool.hpp>
#include "../net/tcp_server.h"
#include "../service/command_dispatcher.h"
#include "game_client.h"
#include "game_plugin.h"
#include "char_key_op.h"
#include "service_error.h"
#include "http_request.h"

using namespace boost::asio;
using namespace dooqu_server::net;

namespace dooqu_server
{
	namespace service
	{
		class game_zone;

		class game_service : public command_dispatcher, public tcp_server
		{
			typedef std::map<char*, game_plugin*, char_key_op> game_plugin_map;
			typedef std::map<char*, game_zone*, char_key_op> game_zone_map;
			typedef std::set<game_client*> game_client_map;

		protected:
			enum{ MAX_AUTH_SESSION_COUNT = 200 };
			game_plugin_map plugins_;
			game_zone_map zones_;
			game_client_map clients_;
			boost::mutex clients_mutex_;
			boost::mutex plugins_mutex_;
			boost::mutex zones_mutex_;
			boost::asio::deadline_timer check_timeout_timer;

			std::set<http_request*> http_request_working_;
			boost::recursive_mutex http_request_mutex_;

			virtual void on_init();
			virtual void on_start();
			virtual void on_stop();
			inline virtual void on_command(game_client* client, command* command);
			virtual tcp_client* on_create_client();
			virtual void on_client_join(tcp_client* client);
			virtual void on_client_leave(game_client* client, int code);
			virtual void on_destroy_client(tcp_client*);
			virtual void on_check_timeout_clients(const boost::system::error_code &error);

			//非虚方法
			void begin_auth(game_plugin* plugin, game_client* client, command* cmd);
			void end_auth(const boost::system::error_code& code, const int, const std::string& response_string, http_request* request, game_plugin* plugin, game_client* client);
			void dispatch_bye(game_client* client);

			//应用层注册
			void on_client_login_handle(game_client* client, command* command);
			void on_robot_login_handle(game_client* client, command* command);

		public:

			game_service(unsigned int port);

			game_plugin* create_plugin(game_plugin* game_plugin, char* zone_id);
			game_plugin_map* get_plugins(){ return &this->plugins_; }

			http_request* on_create_http_request();
			void on_destroy_http_request(http_request*);

			virtual ~game_service();

		};
	}
}
#endif
