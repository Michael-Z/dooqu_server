// ddz_plugin.cpp : ���� DLL Ӧ�ó���ĵ���������
//

#include <assert.h>
#include <stdio.h>
#include "ddz_plugin.h"
#include "../service/game_service.h"



//#define SERVER_STATUS_DEBUG

namespace dooqu_server
{
	namespace ddz
	{
		void log(char* content)
		{
			FILE* fp = fopen("error.log", "at");

			if (fp != NULL)
			{
				fputs(content, fp);
				fputc((int)'\n', fp);

				fclose(fp);
			}
		}

		const char* ddz_plugin::POS_STRINGS[3] = { "0", "1", "2" };

		ddz_plugin::ddz_plugin(game_service* service, char* game_id, char* title, double frequence, int desk_count, int game_multiple, int max_waiting_duration) : game_plugin(service, game_id, title, frequence)//, desk_count_(desk_count), multiple_(multiple), /*timer_(service->get_io_service()),*/ max_waiting_duration_(21000)
		{
			this->desk_count_ = desk_count;
			this->multiple_ = game_multiple;
			this->max_waiting_duration_ = max_waiting_duration;
		}

		ddz_plugin::~ddz_plugin()
		{
			//������Ϸ������Դ
		}


		void ddz_plugin::on_load()
		{
			this->regist_handle("IDK", make_handler(ddz_plugin::on_join_desk_handle));
			this->regist_handle("RDY", make_handler(ddz_plugin::on_client_ready_handle));
			this->regist_handle("BID", make_handler(ddz_plugin::on_client_bid_handle));
			this->regist_handle("DLA", make_handler(ddz_plugin::on_client_declare_handle));
			this->regist_handle("CAD", make_handler(ddz_plugin::on_client_card_handle));
			this->regist_handle("MSG", make_handler(ddz_plugin::on_client_msg_handle));
			this->regist_handle("PNG", make_handler(ddz_plugin::on_client_ping_handle));
			this->regist_handle("RBT", make_handler(ddz_plugin::on_client_robot_handle));
			this->regist_handle("BYE", make_handler(ddz_plugin::on_client_bye_handle));

			game_plugin::on_load();
		}


		void ddz_plugin::on_unload()
		{
			this->remove_all_handles();
			{
				boost::recursive_mutex::scoped_lock lock(this->clients_lock_);

				for (game_client_map::iterator curr_client_pair = this->clients_.begin();
					curr_client_pair != this->clients_.end();)
				{
					game_client* curr_client = (curr_client_pair++)->second;

					curr_client->disconnect(service_error::SERVER_CLOSEING);

					//this->remove_client(curr_client);
				}
			}

			game_plugin::on_unload();
		}


		void ddz_plugin::on_update()
		{
			if (this->is_onlined() == false)
				return;

			for (int i = 0; i < this->desks_.size(); ++i)
			{
				ddz_desk* desk = this->desks_.at(i);
				boost::recursive_mutex::scoped_lock lock(desk->status_mutex());

				if (desk->is_running() == true)
				{
					if (desk->elapsed_active_time() > this->max_waiting_duration_)
					{
						if (desk->current() == NULL)
							continue;

						if (desk->get_landlord() == NULL)
						{
							this->on_client_bid(desk->current(), desk, false);
						}
						else
						{
							//return;
							int pos_index = desk->pos_index_of(desk->current());

#ifdef SERVER_STATUS_DEBUG
							if (pos_index == -1)
							{
								log("110");
							}
							assert(pos_index != -1);
#endif

							if (pos_index == -1)
								continue;

							//��������˿�
							if (desk->pos_pokers(pos_index)->size() > 0)
							{
								if (desk->curr_poker_.type == poker_info::ZERO)
								{
									char command_buffer[7];
									command_buffer[sprintf(command_buffer, "CAD %s", (*desk->pos_pokers(pos_index)->begin()))] = 0;
									desk->current()->simulate_on_command(command_buffer, false);
								}
								else
								{
									desk->current()->simulate_on_command("CAD 0", true);
								}
							}
						}
					}
				}
				else
				{
					//��Ϸ�������û�����У���Ҫ�������������¶�һֱ��׼�����������
				}
			}
			game_plugin::on_update();
		}


		int ddz_plugin::on_befor_client_join(game_client* client)
		{
			//��������
			if (this->clients_.size() >= this->desk_count_ * ddz_desk::DESK_CAPACITY)
			{
				return service_error::GAME_IS_FULL;
			}

			return game_plugin::on_befor_client_join(client);
		}


		void ddz_plugin::on_client_join(game_client* client)
		{
			//printf("ddz_plugin::on_client_join\n");
			ddz_game_info* gameinfo = new ddz_game_info();

			gameinfo->money_ = 10000;
			gameinfo->experience_ = 10000;

			client->set_game_info(gameinfo);

			game_plugin::on_client_join(client);

			char data_buffer[BUFFER_SIZE_MIDDLE];
			int n = sprintf(data_buffer, "LOG %s %s %d", this->game_id(), client->id(), this->max_waiting_duration_);
			data_buffer[std::min(n - 1, BUFFER_SIZE_MIDDLE - 1)] = 0;

			client->write(data_buffer);
		}

		//client �뿪���ᷢ��BYE���
		//BYE������ȵ�game_plugin::on_command�У��ȴ���ddz_plugin�ж�BYE�����Ѿ�ע���handle��Ҳ����on_client_bye_handle
		//Ȼ����on_commandд������Ķ�BYE���˴����������remove_client����
		void ddz_plugin::on_client_leave(game_client* client, int code)
		{
			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();

			if (game_info != NULL)
			{
				ddz_desk* desk = game_info->get_desk();
				if (desk != NULL)
				{
					boost::recursive_mutex::scoped_lock lock(desk->status_mutex());
					this->on_client_leave_desk(client, desk, client->error_code());
				}

				client->set_game_info(NULL);
				delete game_info;
			}
			game_plugin::on_client_leave(client, code);
		}


