#include "game_client.h"

namespace dooqu_server
{
	namespace service
	{
		game_client::game_client(io_service& ios) :
			tcp_client(ios),
			game_info_(NULL),
			cmd_dispatcher_(NULL),
			error_code_(0)
		{
			this->id_[0] = 0;
			this->name_[0] = 0;
			this->retry_update_times_ = UP_RE_TIMES;
			this->active();
			this->message_monitor_.init(5, 4000);
			this->active_monitor_.init(30, 60 * 1000);
		}


		game_client::~game_client()
		{
			printf("game_client destroyed.\n");
		}


		void game_client::on_data_received(const boost::system::error_code& error, size_t bytes_received)
		{
			{
				//receive_handleҪ�ڹ������ֳ���ִ�У�
				//�������������߳��ϣ�ͬһʱ�䲻�϶�������receive_handle�ĵ��ã���Ϊreceive������һ�����еĶ�����
				//����! ������Ϊ�߼���Ҫ���������������߳��ϵ���disconnect���������Ա���Ҫͬ��status��
				//��ֹ���������Ƭ�ε��м䱻�����߳�disconnect��

				if (!error)
				{
					if (this->cmd_dispatcher_ != NULL)
					{
						this->cmd_dispatcher_->action(this, bytes_received);

						tcp_client::on_data_received(error, bytes_received);
					}
#ifdef __DEBUG__
					else
					{
						printf("game_client's cmd_dispatcher is NULL.\n");
					}
#endif
				}
				else
				{

					thread_status::log("start->game_client::on_date_received.status_lock_");
					boost::recursive_mutex::scoped_lock status_lock(this->status_lock_);
					thread_status::log("end->game_client::on_date_received.status_lock_");

					printf("client.receive canceled:{%s}\n", error.message().c_str());
					this->available_ = false;
					this->on_error(service_error::CLIENT_NET_ERROR);
				}

			}//end lock;
		}


		//on_error����Ҫ�����ǽ��û����뿪���߼����������ݸ�command_dispatcher����������δ���
		void game_client::on_error(const int error)
		{
			//error_code_�ĳ�ʼĬ��ֵΪ0����CLIENT_NET_ERROR��
			//������ֵ���Ķ���˵����on_error֮ǰ�����ù�disconnect�������ݹ��Ͽ���ԭ��
			//������tcp_client�ĶϿ������С���ʹ����0��Ҳ���ᱻ��ֵ��
			if (this->error_code_ == service_error::CLIENT_NET_ERROR)
			{
				this->error_code_ = error;
			}

			printf("game_client{%s}.on_error:{%d}\n", this->id(), this->error_code_);

			if (this->cmd_dispatcher_ != NULL)
			{
				this->cmd_dispatcher_->dispatch_bye(this);
			}
		}


		void game_client::fill(char* id, char* name, char* profile)
		{
			int id_len = std::strlen(id);
			int name_len = std::strlen(name);
			int pro_len = (profile == NULL) ? 0 : std::strlen(profile);

			int min_id_len = std::min((ID_LEN - 1), id_len);
			int min_name_len = std::min((NAME_LEN - 1), name_len);

			strncpy(this->id_, id, min_id_len);
			strncpy(this->name_, name, min_name_len);

			this->id_[min_id_len] = 0;
			this->name_[min_name_len] = 0;
		}


		void game_client::simulate_on_command(char* command_data, bool is_const_string = true)
		{

			if (this->available() == false || this->cmd_dispatcher_ == NULL)
				return;

			char* command_data_clone = NULL;

			//����Ǵ������һ������������ô����Ҫ��������һ����д�Ļ�����
			if (is_const_string)
			{
				//���ɿ�д�Ļ�����
				command_data_clone = new char[std::strlen(command_data) + 1];
				std::strcpy(command_data_clone, command_data);
			}
			else
			{
				command_data_clone = command_data;
			}

			//����
			this->cmd_dispatcher_->simulate_client_data(this, command_data_clone);

			if (is_const_string)
			{
				delete[] command_data_clone;
			}
		}


		void game_client::set_command_dispatcher(command_dispatcher* dispatcher)
		{
			this->cmd_dispatcher_ = dispatcher;
		}


		//disconnect(int reason_code)����Ҫ����:
		//1���жϿͻ����Ƿ��ڿ���״̬
		//2��������ý�bye������ͻ��ˣ�codeΪ�Ͽ���ԭ��
		//3��tcp_client::disconnectֻ����client receive�����Ѿ�Ͷ�ݺ󣬲��ܴ���error�� 
		//4��so��disconnect��receive_handle�������������������߳��ϡ��л�����receive_handle�У���û
		//���ü�Ͷ����һ��receive���ͱ�close������͵���on_error���ᴥ����so so:����
		void game_client::disconnect(int code)
		{
			if (this->available())
			{
				//�趨�����
				this->error_code_ = code;

				this->write("ERR %d%c", this->error_code_, NULL);
				this->disconnect_when_io_end();
			}
		}
	}
}
