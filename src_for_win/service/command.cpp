#include "command.h"
#include <cstring>

namespace dooqu_server
{
	namespace service
	{
		command::command()
		{
			this->name_ = NULL;
			this->params_ = NULL;
			this->command_data_ = NULL;
			this->is_ok_ = false;			

			this->null();
		}

		command::command(char* command_data)
		{
			this->name_ = NULL;
			this->params_ = NULL;
			this->command_data_ = NULL;
			this->is_ok_ = false;

			this->reset(command_data);
		}

		void command::null()
		{
			this->is_ok_ = false;
			this->name_ = NULL;
			
			if(this->params_ != NULL)
			{
				this->params_->clear();
				delete this->params_;
				this->params_ = NULL;
			}

			if(this->command_data_ != NULL)
			{
				delete [] this->command_data_;
				this->command_data_ = NULL;
			}
		}

		char* command::name()
		{
			return this->name_;
		}

		char* command::params(int pos)
		{
			return this->params_->at(pos);
		}

		int command::param_size()
		{
			if(this->params_ != NULL)
			{
				return this->params_->size();
			}
			return 0;
		}

		bool command::is_ok()
		{
			return this->is_ok_;
		}

		void command::reset(char* command_data)
		{
			this->null();
			this->is_ok_ = this->init(command_data);
		}

		bool command::init(char* command_data)
		{
			int str_length = std::strlen(command_data);	

			if(str_length <= 0)
				return false;

			for(int i = (str_length - 1); i >= 0; i--)
			{
				if(command_data[i] == ' ')
				{
					command_data[i] = '\0';
				}
				else
				{
					break;
				}
			}

			str_length = std::strlen(command_data);

			if(str_length <= 0)
				return false;
			
			this->params_ = new std::vector<char*>();

			int curr_pos = 0;
			for(int i = 0; i < str_length; i++)
			{
				if(command_data[i] == '\0' || command_data[i] == ' ')
				{
					command_data[i] = '\0';

					if((i + 1) < str_length && (command_data[i + 1] != '\0' && command_data[i + 1] != ' '))
					{
						if(this->name_ == NULL)
						{
							this->name_ = &command_data[curr_pos];
						}
						else
						{
							this->params_->push_back(&command_data[curr_pos]);
						}
						curr_pos = i + 1;
					}
				}
				else if(i == (str_length - 1))
				{
					if(this->name_ == NULL)
					{
						this->name_ = &command_data[curr_pos];
					}
					else
					{
						this->params_->push_back(&command_data[curr_pos]);
					}
				}
			}

			return true;
		}

		command::~command()
		{			
			this->null();
			//printf("~command");
		}
	}
}
