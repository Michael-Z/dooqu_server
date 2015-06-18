#include "command_dispatcher.h"
#include <iostream>
#include <cstring>

#include "game_client.h"

namespace dooqu_server
{
	namespace service
	{
		bool command_dispatcher::regist_handle(char* cmd_name, command_handler handler)
		{
			std::map<char*, command_handler, pchar_key_cmp>::iterator handle_pair = this->handles.find(cmd_name);

			if (handle_pair == this->handles.end())
			{
				this->handles[cmd_name] = handler;
				return true;
			}
			else
			{
				return false;
			}
		}


		void command_dispatcher::action(game_client* client, size_t bytes_received)
		{
			this->on_client_data_received(client, bytes_received);
		}

		void command_dispatcher::on_client_data_received(game_client* client, size_t bytes_received)
		{
			client->buffer_pos += bytes_received;

			if (client->buffer_pos > tcp_client::MAX_BUFFER_SIZE)
			{
				client->disconnect(service_error::DATA_OUT_OF_BUFFER);

				//�˺�һ��on_error������ӣ���Ϊ�ö�����Ѿ�return�� û�м���recv��Ҳ���޷�����0����
				client->on_error(service_error::DATA_OUT_OF_BUFFER);
				//delete this;
				return;
			}

			for (int i = 0, cmd_start_pos = 0; i < client->buffer_pos; ++i)
			{
				if (client->buffer[i] == 0)
				{
					this->on_client_data(client, &client->buffer[cmd_start_pos]);

					//���on_data ������Ҫ���û��������ߴ�����ôֻ�����disconnect
					//��disconnect�У�����available = false�����رս��գ��ر�socket��
					//��ô��֮��ļ���У��ж���Ҫ���û����뿪���д�������on_error�����������뿪���߼�ѭ����
					if (client->available() == false)
					{
						//delete this;
						client->on_error(NULL);
						return;
					}

					if (i < (client->buffer_pos - 1))
					{
						cmd_start_pos = i + 1;
					}
				}

				if (i == (client->buffer_pos - 1))
				{
					if (client->buffer[i] != 0)
					{
						if ((client->buffer_pos - cmd_start_pos) >= tcp_client::MAX_BUFFER_SIZE)
						{
							client->disconnect(service_error::DATA_OUT_OF_BUFFER);

							//�˺�һ��on_error������ӣ���Ϊ�ö�����Ѿ�return�� û�м���recv��Ҳ���޷�����0����
							client->on_error(service_error::DATA_OUT_OF_BUFFER);
							//delete this;
							return;
						}

						if (cmd_start_pos != 0)
						{
							std::copy(client->buffer + cmd_start_pos, client->buffer + client->buffer_pos, client->buffer);
							client->buffer_pos -= cmd_start_pos;
						}
					}
					else
					{
						//�趨bufferPos = 0;��ʾbuffer���Դ�ͷ�����ˡ�
						client->buffer_pos = 0;
					}
				}
			}

			if (client->available() == false)
			{
				//delete this;
				client->on_error(service_error::CLIENT_NET_ERROR);
				return;
			}

			//�趨�û���������ʼ��������������
			client->p_buffer = &client->buffer[client->buffer_pos];
			client->read_from_client();
		}

		void command_dispatcher::on_client_data(game_client* client, char* data)
		{
			boost::recursive_mutex::scoped_lock lock(client->commander_mutex_);

			client->commander_.reset(data);

			if (client->commander_.is_correct())
			{
				this->on_client_command(client, &client->commander_);
			}
		}


		void command_dispatcher::simulate_client_data(game_client* client, char* data)
		{
			this->on_client_data(client, data);
		}

		void command_dispatcher::on_client_command(game_client* client, command* command)
		{
			std::map<char*, command_handler, pchar_key_cmp>::iterator handle_pair = this->handles.find(command->name());

			if (handle_pair != this->handles.end())
			{
				command_handler* handle = &handle_pair->second;
				(this->**handle)(client, command);
			}
		}


		void command_dispatcher::unregist(char* cmd_name)
		{
			this->handles.erase(cmd_name);
		}

		void command_dispatcher::remove_all_handles()
		{
			this->handles.clear();
		}
	}
}