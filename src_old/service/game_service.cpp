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


            			//������ݵ�zoneid��Ϊ�գ���ôΪ��Ϸ������Ϸ��
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

			//������ݵ�zoneid��Ϊ�գ���ôΪ��Ϸ������Ϸ��
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

			//�������е����з���
			for(game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				(*curr_zone).second->load();
			}

			//�������е���Ϸ�߼�
			for(game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				(*curr_game).second->load();
			}

			printf("game_service::on_start()\n");
		}

		void game_service::on_started()
		{
			//����update�߳�
			//boost::thread t(boost::bind(&game_service::thread_for_update, this));

			//��������¼��ʱ�û��Ķ�ʱ��
			this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(3));
			this->check_timeout_timer.async_wait(boost::bind(&game_service::on_check_timeout_clients, this, boost::asio::placeholders::error));
		}

		void game_service::on_stop()
		{
			printf("service is stoping.");
			//ֹͣ�Ե�¼��ʱ�ļ��
			this->check_timeout_timer.cancel();

			//ֹͣ���е���Ϸ�߼�
			for(game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				(*curr_game).second->unload();
			}

			//ֹͣ���е���Ϸ��
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
			printf("\nSTART game_service.on_client_leave.{%d}", client->name(), leave_code);
			//����������ڽ�����Ϸ����֮ǰ���뿪�ˣ� ��ôҪ��������ʱ����ȥ����
			//�ӵ�¼��Ա���ﱻɾ�����������
			//1����ҵ�¼�˷������е�ĳ����Ϸ����; ��join_client�б��ӳ�Ա����ɾ��
			//2����ҵ�¼��ʱ�����������Ͽ������ڼ�鳬ʱ�ĳ�Ա�����б��ӳ�Ա����ɾ����
			//3������������⡢���Լ�����BYE�뿪���Ǿ�ֻ������ɾ����
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
				//��tcp_client.receive_handle�У�����ǰ���BYE��on_data֮��֪����ǰ�û��Ѿ������ã���ʼ����delete this.
				client->disconnect();
				//client->disconnect(service_error::CLIENT_EXIT);
			}

			printf("\nEND game_service.on_client_leave.{%d}", client->name());
		}


		//ʹ���˶�ʱ��
		void game_service::on_check_timeout_clients(const boost::system::error_code &error)
		{
			if(!error)
			{
				//����װ��ʱ�û�����ʱ����
				std::vector<game_client*> timeout_clients;

				//�Ե�¼�û�����
				boost::mutex::scoped_lock lock(this->clients_mutex_);

				//�������û���activeʱ����жԱȣ�����20�뻹û�е�¼�������û���װ����ʱ����
				for(game_client_map::iterator curr_client = this->clients_.begin();
					curr_client != this->clients_.end(); curr_client++)
				{
					game_client* client = (*curr_client);

					if(client->get_actived() > 20 * 1000)
					{
						timeout_clients.push_back(client);
					}
				}

				//����ʱ�����е���ҽ��г�ʱ�Ͽ�������
				for(std::vector<game_client*>::iterator curr_timeout_client = timeout_clients.begin();
					curr_timeout_client != timeout_clients.end(); curr_timeout_client++)
				{
					this->clients_.erase((*curr_timeout_client));
					(*curr_timeout_client)->disconnect(service_error::TIME_OUT);
				}

				//�����ʱ����
				timeout_clients.clear();

				//�������û��ֹͣ�� ��ô������һ�μ�ʱ
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
				//�Ե�¼���û��б��������
				boost::mutex::scoped_lock lock(this->clients_mutex_);
				game_client_map::iterator finder = this->clients_.find(request_data->client);

				//client ������һ���߳��Ѿ������٣���ʱ�ˣ����������뿪�ȵ������
				//�����Ͼ�������this->clients_���жϵ�ǰ��client�����Ƿ��Ѿ��������ˡ�
				if(finder == this->clients_.end())
					return;

				//�û���¼������ɣ����Ŷ��û���ɾ����
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
				//��ͨ��Ϣ��������Ϊ����Ҫ��"BYE"�������
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

			//���¼���ʱ�䣬��ֹ������һ���̵߳ĸ������ٵ���
			client->active();

			int result_code = service_error::OK;

			//�����Ƿ���ڶ�Ӧ��������ĵ�game_plugin
			std::map<char*, game_plugin*, char_key_op>::iterator it = this->plugins_.find(command->params(0));
			if(it != this->plugins_.end())
			{
				//û����plugin.begin,��Ϊ�ص���������֮��plugin�����Ѿ��������ˡ�
				//����Ҫͳһ�ص���game_service�ķ����ϣ�Ȼ�����ȶ�is_running���м�⡣
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

			//�������е��������
			for(game_client_map::iterator curr_client = this->clients_.begin();
				curr_client != this->clients_.end(); curr_client++)
			{
				delete (*curr_client);
			}
			this->clients_.clear();

			//ֹͣ���е���Ϸ���
			for(game_plugin_map::iterator curr_game = this->plugins_.begin();
				curr_game != this->plugins_.end(); curr_game++)
			{
				delete (*curr_game).second;
			}
			this->plugins_.clear();


			//ֹͣ���е���Ϸ��
			for(game_zone_map::iterator curr_zone = this->zones_.begin();
				curr_zone != this->zones_.end(); curr_zone++)
			{
				delete (*curr_zone).second;
			}

			this->zones_.clear();
		}
	}
}
