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
			//״̬��
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if (this->is_onlined_ == false)
			{
				this->is_onlined_ = true;
				//��ʼ����ص��߳���Դ
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
			//״̬��
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
			//״̬��
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if (this->is_onlined_)
			{
				timer* timer_ = NULL;
				{
					//�����������ȼ��������Ƿ��п��õ�timer
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
			//״̬��
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if (!error && this->is_onlined_)
			{
				{
					//�������timer�����������г�
					boost::recursive_mutex::scoped_lock lock(this->timer_queue_mutex_);

					//���������timer���ظ����гأ����ڳص�ǰ��
					this->timers_.push_front(timer_);
					//������ļ���ʱ��
					timer_->last_actived_time.restart();

					//�Ӻ󷽼��ջ����������Ԫ���Ƿ��ǿ�����ָ��ʱ��
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
				//�������ʧЧ�������Ѿ�unload���ǾͲ��÷����˶ֱ��delete
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
