#ifndef __QUESTIONS_H__
#define __QUESTIONS_H__

#include "../data/mysql_connection_pool.h"

using namespace dooqu_server::data;

namespace dooqu_server
{
	namespace plugins
	{
		struct question
		{
			char title_[200];
			char options_[5][100];
			int key_index_;

			question() :key_index_(-1)
			{
				memset(title_, 0, sizeof(title_));
				memset(options_, 0, sizeof(options_));
			}

			void set_title(const char* q_title)
			{
				strncpy(this->title_, q_title, std::min(sizeof(this->title_) - 1, sizeof(q_title)));
			}

			char* get_title()
			{
				return this->title_;
			}

			void set_option(int option_index, const char* option_str)
			{				
				strncpy(&options_[option_index][0], option_str, std::min((int)sizeof(option_str) , 100 - 1));
				printf("arg=%s\n", option_str);
				printf("[]=%s\n", &options_[option_index][0]);
			}

			char* get_option(int option_index)
			{
				return &options_[option_index][0];
			}

			int get_key()
			{
				return this->key_index_;
			}
		};

		class question_collection
		{
		protected:
		
			vector<question*> questions_;

		public:
			question_collection(): questions_(4000)
			{
			}

			void get(int random_size, vector<question*>** questions_to_fill)
			{
				if (questions_to_fill == NULL || *questions_to_fill == NULL)
					return;

				
			}

			void fill()
			{				
				sql::ResultSet* result = NULL;
				sql::Statement* stmt = NULL;
				sql::Connection* conn = NULL;
				
				try
				{

					sql::Driver* driver = sql::mysql::get_mysql_driver_instance();

					conn = driver->connect("tcp://192.168.1.102:3306", "root", "12345678");
					stmt = conn->createStatement();

					stmt->execute("use dooqu;");
					result = stmt->executeQuery("select `title`, `option_a`, `option_b`, `option_c`, `option_d`, `option_e`, `key` from game_ask");

					while (result->next())
					{
						question* q = new question();

						printf("title=%s\n", (std::string)result->getString(1));

						//const wchar* title = result->getString(1)->();

						//q->set_title(result->getString(1)->c_str());

						//q->set_option(0, result->getString(2)->c_str());
						//q->set_option(1, result->getString(3)->c_str());
						//q->set_option(2, result->getString(4)->c_str());
						//q->set_option(3, result->getString(5)->c_str());
						//q->set_option(4, result->getString(6)->c_str());

						q->key_index_ = result->getInt(7);
						
						printf("%d\n", q->key_index_);

						this->questions_.push_back(q);
					}

				}
				catch (sql::SQLException& ex)
				{
					printf("mysql error:%s\n", ex.getSQLStateCStr());
					
				}
				
				//delete result;
				delete stmt;
				delete conn;
			}

		};
	}
}

#endif