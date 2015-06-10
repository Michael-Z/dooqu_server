#include "plane_plugin.h"
#include <set>

namespace dooqu_server
{
	namespace plane
	{
		void plane_plugin::on_load()
		{
			for (int i = 0; i < 100; i++)
			{
				plane_desk* desk = new plane_desk(i);
				this->desks_.push_back(desk);
			}
			this->regist_handle("IDK", make_handler(plane_plugin::on_client_join_desk_handle));
			this->regist_handle("BYE", make_handler(plane_plugin::on_client_bye_handle));
			this->regist_handle("RDY", make_handler(plane_plugin::on_client_ready_handle));
			this->regist_handle("ATK", make_handler(plane_plugin::on_client_attack_handle));

			game_plugin::on_load();
		}


		void plane_plugin::on_unload()
		{
			this->remove_all_handles();

			game_plugin::on_unload();
		}


		void plane_plugin::on_update()
		{
			for (int i = 0; this->is_onlined() && i < this->desks_.size(); i++)
			{
				plane_desk* curr_desk = this->desks_.at(i);
				boost::recursive_mutex::scoped_lock lock(curr_desk->status_lock());

				switch (curr_desk->get_desk_status())
				{
				case plane_desk::DESK_STATUS_READY_MAP:
					if (curr_desk->elapsed_active_time() < 22000)
						continue; //确定下流程

					//将装填更新为
					curr_desk->set_desk_status(plane_desk::DESK_STATUS_WAITTING);
					curr_desk->active();

					for (int i = 0; i < plane_desk::DESK_CAPACITY; i++)
					{
						game_client* curr_client = curr_desk->pos_client(i);

						assert(curr_client != NULL);

						plane_game_info* game_info = (plane_game_info*)curr_client->get_game_info();
						if (curr_desk->pos_client_planes(i)->size() == 0)
						{
							//如果size == 0说明他没摆
							curr_desk->set_pos_client(i, NULL);
							this->on_client_leave_desk(curr_client, curr_desk, service_error::READY_MAP_TIME_OUT);
						}
					}
					break;

				case plane_desk::DESK_STATUS_RUNNING:
					if (curr_desk->elapsed_active_time() > 22000)
					{

					}
					break;
				}
			}

			game_plugin::on_update();
		}


		int plane_plugin::on_befor_client_join(game_client* client)
		{
			return game_plugin::on_befor_client_join(client);
		}


		void plane_plugin::on_client_join(game_client* client)
		{
			plane_game_info* game_info = new plane_game_info();
			client->set_game_info(game_info);

			char data_buffer[64];
			int n = sprintf(data_buffer, "LOG %s %s %d", this->game_id(), client->id());
			data_buffer[std::min(n - 1, 64 - 1)] = 0;
			client->write(data_buffer);

			game_plugin::on_client_join(client);
		}


		void plane_plugin::on_client_leave(game_client* client, int code)
		{
			plane_game_info* game_info_ = (plane_game_info*)client->get_game_info();

			if (game_info_ != NULL)
			{
				client->set_game_info(NULL);
				delete game_info_;				
			}

			game_plugin::on_client_leave(client, code);
		}


		void plane_plugin::on_client_join_desk(game_client* client, plane_desk* desk, int pos_index)
		{
			//获取到当前的client
			game_client* curr_client = desk->pos_client(pos_index);

			//整理IDK消息
			char data_buffer[64] = { 0 };
			sprintf(data_buffer, "IDK %s", desk->get_desk_id());
			curr_client->write(data_buffer);

			//获取另一位玩家的信息
			game_client* next_client = desk->pos_client((pos_index + 1) % plane_desk::DESK_CAPACITY);

			//如果另一个位置有玩家在
			if (next_client != NULL)
			{
				//先把另一位玩家的信息送给后进来的玩家
				int n = sprintf(data_buffer, "LSD %s %s", next_client->id(), next_client->name());
				data_buffer[std::min(n, 63)] = 0;
				client->write(data_buffer);

				//再通知另一个位置的玩家:有新人进来了
				n = sprintf(data_buffer, "JDK %s %s", client->id(), client->name());
				data_buffer[std::min(n, 63)] = 0;
				next_client->write(data_buffer);

				desk->active();
				desk->set_desk_status(plane_desk::DESK_STATUS_READY_MAP);
				this->zone_->queue_task(boost::bind(&plane_plugin::on_game_ready, this, desk, ++desk->verify_code), 1500);
				return;
			}
			desk->active();
		}


