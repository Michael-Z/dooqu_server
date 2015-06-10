#ifndef __DDZ_DESK_H__
#define __DDZ_DESK_H__

//#include <boost\thread\mutex.hpp>
#include <boost\thread\recursive_mutex.hpp>
#include <boost\noncopyable.hpp>
#include <set>
#include "poker_info.h"
#include "..\service\game_client.h"
#include "..\service\char_key_op.h"
#include "..\service\timer.h"

namespace dooqu_server
{
	namespace ddz
	{
		using namespace dooqu_server::service;

		typedef std::set<const char*, char_key_op> pos_poker_list;		

		class ddz_desk : boost::noncopyable
		{
		public:
			friend class ddz_plugin;
			enum{DESK_CAPACITY = 3};


		protected:
			char desk_id_[4];


			//����״̬
			bool is_running_;


			//��ǰ���Ƶ����
			game_client* curr_shower_;


			//��ǰ���Ϊ���������
			game_client* landlord_;


			//��ʱ����
			game_client* temp_landlord;


			//�������
			game_client* clients_[DESK_CAPACITY];


			//����һ�ѳ�������
			poker_info curr_poker_;


			//
			int ApplyHolderIndex;


			//����ʱ���
			timer last_actived_timer_;


			//����״̬��
			boost::recursive_mutex status_lock_;


			//��¼ը�����͵ĳ��ִ���
			int bomb_count_;


			//��¼���ƵĴ���
			int declare_count_;
			


			int doublejoker_count_;


			//
			bool HasSendApplyCMD;


			//��ǰ�����
			int curr_pos_;


			//��ʼ�ĵ�һ����ʼ���
			int first_pos_;


			static const char* DESK_POKERS[54];


			const char* desk_pokers_[54];


			pos_poker_list* pos_pokers_[DESK_CAPACITY];

			//ׯ�ҵ�λ��
			int bid_pos_;

			//������Ϸ�ı���
			int multiple_;

			//�е�������Ϣ
			int bid_info[DESK_CAPACITY];


			

		public:
			static const char* POS_STRINGS[];

			int verifi_code;


			ddz_desk(int desk_id);


			virtual ~ddz_desk();

			//��ǰ������ҵ�λ��
			int curr_pos(){ return this->curr_pos_; }

			game_client* get_landlord();

			void set_landlord(game_client*);

			void active();

			//���ӵ�ǰ��Ϸ����Ϸ����
			void increase_multiple();

			boost::recursive_mutex& status_mutex(){return this->status_lock_;}

			//��ȡ��ǰ��Ϸ���ӵ�id
			char* id(){ return &this->desk_id_[0];}

			//��ȡ�����ϵ����ָ�����飬Ĭ��ָ��ָ��λ������Ϊ0�����
			game_client* clients(){return this->clients_[0];}

			//�����ƶ�λ�������������Ϣ
			void set(int pos_index, game_client* client);


			//���õ�һ����ҵ���ʼλ��
			void set_start_pos(int pos_index);


			//����ָ����νλ�����Ƿ�û���������
			bool is_null(int pos_index);


			//��ǰ��Ϸ�����Ƿ�����Ϸ��
			bool is_running(){return this->is_running_;}


			//��ȡ��ֵ������ҵ�λ��
			int bid_pos(){return this->bid_pos_;}


			//�������ƶ�����һ����ֵ������ҵ�λ���ϣ�������������λ��
			int move_to_next_bid_pos(){this->bid_pos_ = ++this->bid_pos_ % DESK_CAPACITY; return this->bid_pos_;}

			//������Ϸ�����Ƿ�����Ϸ��״̬���ú���ֻ�޸�״ֵ̬
			void set_running(bool is_running);

			//���������λ���������б�
			void clear_pos_pokers();

			//������ҷ���
			void allocate_pokers();

			//�����Ϸ�����κ�һ��λ�����Ϊ�գ��Ƿ��ظ�λ�����������򷵻�-1
			int any_null();

			//��ȡָ������������ϵ�λ������
			int pos_index_of(game_client* client);

			//��ȡ���ƵĴ���
			int declare_count();

			//��ȡָ������λ�������Ŀǰ���������
			pos_poker_list* pos_pokers(int pos_index);

			//��ȡ�ƶ�����λ�õ������Ϣ
			game_client* pos_client(int pos_index);

			//��ǰ��Ϸ������ֵ�ĵ�һ�����
			game_client* first();	


			//��ȡ��ǰ��Ϸ������ֵ�ĵ�ǰ���
			game_client* current();	


			//��ȡ��ǰ��Ϸ���ӽ�����ҵ���һ�����
			game_client* next();


			//��ȡ��ǰ��Ϸ���ӵ����һ���������
			game_client* last();


			//��ȡ��ǰ������ҵ�ǰһ�����
			game_client* previous();


			//����ֵ��������һ������ƶ�
			void move_next();

			//��ȡ��ǰ��ֵ�������һ������
			int get_next_pos_index();


			long long elapsed_active_time(){return this->last_actived_timer_.elapsed();}


			//��ȡ��ǰ���ӵı���
			int multiple(){return this->multiple_;}


			void write_to_everyone(char* message);


			void reset();


		};
	}
}

#endif