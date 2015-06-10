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

		//�Է��ص���֤���ݽ��з����������û��Ƿ�ͨ����֤��ͬʱ������Ҫ�������£�
		//1�����û�����Ϣ�������
		//2����game_info��ʵ���������
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

			//��ֹ�û��ظ���¼
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

				//��ʼ��frequence_����on_run�ĵ���
				this->update_timer_.expires_from_now(boost::posix_time::milliseconds(this->frequence_));
				this->update_timer_.async_wait(boost::bind(&game_plugin::on_run, this));
			}
		}

		void game_plugin::unload()
		{
			if (this->is_onlined_ == true)
			{
				//ֹͣ����on_run(); �������ʵ�Ѿ�������ios_stop();
				this->update_timer_.cancel();

				//��timer������ʲô���ˣ���ע��
				//this->timer_.restart();
				this->is_onlined_ = false;

				this->on_unload();
			}
		}

		int game_plugin::join_client(game_client* client)
		{
			//�Գ�Ա���������
			boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

			//���Ե�ǰ���������Ƿ����ʸ�����Ա��
			int ret = this->on_befor_client_join(client);

			//���������
			if (ret == service_error::OK)
			{
				//�ӹܵ�ǰ��ҵ���Ϣ
				client->set_command_dispatcher(this);

				//�ѵ�ǰ��Ҽ����Ա��
				this->clients_[client->id()] = client;

				//������ҽ����Ա�����¼�
				this->on_client_join(client);
			}

			//���ؽ��
			return ret;
		}


		//����������뿪game_plugin��������߼�
		//(1)���û������Ƴ�
		//(2)����on_client_leave�¼�
		//(3)��client���¼��ص��йܶ�������Ϊgame_service
		//(4)���´���BYE�¼���game_service
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