		void plane_plugin::on_client_leave_desk(game_client* client, plane_desk* desk, int reaspon)
		{
			plane_game_info* game_info = (plane_game_info*)client->get_game_info();

			game_info->desk_ = NULL;

			for (int i = 0; i < 2; i++)
			{
				game_client* curr_client = desk->pos_client(i);
				if (curr_client != NULL)
				{
					char buffer[16] = { 0 };
					sprintf(buffer, "LDK %s", client->id());

					curr_client->write(buffer);
					break;
				}
			}
		}

		void plane_plugin::on_game_ready(plane_desk* desk, int verify_code)
		{
			if (this->is_onlined() == false)
				return;

			boost::recursive_mutex::scoped_lock lock(desk->status_lock());
			if (desk->verify_code != verify_code || desk->get_desk_status() != plane_desk::DESK_STATUS_READY_MAP)
				return;

			if (desk->is_full())
			{
				desk->active();
				desk->write_to_all("RDY\0");
			}
		}

		void plane_plugin::on_game_start(plane_desk* desk, int verify_code)
		{
			if (this->is_onlined() == false)
				return;

			boost::recursive_mutex::scoped_lock lock(desk->status_lock());

			//如果参照码错误 或者 桌子状态错误
			if (desk->verify_code != verify_code || desk->get_desk_status() != plane_desk::DESK_STATUS_RUNNING)
				return;

			//如果任何位置上为空
			if (desk->is_full() == false)
				return;

			desk->active();
			desk->write_to_all("STT\0");
		}

		void plane_plugin::on_game_stop(plane_desk* desk, game_client* client, bool normal)
		{

		}

		void plane_plugin::on_client_join_desk_handle(game_client* client, command* cmd)
		{
			plane_game_info* game_info = (plane_game_info*)client->get_game_info();

			plane_desk* desk = game_info->get_desk();

			if (desk != NULL)
			{
				boost::recursive_mutex::scoped_lock lock(desk->status_lock());

				int pos_index = desk->get_client_pos(client);
				desk->set_pos_client(pos_index, NULL);
				this->on_client_leave_desk(client, desk, 0);
			}

			for (int i = 0; this->is_onlined() && i < this->desks_.size(); i++)
			{
				plane_desk* curr_desk = this->desks_.at(i);
				boost::recursive_mutex::scoped_lock lock(curr_desk->status_lock());

				//如果当前桌子不是等待状态，那么返回
				if (curr_desk->get_desk_status() > plane_desk::DESK_STATUS_WAITTING)
					continue;

				game_client* client_0 = curr_desk->pos_client(0);
				game_client* client_1 = curr_desk->pos_client(1);

				if (client_0 != NULL && client_1 == NULL
					|| client_0 == NULL && client_1 != NULL)
				{
					int free_index = (client_0 == NULL) ? 1 : 0;
					curr_desk->set_pos_client(free_index, client);
					((plane_game_info*)client->get_game_info())->desk_ = curr_desk;

					this->on_client_join_desk(client, curr_desk, free_index);
					return;
				}
			}

			for (int i = 0; this->is_onlined() && i < this->desks_.size(); i++)
			{
				plane_desk* curr_desk = this->desks_.at(i);
				boost::recursive_mutex::scoped_lock lock(curr_desk->status_lock());
				if (curr_desk->get_desk_status() > plane_desk::DESK_STATUS_WAITTING)
					continue;

				game_client* client_0 = curr_desk->pos_client(0);
				game_client* client_1 = curr_desk->pos_client(1);

				if (client_0 == NULL && client_1 == NULL)
				{
					curr_desk->set_pos_client(1, client);
					((plane_game_info*)client->get_game_info())->desk_ = curr_desk;
					this->on_client_join_desk(client, curr_desk, 1);
					return;
				}
			}

			client->write("IDK -1\0");
		}

