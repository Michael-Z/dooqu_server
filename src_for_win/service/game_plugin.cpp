#include "game_plugin.h"
#include "game_service.h"
#include "service_error.h"
#include <cstring>
#include <boost\property_tree\ptree.hpp>
#include <boost\property_tree\xml_parser.hpp>

namespace dooqu_server
{
	namespace service
	{
		game_plugin::game_plugin(game_service* game_service, char* gameid, char* title, double frequence) : game_service_(game_service), update_timer_(this->game_service_->get_io_service())
		{
			this->gameid_ = NULL;
			this->title_ = NULL;

			this->gameid_ = new char[std::strlen(gameid) + 1];
			std::strcpy(this->gameid_, gameid);

			this->title_ = new char[std::strlen(title) + 1];
			std::strcpy(this->title_, title);

			this->frequence_ = frequence;

			this->is_onlined_ = false;
			this->timer_.restart();
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
			printf("\ngame_plugin::on_load()");
		}

		void game_plugin::on_unload()
		{
			printf("\ngame_plugin::on_unload()");
		}

		void game_plugin::on_update()
		{
		}

		void game_plugin::on_run()
		{
			if(this->is_onlined() == false)
				return;

			this->on_update();

			if(this->is_onlined())
			{
				this->update_timer_.expires_from_now(boost::posix_time::milliseconds(this->frequence_));
				this->update_timer_.async_wait(boost::bind(&game_plugin::on_run, this));
			}
		}

		int game_plugin::auto_client(game_client* client, std::string& auth_response_string)
		{
			int ret = this->on_auth_client(client, auth_response_string);

			if(ret == service_error::OK)
			{
				return this->join_client(client);
			}

			return ret;
		}

		//�Է��ص���֤���ݽ��з����������û��Ƿ�ͨ����֤��ͬʱ������Ҫ�������£�
		//1�����û�����Ϣ�������
		//2����game_info��ʵ���������
		int game_plugin::on_auth_client(game_client* client, std::string& auth_response_string)
		{
			return service_error::OK;
		}

		int game_plugin::on_befor_client_join(game_client* client)
		{
			if(std::strlen(client->id()) > 20 || std::strlen(client->name()) > 32)
			{
				return service_error::ARGUMENT_ERROR;
			}

			//��ֹ�û��ظ���¼
			game_client_map::iterator curr_client = this->clients_.find(client->id());
			return (curr_client == this->clients_.end())? service_error::OK : service_error::CLIENT_HAS_LOGINED;
		} 

		void game_plugin::on_client_join(game_client* client)
		{
			printf("\n%s join plugin.", client->name());
		}

		void game_plugin::on_client_leave(game_client* client, int code)
		{
			printf("\n%s: leave plugin.", client->name());
		}

		game_zone* game_plugin::on_create_zone(char* zone_id)
		{
			return new game_zone(zone_id);
		}

		void game_plugin::on_command(game_client* client, command* command)
		{
			//printf("\ngame_plugin::command:%s", command->name());

			command_dispatcher::on_command(client, command);

			if(std::strcmp(command->name(), "BYE") == 0)
			{
				this->remove_client(client);
			}
		}

		void game_plugin::load()
		{
			if(this->is_onlined_ == false)
			{	
				this->is_onlined_ = true;
				this->timer_.restart();

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

				this->timer_.restart();
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
			if(ret == service_error::OK)
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

		void game_plugin::remove_client(game_client* client)
		{
			printf("\ngame_plugin::remove_client:%", client->id());
			{
				boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

				game_client_map::iterator curr_client = this->clients_.find(client->id());
				if(curr_client != this->clients_.end())
				{
					this->clients_.erase(client->id());
				}

				this->on_client_leave(client, client->error_code_);
			}

			client->set_command_dispatcher(this->game_service_);

			char buffer[32];
			std::memset(buffer, 0, sizeof(buffer));
			sprintf(buffer, "BYE %d", client->error_code_);

			client->simulate_on_command(buffer, false);
		}


		void game_plugin::broadcast(char* message)
		{
			boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

			game_client_map::iterator curr_client;

			for(curr_client = this->clients_.begin(); curr_client != this->clients_.end(); curr_client++)
			{
				(*curr_client).second->write(true, message);
			}
		}

		game_plugin::~game_plugin()
		{
			printf("\n~game_plugin");

			if(this->gameid_ != NULL)
			{
				delete this->gameid_;
			}

			if(this->title_ != NULL)
			{
				delete this->title_;
			}

			this->game_service_ = NULL;
		}
	}
}