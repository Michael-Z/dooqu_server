#ifndef __DDZ_GAME_INFO_H__
#define __DDZ_GAME_INFO_H__

#include "..\service\game_info.h"
#include "ddz_desk.h"
#include "..\service\char_key_op.h"
#include "..\service\timer.h"
#include <set>

//#define get_game_info(game_info_) (ddz_game_info*)game_info_

namespace dooqu_server
{
	namespace ddz
	{
		using namespace dooqu_server::service;

		class ddz_game_info : public game_info
		{			
		protected:
			friend class ddz_plugin;

			//��ҵ���Ϸ�������
			int money_;

			//��ҵ���Ϸ����ֵ
			int experience_;


			int money_modify_;


			int experience_modify_;


			int win_count_;


			int fail_count_;


			int win_count_modify_;


			int fail_count_modify_;


			//��ҵ�ǰ���ڵ���Ϸ����
			ddz_desk* desk_;

			//��ҽ������Ӻ��Ƿ��Ѿ�׼��
			bool is_ready_;

			bool is_robot_mode_;

			//��ҽ������ӵ�ʱ���
			timer in_desk_timer_;

			//long verify_code_;

		public:
			ddz_game_info();
			virtual ~ddz_game_info();
			int bit_count;
			long verify_code;
			ddz_desk* get_desk(){ return this->desk_;}
			int money(){ return this->money_;}
			int experience(){ return this->experience_;}	
			int win_count(){return this->win_count_;}
			int fail_count(){return this->fail_count_;}
			bool is_ready(){ return this->is_ready_;}
			void reset(bool leave_desk);
			void robot_mode(bool in_robot);
			bool is_robot_mode();
			void update(int money_modi, int experience_modi, int win_modi, int fail_modi);
		};
	}
}
#endif