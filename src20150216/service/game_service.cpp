#include "game_service.h"
#include "game_zone.h"

namespace dooqu_server
{
	namespace service
	{
		game_service::game_service(unsigned int port) : tcp_server(port), check_timeout_timer(ios)
		{

		}


		game_plugin* game_service::create_plugin(game_plugin* game_plugin, char* zone_id)
		{
			if (game_plugin == NULL || game_plugin->game_id() == NULL)
				return NULL;

			game_plugin_map::iterator curr_plugin = this->plugins_.find(game_plugin->game_id());

			if (curr_plugin != this->plugins_.end())
			{
				printf("game_plugin {%d} create error: has existed.\n", game_plugin->game_id());
				return NULL;
			}

			this->plugins_[game_plugin->game_id()] = game_plugin;


			//如果传递的zoneid不为空，那么为游戏挂载游戏区
			if (zone_id != NULL)
			{
				game_zone_map::iterator curr_zone = this->zones_.find(zone_id);

				if (curr_zone != this->zones_.end())
				{
					game_plugin->zone_ = (*curr_zone).second;
				}
				else
				{
					game_zone* zone = game_plugin->on_create_zone(zone_id);

					if (this->is_running_)
					{
						zone->load();

						boost::thread t(boost::bind(&io_service::run, &this->ios));
						this->threads_status_[t.get_id()] = new tick_count_t();
						printf("create zone thread:%d\n", t.get_id());
						t.detach();
					}
					this->zones_[zone->get_id()] = zone;
					game_plugin->zone_ = zone;
				}
			}

			if (this->is_running_)
			{
				game_plugin->load();
			}

			return game_plugin;
		}


		void game_service::on_start()
		{
			this->regist_handle("LOG", make_handler(game_service::on_client_login_handle));
			this->regist_handle("RLG", make_handler(game_service::on_robot_login_handle));

			//加载所有的所有分区
			for (game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				(*curr_zone).second->load();

				boost::thread t(boost::bind(&io_service::run, &this->ios));
				this->threads_status_[t.get_id()] = new tick_count_t();
				printf("create zone thread:%d\n", t.get_id());
				t.detach();
			}

			//加载所有的游戏逻辑
			for (game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				(*curr_game).second->load();
			}

			printf("game_service::on_start()\n");
		}


		void game_service::on_started()
		{
			//启动检查登录超时用户的定时器
			this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(3));
			this->check_timeout_timer.async_wait(boost::bind(&game_service::on_check_timeout_clients, this, boost::asio::placeholders::error));
		}


		void game_service::on_stop()
		{
			printf("service is stoping...\n");

			//停止对登录超时的检测
			this->check_timeout_timer.cancel();

			//清理临时登录用户的内存资源
			{
				boost::mutex::scoped_lock lock(this->clients_mutex_);

				for (game_client_map::iterator curr_client = this->clients_.begin();
					curr_client != this->clients_.end(); curr_client++)
				{
					game_client* client = (*curr_client);
					//client->disconnect(service_error::SERVER_CLOSEING);
					//continue;
					client->set_command_dispatcher(NULL);
					this->on_destroy_client(client);
				}

				this->clients_.clear();
			}

			//停止所有的游戏逻辑
			for (game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				(*curr_game).second->unload();
			}

			//停止所有的游戏区
			for (game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				(*curr_zone).second->unload();
			}

			//清理未返回的http_request对象
			boost::recursive_mutex::scoped_lock lock(this->http_request_mutex_);
			for (std::set<http_request*>::iterator pos_http_request = this->http_request_working_.begin();
				pos_http_request != this->http_request_working_.end(); ++pos_http_request)
			{
				this->on_destroy_http_request(*pos_http_request);
			}
			this->http_request_working_.clear();
		}


		void game_service::on_command(game_client* client, command* command)
		{
			bool client_can_actived = client->can_active();

			if (this->is_running_ && client_can_actived)
			{
				//boost::recursive_mutex::scoped_lock lock(*this->thread_mutexs()->find(boost::this_thread::get_id())->second);
				command_dispatcher::on_command(client, command);
			}

			if (std::strcmp(command->name(), "BYE") == 0)
			{
				this->on_client_leave(client, client->error_code_);
			}
			else
			{
				if (client_can_actived == false)
					client->disconnect(service_error::TOO_MANY_DATA);
			}
		}


		tcp_client* game_service::on_create_client()
		{
			void* client_mem = boost::singleton_pool<game_client, sizeof(game_client)>::malloc();

			return new(client_mem)game_client(this->ios);
		}



