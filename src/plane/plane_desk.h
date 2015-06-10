#ifndef __PLANE_DESK_H__
#define __PLANE_DESK_H__

#include <boost\thread\recursive_mutex.hpp>
#include "../service/game_client.h"

using namespace dooqu_server::service;

namespace dooqu_server
{
	namespace plane
	{
		enum plane_type
		{			
			UNKNOWN_TYPE = 0,
			SPY = 1,
			FIGHTER = 2,
			BOMBER = 3
		};

		enum plane_dir
		{
			UNKNOWN_DIR = 0,
			UP = 1,
			RIGHT = 2,
			DOWN = 3,
			LEFT = 4
		};

		enum attack_result
		{
			ATK_NOTHING = 0,
			HEAD_ATK = 1,
			ONE_ATK = 2,
			FIND_ENEMY = 3			
		};


		struct attack_info
		{
			plane_type plane_t;
			unsigned int pos_x;
			unsigned int pos_y;

			attack_info(plane_type p_type, unsigned int x, unsigned int y) : plane_t(p_type), pos_x(x), pos_y(y)
			{
			}
		};



		struct plane_info : public attack_info
		{
			plane_dir dir;
			bool is_living;


			plane_info(plane_type p_type, plane_dir p_dir, unsigned int x, unsigned int y) : 
				attack_info(p_type, x, y)
			{
				is_living = true;
			}

			plane_info(const plane_info& info) : attack_info(info)
			{
				this->dir = info.dir;
				is_living = info.is_living;
			}

			plane_info& operator = (const plane_info& info)
			{
				this->plane_t = info.plane_t;
				this->dir = info.dir;
				this->pos_x = info.pos_x;
				this->pos_y = info.pos_y;
				is_living = info.is_living;
			}

			void reset(plane_type p_type, plane_dir p_dir, unsigned int x, unsigned int y)
			{
				this->plane_t = plane_t;
				this->dir = p_dir;
				this->pos_x = x;
				this->pos_y = y;
				is_living = true;
			}
		};


		//         低位        高位           低位      高位        低位         高位    低位     高位
		//             +1byte+                  +1byte+              +1byte+           +1byte+
		//         4bit        4bit         4bit      4bit       4bit        4bit    4bit   4bit
		//ATR   攻击者ID    被攻击者ID    攻击飞机类型   轰炸结果   被攻击飞机类型    方向    x_pos   y_pos
		//ATR     xxxx       xxxx           xxxx      xxxx       xxxx        xxxx     xxxx    xxxx
		struct attack_result_info
		{
			int atk_pos_index;
			int inj_pos_index;

			plane_type atk_plane_type;
			plane_type inj_plane_type;

			plane_dir inj_plane_dir;
			attack_result atk_result;			

			int pos_x;
			int pos_y;

			attack_result_info()
			{
				atk_pos_index = 0;
				inj_pos_index = 0;

				atk_plane_type = UNKNOWN_TYPE;
				inj_plane_type = UNKNOWN_TYPE;

				inj_plane_dir = UNKNOWN_DIR;

				atk_result = ATK_NOTHING;
				pos_x = 0;  
				pos_y = 0;
			}
		};



		class plane_desk
		{
		public:
			int verify_code = 0;
			enum{ DESK_CAPACITY = 2, MAP_WIDTH = 9, MAP_HEIGHT = 9, DESK_ID_LEN = 5 };
			enum{DESK_STATUS_WAITTING = 0, DESK_STATUS_READY_MAP = 1, DESK_STATUS_RUNNING = 2};
		protected:
			//激活时间戳
			tick_count last_actived_time_;
			game_client* clients_[DESK_CAPACITY];
			unsigned char maps_[DESK_CAPACITY][MAP_WIDTH][MAP_HEIGHT];
			vector<plane_info> client_planes[DESK_CAPACITY];
			vector<attack_info> client_attacks[DESK_CAPACITY];
			char desk_id[DESK_ID_LEN];
			//bool is_running_;
			int desk_status_;
			boost::recursive_mutex state_lock_;
		public:
			plane_desk(int id)
			{
				memset(desk_id, 0, sizeof(desk_id));
				sprintf(desk_id, "%d", id);

				this->reset_maps(0);
				this->reset_maps(1);

				this->desk_status_ = DESK_STATUS_WAITTING;

				std::memset(clients_, 0, sizeof(game_client*)* DESK_CAPACITY);
				//printf("desk's info is %d", this->client_planes[0].size());
			}

			vector<attack_info>* pos_client_attacks(int client_pos)
			{
				if (client_pos < 0 || client_pos > DESK_CAPACITY)
					return NULL;

				return &(this->client_attacks[client_pos]);
			}

