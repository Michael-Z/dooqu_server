#include "game_service.h"
#include "game_client.h"
#include "service_error.h"
#include "game_zone.h"
#include "../ddz/ddz_plugin.h"
#include "http_request.h"
#include <boost/shared_ptr.hpp>


namespace dooqu_server
{
	namespace service
	{

		game_service::game_service(io_service& ios, unsigned int port) : tcp_server(ios, port), check_timeout_timer(ios)
		{
		}

		game_plugin* game_service::create_plugin(game_plugin* game_plugin, char* zone_id)
		{
            if(game_plugin == NULL || game_plugin->game_id() == NULL)
                return NULL;

			game_plugin_map::iterator curr_plugin = this->plugins_.find(game_plugin->game_id());

			if(curr_plugin != this->plugins_.end())
			{
				printf("\ngame_plugin {%d} create error: has existed.", game_plugin->game_id());
				return NULL;
			}

            this->plugins_[game_plugin->game_id()] = game_plugin;


            			//如果传递的zoneid不为空，那么为游戏挂载游戏区
			if(zone_id != NULL)
			{
				game_zone_map::iterator curr_zone = this->zones_.find(zone_id);

				if(curr_zone != this->zones_.end())
				{
					game_plugin->zone_ = (*curr_zone).second;
				}
				else
				{
					game_zone* zone = game_plugin->on_create_zone(zone_id);

					if(this->is_running)
					{
						zone->load();
					}
					this->zones_[zone->get_id()] = zone;
					game_plugin->zone_ = zone;
				}
			}

			if(this->is_running)
			{
				game_plugin->load();
			}

			return game_plugin;
		}

		game_plugin* game_service::create_game(char* game_id, char* title, char* zoneid, int frequence, ...)
		{
			game_plugin_map::iterator curr_plugin = this->plugins_.find(game_id);

			if(curr_plugin != this->plugins_.end())
			{
				printf("\ngame_plugin {%d} create error: has existed.", game_id);
				return NULL;
			}

			char* argc = NULL;
			va_list arg_ptr;
			va_start(arg_ptr, frequence);

			vector<char*> arguments;

			while((argc = va_arg(arg_ptr, char*)) != NULL)
			{
				arguments.push_back(argc);
			}

			va_end(arg_ptr);

			int game_argc = 0;
			char** game_argv = new char*[arguments.size()];
			std::copy(arguments.begin(), arguments.end(), game_argv);

			game_plugin* game = NULL;//new dooqu_server::ddz::ddz_plugin(this, game_id, title, frequence, arguments.size(), game_argv);
			this->plugins_[game->game_id()] = game;

			delete [] game_argv;

			//如果传递的zoneid不为空，那么为游戏挂载游戏区
			if(zoneid != NULL)
			{
				game_zone_map::iterator curr_zone = this->zones_.find(zoneid);

				if(curr_zone != this->zones_.end())
				{
					game->zone_ = (*curr_zone).second;
				}
				else
				{
					game_zone* zone = game->on_create_zone(zoneid);

					if(this->is_running)
					{
						zone->load();
					}
					this->zones_[zone->get_id()] = zone;
					game->zone_ = zone;
				}
			}

			if(this->is_running)
			{
				game->load();
			}

			return game;
		}

		void game_service::thread_for_update()
		{
			//printf("game_service::thread_for_update()\n");
			//update_timer_.restart();

			//while(this->is_running)
			//{
			//	if(this->update_timer_.elapsed() >= 100)
			//	{
			//		this->update_timer_.restart();

			//		for(game_plugin_map::iterator curr_game = this->plugins_.begin();
			//			curr_game != this->plugins_.end(); curr_game++)
			//		{
			//			//(*curr_game).second->on_run();
			//			this->ios.post(boost::bind(&game_plugin::on_run, (*curr_game).second));
			//		}
			//	}
			//	else
			//	{
			//		boost::this_thread::sleep(boost::posix_time::milliseconds(30));
			//	}
			//}
		}

		void game_service::on_start()
		{
			this->regist("LOG", make_handler(game_service::on_client_login_handle));
			this->regist("RLG", make_handler(game_service::on_robot_login_handle));

			//加载所有的所有分区
			for(game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				(*curr_zone).second->load();
			}

			//加载所有的游戏逻辑
			for(game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				(*curr_game).second->load();
			}

			printf("game_service::on_start()\n");
		}

