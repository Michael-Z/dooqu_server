#ifndef __GAME_ZONE_H__
#define __GAME_ZONE_H__

#include <iostream>
#include <queue>
#include <vector>
#include <boost\asio.hpp>
#include <boost\thread\recursive_mutex.hpp>
#include <boost\noncopyable.hpp>


namespace dooqu_server
{
	namespace service
	{
		using namespace boost::asio;	

		class game_plugin;

		typedef void (game_plugin::*plugin_callback_handle)(void*, void*, int verifi_code);
		
		#define make_plugin_handle(handle) (plugin_callback_handle)&handle

		class game_zone : boost::noncopyable
		{
		friend class game_service;

		private:
			void load();
			void unload();
			void task_handle(const boost::system::error_code& error, deadline_timer* timer, game_plugin* plugin, plugin_callback_handle handle, void* argument1, void* argument2, int verifi_code);
		protected:
			std::queue<deadline_timer*> timer_queue_;
			boost::recursive_mutex timer_queue_mutex_;
			boost::recursive_mutex status_mutex_;
			char* id_;		
			bool is_onlined_;
			boost::asio::io_service io_service_;
			boost::asio::io_service::work work_;
			virtual void on_load();
			virtual void on_unload();
			
		public:
			game_zone(const char* id);
			char* get_id(){return this->id_;}
			bool is_onlined(){return this->is_onlined_;}
			io_service* get_io_service(){return &this->io_service_;}
			void queue_task(game_plugin* plugin, plugin_callback_handle handle, void* argument1, void* argument2, int verifi_code, int sleep_duration);

			virtual ~game_zone();			
		};
	}
}

#endif
