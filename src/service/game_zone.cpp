#include "game_zone.h"
#include "game_plugin.h"
#include <functional>

namespace dooqu_server
{
	namespace service
	{
		bool game_zone::LOG_TIMERS_INFO = false;

		game_zone::game_zone(game_service* service, const char* id)
		{
			int n = std::strlen(id);
			this->id_ = new char[n + 1];
			std::strcpy(this->id_, id);
			this->id_[n] = '\0';

			this->game_service_ = service;
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

				}

				this->on_load();
			}
		}

		void game_zone::unload()
		{
			{
				boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

				if (this->is_onlined_ == true)
				{
					this->is_onlined_ = false;

					{
						boost::recursive_mutex::scoped_lock lock(this->working_timers_mutex_);

						for (std::set<timer*>::iterator pos_timer = this->working_timers_.begin();
							pos_timer != this->working_timers_.end();
							++pos_timer)
						{
							(*pos_timer)->cancel();
						}
					}

					{
						boost::recursive_mutex::scoped_lock lock(this->free_timers_mutex_);
						for (int i = 0; i < this->free_timers_.size(); ++i)
						{
							this->free_timers_.at(i)->cancel();
						}
					}

					this->on_unload();
				}
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


		//����һ��ָ����ʱ�Ļص���������Ϊtimer����ҪƵ����ʵ���������Բ���deque�Ľṹ��timer������гػ���
		//queue_task���deque��ͷ��������Ч��timer��������󣬴��·Żص�ͷ��������dequeͷ���Ķ���Ϊ��Ծtimer
		//��timer������к󲿵Ķ���ʱ��δ��ʹ�ã�˵����ǰ���󱻿��У����Ի��ա�
		//ע�⣺�����game_zone��ʹ�õ�io_service����cancel������ô�û�����ע���callback_handle�ǲ��ᱻ���õģ���
		void game_zone::queue_task(boost::function<void(void)> callback_handle, int sleep_duration)
		{
			//״̬��
			boost::recursive_mutex::scoped_lock lock(this->status_mutex_);

			if (this->game_service_->is_running() && this->is_onlined_)
			{
				//Ԥ��timer��ָ�룬�����Դ�timer�������ȡ��һ�����е�timer����ʹ��
				//########################################################
				timer* curr_timer_ = NULL;
				{
					//�����������ȼ��������Ƿ��п��õ�timer
					boost::recursive_mutex::scoped_lock lock(this->free_timers_mutex_);

					if (this->free_timers_.size() > 0)
					{
						curr_timer_ = this->free_timers_.front();
						this->free_timers_.pop_front();

						if (game_zone::LOG_TIMERS_INFO)
						{
							printf("{%s} use the existed timer of 1 / %d : %d.\n", this->get_id(), this->free_timers_.size(), this->working_timers_.size());
						}
					}
				}
				//########################################################

				//��������������Ч��timer�����ٽ���ʵ����
				if (curr_timer_ == NULL)
				{
					void* timer_mem = boost::singleton_pool<timer, sizeof(timer)>::malloc();
					curr_timer_ = new(timer_mem)timer(this->game_service_->get_io_service());

					if (game_zone::LOG_TIMERS_INFO)
					{
						printf("##################>new timer");
					}
				}

				{
					boost::recursive_mutex::scoped_lock lock(this->working_timers_mutex_);
					//���ǰ�if(curr_timer == NULL)��һ�δ�����õ���λ�ã���ֹ����Ұtimer
					//�������������lock + destroy֮�������ּ�����
					this->working_timers_.insert(curr_timer_);
				}

				//���ò���
				curr_timer_->expires_from_now(boost::posix_time::milliseconds(sleep_duration));
				curr_timer_->async_wait(boost::bind(&game_zone::task_handle, this,
					boost::asio::placeholders::error, curr_timer_, callback_handle));
			}
		}

		//queue_task�����ûص�����
		//1���жϻص�״̬
		//2������timer��Դ
		//3�������ϲ�ص�
		void game_zone::task_handle(const boost::system::error_code& error, timer* timer_, boost::function<void(void)> callback_handle)
		{
			//�����ǰ��io����������
			if (!error)
			{
				//����״̬����game_zone��״̬���������running����ô���������߼���������timer
				boost::recursive_mutex::scoped_lock lock(this->status_mutex_);
				if (this->game_service_->is_running() && this->is_onlined_ == true)
				{
					//#######�˴����봦�������timer�����������г�##########
					//#################################################
					{
						boost::recursive_mutex::scoped_lock lock(this->working_timers_mutex_);
						this->working_timers_.erase(timer_);
					}

					{
					boost::recursive_mutex::scoped_lock lock(this->free_timers_mutex_);

					//���������timer���ظ����гأ����ڳص�ǰ��
					this->free_timers_.push_front(timer_);

					//������ļ���ʱ��
					timer_->last_actived_time.restart();

					//�Ӻ󷽼��ջ����������Ԫ���Ƿ��ǿ�����ָ��ʱ��
					if (this->free_timers_.size() > MIN_ACTIVED_TIMER
						&& this->free_timers_.back()->last_actived_time.elapsed() > MAX_TIMER_FREE_TICK)
					{
						if (game_zone::LOG_TIMERS_INFO)
						{
							printf("{%s} free timers were destroyed,left=%d.\n", this->get_id(), this->free_timers_.size());
						}
						timer* free_timer = this->free_timers_.back();
						this->free_timers_.pop_back();

						//delete free_timer;
						free_timer->~timer();
						boost::singleton_pool<timer, sizeof(timer)>::free(free_timer);
					}
				}
					//########����timer���###############################
					//###################################################

					//�ص��ϲ��߼�callback
					callback_handle();
					//���ز�ִ�к����߼�
					return;
				}
			}

			//�������ʧЧ�������Ѿ�unload���ǾͲ��÷����˶ֱ��delete
			//delete timer_;
			//timer_->~timer();
			//boost::singleton_pool<timer, sizeof(timer)>::free(timer_);
			printf("timer callback was canceld, timer has deleted.\n");
		}



		///Ϊʲô����ָ������onunload��
		///��Ϊgame_zone��������ֹͣ��ȫ��game_service�йܣ�
		///���һ��game_zone�Ѿ�onload����ô����onunloadһ��������service��onunload�����ã�
		///��service��onunloadһ���ᱻ���ã�so��game_zone~���ù����Լ���onunload״̬
		game_zone::~game_zone()
		{

			{
				boost::recursive_mutex::scoped_lock lock(this->working_timers_mutex_);

				for (std::set<timer*>::iterator pos_timer = this->working_timers_.begin();
					pos_timer != this->working_timers_.end();
					++pos_timer)
				{
					//(*pos_timer)->cancel();
					(*pos_timer)->~timer();
					boost::singleton_pool<timer, sizeof(timer)>::free(*pos_timer);
				}
				this->working_timers_.clear();
			}

			{
				boost::recursive_mutex::scoped_lock lock(this->free_timers_mutex_);
				for (int i = 0; i < this->free_timers_.size(); ++i)
				{
					//this->free_timers_.at(i)->cancel();
					this->free_timers_.at(i)->~timer();
					boost::singleton_pool<timer, sizeof(timer)>::free(this->free_timers_.at(i));
				}
				this->free_timers_.clear();
			}

			delete[] this->id_;
		}
	}
}