		void game_service::on_started()
		{
			//启动update线程
			//boost::thread t(boost::bind(&game_service::thread_for_update, this));

			//启动检查登录超时用户的定时器
			this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(3));
			this->check_timeout_timer.async_wait(boost::bind(&game_service::on_check_timeout_clients, this, boost::asio::placeholders::error));
		}

		void game_service::on_stop()
		{
			printf("service is stoping.");
			//停止对登录超时的检测
			this->check_timeout_timer.cancel();

			//停止所有的游戏逻辑
			for(game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				(*curr_game).second->unload();
			}

			//停止所有的游戏区
			for(game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				(*curr_zone).second->unload();
			}
		}

		void game_service::on_command(game_client* client, command* command)
		{
			printf("\ngame_service::on_command:%s", command->name());
			command_dispatcher::on_command(client, command);

			if(std::strcmp(command->name(), "BYE") == 0)
			{
				this->on_client_leave(client, client->error_code_);
			}
		}

		tcp_client* game_service::on_create_client()
		{
			game_client* client = new game_client(this->ios);
			return client;
		}

		void game_service::on_client_join(tcp_client* client)
		{
            printf("\nclient connected");
			game_client* g_client = (game_client*)client;
			{
				//对用户组上锁
				boost::mutex::scoped_lock lock(this->clients_mutex_);
				//在登录用户组中注册
				this->clients_.insert(g_client);
			}
			//重置活动时间戳
			g_client->active();

			//设置socket为激活状态
			g_client->available_ = true;

			//设置命令的监听者为当前service
			g_client->set_command_dispatcher(this);

			//开始读取用户的网络消息
			g_client->read_from_client();
		}

		void game_service::on_client_leave(game_client* client, int leave_code)
		{
			printf("\nSTART game_service.on_client_leave.{%d}", client->name(), leave_code);
			//如果这个玩家在进入游戏房间之前就离开了， 那么要把他从临时组里去掉；
			//从登录成员组里被删除有三种情况
			//1、玩家登录了服务器中的某个游戏房间; 在join_client中被从成员组中删除
			//2、玩家登录超时，被服务器断开，并在检查超时的成员方法中被从成员组中删除；
			//3、玩家网络问题、或自己发送BYE离开，那就只能在这删除了
			{
				boost::mutex::scoped_lock lock(this->clients_mutex_);

				game_client_map::iterator curr_client = this->clients_.find(client);

				if(curr_client != this->clients_.end())
				{
					this->clients_.erase(client);
				}
			}

			client->set_command_dispatcher(NULL);

			if(client->available() && leave_code != service_error::CLIENT_NET_ERROR)
			{
				if(leave_code != service_error::CLIENT_EXIT)
				{
					client->write(false, "BYE", client->commander_.params(0), NULL);
				}
				else
				{
					client->write(false, "BYE\0");
				}
			}

			if(client->available() && leave_code == service_error::CLIENT_EXIT)
			{
				//让tcp_client.receive_handle中，处理当前这个BYE的on_data之后，知道当前用户已经不可用，开始调用delete this.
				client->disconnect();
				//client->disconnect(service_error::CLIENT_EXIT);
			}

			printf("\nEND game_service.on_client_leave.{%d}", client->name());
		}


