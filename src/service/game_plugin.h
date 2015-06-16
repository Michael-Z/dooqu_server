#ifndef __GAME_PLUGIN_H__
#define __GAME_PLUGIN_H__

#include <iostream>
#include <map>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/noncopyable.hpp>
#include "command_dispatcher.h"
#include "game_client.h"
#include "char_key_op.h"
#include "game_zone.h"
#include "http_request.h"

namespace dooqu_server
{
	namespace service
	{
		class game_service;

		//集成继承抽象类game_plugin，可以创建一个游戏逻辑插件；
		//通过重写game_plugin的事件函数，可实现控制游戏的基本流程；
		//可重写的事件包括:
		//on_load,游戏加载时候调用
		//on_unload，游戏卸载的时候调用
		//on_update，游戏更新的时候调用
		//on_auth_client，在通过以http形式访问鉴权地址后的回调事件函数，http的返回结果会作为参数传递过来；
		//on_befor_client_join，在一个client加入游戏插件之前调用，通过重写该函数可由返回值决定该玩家是否可以加入游戏；
		//on_client_join， 在玩家加入游戏后调用，可在此处理一些初始化的逻辑；
		//on_client_leave，在玩家离开游戏后调用，可在此处理一些清理的逻辑
		//need_update_offline_client，通过重写该时间，传递返回值true，来决定是否在用户离开后，http形式更新用户数据
		class game_plugin : public command_dispatcher, boost::noncopyable
		{
			typedef std::map<char*, game_client*, char_key_op> game_client_map;

			friend class game_service;
			friend class game_zone;

		private:
			void load();
			void unload();
			int auth_client(game_client* client, const std::string&);

		protected:
			char* gameid_;
			char* title_;
			double frequence_;
			double last_run_;
			bool is_onlined_;

			game_service* game_service_;
			game_zone* zone_;
			game_client_map clients_;
			boost::asio::deadline_timer update_timer_;

			virtual void on_load();

			virtual void on_unload();

			virtual void on_update();

			virtual void on_run();

			virtual int on_auth_client(game_client* client, const std::string&);

			virtual int on_befor_client_join(game_client* client);

			virtual void on_client_join(game_client* client);

			virtual void on_client_leave(game_client* client, int code);

			inline virtual void on_client_command(game_client* client, command* command);

			virtual game_zone* on_create_zone(char* zone_id);

			virtual void on_update_timeout_clients();
			
			//如果get_offline_update_url返回true，请重写on_update_offline_client，并根据error_code的值来确定update操作的返回值。
			virtual bool need_update_offline_client(game_client* client, string& server_url, string& request_path);
			
			//如果当game_client离开game_plugin需要调用http协议的外部地址来更新更新game_client的状态；
			//那么请在此函数返回true，并且正确的赋值server_url和request_path的值；
			virtual void on_update_offline_client(const boost::system::error_code& err, const int status_code, game_client* client);

			void dispatch_bye(game_client* client);
			void broadcast(char* message, bool asynchronized = true);
			void begin_update_client(game_client* client, string& server_url, string& request_path);
			void end_update_client(const boost::system::error_code& err, const int status_code, const string& result, game_client* client, http_request* request);
			void remove_client_from_plugin(game_client* client);

		public:
			boost::recursive_mutex clients_lock_;
			game_plugin(game_service* game_service, char* gameid, char* title, double frequence = 2000);
			char* game_id();
			char* title();
			bool is_onlined();
			const game_client_map* clients(){ return &this->clients_; };
			int join_client(game_client* client);
			void remove_client(game_client* client);
			virtual ~game_plugin();
		};
	}
}

#endif
