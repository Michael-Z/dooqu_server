#include "plane_game_info.h"

namespace dooqu_server
{
	namespace plane
	{
		plane_game_info::plane_game_info()
		{
			money_ = 0;

			experience_ = 0;


			money_modify_ = 0;


			experience_modify_ = 0;


			win_count_ = 0;


			fail_count_ = 0;


			win_count_modify_ = 0;


			fail_count_modify_ = 0;


			desk_ = NULL;

			is_ready_ = false;

			is_robot_mode_ = false;
		}

		plane_game_info::~plane_game_info()
		{
			printf("~plane_game_info");
		}

		void plane_game_info::reset(bool leave_desk)
		{

		}
	}
}
