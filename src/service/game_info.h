#ifndef __GAME_INFO_H__
#define __GAME_INFO_H__

#include <boost/noncopyable.hpp>

namespace dooqu_server
{
	namespace service
	{
		class game_info : boost::noncopyable
		{
		public:
			virtual ~game_info(){};
		};
	}
}

#endif