		//����Ҽ������Ӻ���¼�
		void ddz_plugin::on_client_join_desk(game_client* client, ddz_desk* desk, int pos_index)
		{
			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();
			game_info->desk_ = desk;

			//��֪ͨ�û���������
			client->write("IDK %s %d\0", desk->id(), pos_index);

			//������û�����Ϸ����Ϣ
			//char client_in_money[BUFFER_SIZE_SMALL] = { 0 };
			//sprintf(client_in_money, "%d", game_info->money());


			//������ÿ����ҵ���Ϸ����Ϣ
			char client_curr_money[BUFFER_SIZE_SMALL];

			for (int i = 0; i < ddz_desk::DESK_CAPACITY; ++i)
			{
				if (desk->pos_client(i) == NULL)
					continue;

				ddz_game_info* curr_game_info = ((ddz_game_info*)desk->pos_client(i)->get_game_info());

				//��䵱ǰ��ҵ���Ϸ����Ϣ
				//std::memset(client_curr_money, 0, sizeof(client_curr_money));
				//sprintf(client_curr_money, "%d", curr_game_info->money());

				//��������ӵ����֪ͨ��ǰ���ӵ�����б�
				client->write("LSD %s %s %s %c %d\0", desk->pos_client(i)->id(), desk->pos_client(i)->name(), ddz_plugin::POS_STRINGS[i], (curr_game_info->is_ready()) ? '1' : '0', curr_game_info->money());

				//�����е����֪ͨ������ҵ���Ϣ
				if (desk->pos_client(i) != client)
				{
					desk->pos_client(i)->write("JDK %s %s %s %c %d\0", client->id(), client->name(), ddz_plugin::POS_STRINGS[pos_index], '0', game_info->money());
				}
			}
		}


		//������뿪���ӵ��¼�
		void ddz_plugin::on_client_leave_desk(game_client* client, ddz_desk* desk, int reaspon)
		{
			//printf("ddz_plugin::on_client_leave_desk:%s\n", client->id());
			//�ҵ�����û�����Ϸ�����ϵ�λ������
			int pos_index = desk->pos_index_of(client);

#ifdef SERVER_STATUS_DEBUG
			if (pos_index == -1)
			{
				log("242");
			}
			assert(pos_index != -1);
#endif

			if (pos_index < 0)
				return;

			//�����λ���ÿ�
			desk->set(pos_index, NULL);

			//�����һ����ߣ�֪ͨ���Ѿ��ɹ��뿪
			if (client->available())
			{
				client->write("ODK %s %s\0", desk->id(), ddz_desk::POS_STRINGS[pos_index]);
			}

			//֪ͨÿ����LDK����Ϣ
			for (int i = 0; i < ddz_desk::DESK_CAPACITY; ++i)
			{
				game_client* curr_client = desk->pos_client(i);

				if (curr_client != NULL)
				{
					curr_client->write("LDK %s %s\0", client->id(), ddz_desk::POS_STRINGS[pos_index]);
				}
			}

			((ddz_game_info*)client->get_game_info())->reset(true);

			//�����ǰ��������Ϸ�У� ��ô�����뿪���߼�
			if (desk->is_running())
			{
				desk->set_running(false);

				if (reaspon != service_error::SERVER_CLOSEING)
				{
					this->on_game_stop(desk, client, false);
					desk->reset();
				}
			}
		}


		//����Ϸ���ӿ�ʼ��һ����Ϸ
		void ddz_plugin::on_game_start(ddz_desk* desk, int verifi_code)
		{
			if (desk == NULL)
				return;

			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());

#ifdef SERVER_STATUS_DEBUG
			if (desk->verifi_code != verifi_code)
			{
				log("292");
			}

			assert(desk->verifi_code == verifi_code);
#endif

			if (desk->verifi_code != verifi_code)
			{
				//���������ӵ�״̬����
				return;
			}

			if (desk->is_running() == false || desk->any_null() != -1)
			{
				return;
			}

			//����Ѿ���������
			desk->clear_pos_pokers();
			//����������
			desk->allocate_pokers();

			for (int i = 0; i < ddz_desk::DESK_CAPACITY; ++i)
			{
				ddz_game_info* curr_game_info = (ddz_game_info*)desk->pos_client(i)->get_game_info();
				curr_game_info->reset(false);

				pos_poker_list::iterator poker_list = desk->pos_pokers(i)->begin();

				desk->pos_client(i)->write("STT %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s %s\0",
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*++poker_list),
					(*poker_list), NULL);
			}

			++desk->verifi_code;

