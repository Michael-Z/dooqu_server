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

				//此后一句on_error必须添加，因为该段最后已经return， 没有继续recv，也就无法触发0接收
				client->on_error(service_error::DATA_OUT_OF_BUFFER);
				//delete this;
				return;
			}

			for (int i = 0, cmd_start_pos = 0; i < client->buffer_pos; ++i)
			{
				if (client->buffer[i] == 0)
				{
					this->on_client_data(client, &client->buffer[cmd_start_pos]);

					//如果on_data 中如需要对用户进行离线处理，那么只需调用disconnect
					//在disconnect中，设置available = false，并关闭接收，关闭socket。
					//那么在之后的检查中，判断需要对用户的离开进行处理，调用on_error进行清理，并离开大逻辑循环。
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

							//此后一句on_error必须添加，因为该段最后已经return， 没有继续recv，也就无法触发0接收
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
						//设定bufferPos = 0;表示buffer可以从头开用了。
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

			//设定好缓冲区的起始，继续接收数据
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