#ifndef __GAME_ZONE_H__
#define __GAME_ZONE_H__

#include <iostream>
#include <cstring>
#include <deque>
#include <vector>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/noncopyable.hpp>
#include "tick_count.h"
#include "game_plugin.h"

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
			//继承deadline_timer，并按照需求，增加一个标示最后激活的字段。
			class timer : public boost::asio::deadline_timer
			{
			public:
				tick_count last_actived_time;
				timer(io_service& ios) : deadline_timer(ios){}
			};

		friend class game_service;

		private:

			//game_zone.onload.
			void load();

			//game_zone.unonload.
			void unload();

			//queue_task的回调函数
			void task_handle(const boost::system::error_code& error, timer* timer_, game_plugin* plugin, plugin_callback_handle handle, void* argument1, void* argument2, int verifi_code);
		protected:

			//timer池中最小保有的数量，timer_.size > 此数量后，会启动空闲timer检查
			const static int MIN_ACTIVED_TIMER = 5;

			//如timer池中的数量超过{MIN_ACTIVED_TIMER}定义的数量， 并且队列后部的timer空闲时间超过
			//MAX_TIMER_FREE_TICK的值，会被强制回收
			const static int MAX_TIMER_FREE_TICK = 5 * 60 * 1000;

			//存放定时器的双向队列
			//越是靠近队列前部的timer越活跃，越是靠近尾部的timer越空闲
			std::deque<timer*> timers_;

			//timer队列池的状态锁
			boost::recursive_mutex timer_queue_mutex_;

			//game_zone 的状态锁
			boost::recursive_mutex status_mutex_;

			//game_zone的唯一id
			char* id_;

			//game_zone是否在线
			bool is_onlined_;

			//io_service object.
			boost::asio::io_service io_service_;

			//work object.
			boost::asio::io_service::work& work_;

			//onload事件
			virtual void on_load();

			//onunload事件
			virtual void on_unload();

		public:
			game_zone(const char* id);

			//get game_zone's id
			char* get_id(){return this->id_;}

			//whether game_zone is online.
			bool is_onlined(){return this->is_onlined_;}

			//get io_service object.
			io_service* get_io_service(){return &this->io_service_;}

			//queue a plugin's delay method.
			void queue_task(game_plugin* plugin, plugin_callback_handle handle, void* argument1, void* argument2, int verifi_code, int sleep_duration);


			virtual ~game_zone();
		};
	}
}

#endif