			this->zone_->queue_task(boost::bind(&ddz_plugin::on_game_bid, this, desk, desk->verifi_code), 4300);
			//����this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_game_bid), desk, NULL, desk->verifi_code, 4300);
		}


		//�����ҷ������Ƶ���Ϣ
		void ddz_plugin::on_client_declare_handle(game_client* client, command* cmd)
		{
			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();

			if (game_info == NULL)
				return;

			ddz_desk* desk = game_info->get_desk();

			if (desk == NULL)
				return;

			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());

			if (desk->is_running() == false)
				return;

			if (game_info->is_card_declared())
				return;

			int client_pos = desk->pos_index_of(client);

			if (client_pos == -1)
				return;

			if (desk->is_status_bided())
			{
				if (desk->get_landlord() == NULL ||
					desk->get_landlord() != client ||
					desk->current() != client ||
					desk->multiple() == -1 ||
					desk->pos_pokers(client_pos)->size() != 20)
				{
					return;
				}
			}
			else
			{
				if (desk->multiple() != -1)
					return;
			}

			if (desk->declare_count() == 0)
				desk->bid_pos_ = client_pos;

			desk->declare_count_++;
			game_info->set_card_declared(true);

			char poker_buffer[66] = { 0 };
			int cp_pos = sprintf(poker_buffer, "DLA %d ", client_pos);

			pos_poker_list* poker_list = desk->pos_pokers(client_pos);
			pos_poker_list::iterator poker_finder = poker_list->begin();

			for (; poker_finder != poker_list->end(); ++poker_finder)
			{
				cp_pos += sprintf(&poker_buffer[cp_pos], "%s ", *poker_finder);
			}
			poker_buffer[59] = 0;

			desk->write_to_everyone(poker_buffer);
		}


		//����Ϸ��ʼ����
		void ddz_plugin::on_game_bid(ddz_desk* desk, int verifi_code)
		{

#ifdef SERVER_STATUS_DEBUG
			if (desk->verifi_code != verifi_code)
			{
				log("349");
			}
			assert(desk->verifi_code == verifi_code);
#endif

			if (desk->verifi_code != verifi_code)
				return;

			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());

			if (desk->is_running() == false || desk->any_null() >= 0)
			{
				return;
			}

			desk->active();

			if (desk->declare_count() == 0)
			{
				desk->move_to_next_bid_pos();
			}

			desk->set_start_pos(desk->bid_pos());

			game_client* curr_client = desk->current();

			if (curr_client != NULL)
			{
				desk->set_status_bided(true);

				char buffer[BUFFER_SIZE_SMALL];
				int n = sprintf(buffer, "BID %d %d", desk->curr_pos(), desk->multiple());
				buffer[n] = 0;

				for (int i = 0; i < ddz_desk::DESK_CAPACITY; ++i)
				{
					desk->pos_client(i)->write(buffer);
				}

				//������ģʽ�߼�������ǻ�����ģʽ����ô�Զ��С�������
				ddz_game_info* curr_game_info = (ddz_game_info*)curr_client->get_game_info();
				if (curr_game_info->is_robot_mode())
				{
					curr_client->simulate_on_command("BID 1", true);
				}
			}
		}


		//��ǰ��Ϸ���ӵĵ����Ѿ�����
		void ddz_plugin::on_game_landlord(ddz_desk* desk, game_client* landlord, int verifi_code)
		{
			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());

			//	game_client* landlord = (game_client*)landlord_;

#ifdef SERVER_STATUS_DEBUG
			if (desk->verifi_code != verifi_code)
				log("397");

			assert(desk->verifi_code == verifi_code);
#endif

			if (desk->verifi_code != verifi_code)
				return;

			int pos_index = desk->pos_index_of(landlord);

#ifdef SERVER_STATUS_DEBUG
			if (pos_index == -1)log("407");
			if (desk->is_running() == false)log("408");
			if (desk->landlord_ == NULL)log("409");
			if (desk->landlord_ != landlord)log("410");

			//pos_index != -1 ��ʵ����ȥ���ģ� ��Ϊ-1��������
			assert(pos_index != -1);
			assert(desk->is_running());
			assert(desk->landlord_ != NULL);
			assert(desk->landlord_ == landlord);