			vector<plane_info>* pos_client_planes(int client_pos)
			{
				if (client_pos < 0 || client_pos > DESK_CAPACITY)
					return NULL;

				return &(this->client_planes[client_pos]);
			}

			int get_client_pos(game_client* client)
			{
				if (this->pos_client(0) == client)
					return 0;
				else if (this->pos_client(1) == client)
					return 1;
				return -1;
			}

			void write_to_all(char* message)
			{
				this->pos_client(0)->write(message);
				this->pos_client(1)->write(message);
			}

			void try_write_to_all(char* message)
			{
				for (int i = 0; i < plane_desk::DESK_CAPACITY; ++i)
				{
					game_client* client = this->pos_client(i);
					if (client != NULL)
						client->write(message);
				}

			}

			char* get_desk_id(){ return &this->desk_id[0]; }

			void reset_maps(int client_pos){ memset(maps_[client_pos], 0, sizeof(maps_[client_pos])); }

			int get_desk_status(){ return this->desk_status_; }

			void set_desk_status(int status){ this->desk_status_ = status; }

			game_client* pos_client(int pos_index){ return this->clients_[pos_index]; }

			void set_pos_client(int pos_index, game_client* client){ this->clients_[pos_index] = client; }

			boost::recursive_mutex& status_lock(){ return this->state_lock_; }

			bool set_plane(int client_index, plane_info& info)
			{
				return set_plane(client_index, info.plane_t, info.pos_x, info.pos_y, info.dir);
			}

			unsigned char* map_value(int client_index, int pos_x, int pos_y)
			{
				return &this->maps_[client_index][pos_x][pos_y];
			}

			bool is_full()
			{
				return this->pos_client(0) != NULL && this->pos_client(1) != NULL;
			}

			void active()
			{
				this->last_actived_time_.restart();
			}

			long long elapsed_active_time(){ return this->last_actived_time_.elapsed(); }

			bool set_plane(int client_index, plane_type type, unsigned int posx, unsigned posy, plane_dir dir)
			{
				switch (type)
				{
				case plane_type::BOMBER:
					switch (dir)
					{
					case plane_dir::UP:
						if (posx - 2 < 0 || (MAP_WIDTH - posx) < 3 || (MAP_HEIGHT - posy) < 4)
							return false;

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx][posy + 1]
							|| maps_[client_index][posx][posy + 2]
							|| maps_[client_index][posx][posy + 3]
							|| maps_[client_index][posx - 1][posy + 1]
							|| maps_[client_index][posx - 2][posy + 1]
							|| maps_[client_index][posx][posy + 1]
							|| maps_[client_index][posx + 1][posy + 1]
							|| maps_[client_index][posx + 2][posy + 1]
							|| maps_[client_index][posx - 1][posy + 3]
							|| maps_[client_index][posx + 1][posy + 3])
						{
							return false;
						}

						break;
					case plane_dir::RIGHT:
						if (posx >= MAP_WIDTH || posx < 3 || posy < 2 || (MAP_HEIGHT - posy) < 3)
							return false;

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx - 1][posy]
							|| maps_[client_index][posx - 2][posy]
							|| maps_[client_index][posx - 3][posy]
							|| maps_[client_index][posx - 1][posy - 1]
							|| maps_[client_index][posx - 1][posy - 2]
							|| maps_[client_index][posx - 1][posy + 1]
							|| maps_[client_index][posx - 1][posy + 2]
							|| maps_[client_index][posx - 3][posy - 1]
							|| maps_[client_index][posx - 3][posy + 1])
						{
							return false;
						}

						break;

