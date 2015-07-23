#ifndef __ASK_ROOM_H__
#define __ASK_ROOM_H__

#include <vector>
#include "../service/game_client.h"
#include "questions.h"

using namespace dooqu_server::service;

namespace dooqu_server
{
	namespace plugins
	{
		class ask_room
		{
		public:
			enum{ ROOM_CAPACITY = 8 };


		protected:
			game_client* clients_[ROOM_CAPACITY];
			bool is_running_;
			bool is_team_mode_;
			vector<question*> questions_;
			int curr_pos_index_;
			int room_id_;

		public:
			ask_room(int room_id) : 
				is_running_(false), 
				is_team_mode_(false),
				curr_pos_index_(-1),
				room_id_(room_id)
			{
				std::memset(clients_, 0, sizeof(clients_));
			}

			void set_pos_client(unsigned int index, game_client* client)
			{
				this->clients_[index] = client;
			}

			game_client* get_pos_client(unsigned int index)
			{
				return this->clients_[index];
			}

			void move_next()
			{
				if (this->is_team_mode_)
				{

				}
				else
				{

				}
			}
		};
	}
}
#endif