#endif

			if (desk->is_running() == false || desk->landlord_ == NULL ||
				desk->landlord_ != landlord || pos_index == -1)
				return;

			desk->set_start_pos(pos_index);

			//�ѵ��Ʒָ�����
			desk->pos_pokers(pos_index)->insert(&desk->desk_pokers_[51], &desk->desk_pokers_[54]);

			char buffer[BUFFER_SIZE_SMALL] = { 0 };
			sprintf(buffer, "LRD %s %s %s %s", ddz_desk::POS_STRINGS[pos_index],
				desk->desk_pokers_[51], desk->desk_pokers_[52], desk->desk_pokers_[53]);

			desk->write_to_everyone(buffer);

			ddz_game_info* lord_game_info = (ddz_game_info*)desk->current()->get_game_info();

			//�������Ŀǰ���ڻ�����ģʽ����ô�ӳ�1000�����Զ���������
			if (lord_game_info->is_robot_mode())
			{
				++lord_game_info->verify_code;

				this->zone_->queue_task(boost::bind(&ddz_plugin::on_robot_card, this, desk, landlord, lord_game_info->verify_code), 1000);

				//����:this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_robot_card), desk, landlord, lord_game_info->verify_code, 1000);
			}
		}



		void ddz_plugin::on_client_show_poker(game_client* client, ddz_desk* desk, command* cmd)
		{
			desk->active();
			int pos_index = desk->pos_index_of(client);

#ifdef SERVER_STATUS_DEBUG
			if (pos_index == -1)log("441");

			assert(pos_index != -1);
#endif
			if (pos_index == -1)
				return;

			char buffer[76] = { 0 };
			int p_size = sprintf(buffer, "CAD %s ", ddz_desk::POS_STRINGS[pos_index]);

			for (int i = 0; i < cmd->param_size(); ++i)
			{
				p_size += sprintf(&buffer[p_size], (i == 0) ? "%s" : ",%s", cmd->params(i));
				desk->pos_pokers(pos_index)->erase(cmd->params(i));
				//������������Ƶ��������erase��
			}

			if (desk->pos_pokers(pos_index)->size() == 0)
			{
				desk->write_to_everyone(buffer);
				desk->set_running(false);
				this->on_game_stop(desk, client, true);
				desk->reset();
			}
			else
			{
				desk->move_next();
				sprintf(&buffer[p_size], " %s,%c", ddz_desk::POS_STRINGS[desk->curr_pos()], (desk->curr_poker_.type == poker_info::ZERO) ? '1' : '0');
				desk->write_to_everyone(buffer);

				ddz_game_info* curr_game_info = (ddz_game_info*)desk->current()->get_game_info();
				//����¼Ҵ��ڻ�����ģʽ����ô1000����֮���Զ���������
				if (curr_game_info->is_robot_mode())
				{
					++curr_game_info->verify_code;

					this->zone_->queue_task(boost::bind(&ddz_plugin::on_robot_card, this, desk, desk->current(), curr_game_info->verify_code), 1000);

					//����this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_robot_card), desk, desk->current(), curr_game_info->verify_code, 1000);
				}
			}
		}


		void ddz_plugin::on_game_stop(ddz_desk* desk, game_client* client, bool normal)
		{
			//printf("\nddz_plugin::on_game_stop");

			int game_tax = 0;
			char cmd_buffer[BUFFER_SIZE_SMALL] = { 0 };

			sprintf(cmd_buffer, (normal) ? "STP 1" : "STP 0");
			desk->write_to_everyone(cmd_buffer);

			if (normal)
			{
				//�Ƚ�ʣ���������Ϣͬ����ȥ
				char buffer[BUFFER_SIZE_LARGE];
				for (int curr_index = 0; curr_index < ddz_desk::DESK_CAPACITY; ++curr_index)
				{
					pos_poker_list* pos_pokers = desk->pos_pokers(curr_index);

					//����׼��״̬
					ddz_game_info* curr_game_info = (ddz_game_info*)desk->pos_client(curr_index)->get_game_info();
					curr_game_info->is_ready_ = false;

					if (pos_pokers->size() == 0)
						continue;

					std::memset(buffer, 0, sizeof(buffer));
					std::strncpy(buffer, "LFC", 3);

					int n = 0;
					for (pos_poker_list::iterator plist = pos_pokers->begin(); plist != pos_pokers->end(); ++plist, ++n)
					{
						if (plist == pos_pokers->begin())
						{
							buffer[3] = ' ';
							buffer[4] = *ddz_desk::POS_STRINGS[curr_index];
						}

						buffer[5 + (n * 3)] = ' ';
						buffer[5 + (n * 3) + 1] = (*plist)[0];
						buffer[5 + (n * 3) + 2] = (*plist)[1];
					}

					//��������Ϣ���͸���ǰcurr_pos���¼Һ����¼�
					desk->pos_client((curr_index + 1) % 3)->write(buffer);
					desk->pos_client((curr_index + 2) % 3)->write(buffer);
				}

				//ͬ���ȷ���Ϣ
				//�׷�,�зֱ���,ը��,��ը,���� ��Ǯ,��Ǯ�䶯,����,����䶯
				char res_buffer[256];
				int n = std::sprintf(res_buffer, "RST %d,%d,%d,%d,%d", this->multiple_, desk->multiple(), desk->declare_count(), desk->bomb_count_, desk->doublejoker_count_);
				res_buffer[n] = 0;

				int total_multiple = this->multiple_ * (int)std::pow(2, (double)((desk->multiple() == -1) ? 0 : desk->multiple())) * (int)std::pow(2, (double)desk->bomb_count_) * (int)std::pow(2, (double)desk->doublejoker_count_) * (int)std::pow(3, (double)desk->declare_count());
				desk->write_to_everyone(res_buffer);

				bool is_landlord_win = (client == desk->landlord_);
				if (is_landlord_win)
				{
					//�����������Ϸ���� && ����Ӯ��
					int money_landlord_win = 0;
					for (int curr_index = 0; curr_index < ddz_desk::DESK_CAPACITY; ++curr_index)
					{
						game_client* curr_client = desk->pos_client(curr_index);
						if (curr_client == NULL || curr_client == desk->landlord_)
							continue;

						ddz_game_info* curr_game_info = (ddz_game_info*)curr_client->get_game_info();

						//int exp_lost = total_multiple;
						//��ǰ���������Ӧ�������ֵ
						int curr_money_lost = total_multiple * 2 + game_tax;

						//�����������ֵ�Ѿ�>���Ŀǰ�����Ǯ
						if (curr_money_lost > curr_game_info->money())
						{
							curr_money_lost = curr_game_info->money();
						}

						//ϵͳ���յĵ�ǰ�����ҵ�Ǯ����Ҫ�ٳ���ǰ������Ӧ�ý��ɵ�����
						int curr_money_lost_to_winner = curr_money_lost - game_tax;
						//���Ӯ��Ǯ�������Ѻ�<0��˵��Ӯ��̫���ˣ� ��ô��������Ǯ��û���ˣ������ٳ�������
						if (curr_money_lost_to_winner < 0)
						{
							curr_money_lost_to_winner = 0;
						}
						//�ۼ�����������ܽ��
						money_landlord_win += curr_money_lost_to_winner;
						//���µ�ǰ��ҵ�����
						curr_game_info->update(curr_money_lost, total_multiple, 0, 1);

						std::memset(res_buffer, 0, sizeof(res_buffer));
						//      %s       %s   %s        %d   %d       %d      %d      %d         %d        %d       %d
						//UPD clientID nick pos_index role iswin money_modi money exper_modi experience wincount failcount.
						int n = sprintf(res_buffer, "UPD %s %s %d 0 0 %d %d %d %d %d %d",
							curr_client->id(),
							curr_client->name(),
							curr_index,
							0 - curr_money_lost,
							curr_game_info->money(),
							0 - total_multiple,
							curr_game_info->experience(),
							curr_game_info->win_count(),
							curr_game_info->fail_count());

						res_buffer[n] = 0;
						desk->write_to_everyone(res_buffer);
					}

					ddz_game_info* landlord_game_info = (ddz_game_info*)desk->landlord_->get_game_info();

					money_landlord_win -= game_tax;
					if (money_landlord_win < 0 && (landlord_game_info->money() + money_landlord_win) < 0)
					{
						money_landlord_win = 0 - landlord_game_info->money();
					}

					//std::memset(res_buffer, 0, sizeof(res_buffer));
					int n = sprintf(res_buffer, "UPD %s %s %d 1 1 %d %d %d %d %d %d",
						desk->landlord_->id(),
						desk->landlord_->name(),
						desk->pos_index_of(desk->landlord_),
						money_landlord_win,
						landlord_game_info->money(),
						total_multiple,
						landlord_game_info->experience(),
						landlord_game_info->win_count(),
						landlord_game_info->fail_count());

					res_buffer[n] = 0;
					desk->write_to_everyone(res_buffer);
				}
				else
				{
					//�����������Ϸ���� && ��������
					int exp_landlord_lost = total_multiple * 2;
					int money_landlord_lost = exp_landlord_lost * 2 - game_tax;
					//�۵�����Ϸ���� + ����
					ddz_game_info* landlord_game_info = (ddz_game_info*)desk->landlord_->get_game_info();

					//assert(landlord_game_info != NULL);

					//���Ҫ�۵�Ǯ���ȵ�������Ĵ��࣬ ��ôֻ�ܰѴ��۵��㣬��Ǯ����Ϊ��
					if (money_landlord_lost > landlord_game_info->money())
					{
						money_landlord_lost = landlord_game_info->money();
					}

					//���ũ���Ҫ����Ǯ�Ļ����ϣ���ȥϵͳ���ѣ����Ҫϵͳ���Ȼ��ա�
					int money_lost_to_peasants = money_landlord_lost - game_tax;

					//������ũ���Ǯ�۵����Ѷ�<0�˵Ļ��� ��ũ��ò����֣���������ϵͳ�������ѡ�
					if (money_lost_to_peasants < 0)
					{
						money_lost_to_peasants = 0;
					}

					//���·������ڴ������е�����
					landlord_game_info->update(0 - money_landlord_lost, 0 - exp_landlord_lost, 0, 1);

					int n = sprintf(res_buffer, "UPD %s %s %d 1 1 %d %d %d %d %d %d",
						desk->landlord_->id(),
						desk->landlord_->name(),
						desk->pos_index_of(desk->landlord_),
						0 - money_landlord_lost,
						landlord_game_info->money(),
						0 - exp_landlord_lost,
						landlord_game_info->experience(),
						landlord_game_info->win_count(),
						landlord_game_info->fail_count());

					res_buffer[n] = 0;
					desk->write_to_everyone(res_buffer);


					for (int curr_index = 0; curr_index < ddz_desk::DESK_CAPACITY; ++curr_index)
					{
						game_client* curr_client = desk->pos_client(curr_index);

#ifdef SERVER_STATUS_DEBUG
						if (curr_client == NULL)
							log("646");

						assert(curr_client != NULL);
#endif

						if (curr_client == NULL || curr_client == desk->landlord_)
							continue;

						ddz_game_info* curr_game_info = (ddz_game_info*)curr_client->get_game_info();

#ifdef SERVER_STATUS_DEBUG
						if (curr_game_info == NULL)
							log("655");

						assert(curr_game_info != NULL);
#endif

						int exp_peasant_win = total_multiple;
						int money_peasant_win = money_lost_to_peasants / 2;

						//�۵����ѵ��ֵ�
						int money_peasant_hold = money_peasant_win - game_tax;
						if (money_peasant_hold < 0)
						{
							if ((curr_game_info->money() + money_peasant_hold) < 0)
							{
								money_peasant_hold = 0 - curr_game_info->money();
							}
						}

						curr_game_info->update(money_peasant_hold, exp_peasant_win, 1, 0);

						int n = sprintf(res_buffer, "UPD %s %s %d 0 0 %d %d %d %d %d %d",
							curr_client->id(),
							curr_client->name(),
							curr_index,
							money_peasant_hold,
							curr_game_info->money(),
							exp_peasant_win,
							curr_game_info->experience(),
							curr_game_info->win_count(),
							curr_game_info->fail_count());

						res_buffer[n] = 0;
						desk->write_to_everyone(res_buffer);
					}
				}
			}
			else
			{
			}
			desk->reset();
		}



		//���û������������ӵ�����
		void ddz_plugin::on_join_desk_handle(game_client* client, command* cmd)
		{
			ddz_game_info* game_info = (ddz_game_info*)(client->get_game_info());
			ddz_desk* last_desk = game_info->get_desk();

			if (last_desk != NULL)
			{
				//�ǵ�����
				boost::recursive_mutex::scoped_lock lock(last_desk->status_mutex());
				this->on_client_leave_desk(client, last_desk, service_error::CLIENT_EXIT);
			}

			for (int free_count_required = 1; free_count_required < 4; free_count_required++)
			{
				for (int i = 0; this->is_onlined() && i < this->desks_.size(); i++)
				{
					//��ȡ��ǰҪ��ѯ�����ӵ�ָ������
					ddz_desk* curr_desk = this->desks_.at(i);
					//�����ǰ�������ϴ��������ӣ���ô����
					if (curr_desk == last_desk)
					{
						continue;
					}

					//��ǰ��������
					boost::recursive_mutex::scoped_lock lock(curr_desk->status_mutex());

					//�����ǰ��Ϸ���ӵ�״̬��������Ϸ��ҲҪ����
					if (curr_desk->is_running())
					{
						continue;
					}

					int free_count = 0;
					int free_pos = -1;
					for (int pos_index = 0; pos_index < ddz_desk::DESK_CAPACITY; pos_index++)
					{
						if (curr_desk->pos_client(pos_index) == NULL)
						{
							free_count++;
							//�����ǰ���е�λ�ô���Ŀ����������ô�����������
							if (free_count > free_count_required)
							{
								break;
							}

							if (free_pos == -1)
							{
								free_pos = pos_index;
							}
						}
					}

					if (free_pos != -1 && free_count == free_count_required)
					{
						this->desks_.at(i)->set(free_pos, client);
						this->on_client_join_desk(client, curr_desk, free_pos);
						return;
					}
				}
			}

			client->write("IDK -1\0");
		}


		//���û�����׼������
		void ddz_plugin::on_client_ready_handle(game_client* client, command* cmd)
		{
			if (cmd->param_size() != 0)
				return;

			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();

			//�������Ѿ�׼���ˣ� ����
			if (game_info->is_ready())
				return;

			ddz_desk* desk = game_info->get_desk();

			//�����һ�û���뵽�κ����ӣ�����
			if (desk == NULL)
				return;

			//����
			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());

			if (desk->is_running())
				return;

			//�����λ��û�ҵ���ң�����
			int pos_index = desk->pos_index_of(client);

