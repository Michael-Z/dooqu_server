#ifndef __ASK_PLUGIN__
#define __ASK_PLUGIN__

#include "../service/game_plugin.h"
#include "ask_zone.h"

namespace dooqu_server
{
	namespace plugins
	{
		using namespace dooqu_server::service;

		class ask_plugin : public game_plugin
		{
		protected:
			void on_load();

			void on_unload();

			void on_update(); 

			int on_auth_client(game_client* client, const std::string&);

			int on_befor_client_join(game_client* client);

			void on_client_join(game_client* client);

			void on_client_leave(game_client* client, int code);

			game_zone* on_create_zone(char* zone_id);

		public:
			ask_plugin(game_service* service, char* game_id, char* title) : game_plugin(service, game_id, title)
			{

			}
		};
	}
}

#endif