#ifndef __PLANE_PLUGIN_H__
#define __PLANE_PLUGIN_H__

#include <assert.h>
#include "../service/game_plugin.h"
#include "plane_desk.h"
#include "plane_game_info.h"
#include "service_error.h"

using namespace dooqu_server::service;

namespace dooqu_server
{
	namespace plane
	{
		class plane_plugin : public game_plugin
		{
		protected:
			int desk_count_;
			std::vector<plane_desk*> desks_;
			int max_waiting_duration_;
			virtual void on_load();
			virtual void on_unload();
			virtual void on_update();
			virtual int on_befor_client_join(game_client* client);
			virtual void on_client_join(game_client* client);
			virtual void on_client_leave(game_client* client, int code);
			virtual void on_client_join_desk(game_client* client, plane_desk* desk, int pos_index);
			virtual void on_client_leave_desk(game_client* client, plane_desk* desk, int reaspon);
			virtual void on_game_ready(plane_desk*, int);
			virtual void on_game_start(plane_desk*, int);
			virtual void on_game_attack_result(plane_desk*);
			virtual void on_game_stop(plane_desk* desk, game_client* client, bool normal);
			
			virtual void on_client_join_desk_handle(game_client* client, command* cmd);
			virtual void on_client_ready_handle(game_client* client, command* cmd);
			virtual void on_client_attack_handle(game_client* client, command* cmd);
			virtual void on_client_bye_handle(game_client* client, command* cmd);

			bool get_offline_update_url(game_client* client, string& server_url, string& request_path);

		public:
			plane_plugin(game_service* service, char* game_id, char* game_title) : game_plugin(service, game_id, game_title, 2000)
			{

			}
		};
	}
}
#endif