#ifdef SERVER_STATUS_DEBUG
			if (pos_index == -1) log("792");

			assert(pos_index != -1);
#endif

			if (pos_index == -1)
				return;

			game_info->is_ready_ = true;

			bool is_all_ready = true;
			for (int i = 0; i < ddz_desk::DESK_CAPACITY; i++)
			{
				game_client* curr_client = desk->pos_client(i);

				if (curr_client == NULL)
				{
					is_all_ready = false;
				}
				else
				{
					if (((ddz_game_info*)curr_client->get_game_info())->is_ready() == false)
					{
						is_all_ready = false;
					}
					curr_client->write("RDY %s %s\0", client->id(), ddz_desk::POS_STRINGS[pos_index]);
				}
			}

			if (is_all_ready)
			{
				//�������ӵļ���ʱ��
				desk->active();

				//�������ӵ���Ϣ
				desk->reset();

				//�趨��Ϸ״̬Ϊ��ʼ��Ϸ
				desk->set_running(true);

				if (this->zone_ != NULL)
				{
					++desk->verifi_code;

					this->zone_->queue_task(boost::bind(&ddz_plugin::on_game_start, this, desk, desk->verifi_code), 500);
					//this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_game_start), desk, NULL, desk->verifi_code, 500);
				}
			}
		}


		void ddz_plugin::on_client_bid(game_client* client, ddz_desk* desk, bool is_bid)
		{
			//���(��ǰ���Ӳ�����Ϸ״̬} || ��ǰ������û�о�λ����ô���� || ������ǵ�ǰ��ҽ���
			if (desk->is_running() == false || desk->get_landlord() != NULL || desk->current() != client)
				return;

			int pos_index = desk->pos_index_of(client);

#ifdef SERVER_STATUS_DEBUG
			if (pos_index == -1)
				log("848");

			assert(pos_index != -1);
#endif

			if (pos_index == -1)
				return;

			desk->active();
			desk->bid_info[pos_index] = (is_bid) ? 1 : -1;

			//##�����Ǹ����̶�ֵ����Ҫע�⣬��ֹ���������!##
			char buffer[BUFFER_SIZE_SMALL] = { 0 };
			//std::memset(buffer, 0, sizeof(buffer));
			int mess_size = sprintf(buffer, "RBD %s %c %d", ddz_desk::POS_STRINGS[pos_index], (is_bid) ? '1' : '0', desk->multiple());
			desk->write_to_everyone(buffer);//

			if (desk->multiple() == -1)
			{
				if (is_bid)
				{
					desk->increase_multiple();
					desk->temp_landlord = client;
					desk->set_start_pos(desk->get_next_pos_index());

					for (int curr_pos = desk->curr_pos(), count = 0; count < ddz_desk::DESK_CAPACITY; count++)
					{
						if (count < 2)
						{
							if (desk->bid_info[curr_pos] == -1)
							{
								desk->move_next();
							}
							else
							{
								std::memset(buffer, 0, sizeof(buffer));
								//sprintf(&buffer[mess_size], " %s %d", ddz_desk::POS_STRINGS[desk->curr_pos()], desk->multiple());
								sprintf(buffer, "BID %s %d", ddz_desk::POS_STRINGS[desk->curr_pos()], desk->multiple());
								desk->write_to_everyone(buffer);

								goto ROBOT_BID;
								//return;
							}
						}
						else
						{
							//desk->write_to_everyone(buffer);
							desk->landlord_ = desk->temp_landlord;
							++desk->verifi_code;

							this->zone_->queue_task(boost::bind(&ddz_plugin::on_game_landlord, this, desk, desk->landlord_, desk->verifi_code), 500);
							//����this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_game_landlord), desk, desk->landlord_, desk->verifi_code, 500);
							return;
						}
					}
				}
				else
				{
					if (desk->next() == desk->first())
					{
						//desk->write_to_everyone(buffer);
						//���û�������ƹ�
						if (desk->declare_count() == 0)
						{
							++desk->verifi_code;
							this->zone_->queue_task(boost::bind(&ddz_plugin::on_game_start, this, desk, desk->verifi_code), 500);
							//����this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_game_start), desk, NULL, desk->verifi_code, 500);
						}
						else
						{
							desk->landlord_ = desk->pos_client(desk->bid_pos());
							++desk->verifi_code;

							this->zone_->queue_task(boost::bind(&ddz_plugin::on_game_landlord, this, desk, desk->landlord_, desk->verifi_code), 500);
							//����this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_game_landlord), desk, desk->landlord_, desk->verifi_code, 500);
						}
						return;
					}
					else
					{
						desk->move_next();
						std::memset(buffer, 0, sizeof(buffer));
						sprintf(buffer, "BID %s %d", ddz_desk::POS_STRINGS[desk->curr_pos()], desk->multiple());
						desk->write_to_everyone(buffer);

						goto ROBOT_BID;
					}
				}
			}
			else
			{
				if (is_bid)
				{
					desk->increase_multiple();
					desk->temp_landlord = client;
				}

				for (int i = 0; i < 2; i++)
				{
					if (desk->next() == desk->first())
					{
						desk->landlord_ = desk->temp_landlord;
						//��Ӧ����
						//desk->write_to_everyone(buffer);
						++desk->verifi_code;

						this->zone_->queue_task(boost::bind(&ddz_plugin::on_game_landlord, this, desk, desk->landlord_, desk->verifi_code), 500);
						//this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_game_landlord), desk, desk->landlord_, desk->verifi_code, 500);
						return;
					}
					else
					{
						desk->move_next();

						if (desk->bid_info[desk->curr_pos()] == -1)
						{
							continue;
						}
						else
						{
							if (desk->current() == desk->temp_landlord)
							{
								desk->landlord_ = desk->temp_landlord;
								//desk->write_to_everyone(buffer);
								++desk->verifi_code;

								this->zone_->queue_task(boost::bind(&ddz_plugin::on_game_landlord, this, desk, desk->landlord_, desk->verifi_code), 500);

								//����this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_game_landlord), desk, desk->landlord_, desk->verifi_code, 500);

								return;
							}
							else
							{
								std::memset(buffer, 0, sizeof(buffer));
								sprintf(buffer, "BID %s %d", ddz_desk::POS_STRINGS[desk->curr_pos()], desk->multiple());
								desk->write_to_everyone(buffer);

								goto ROBOT_BID;
								//return;
							}
						}
					}
				}
			}
			return;


		ROBOT_BID:
			ddz_game_info* curr_game_info = (ddz_game_info*)desk->current()->get_game_info();

#ifdef SERVER_STATUS_DEBUG
			assert(curr_game_info != NULL);
#endif

			if (curr_game_info->is_robot_mode())
			{
				desk->current()->simulate_on_command("BID 1", true);
			}
		}


		//���û�������������
		void ddz_plugin::on_client_bid_handle(game_client* client, command* cmd)
		{
			if (cmd->param_size() != 1)
				return;

			bool is_bid = (std::strcmp(cmd->params(0), "1") == 0);
			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();
			//if(game_info->is_robot_mode())
			//	return;

			ddz_desk* desk = game_info->get_desk();
			if (desk == NULL)
				return;

			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());
			this->on_client_bid(client, desk, is_bid);
		}



		void ddz_plugin::on_client_card_handle(game_client* client, command* cmd)
		{
			if (cmd->param_size() <= 0)
				return;

			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();
			////����ǻ�����ģʽ
			//if(game_info->is_robot_mode())
			//	return;

			ddz_desk* desk = game_info->get_desk();

#ifdef SERVER_STATUS_DEBUG
			if (desk == NULL)
				log("1005");

			assert(desk != NULL);
#endif

			if (desk == NULL)
				return;

			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());
			if (desk->is_running() == false || desk->landlord_ == NULL)
			{
				return;
			}

			int pos_index = desk->pos_index_of(client);

