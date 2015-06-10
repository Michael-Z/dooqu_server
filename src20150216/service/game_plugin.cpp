#include "game_plugin.h"
#include "game_service.h"
#include "service_error.h"
#include <cstring>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace dooqu_server
{
	namespace service
	{
		game_plugin::game_plugin(game_service* game_service, char* gameid, char* title, double frequence) : game_service_(game_service), update_timer_(this->game_service_->get_io_service())
		{
			this->gameid_ = new char[std::strlen(gameid) + 1];
			std::strcpy(this->gameid_, gameid);

			this->title_ = new char[std::strlen(title) + 1];
			std::strcpy(this->title_, title);

			this->frequence_ = frequence;
			this->is_onlined_ = false;
		}

		char* game_plugin::game_id()
		{
			return this->gameid_;
		}

		char* game_plugin::title()
		{
			return this->title_;
		}

		bool game_plugin::is_onlined()
		{
			return this->is_onlined_;
		}


		void game_plugin::on_load()
		{
			printf("game_plugin::on_load()\n");
		}

		void game_plugin::on_unload()
		{
			printf("game_plugin::on_unload()\n");
		}

		void game_plugin::on_update()
		{
		}

		void game_plugin::on_run()
		{
			//printf("game_plugin::on_run's thread id:%d\n", boost::this_thread::get_id());

			if (this->is_onlined() == false)
				return;

			this->on_update();

			if (this->is_onlined())
			{
				this->update_timer_.expires_from_now(boost::posix_time::milliseconds(this->frequence_));
				this->update_timer_.async_wait(boost::bind(&game_plugin::on_run, this));
			}
		}


		int game_plugin::auth_client(game_client* client, const std::string& auth_response_string)
		{
			int ret = this->on_auth_client(client, auth_response_string);

			if (ret == service_error::OK)
			{
				return this->join_client(client);
			}

			return ret;
		}

		//对返回的认证内容进行分析，决定用户是否通过认证，同时在这里要做两件事：
		//1、对用户的信息进行填充
		//2、对game_info的实例进行填充
		int game_plugin::on_auth_client(game_client* client, const  std::string& auth_response_string)
		{
			return service_error::OK;
		}

		int game_plugin::on_befor_client_join(game_client* client)
		{
			if (std::strlen(client->id()) > 20 || std::strlen(client->name()) > 32)
			{
				return service_error::ARGUMENT_ERROR;
			}

			//防止用户重复登录
			game_client_map::iterator curr_client = this->clients_.find(client->id());

			if (curr_client == this->clients_.end())
			{
				return service_error::OK;
			}
			else
			{
				return service_error::CLIENT_HAS_LOGINED;
			}
		}

		void game_plugin::on_client_join(game_client* client)
		{
			printf("{%s} join plugin.\n", client->name());
		}

		void game_plugin::on_client_leave(game_client* client, int code)
		{
			printf("{%s}: leave plugin {%d}.\n", client->name(), code);
		}

		game_zone* game_plugin::on_create_zone(char* zone_id)
		{
			return new game_zone(zone_id, this->game_service_->get_io_service());
		}

		void game_plugin::on_command(game_client* client, command* command)
		{
			bool client_can_active = client->can_active();

			if (this->game_service_->is_running() && client_can_active)
			{
				//boost::recursive_mutex::scoped_lock lock(*this->game_service_->thread_mutexs()->find(boost::this_thread::get_id())->second);
				command_dispatcher::on_command(client, command);
			}

			if (std::strcmp(command->name(), "BYE") == 0)
			{
				this->remove_client(client);
			}
			else
			{
				if (client_can_active == false)
					client->disconnect(service_error::TOO_MANY_DATA);
			}

			{
				using namespace dooqu_server::net;
				thread_status_map::iterator curr_thread_pair = this->game_service_->threads_status()->find(boost::this_thread::get_id());

				if (curr_thread_pair != this->game_service_->threads_status()->end())
				{
					curr_thread_pair->second->restart();
				}
			}
		}

		void game_plugin::load()
		{
			if (this->is_onlined_ == false)
			{
				this->is_onlined_ = true;

				this->on_load();

				//开始按frequence_进行on_run的调用
				this->update_timer_.expires_from_now(boost::posix_time::milliseconds(this->frequence_));
				this->update_timer_.async_wait(boost::bind(&game_plugin::on_run, this));
			}
		}

		void game_plugin::unload()
		{
			if (this->is_onlined_ == true)
			{
				//停止更新on_run(); 最外层其实已经调用了ios_stop();
				this->update_timer_.cancel();

				//此timer忘了做什么用了，先注释
				//this->timer_.restart();
				this->is_onlined_ = false;

				this->on_unload();
			}
		}

		int game_plugin::join_client(game_client* client)
		{
			//对成员组进行上锁
			boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

			//测试当前加入的玩家是否有资格进入成员组
			int ret = this->on_befor_client_join(client);

			//如果无问题
			if (ret == service_error::OK)
			{
				//接管当前玩家的消息
				client->set_command_dispatcher(this);

				//把当前玩家加入成员组
				this->clients_[client->id()] = client;

				//调用玩家进入成员组后的事件
				this->on_client_join(client);
			}

			//返回结果
			return ret;
		}


		//负责处理玩家离开game_plugin后的所有逻辑
		//(1)从用户组中移除
		//(2)触发on_client_leave事件
		//(3)将client的事件回调托管对象设置为game_service
		//(4)向下传递BYE事件到game_service
		void game_plugin::remove_client(game_client* client)
		{
			//printf("game_plugin::remove_client:%s\n", client->id());
			{
				boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

				this->clients_.erase(client->id());
				this->on_client_leave(client, client->error_code_);

/*				string host_url;
				string request_path;
				if (get_offline_update_url(host_url, request_path) == false)
				{
					this->on_client_leave(client, client->error_code_);
				}
				else
				{

				}	*/			
			}

			client->set_command_dispatcher(this->game_service_);

			char buffer[32];
			std::memset(buffer, 0, sizeof(buffer));
			sprintf(buffer, "BYE %d", client->error_code_);

			client->simulate_on_command(buffer, false);
		}

		bool game_plugin::get_offline_update_url(string& server_url, string& request_path)
		{
			return false;
		}


		void game_plugin::broadcast(char* message)
		{
			boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

			game_client_map::iterator curr_client;

			for (curr_client = this->clients_.begin(); curr_client != this->clients_.end(); curr_client++)
			{
				(*curr_client).second->write(true, message);
			}
		}

		game_plugin::~game_plugin()
		{
			printf("~game_plugin\n");

			if (this->gameid_ != NULL)
			{
				delete this->gameid_;
			}

			if (this->title_ != NULL)
			{
				delete this->title_;
			}

			//this->game_service_ = NULL;
		}
	}
}