		//当玩家筹备自己的飞机地图
		//     高     低         高   低
		//RDY xxxx   xxxx      xxxx   xxxx
		//  飞机方向 飞机类型     Y位置    X位置
		void plane_plugin::on_client_ready_handle(game_client* client, command* cmd)
		{
			if (cmd->param_size() != 1)
				return;

			plane_game_info* game_info = (plane_game_info*)client->get_game_info();
			if (game_info->is_ready())
				return;

			plane_desk* curr_desk = game_info->get_desk();
			//如果他没落桌
			if (curr_desk == NULL)
				return;

			boost::recursive_mutex::scoped_lock lock(curr_desk->status_lock());
			if (curr_desk->get_desk_status() != plane_desk::DESK_STATUS_READY_MAP)
				return;

			if (curr_desk->is_full() == false)
				return;

			int byte_count = std::strlen(cmd->params(0));

			if ((byte_count % 2) != 0)
			{
				return;
			}

			int plane_num = byte_count / 2;

			if (plane_num > 3)
			{
				return;
			}

			int client_pos = curr_desk->get_client_pos(client);

			//如果桌子数据错误
			if (client_pos == -1)
				return;

			//如果他已经摆过了飞机
			if (curr_desk->pos_client_planes(client_pos)->size() > 0)
				return;

			for (int i = 0; i < plane_num; i++)
			{

				int plane_type_ = cmd->params(0)[(2 * i)] & 15;
				int plane_dir_ = cmd->params(0)[(2 * i)] >> 4;
				int plane_pos_x = cmd->params(0)[(2 * i) + 1] & 15;
				int plane_pos_y = cmd->params(0)[(2 * i) + 1] >> 4;

				plane_type plane_t = UNKNOWN_TYPE;
				plane_dir plane_d = UNKNOWN_DIR;

				if (plane_type_ > plane_type::UNKNOWN_TYPE && plane_type_ < 5)
				{
					plane_type plane_t = (plane_type)plane_type_;
				}
				else
				{
					curr_desk->reset_maps(client_pos);
					curr_desk->pos_client_planes(client_pos)->clear();
					return;
				}

				if (plane_dir_ > plane_dir::UNKNOWN_DIR && plane_dir_ < 5)
				{
					plane_dir plane_d = (plane_dir)plane_dir_;
				}
				else
				{
					curr_desk->reset_maps(client_pos);
					curr_desk->pos_client_planes(client_pos)->clear();
					return;
				}

				plane_info pi(plane_t, plane_d, plane_pos_x, plane_pos_y);
				//pi->reset(plane_t, plane_d, plane_pos_x, plane_pos_y);
				if (curr_desk->set_plane(client_pos, pi))
				{
					curr_desk->pos_client_planes(client_pos)->push_back(pi);
				}
				else
				{
					//放置的位置不对
					curr_desk->reset_maps(client_pos);
					curr_desk->pos_client_planes(client_pos)->clear();
					client->write("RDY -1\0");
					return;
				}
			}

			game_info->is_ready_ = true;
			client->write("RDY 1\0");
			game_client* another_client = curr_desk->pos_client((client_pos + 1) % plane_desk::DESK_CAPACITY);

			if (another_client != NULL)
			{
				plane_game_info* another_game_info = (plane_game_info*)another_client->get_game_info();
				if (another_game_info->is_ready())
				{
					this->zone_->queue_task(boost::bind(&plane_plugin::on_game_start, this, curr_desk, ++curr_desk->verify_code), 1000);
				}
			}
		}

		//         低位        高位           低位      高位        低位         高位    低位     高位
		//             +1byte+                  +1byte+              +1byte+           +1byte+
		//         4bit        4bit         4bit      4bit       4bit        4bit    4bit   4bit
		//ATR   攻击者ID    被攻击者ID    攻击飞机类型   轰炸结果   被攻击飞机类型    方向    x_pos   y_pos
		//ATR     xxxx       xxxx           xxxx      xxxx       xxxx        xxxx    xxxx    xxxx
		void plane_plugin::on_game_attack_result(plane_desk* desk)
		{
			vector<attack_result_info> attack_results[plane_desk::DESK_CAPACITY];

			//枚举所有玩家的索引pos_index
			for (int pos_index = 0; pos_index < plane_desk::DESK_CAPACITY; ++pos_index)
			{
				//获取当前玩家的所有攻击列表
				vector<attack_info>& pos_attacks = *(desk->pos_client_attacks(pos_index));
				int attack_count = pos_attacks.size();
				
				int next_client_pos = (pos_index + 1) % plane_desk::DESK_CAPACITY;
				vector<plane_info>& other_pos_planes = *(desk->pos_client_planes(next_client_pos));				
				
				bool all_plane_is_dead = true;
				//枚举当前玩家的所有攻击
				for (int attack_index = 0; attack_index < attack_count; ++attack_index)
				{
					attack_info& ai = pos_attacks.at(attack_index);
					
					//枚举对方的所有飞机
					for (int plane_index = 0; plane_index < other_pos_planes.size(); ++plane_index)
					{
						plane_info& pi = other_pos_planes.at(plane_index);

						//如果当前的飞机已经挂了，就跳过
						if (pi.is_living == false)
							continue;

						attack_result_info atk_res_info;

						if (pi.plane_t == ai.plane_t && pi.pos_x == ai.pos_x && pi.pos_y == ai.pos_y)
						{
							atk_res_info.atk_result = HEAD_ATK;

							atk_res_info.atk_plane_type = ai.plane_t;
							atk_res_info.inj_plane_type = pi.plane_t;
							atk_res_info.inj_plane_dir = pi.dir;

							atk_res_info.atk_pos_index = pos_index;
							atk_res_info.inj_pos_index = next_client_pos;

							atk_res_info.pos_x = pi.pos_x;
							atk_res_info.pos_y = pi.pos_y;
							pi.is_living = false;

							desk->draw_plane(pos_index, atk_res_info.inj_plane_type, pi.pos_x, pi.pos_y, atk_res_info.inj_plane_dir, 0);
						}
						else if (*(desk->map_value(next_client_pos, ai.pos_x, ai.pos_y)) > 0)
						{
							atk_res_info.atk_result = ONE_ATK;
							atk_res_info.atk_plane_type = ai.plane_t;
							
							atk_res_info.atk_pos_index = pos_index;
							atk_res_info.inj_pos_index = next_client_pos;

							atk_res_info.pos_x = pi.pos_x;
							atk_res_info.pos_y = pi.pos_y;

							*(desk->map_value(next_client_pos, ai.pos_x, ai.pos_y)) = 0;

							all_plane_is_dead = false;
						}
						else
						{
							atk_res_info.atk_result = ATK_NOTHING;
							all_plane_is_dead = false;
						}

						//
						attack_results[pos_index].push_back(atk_res_info);
					}
				}
			}
		}