		//使用了定时器
		void game_service::on_check_timeout_clients(const boost::system::error_code &error)
		{
			if(!error)
			{
				//用来装超时用户的临时数组
				std::vector<game_client*> timeout_clients;

				//对登录用户上锁
				boost::mutex::scoped_lock lock(this->clients_mutex_);

				//对所有用户的active时间进行对比，超过20秒还没有登录动作的用户被装进临时数组
				for(game_client_map::iterator curr_client = this->clients_.begin();
					curr_client != this->clients_.end(); curr_client++)
				{
					game_client* client = (*curr_client);

					if(client->get_actived() > 20 * 1000)
					{
						timeout_clients.push_back(client);
					}
				}

				//对临时数组中的玩家进行超时断开操作。
				for(std::vector<game_client*>::iterator curr_timeout_client = timeout_clients.begin();
					curr_timeout_client != timeout_clients.end(); curr_timeout_client++)
				{
					this->clients_.erase((*curr_timeout_client));
					(*curr_timeout_client)->disconnect(service_error::TIME_OUT);
				}

				//清空临时数组
				timeout_clients.clear();

				//如果服务没有停止， 那么继续下一次计时
				if(this->is_running)
				{
					this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(5));
					this->check_timeout_timer.async_wait(boost::bind(&game_service::on_check_timeout_clients, this, boost::asio::placeholders::error));
				}
			}
		}

		void game_service::begin_auth(game_plugin* plugin, game_client* client, command* cmd)
		{
			if(plugin->is_onlined() && client->available())
			{
				auth_request_data* request_data = new auth_request_data();
				request_data->client = client;
				request_data->plugin = plugin;

				//char buffer[] = {0};
				//string s(buffer);
				//this->end_auth( boost::asio::error::eof, request_data, s);
				//void* p = request_data;

				http_request<game_service>* request = new http_request<game_service>(this->ios,
					"127.0.0.1",
					"/auth.xml",
					//"/auth.aspx?id",
					this,
					&game_service::end_auth,
					request_data);
			}
			else
			{
				printf("invalidate");
			}
		}

		void game_service::end_auth(const boost::system::error_code& code, void* user_data, const std::string& response_string)
		{
           // printf("end_auth");

			std::auto_ptr<auth_request_data> user_data_ptr((auth_request_data*)user_data);

			if(this->is_running == false)
				return;

			auth_request_data* request_data = (auth_request_data*)user_data;

			if(request_data->plugin->is_onlined() == false)
				return;

			int ret = service_error::OK;

			{
				//对登录的用户列表进行上锁
				boost::mutex::scoped_lock lock(this->clients_mutex_);
				game_client_map::iterator finder = this->clients_.find(request_data->client);

				//client 在另外一个线程已经被销毁，超时了，或者主动离开等等情况。
				//本质上就是依靠this->clients_来判断当前的client对象是否已经被销毁了。
				if(finder == this->clients_.end())
					return;

				//用户登录动作完成，从排队用户中删除。
				this->clients_.erase(request_data->client);

				if(request_data->client->available() == false)
					return;
			}


			//http ok
			if(code == boost::asio::error::eof)
			{
				ret = request_data->plugin->auto_client(request_data->client, response_string);
				//std::cout<<response_string;
			}
			else
			{
				ret = service_error::LOGIN_SERVICE_ERROR;
			}

			if(ret != service_error::OK)
			{
               // printf("\nauthenticate error.");
				//接通消息监听，因为这里要有"BYE"命令发过来
				//request_data->client->set_command_dispatcher(this);
				request_data->client->disconnect(ret);
			}
		}

		void game_service::on_client_login_handle(game_client* client, command* command)
		{

			if(command->param_size() != 2)
			{
                printf("login error");

				client->write("LOG -1");
				client->disconnect();
				return;
			}

			if(client->available() == false)
				return;

			//更新激活时间，防止被另外一个线程的更新销毁掉。
			client->active();

			int result_code = service_error::OK;

			//查找是否存在对应玩家想加入的的game_plugin
			std::map<char*, game_plugin*, char_key_op>::iterator it = this->plugins_.find(command->params(0));
			if(it != this->plugins_.end())
			{
				//没法用plugin.begin,因为回调函数回来之后，plugin可能已经被销毁了。
				//所以要统一回调到game_service的方法上，然后首先对is_running进行检测。
				//client->fill(command->params(1), command->params(1), NULL);
				//game_plugin* plugin = (*it).second;
				//result_code = plugin->join_client(client);
				client->fill(command->params(1), command->params(1), NULL);
				this->begin_auth((*it).second, client, command);
			}
			else
			{
                printf("game not existed.");
				client->disconnect(service_error::GAME_NOT_EXISTED);
			}
		}

		void game_service::on_robot_login_handle(game_client* client, command* command)
		{
		}

		game_service::~game_service()
		{
			printf("\n~game_service");

			//销毁所有的玩家资料
			for(game_client_map::iterator curr_client = this->clients_.begin();
				curr_client != this->clients_.end(); curr_client++)
			{
				delete (*curr_client);
			}
			this->clients_.clear();

			//停止所有的游戏插件
			for(game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				delete (*curr_game).second;
			}
			this->plugins_.clear();


			//停止所有的游戏区
			for(game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				delete (*curr_zone).second;
			}

			this->zones_.clear();
		}
	}
}