#ifdef SERVER_STATUS_DEBUG
			if (pos_index == -1)
				log("1020");

			assert(pos_index != -1);
#endif

			if (pos_index == -1)
				return;

			if (desk->current() != client)
			{
				//������Ϊ�����ӳ٣�����ᱻ�˻�
				if (std::strcmp(cmd->params(0), "0") != 0)
				{
					this->on_client_card_refuse(client, cmd);
				}
				return;
			}

			//��������Ҫ����
			if (cmd->param_size() == 1 && std::strcmp(cmd->params(0), "0") == 0)
			{
				//�������Ϸ�ĵ�һ���ƣ��������Լ�����Ȩ���ƣ� �ǲ��������ơ��ġ�
				if (desk->curr_shower_ == NULL || desk->curr_shower_ == desk->current())
				{
					char buffer[32] = { 0 };
					sprintf(buffer, "NOT %s", client->id());
					client->write(buffer);
					return;
				}

				if (desk->next() == desk->curr_shower_)
				{
					desk->curr_poker_.zero();
				}

				this->on_client_show_poker(client, desk, cmd);
			}
			else
			{
				char poker_chars[21] = { 0 };

				bool all_finded = true;
				for (int i = 0; i < cmd->param_size(); i++)
				{
					//����Ƿ�Ϊ��ȷ������
					if (poker_parser::is_poker(cmd->params(i)) == false)
					{
						return;
					}

					//��������Ƿ�ӵ����Щ����
					pos_poker_list::iterator poker_list = desk->pos_pokers(pos_index)->find(cmd->params(i));
					if (poker_list == desk->pos_pokers(pos_index)->end())
					{
						all_finded = false;
						break;
					}

					//������ֵ��¼��char[]�У���������parserʹ��
					poker_chars[i] = cmd->params(i)[0];
				}

				if (all_finded == false)
				{
					this->on_client_card_refuse(client, cmd);
					return;
				}

				poker_info pokerinfo;
				poker_parser::parse(poker_chars, cmd->param_size(), pokerinfo);

				if (pokerinfo.type == poker_info::ERR)
				{
					this->on_client_card_refuse(client, cmd);
					return;
				}

				if (poker_parser::is_large(pokerinfo, desk->curr_poker_) == false)
				{
					this->on_client_card_refuse(client, cmd);
					return;
				}

				switch (pokerinfo.type)
				{
				case poker_info::BOMB:
					++desk->bomb_count_;
					break;

				case poker_info::DOUBLE_JOKER:
					break;
				}

				desk->curr_poker_ = pokerinfo;
				desk->curr_shower_ = client;

				this->on_client_show_poker(client, desk, cmd);
			}
		}



		void ddz_plugin::on_client_card_refuse(game_client* client, command* cmd)
		{
			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();

			ddz_desk* desk = game_info->get_desk();

			pos_poker_list* pokers = desk->pos_pokers(desk->pos_index_of(client));

			//����͹������������20�ţ�˵�����������
			if (cmd->param_size() > 20)
				return;

			char buffer[BUFFER_SIZE_LARGE] = { 0 };
			//std::memset(buffer, 0, sizeof(buffer));

			int p_size = std::sprintf(buffer, "RFS");

			for (int i = 0; i < cmd->param_size(); i++)
			{
				//Ҫ�������͹�����������Ϣ�Ƿ���ȷ���������α�죬�������׻��������
				if (poker_parser::is_poker(cmd->params(i)) == false)
					return;

				p_size += std::sprintf(&buffer[p_size], " %s", cmd->params(i));
			}
			client->write(buffer);
		}


		//
		void ddz_plugin::on_robot_card(ddz_desk* desk, game_client* client, int verify_code)
		{
			//ddz_desk* desk = (ddz_desk*)arg_desk;
			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());

			if (desk->is_running() == false)
				return;

			//	game_client* client = (game_client*)arg_client;

			if (desk->current() != client || desk->current() == NULL)
				return;

			int client_pos = desk->pos_index_of(client);

			//���δ�ҵ������
			if (client_pos == -1)
				return;

			//����˿���Ϊ��
			if (desk->pos_pokers(client_pos)->size() == 0)
				return;

			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();