		void game_service::on_client_join(tcp_client* client)
		{
			//printf("client connected.\n");
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
			client->set_command_dispatcher(NULL);

			//printf("START game_service.on_client_leave:{%d}\n", client->name(), leave_code);
			//如果这个玩家在进入游戏房间之前就离开了， 那么要把他从临时组里去掉；
			//从登录成员组里被删除有三种情况
			//1、玩家登录了服务器中的某个游戏房间; 在join_client中被从成员组中删除
			//2、玩家登录超时，被服务器断开，并在检查超时的成员方法中被从成员组中删除；
			//3、玩家网络问题、或自己发送BYE离开，那就只能在这删除了
			{
				boost::mutex::scoped_lock lock(this->clients_mutex_);

				game_client_map::iterator curr_client = this->clients_.find(client);
				if (curr_client != this->clients_.end())
				{
					this->clients_.erase(client);
				}
			}


			if (client->available() && leave_code != service_error::CLIENT_NET_ERROR)
			{
				char buffer[16] = { 0 };

				if (leave_code != service_error::CLIENT_EXIT)
				{
					sprintf(buffer, "BYE %d", client->commander_.params(0));
				}
				else
				{
					sprintf(buffer, "BYE");
				}

				client->write(false, buffer);
			}

			printf("{%s} game_service.on_client_leave.{%d}\n", client->id(), leave_code);
			//将销毁client的操作，post到另外一个线程， 给当前线程中的receive_handle一个可利用"this"上下文执行的环境，让出执行下文

			if (this->ios.stopped())
			{
				this->on_destroy_client(client);
			}
			else
			{
				this->ios.post(boost::bind(&game_service::on_destroy_client, this, client));
			}
		}



		void game_service::on_destroy_client(tcp_client* t_client)
		{
			game_client* client = (game_client*)t_client;

			{
				//上锁，放置和tcp_client::receive_handle进行冲突
				boost::mutex::scoped_lock lock(client->status_lock_);
				client->~game_client();
				boost::singleton_pool<game_client, sizeof(game_client)>::free(client);
			}
		}



