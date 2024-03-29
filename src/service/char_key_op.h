#ifndef __CHAR_KEY_OPERATOR_H__
#define __CHAR_KEY_OPERATOR_H__

#include <cstring>
namespace dooqu_server
{
	namespace service
	{
		class char_key_op
		{
		public:
			bool operator()(const char* c1, const char* c2)
			{
				return std::strcmp(c1, c2) < 0;
			}
		};
	}
}

#endif