#ifdef SERVER_STATUS_DEBUG
			assert(game_info->verify_code == verify_code);
#endif

			if (game_info == NULL
				|| game_info->is_robot_mode() == false
				|| game_info->verify_code != verify_code)
				return;


			//if(desk->current() == client)
			//{
			poker_array pokers_finded;
			poker_finder::find_big_pokers(desk->curr_poker_, *desk->pos_pokers(client_pos), pokers_finded);

			if (pokers_finded.size() > 0)
			{
				char buffer[BUFFER_SIZE_MAX] = { 0 };
				int n = sprintf(buffer, "CAD");

				for (int i = 0; i < pokers_finded.size(); i++)
				{
					n += sprintf(&buffer[n], " %s", pokers_finded.at(i));
				}

				client->simulate_on_command(buffer, false);
			}
			else
			{
				client->simulate_on_command("CAD 0", true);
			}
			/*	}*/
		}



		void ddz_plugin::on_client_msg_handle(game_client* client, command* cmd)
		{
			if (cmd->param_size() != 2)
				return;

			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();
			ddz_desk* desk = game_info->get_desk();

			//�����������Ϣ
			if (std::strcmp(cmd->params(0), "0") == 0)
			{
				if (desk != NULL)
				{
				}
				else
				{
				}
			}
			else if (std::strcmp(cmd->params(0), "1") == 0)
			{

			}
		}



		void ddz_plugin::on_client_ping_handle(game_client* client, command* cmd)
		{
			//�û�����PNG����ʱ��������
		}


		void ddz_plugin::on_client_robot_handle(game_client* client, command* cmd)
		{
			if (cmd->param_size() != 1)
			{
				return;
			}

			int mode = std::atoi(cmd->params(0));

			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();
			ddz_desk* desk = game_info->get_desk();
			boost::recursive_mutex::scoped_lock lock(desk->status_mutex());

			if (desk->is_running() == false)
				return;

			int client_pos = desk->pos_index_of(client);

			if (client_pos == -1)
				return;


			//RBT index ret
			char buffer[8];
			buffer[7] = 0;

			if (mode == 1 && game_info->is_robot_mode() == false)
			{
				game_info->is_robot_mode_ = true;
				sprintf(buffer, "RBT %s %d", ddz_desk::POS_STRINGS[client_pos], mode);
				desk->write_to_everyone(buffer);

				if (desk->landlord_ == NULL)
				{
					if (desk->current() == client)
					{
						client->simulate_on_command("BID 1", true);
					}
				}
				else
				{
					if (desk->current() == client)
					{
						this->on_robot_card(desk, client, game_info->verify_code);
					}
				}
				//if(desk->current() == client)
				//{
				//	this->zone_->queue_task(this, make_plugin_handle(ddz_plugin::on_robot_card), desk, client, 0, 500);
				//}
			}
			else if (mode == 0 && game_info->is_robot_mode())
			{
				//����verify_code��ʹĿǰ�Ѿ�Ͷ�ݵ��ӳ�������Ч��
				game_info->verify_code = 0;
				game_info->is_robot_mode_ = false;
				sprintf(buffer, "RBT %s %d", ddz_desk::POS_STRINGS[client_pos], mode);
				desk->write_to_everyone(buffer);
			}
		}


		//client �뿪���ᷢ��BYE���
		//BYE������ȵ�on_command�У��ȴ���ddz_plugin�ж�BYE�����Ѿ�ע���handle��Ҳ����on_client_bye_handle
		//Ȼ����on_commandд������Ķ�BYE���˴����������remove_client����
		void ddz_plugin::on_client_bye_handle(game_client* client, command* cmd)
		{
			//printf("ddz_plugin::on_client_bye_handle:%s", client->id());
			ddz_game_info* game_info = (ddz_game_info*)client->get_game_info();
			int reason = (cmd->param_size() > 0) ? std::atoi(cmd->params(0)) : service_error::CLIENT_EXIT;

			ddz_desk* desk = game_info->get_desk();
			if (desk != NULL)
			{
				boost::recursive_mutex::scoped_lock lock(desk->status_mutex());
				this->on_client_leave_desk(client, desk, client->error_code());
			}
		}

		bool ddz_plugin::need_update_offline_client(game_client* c, string& serverURL, string& path)
		{
			serverURL = "127.0.0.1";

			char c_buf[64] = { 0 };

			sprintf(c_buf, "/offline.aspx?id={%s}", c->id());

			path = c_buf;
			return true;
		}
	}
}
