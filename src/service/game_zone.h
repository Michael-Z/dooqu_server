#ifndef __GAME_ZONE_H__
#define __GAME_ZONE_H__

#include <iostream>
#include <cstring>
#include <deque>
#include <set>
#include <vector>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/noncopyable.hpp>
#include "tick_count.h"
#include "game_service.h"


namespace dooqu_server
{
	namespace service
	{
		using namespace boost;
		using namespace boost::asio;

		class game_plugin;

		typedef void (game_plugin::*plugin_callback_handle)(void*, void*, int verifi_code);

#define make_plugin_handle(handle) (plugin_callback_handle)&handle
		//�̳�deadline_timer����������������һ����ʾ��󼤻���ֶΡ�
		class timer : public boost::asio::deadline_timer
		{
		public:
			tick_count last_actived_time;
			timer(io_service& ios) : deadline_timer(ios){}
			virtual ~timer(){ printf("~timer\n"); };
		};


		class game_zone : boost::noncopyable
		{
			friend class game_service;

		private:

			//game_zone.onload.
			void load();

			//game_zone.unonload.
			void unload();

			//queue_task�Ļص�����
			void task_handle(const boost::system::error_code& error, timer* timer_, boost::function<void(void)> callback_handle);

		protected:

			//timer������С���е�������timer_.size > �������󣬻���������timer���
			const static int MIN_ACTIVED_TIMER = 50;

			//��timer���е���������{MIN_ACTIVED_TIMER}����������� ���Ҷ��к󲿵�timer����ʱ�䳬��
			//MAX_TIMER_FREE_TICK��ֵ���ᱻǿ�ƻ���
			const static int MAX_TIMER_FREE_TICK = 1 * 60 * 1000;


			//game_zone ��״̬��
			boost::recursive_mutex status_mutex_;

			//��Ŷ�ʱ����˫�����
			//Խ�ǿ�������ǰ����timerԽ��Ծ��Խ�ǿ���β����timerԽ����
			std::deque<timer*> free_timers_;

			std::set<timer*> working_timers_;

			//timer���гص�״̬��
			boost::recursive_mutex free_timers_mutex_;

			boost::recursive_mutex working_timers_mutex_;

			//game_zone��Ψһid
			char* id_;

			//game_zone�Ƿ�����
			bool is_onlined_;

			//io_service object.
			game_service* game_service_;

			//onload�¼�
			virtual void on_load();

			//onunload�¼�
			virtual void on_unload();

		public:

			static bool LOG_TIMERS_INFO;


			game_zone(game_service* service, const char* id);

			//get game_zone's id
			char* get_id(){ return this->id_; }

			//whether game_zone is online.
			bool is_onlined(){ return this->is_onlined_; }

			//get io_service object.
			//io_service* get_io_service(){ return &this->game_service_->get_io_service(); }

			//queue a plugin's delay method.
			void queue_task(boost::function<void(void)> callback_handle, int sleep_duration);
			virtual ~game_zone();
		};
	}
}

#endif
