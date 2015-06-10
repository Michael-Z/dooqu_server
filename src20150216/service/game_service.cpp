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


			//������ݵ�zoneid��Ϊ�գ���ôΪ��Ϸ������Ϸ��
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

			//�������е����з���
			for (game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				(*curr_zone).second->load();

				boost::thread t(boost::bind(&io_service::run, &this->ios));
				this->threads_status_[t.get_id()] = new tick_count_t();
				printf("create zone thread:%d\n", t.get_id());
				t.detach();
			}

			//�������е���Ϸ�߼�
			for (game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				(*curr_game).second->load();
			}

			printf("game_service::on_start()\n");
		}


		void game_service::on_started()
		{
			//��������¼��ʱ�û��Ķ�ʱ��
			this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(3));
			this->check_timeout_timer.async_wait(boost::bind(&game_service::on_check_timeout_clients, this, boost::asio::placeholders::error));
		}


		void game_service::on_stop()
		{
			printf("service is stoping...\n");

			//ֹͣ�Ե�¼��ʱ�ļ��
			this->check_timeout_timer.cancel();

			//������ʱ��¼�û����ڴ���Դ
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

			//ֹͣ���е���Ϸ�߼�
			for (game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				(*curr_game).second->unload();
			}

			//ֹͣ���е���Ϸ��
			for (game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				(*curr_zone).second->unload();
			}

			//����δ���ص�http_request����
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
				//���û�������
				boost::mutex::scoped_lock lock(this->clients_mutex_);
				//�ڵ�¼�û�����ע��
				this->clients_.insert(g_client);
			}
			//���ûʱ���
			g_client->active();

			//����socketΪ����״̬
			g_client->available_ = true;

			//��������ļ�����Ϊ��ǰservice
			g_client->set_command_dispatcher(this);

			//��ʼ��ȡ�û���������Ϣ
			g_client->read_from_client();
		}



		void game_service::on_client_leave(game_client* client, int leave_code)
		{
			client->set_command_dispatcher(NULL);

			//printf("START game_service.on_client_leave:{%d}\n", client->name(), leave_code);
			//����������ڽ�����Ϸ����֮ǰ���뿪�ˣ� ��ôҪ��������ʱ����ȥ����
			//�ӵ�¼��Ա���ﱻɾ�����������
			//1����ҵ�¼�˷������е�ĳ����Ϸ����; ��join_client�б��ӳ�Ա����ɾ��
			//2����ҵ�¼��ʱ�����������Ͽ������ڼ�鳬ʱ�ĳ�Ա�����б��ӳ�Ա����ɾ����
			//3������������⡢���Լ�����BYE�뿪���Ǿ�ֻ������ɾ����
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
			//������client�Ĳ�����post������һ���̣߳� ����ǰ�߳��е�receive_handleһ��������"this"������ִ�еĻ������ó�ִ������

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
				//���������ú�tcp_client::receive_handle���г�ͻ
				boost::mutex::scoped_lock lock(client->status_lock_);
				client->~game_client();
				boost::singleton_pool<game_client, sizeof(game_client)>::free(client);
			}
		}



		//ʹ���˶�ʱ��
		void game_service::on_check_timeout_clients(const boost::system::error_code &error)
		{
			printf("game_service::on_run's thread id:%d\n", boost::this_thread::get_id());

			//thread_mutex_map::iterator curr_mutex = this->thread_mutexs()->find(boost::this_thread::get_id());
			//boost::recursive_mutex::scoped_lock lock(*curr_mutex->second);
			if (!error && this->is_running_)
			{
				//������ó�ʱ��⣬��ע��return;
				return;
				//����װ��ʱ�û�����ʱ����
				std::vector<game_client*> timeout_clients;

				//�Ե�¼�û�����
				boost::mutex::scoped_lock lock(this->clients_mutex_);

				//�������û���activeʱ����жԱȣ�����20�뻹û�е�¼�������û���װ����ʱ����
				for (game_client_map::iterator curr_client = this->clients_.begin();
					curr_client != this->clients_.end(); curr_client++)
				{
					game_client* client = (*curr_client);

					if (client->get_actived() > 10 * 1000)
					{
						timeout_clients.push_back(client);
					}
				}


				//����ʱ�����е���ҽ��г�ʱ�Ͽ�������
				for (std::vector<game_client*>::iterator curr_timeout_client = timeout_clients.begin();
					curr_timeout_client != timeout_clients.end(); curr_timeout_client++)
				{
					this->clients_.erase((*curr_timeout_client));

					//����֮���Բ�ʹ��disconnect����Ҫ�ǿ��ǵ�timeout���û���������ʵ�������Ѿ����ˣ�
					//����ֱ�ӵ���on_error�����뿪�¼�������һЩ��
					(*curr_timeout_client)->on_error(service_error::TIME_OUT);
				}

				//�����ʱ����
				timeout_clients.clear();

				if (tcp_client::LOG_IO_DATA)
				{
					char io_buffer_msg[64] = { 0 };
					sprintf(io_buffer_msg, "I/O : %ld / %ld\r", tcp_client::CURR_RECE_TOTAL / 5, tcp_client::CURR_SEND_TOTAL / 5);
					printf(io_buffer_msg);

					tcp_client::CURR_RECE_TOTAL = 0;
					tcp_client::CURR_SEND_TOTAL = 0;
				}

				//�������û��ֹͣ�� ��ô������һ�μ�ʱ
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

			//����http_request�Ķ����ڴ�
			void* request_mem = boost::singleton_pool<http_request, sizeof(http_request)>::malloc();

			//�����Ҫ������http server���õ���ע��
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
					//�Ե�¼���û��б��������
					boost::mutex::scoped_lock lock(this->clients_mutex_);
					game_client_map::iterator finder = this->clients_.find(client);

					//client ������һ���߳��Ѿ������٣���ʱ�ˣ����������뿪�ȵ������
					//�����Ͼ�������this->clients_���жϵ�ǰ��client�����Ƿ��Ѿ��������ˡ�
					if (finder == this->clients_.end())
						break;

					//�û���¼������ɣ����Ŷ��û���ɾ����
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

			//����Ҫȷ��http_request����ķ��ʻ��Ϸ��� �����������on_stop���Ѿ����ٵ��ˡ�
			if (this->http_request_working_.find(request) != this->http_request_working_.end())
			{
				this->http_request_working_.erase(request);
				//����http_request��Դ
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

			//���¼���ʱ�䣬��ֹ������һ���̵߳ĸ������ٵ���
			client->active();

			int result_code = service_error::OK;

			//�����Ƿ���ڶ�Ӧ��������ĵ�game_plugin
			std::map<char*, game_plugin*, char_key_op>::iterator it = this->plugins_.find(command->params(0));
			if (it != this->plugins_.end())
			{
				//û����plugin.begin,��Ϊ�ص���������֮��plugin�����Ѿ��������ˡ�
				//����Ҫͳһ�ص���game_service�ķ����ϣ�Ȼ�����ȶ�is_running���м�⡣
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

			//�������е���Ϸ���
			for (game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				delete (*curr_game).second;
			}
			this->plugins_.clear();

			//�������е���Ϸ��
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
