#ifndef __DOOQU_UTILITY_H__
#define __DOOQU_UTILITY_H__

#include <cstring>
#include <cstdarg>

namespace dooqu_server
{
	namespace service
	{
		namespace utility
		{
			char* itoa(int n)
			{
				int buffer_len = 2;
				int num = n;
				while((num /= 10) > 0)
				{
					buffer_len ++;
				}

				char* buffer = new char[buffer_len];
				memset(buffer, 0, buffer_len);

				std::sprintf(buffer, "%d", n);

				return buffer;
			}

			void make_params_string(char* buffer, int buffer_size ...)
			{
				if(buffer_size < 2)
					return;

				memset(buffer, 0, buffer_size);

				char* argc = NULL;
				va_list arg_ptr; 
				va_start(arg_ptr, buffer_size); 

				int curr_pos = 0;
				while((argc = va_arg(arg_ptr, char*)) != NULL && curr_pos < (buffer_size - 2))
				{
					if(curr_pos != 0)
					{
						buffer[curr_pos++] = ' ';
					}

					while( curr_pos < (buffer_size - 1) && *argc)
					{
						buffer[curr_pos++] = *argc++;
					}
				}

				va_end(arg_ptr); 

				buffer[buffer_size - 1] = '\0';	
			}


			int numlen(unsigned int n)
			{
				int l = 1;

				while(n /= 10)
				{
					l++;
				}
				return l;
			}
		}
	}
}

#endif