					case plane_dir::DOWN:
						if (posy >= MAP_HEIGHT || posy < 3 || posx < 2 || (MAP_WIDTH - posx) < 2)
							return false;

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx][posy - 1]
							|| maps_[client_index][posx][posy - 2]
							|| maps_[client_index][posx][posy - 3]
							|| maps_[client_index][posx - 1][posy - 1]
							|| maps_[client_index][posx - 2][posy - 1]
							|| maps_[client_index][posx + 1][posy - 1]
							|| maps_[client_index][posx + 2][posy - 1]
							|| maps_[client_index][posx + 1][posy - 3]
							|| maps_[client_index][posx - 1][posy - 3])
						{
							return false;
						}

						break;

					case plane_dir::LEFT:
						if (posx < 0 || (posx + 4) > MAP_WIDTH || posy < 2 || (posy + 3) > MAP_HEIGHT)
						{
							return false;
						}

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx + 1][posy]
							|| maps_[client_index][posx + 2][posy]
							|| maps_[client_index][posx + 3][posy]
							|| maps_[client_index][posx + 1][posy + 1]
							|| maps_[client_index][posx + 1][posy + 2]
							|| maps_[client_index][posx + 1][posy - 1]
							|| maps_[client_index][posx + 1][posy - 2]
							|| maps_[client_index][posx + 3][posy + 1]
							|| maps_[client_index][posx + 3][posy - 1])
						{
							return false;
						}
						break;

					default:
						return false;
						break;
					}
					break;

				case plane_type::FIGHTER:
					switch (dir)
					{
					case plane_dir::UP: //向上
						if (posx < 1 || (posx + 1) >= MAP_WIDTH || (posy + 4) > MAP_HEIGHT)
							return false;

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx][posy + 1]
							|| maps_[client_index][posx][posy + 2]
							|| maps_[client_index][posx][posy + 3]
							|| maps_[client_index][posx - 1][posy + 2]
							|| maps_[client_index][posx + 1][posy + 2])
						{
							return false;
						}

						break;
					case plane_dir::RIGHT: //向右
						if (posx < 3 || posx >= MAP_WIDTH || posy < 1 || (posy + 1) >= MAP_HEIGHT)
						{
							return false;
						}

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx - 1][posy]
							|| maps_[client_index][posx - 2][posy]
							|| maps_[client_index][posx - 3][posy]
							|| maps_[client_index][posx - 2][posy - 1]
							|| maps_[client_index][posx - 2][posy + 1])
						{
							return false;
						}

						break;
					case plane_dir::DOWN: //向下
						if (posy < 3 || posy >= MAP_HEIGHT || posx < 1 || (posx + 1) >= MAP_WIDTH)
						{
							return false;
						}

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx][posy - 1]
							|| maps_[client_index][posx][posy - 2]
							|| maps_[client_index][posx][posy - 3]
							|| maps_[client_index][posx - 1][posy - 2]
							|| maps_[client_index][posx + 1][posy - 2])
						{
							return false;
						}

						break;
					case plane_dir::LEFT: //向左
						if ((posx + 4) > MAP_WIDTH || posy < 1 || (posy + 1) >= MAP_HEIGHT)
						{
							return true;
						}

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx + 1][posy]
							|| maps_[client_index][posx + 2][posy]
							|| maps_[client_index][posx + 3][posy]
							|| maps_[client_index][posx][posy]
							|| maps_[client_index][posx + 2][posy - 1]
							|| maps_[client_index][posx + 2][posy + 1])
						{
							return false;
						}

						break;
					default:
						return false;
						break;
					}
					break;

				case plane_type::SPY:
					switch (dir)
					{
					case plane_dir::UP://向上
						if (posx < 0 || posx >= MAP_WIDTH || posy < 0 || (posy + 3) > MAP_HEIGHT)
						{
							return false;
						}

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx][posy + 1]
							|| maps_[client_index][posx][posy + 2])
						{
							return false;
						}

						break;

					case plane_dir::RIGHT://向右
						if (posx < 2 || posx >= MAP_WIDTH || posy < 0 || posy >= MAP_HEIGHT)
						{
							return false;
						}

						break;

					case plane_dir::DOWN://向下
						if (posx < 0 || posx >= MAP_WIDTH || posy >= MAP_HEIGHT || posy < 2)
						{
							return false;
						}

						if (maps_[client_index][posx][posy]
							|| maps_[client_index][posx][posy - 1]
							|| maps_[client_index][posx][posy - 2])
						{
							return false;
						}

						break;

					case plane_dir::LEFT:
						if (posx < 0 || (posx + 3) > MAP_WIDTH || posy < 0 || posy > MAP_HEIGHT)
						{
							return false;
						}

						break;

					default:
						return false;
						break;
					}
					break;

				default:
					return false;
				}

				draw_plane(client_index, type, posx, posy, dir, 2);
				return true;
			}

			void draw_plane(int client_index, plane_type type, unsigned int posx, unsigned posy, plane_dir dir, int map_val)
			{
				switch (type)
				{
				case plane_type::BOMBER:
					switch (dir)
					{
					case plane_dir::UP:

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx][posy + 1] = map_val;
						maps_[client_index][posx][posy + 2] = map_val;
						maps_[client_index][posx][posy + 3] = map_val;
						maps_[client_index][posx - 1][posy + 1] = map_val;
						maps_[client_index][posx - 2][posy + 1] = map_val;
						maps_[client_index][posx + 1][posy + 1] = map_val;
						maps_[client_index][posx + 2][posy + 1] = map_val;
						maps_[client_index][posx - 1][posy + 3] = map_val;
						maps_[client_index][posx + 1][posy + 3] = map_val;

						break;
					case plane_dir::RIGHT:

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx - 1][posy] = map_val;
						maps_[client_index][posx - 2][posy] = map_val;
						maps_[client_index][posx - 3][posy] = map_val;
						maps_[client_index][posx - 1][posy - 1] = map_val;
						maps_[client_index][posx - 1][posy - 2] = map_val;
						maps_[client_index][posx - 1][posy + 1] = map_val;
						maps_[client_index][posx - 1][posy + 2] = map_val;
						maps_[client_index][posx - 3][posy - 1] = map_val;
						maps_[client_index][posx - 3][posy + 1] = map_val;

						break;

					case plane_dir::DOWN:

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx][posy - 1] = map_val;
						maps_[client_index][posx][posy - 2] = map_val;
						maps_[client_index][posx][posy - 3] = map_val;
						maps_[client_index][posx - 1][posy - 1] = map_val;
						maps_[client_index][posx - 2][posy - 1] = map_val;
						maps_[client_index][posx + 1][posy - 1] = map_val;
						maps_[client_index][posx + 2][posy - 1] = map_val;
						maps_[client_index][posx + 1][posy - 3] = map_val;
						maps_[client_index][posx - 1][posy - 3] = map_val;

						break;

					case plane_dir::LEFT:

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx + 1][posy] = map_val;
						maps_[client_index][posx + 2][posy] = map_val;
						maps_[client_index][posx + 3][posy] = map_val;
						maps_[client_index][posx + 1][posy + 1] = map_val;
						maps_[client_index][posx + 1][posy + 2] = map_val;
						maps_[client_index][posx + 1][posy - 1] = map_val;
						maps_[client_index][posx + 1][posy - 2] = map_val;
						maps_[client_index][posx + 3][posy + 1] = map_val;
						maps_[client_index][posx + 3][posy - 1] = map_val;
						break;
					}
					break;

				case plane_type::FIGHTER:
					switch (dir)
					{
					case plane_dir::UP: //向上

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx][posy + 1] = map_val;
						maps_[client_index][posx][posy + 2] = map_val;
						maps_[client_index][posx][posy + 3] = map_val;
						maps_[client_index][posx - 1][posy + 2] = map_val;
						maps_[client_index][posx + 1][posy + 2] = map_val;

						break;
					case plane_dir::RIGHT: //向右

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx - 1][posy] = map_val;
						maps_[client_index][posx - 2][posy] = map_val;
						maps_[client_index][posx - 3][posy] = map_val;
						maps_[client_index][posx - 2][posy - 1] = map_val;
						maps_[client_index][posx - 2][posy + 1] = map_val;

						break;
					case plane_dir::DOWN: //向下

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx][posy - 1] = map_val;
						maps_[client_index][posx][posy - 2] = map_val;
						maps_[client_index][posx][posy - 3] = map_val;
						maps_[client_index][posx - 1][posy - 2] = map_val;
						maps_[client_index][posx + 1][posy - 2] = map_val;

						break;
					case plane_dir::LEFT: //向左

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx + 1][posy] = map_val;
						maps_[client_index][posx + 2][posy] = map_val;
						maps_[client_index][posx + 3][posy] = map_val;
						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx + 2][posy - 1] = map_val;
						maps_[client_index][posx + 2][posy + 1] = map_val;

						break;
					}
					break;

				case plane_type::SPY:
					switch (dir)
					{
					case plane_dir::UP://向上

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx][posy + 1] = map_val;
						maps_[client_index][posx][posy + 2] = map_val;

						break;

					case plane_dir::RIGHT://向右

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx - 1][posy] = map_val;
						maps_[client_index][posx - 2][posy] = map_val;

						break;

					case plane_dir::DOWN://向下

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx][posy - 1] = map_val;
						maps_[client_index][posx][posy - 2] = map_val;

						break;

					case plane_dir::LEFT:

						maps_[client_index][posx][posy] = map_val;
						maps_[client_index][posx + 1][posy] = map_val;
						maps_[client_index][posx + 2][posy] = map_val;

						break;
					}
					break;
				}

				for (int y = 0; y < MAP_WIDTH; y++)
				{
					for (int x = 0; x < MAP_HEIGHT; x++)
					{
						if (maps_[0][x][y])
						{
							printf("■");
						}
						else
						{
							printf("□");
						}
					}

					printf("\n");
				}
			}
		};
	}
}

#endif