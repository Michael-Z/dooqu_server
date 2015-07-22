#include "ask_plugin.h"  

namespace dooqu_server
{
	namespace plugins
	{
		void ask_plugin::on_load()
		{

		}

		void ask_plugin::on_unload()
		{

		}

		void ask_plugin::on_update()
		{

		}

		int ask_plugin::on_auth_client(game_client* client, const std::string&)
		{
			return service_error::OK;
		}

		int ask_plugin::on_befor_client_join(game_client* client)
		{
			return service_error::OK;
		}

		void ask_plugin::on_client_join(game_client* client)
		{

		}

		void ask_plugin::on_client_leave(game_client* client, int code)
		{

		}


		game_zone* ask_plugin::on_create_zone(char* zone_id)
		{
			return new ask_zone(this->game_service_, zone_id);
		}
	}
}
