#include "game_zone.h"
#include "game_plugin.h"
#include <cstring>
#include <boost\thread.hpp>
#include <boost\asio.hpp>


namespace dooqu_server
{
	namespace service
	{
		game_zone::game_zone(const char* id) : work_(io_service_)
		{
			int n = std::strlen(id);
			this->id_ = new char[n + 1];
			std::strcpy(this->id_, id);
			this->id_[n] = '\0';

			this->is_onlined_ = false;
		}

		void game_zone::load()
		{
			//状态锁
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if(this->is_onlined_ == false)
			{
				this->is_onlined_ = true;
				//初始化相关的线程资源
				const int WORKER_THREAD_NUM = 1;
				for(int i = 0; i < WORKER_THREAD_NUM; i++)
				{
					boost::thread t(boost::bind(&io_service::run, &this->io_service_));
				}

				this->on_load();
			}
		}

		void game_zone::unload()
		{
			//状态锁
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if(this->is_onlined_ == true)
			{
				this->is_onlined_ = false;
				this->io_service_.stop();

				this->on_unload();
			}
		}

		void game_zone::on_load()
		{
			printf("\ngame_zone:{%s} loaded.", this->get_id());
		}

		void game_zone::on_unload()
		{
			printf("\ngame_zone:{%s} unloaded.", this->get_id());
		}


		void game_zone::queue_task(game_plugin* plugin, plugin_callback_handle handle, void* argument1, void* argument2, int verifi_code, int sleep_duration)
		{
			//状态锁
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if(this->is_onlined_)
			{
				boost::asio::deadline_timer* timer = NULL;
				{
					//队列上锁，先检查队列中是否有可用的timer
					boost::recursive_mutex::scoped_lock lock(this->timer_queue_mutex_);
					if(this->timer_queue_.size() > 0)
					{
						timer = this->timer_queue_.front();
						this->timer_queue_.pop();

						//printf("\n队列中出队已有的timer");
					}
				}

				if(timer == NULL)
				{
					//printf("\n实例化新timer");
					timer = new boost::asio::deadline_timer(this->io_service_);
				}

				timer->expires_from_now(boost::posix_time::milliseconds(sleep_duration));
				timer->async_wait(boost::bind(&game_zone::task_handle, this, 
					boost::asio::placeholders::error, timer, plugin, handle, argument1, argument2, verifi_code));
			}
		}

		void game_zone::task_handle(const boost::system::error_code& error, deadline_timer* timer, game_plugin* plugin, plugin_callback_handle handle, void* argument1, void* argument2, int verifi_code)
		{
			//状态锁
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if(!error && this->is_onlined_)
			{
				{
					//将用完的timer，返还给队列
					boost::recursive_mutex::scoped_lock lock(this->timer_queue_mutex_);
					this->timer_queue_.push(timer);
				}
				if(plugin->is_onlined())
				{
					(plugin->*handle)(argument1, argument2, verifi_code);
				}
			}
			else
			{
				//如果调用失效，或者已经unload，那就不用返还了额，直接delete
				delete timer;
			}
		}


		game_zone::~game_zone()
		{
			if(this->id_ != NULL)
			{
				delete [] this->id_;
			}

			int i= 0;
			while(this->timer_queue_.size())
			{
				delete this->timer_queue_.front();
				this->timer_queue_.pop();
			}



			printf("\n~game_zone:%d", i);
		}
	}
}