#include "game_zone.h"

namespace dooqu_server
{
	namespace service
	{
		game_zone::game_zone(const char* id) : work_(boost::asio::io_service::work(io_service_))
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

			if (this->is_onlined_ == false)
			{
				this->is_onlined_ = true;
				//初始化相关的线程资源
				const int WORKER_THREAD_NUM = 1;
				for (int i = 0; i < WORKER_THREAD_NUM; i++)
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

			if (this->is_onlined_ == true)
			{
				this->is_onlined_ = false;
				this->io_service_.stop();

				this->on_unload();
			}
		}

		void game_zone::on_load()
		{
			printf("game_zone:{%s} loaded.\n", this->get_id());
		}

		void game_zone::on_unload()
		{
			printf("game_zone:{%s} unloaded.\n", this->get_id());
		}


		void game_zone::queue_task(game_plugin* plugin, plugin_callback_handle handle, void* argument1, void* argument2, int verifi_code, int sleep_duration)
		{
			//状态锁
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if (this->is_onlined_)
			{
				timer* timer_ = NULL;
				{
					//队列上锁，先检查队列中是否有可用的timer
					boost::recursive_mutex::scoped_lock lock(this->timer_queue_mutex_);
					if (this->timers_.size() > 0)
					{
						timer_ = this->timers_.front();
						this->timers_.pop_front();

						printf("use the existed timer of 1 / %d.\n", this->timers_.size());
					}
				}

				if (timer_ == NULL)
				{
					timer_ = new timer(this->io_service_);
				}

				timer_->expires_from_now(boost::posix_time::milliseconds(sleep_duration));
				timer_->async_wait(boost::bind(&game_zone::task_handle, this,
					boost::asio::placeholders::error, timer_, plugin, handle, argument1, argument2, verifi_code));
			}
		}

		void game_zone::task_handle(const boost::system::error_code& error, timer* timer_, game_plugin* plugin, plugin_callback_handle handle, void* argument1, void* argument2, int verifi_code)
		{
			//状态锁
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if (!error && this->is_onlined_)
			{
				{
					//将用完的timer，返还给队列池
					boost::recursive_mutex::scoped_lock lock(this->timer_queue_mutex_);

					//将用用完的timer返回给队列池，放在池的前部
					this->timers_.push_front(timer_);
					//标记最后的激活时间
					timer_->last_actived_time.restart();

					//从后方检查栈队列中最后的元素是否是空闲了指定时间
					if (this->timers_.size() > MIN_ACTIVED_TIMER 
						&& this->timers_.back()->last_actived_time.elapsed() > MAX_TIMER_FREE_TICK)
					{
						printf("free timers were destroyed,left=%d.\n", this->timers_.size());
						timer* free_timer = this->timers_.back();
						this->timers_.pop_back();
						delete free_timer;
					}
				}

				if (plugin->is_onlined())
				{
					(plugin->*handle)(argument1, argument2, verifi_code);
				}
			}
			else
			{
				//如果调用失效，或者已经unload，那就不用返还了额，直接delete
				delete timer_;
			}
		}


		game_zone::~game_zone()
		{
			printf("destroy game_zone:{%s}\n", this->get_id());

			if (this->id_ != NULL)
			{
				delete[] this->id_;
			}

			{
				boost::recursive_mutex::scoped_lock lock(this->timer_queue_mutex_);
				for (int i = 0; i < this->timers_.size(); i++)
				{
					delete this->timers_[i];
				}
			}
		}
	}
}