		//使用了定时器
		void game_service::on_check_timeout_clients(const boost::system::error_code &error)
		{
			printf("game_service::on_run's thread id:%d\n", boost::this_thread::get_id());

			//thread_mutex_map::iterator curr_mutex = this->thread_mutexs()->find(boost::this_thread::get_id());
			//boost::recursive_mutex::scoped_lock lock(*curr_mutex->second);
			if (!error && this->is_running_)
			{
				//如果禁用超时检测，请注释return;
				return;
				//用来装超时用户的临时数组
				std::vector<game_client*> timeout_clients;

				//对登录用户上锁
				boost::mutex::scoped_lock lock(this->clients_mutex_);

				//对所有用户的active时间进行对比，超过20秒还没有登录动作的用户被装进临时数组
				for (game_client_map::iterator curr_client = this->clients_.begin();
					curr_client != this->clients_.end(); curr_client++)
				{
					game_client* client = (*curr_client);

					if (client->get_actived() > 10 * 1000)
					{
						timeout_clients.push_back(client);
					}
				}


				//对临时数组中的玩家进行超时断开操作。
				for (std::vector<game_client*>::iterator curr_timeout_client = timeout_clients.begin();
					curr_timeout_client != timeout_clients.end(); curr_timeout_client++)
				{
					this->clients_.erase((*curr_timeout_client));

					//这里之所以不使用disconnect，主要是考虑到timeout的用户的连接其实本质上已经断了；
					//所以直接调用on_error触发离开事件，靠谱一些。
					(*curr_timeout_client)->on_error(service_error::TIME_OUT);
				}

				//清空临时数组
				timeout_clients.clear();

				if (tcp_client::LOG_IO_DATA)
				{
					char io_buffer_msg[64] = { 0 };
					sprintf(io_buffer_msg, "I/O : %ld / %ld\r", tcp_client::CURR_RECE_TOTAL / 5, tcp_client::CURR_SEND_TOTAL / 5);
					printf(io_buffer_msg);

					tcp_client::CURR_RECE_TOTAL = 0;
					tcp_client::CURR_SEND_TOTAL = 0;
				}

				//如果服务没有停止， 那么继续下一次计时
				if (this->is_running_)
				{
					this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(5));
					this->check_timeout_timer.async_wait(boost::bind(&game_service::on_check_timeout_clients, this, boost::asio::placeholders::error));
				}
			}
		}



		void game_service::begin_auth(game_plugin* plugin, game_client* client, command* cmd)
		{
			boost::recursive_mutex::scoped_lock lock(this->http_request_mutex_);

			if (this->http_request_working_.size() > MAX_AUTH_SESSION_COUNT)
			{
				client->disconnect(service_error::LOGIN_SERVICE_ERROR);
				return;
			}

			//分配http_request的对象内存
			void* request_mem = boost::singleton_pool<http_request, sizeof(http_request)>::malloc();

			//如果需要不请求http server请拿掉此注释
			//this->end_auth(boost::asio::error::eof, std::string(), request_data);
			this->http_request_working_.insert((http_request*)request_mem);

			http_request* request = new(request_mem)http_request(this->ios,
				"127.0.0.1",
				"/auth.aspx",
				boost::bind(&game_service::end_auth, this, _1, _2, (http_request*)request_mem, plugin, client));
		}


		void game_service::end_auth(const boost::system::error_code& code, const std::string& response_string, http_request* request, game_plugin* plugin, game_client* client)
		{
			do
			{
				if (this->is_running_ == false || plugin->is_onlined() == false)
					break;

				int ret = service_error::OK;

				{
					//对登录的用户列表进行上锁
					boost::mutex::scoped_lock lock(this->clients_mutex_);
					game_client_map::iterator finder = this->clients_.find(client);

					//client 在另外一个线程已经被销毁，超时了，或者主动离开等等情况。
					//本质上就是依靠this->clients_来判断当前的client对象是否已经被销毁了。
					if (finder == this->clients_.end())
						break;

					//用户登录动作完成，从排队用户中删除。
					this->clients_.erase(client);

					if (client->available() == false)
						break;
				}


				//http ok
				if (code == boost::asio::error::eof)
				{
					//printf("http authentication ok.\n");
					ret = plugin->auth_client(client, response_string);
				}
				else
				{
					printf("http authentication server error.\n");
					ret = service_error::LOGIN_SERVICE_ERROR;
				}

				if (ret != service_error::OK)
				{
					printf("disconnect client because auth error.\n");
					client->disconnect(ret);
				}

				break;

			} while (true);


			boost::recursive_mutex::scoped_lock lock(this->http_request_mutex_);

			//这里要确认http_request对象的访问还合法， 极限情况，在on_stop中已经销毁掉了。
			if (this->http_request_working_.find(request) != this->http_request_working_.end())
			{
				this->http_request_working_.erase(request);
				//回收http_request资源
				if (this->ios.stopped() == false)
				{
					this->ios.post(boost::bind(&game_service::on_destroy_http_request, this, request));
				}
				else
				{
					this->on_destroy_http_request(request);
				}
			}
		}


		http_request* game_service::on_create_http_request()
		{
			return NULL;
		}


		void game_service::on_destroy_http_request(http_request* request)
		{
			if (request != NULL)
			{
				request->~http_request();
				boost::singleton_pool<http_request, sizeof(http_request)>::free((void*)request);
			}
		}



		void game_service::on_client_login_handle(game_client* client, command* command)
		{
			if (command->param_size() != 2)
			{
				printf("login error\n");

				client->write("LOG -1");
				client->disconnect();
				return;
			}

			if (client->available() == false)
				return;

			//更新激活时间，防止被另外一个线程的更新销毁掉。
			client->active();

			int result_code = service_error::OK;

			//查找是否存在对应玩家想加入的的game_plugin
			std::map<char*, game_plugin*, char_key_op>::iterator it = this->plugins_.find(command->params(0));
			if (it != this->plugins_.end())
			{
				//没法用plugin.begin,因为回调函数回来之后，plugin可能已经被销毁了。
				//所以要统一回调到game_service的方法上，然后首先对is_running进行检测。
				client->fill(command->params(1), command->params(1), NULL);
				this->begin_auth((*it).second, client, command);
			}
			else
			{
				printf("game not existed.\n");
				client->disconnect(service_error::GAME_NOT_EXISTED);
			}
		}


		void game_service::on_robot_login_handle(game_client* client, command* command)
		{
		}


		game_service::~game_service()
		{
			if (this->is_running_ == true)
				this->stop();

			//销毁所有的游戏插件
			for (game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				delete (*curr_game).second;
			}
			this->plugins_.clear();

			//销毁所有的游戏区
			for (game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				delete (*curr_zone).second;
			}
			this->zones_.clear();


			for (thread_status_map::iterator pos_thread_status_pair = this->threads_status_.begin();
				pos_thread_status_pair != this->threads_status_.end();
				++pos_thread_status_pair)
			{
				delete (*pos_thread_status_pair).second;
			}
			this->threads_status_.clear();

			boost::singleton_pool<game_client, sizeof(game_client)>::purge_memory();
			boost::singleton_pool<http_request, sizeof(http_request)>::purge_memory();
			boost::singleton_pool<timer, sizeof(timer)>::purge_memory();

			printf("~game_service\n");
		}
	}
}