		//当玩家发出攻击的命令
		//     高   低        高   低
		//ATK xxxx xxxx     xxxx xxxx
		//   空位 攻击类型    Y位置 X位置
		//3B   1 BYTE        1 BYTE
 		void plane_plugin::on_client_attack_handle(game_client* client, command* cmd)
		{
			//ATK TYPE, ATK POS
			plane_game_info* game_info = (plane_game_info*)client->get_game_info();
			plane_desk* desk = game_info->get_desk();

			if (desk == NULL)
				return;

			boost::recursive_mutex::scoped_lock lock(desk->status_lock());
			if (desk->get_desk_status() != plane_desk::DESK_STATUS_RUNNING)
				return;

			char* attach_data = cmd->params(0);
			int data_len = std::strlen(attach_data);

			if ((data_len % 2) != 0)
				return;

			int client_pos = desk->get_client_pos(client);
			if (client_pos == -1)
				return;

			int attach_count = data_len / 2;

			int spy_pow_count = 0;
			int fighter_pow_count = 0;
			int bomber_pow_count = 0;

			vector<attack_info>& attacks = *desk->pos_client_attacks(client_pos);
			vector<plane_info>& client_planes = *desk->pos_client_planes(client_pos);

			for (int i = 0; i < client_planes.size(); i++)
			{
				switch (client_planes.at(i).plane_t)
				{
				case plane_type::BOMBER:
					bomber_pow_count += 2;
					break;

				case plane_type::FIGHTER:
					fighter_pow_count += 1;
					break;

				case plane_type::SPY:
					spy_pow_count += 1;
					break;
				}
			}

			bool is_pow_ok = true;
			for (int i = 0; i < attach_count; i++)
			{
				int plane_type = attach_data[i] & 15;
				int x_pos = attach_data[i + 1] & 15;
				int y_pos = attach_data[i + 1] >> 4;

				switch (plane_type)
				{
				case plane_type::SPY:
					if (--spy_pow_count >= 0)
						attacks.push_back(attack_info(plane_type::SPY, x_pos, y_pos));
					else
						is_pow_ok = false;
					break;

					break;

				case plane_type::FIGHTER:
					if (--fighter_pow_count >= 0)
						attacks.push_back(attack_info(plane_type::FIGHTER, x_pos, y_pos));
					else
						is_pow_ok = false;
					break;

				case plane_type::BOMBER:
					if (--bomber_pow_count >= 0)
						attacks.push_back(attack_info(plane_type::BOMBER, x_pos, y_pos));
					else
						is_pow_ok = false;

				default:
					is_pow_ok = false;
					break;
				}

				if (is_pow_ok == false)
				{
					attacks.clear();
					break;
				}
			}

			if (is_pow_ok == false)
				return;

			int next_client_pos = (client_pos + 1) % plane_desk::DESK_CAPACITY;

			if (desk->pos_client_attacks(next_client_pos)->size() > 0)
			{
				this->on_game_attack_result(desk);
			}
		}

		void plane_plugin::on_client_bye_handle(game_client* client, command* cmd)
		{

		}
	}
}
