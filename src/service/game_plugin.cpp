#include "game_plugin.h"
#include "game_service.h"
#include "service_error.h"
#include <cstring>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include "..\net\threads_lock_status.h"

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
			printf("game_plugin{%s}::on_load()\n", this->game_id());
		}


		void game_plugin::on_unload()
		{
			printf("game_plugin{%s}::on_unload()\n", this->game_id());
		}


		void game_plugin::on_update()
		{
		}


		void game_plugin::on_run()
		{
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
			printf("{%s} leave plugin. ret={%d}.\n", client->name(), code);
		}


		game_zone* game_plugin::on_create_zone(char* zone_id)
		{
			return new game_zone(this->game_service_, zone_id);
		}


		void game_plugin::dispatch_bye(game_client* client)
		{
			this->remove_client(client);
		}


		void game_plugin::on_client_command(game_client* client, command* command)
		{
			if (this->game_service_->is_running() == false)
				return;

			if (client->active_monitor_.can_active() == false)
			{
				client->disconnect(service_error::CONSTANT_REQUEST);
				return;
			}

			command_dispatcher::on_client_command(client, command);


			printf("CMD:%s->%s", command->name(), client->id());

			{
				using namespace dooqu_server::net;

				thread_status_map::iterator curr_thread_pair = this->game_service_->threads_status()->find(boost::this_thread::get_id());

				if (curr_thread_pair != this->game_service_->threads_status()->end())
				{
					curr_thread_pair->second->restart();
				}
			}
		}


		void game_plugin::on_update_timeout_clients()
		{
			boost::recursive_mutex::scoped_lock lock(this->clients_lock_);
			for (game_client_map::iterator e = this->clients_.begin();
				e != this->clients_.end();
				++e)
			{
				game_client* client = (*e).second;
				if (client->actived_time.elapsed() > 60 * 1000)
				{
					this->game_service_->get_io_service().post(boost::bind(&game_client::disconnect, client, service_error::TIME_OUT));// client->disconnect(service_error::TIME_OUT);
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

				
				boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

				for (game_client_map::iterator curr_client_pair = this->clients_.begin();
					curr_client_pair != this->clients_.end(); ++curr_client_pair)
				{
					game_client* curr_client = (curr_client_pair)->second;

					curr_client->disconnect(service_error::SERVER_CLOSEING);
				}
			}
		}


		int game_plugin::join_client(game_client* client)
		{
			thread_status::log("start->game_plugin::join_client.clients_lock");
			//�Գ�Ա���������
			boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

			thread_status::log("end->game_plugin::join_client.clients_lock");


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


		void game_plugin::remove_client_from_plugin(game_client* client)
		{
			{
				thread_status::log("start->game_plugin::remove_client_from_plugin.clients_lock");
				boost::recursive_mutex::scoped_lock lock(this->clients_lock_);
				thread_status::log("end->game_plugin::remove_client_from_plugin.clients_lock");

				this->clients_.erase(client->id());
				this->on_client_leave(client, client->error_code_);
			}

			//client->set_command_dispatcher(this->game_service_);
			//client->cmd_dispatcher_->dispatch_bye(client);
			((command_dispatcher*)this->game_service_)->dispatch_bye(client);
		}


		//����������뿪game_plugin��������߼�
		void game_plugin::remove_client(game_client* client)
		{
			client->set_command_dispatcher(NULL);

			string server_url, path_url;

			if (this->need_update_offline_client(client, server_url, path_url))
			{
				this->begin_update_client(client, server_url, path_url);
			}
			else
			{
				this->remove_client_from_plugin(client);
			}
		}


		void game_plugin::begin_update_client(game_client* client, string& server_url, string& path_url)
		{
			if (this->game_service_ != NULL)
			{
				io_service* ios = NULL;//&(this->game_service_->get_io_service().stopped() ? this->game_service_->get_io_service_for_unload() : this->game_service_->get_io_service());

				if (this->game_service_->get_io_service().stopped())
				{
					ios = &this->game_service_->get_io_service_for_unload();
				}
				else
				{
					ios = &this->game_service_->get_io_service();
				}

				void* http_req_mem = this->game_service_->get_http_request();

				http_request* request = new(http_req_mem)http_request(
					*ios,
					server_url,
					path_url,
					boost::bind(&game_plugin::end_update_client, this, _1, _2, _3, client, (http_request*)http_req_mem));
			}
		}


		void game_plugin::end_update_client(const boost::system::error_code& err,
			const int status_code,
			const string& result, game_client* client, http_request* request)
		{
			this->game_service_->free_http_request(request);

			//���http����ɹ���ͬʱhttp�ķ�������200
			if (err == boost::asio::error::eof && status_code == 200)
			{
				printf("http update client successed.\n");
				this->on_update_offline_client(err, status_code, client);
			}
			else
			{
				printf("http update client failed,retry...\n");
				//���http����û�гɹ�				
				//����game_zone�ٷ�����ʱ����
				if (this->zone_ != NULL && --client->retry_update_times_ > 0)
				{
					this->zone_->queue_task(boost::bind(&game_plugin::remove_client, this, client), 3000);
					return;
				}
			}

			this->remove_client_from_plugin(client);

			printf("callback threadid is: %d\n", boost::this_thread::get_id());
		}


		//�����game_client�뿪game_plugin��Ҫ����httpЭ����ⲿ��ַ�����¸���game_client��״̬��
		//��ô���ڴ˺�������true��������ȷ�ĸ�ֵserver_url��request_path��ֵ��
		bool game_plugin::need_update_offline_client(game_client* client, string& server_url, string& request_path)
		{
			return false;
		}


		//���get_offline_update_url����true������дon_update_offline_client��������error_code��ֵ��ȷ��update�����ķ���ֵ��
		void game_plugin::on_update_offline_client(const boost::system::error_code& err, const int status_code, game_client* client)
		{
		}


		void game_plugin::broadcast(char* message, bool asynchronized)
		{
			thread_status::log("start->game_plugin::broadcast.clients_lock");
			boost::recursive_mutex::scoped_lock lock(this->clients_lock_);
			thread_status::log("end->game_plugin::broadcast.clients_lock");

			game_client_map::iterator curr_client;

			for (curr_client = this->clients_.begin(); curr_client != this->clients_.end(); curr_client++)
			{
				(*curr_client).second->write(message);
			}
		}


		game_plugin::~game_plugin()
		{
			printf("game_plugin destroyed.\n");

			if (this->gameid_ != NULL)
			{
				delete this->gameid_;
			}

			if (this->title_ != NULL)
			{
				delete this->title_;
			}
		}
	}
}
