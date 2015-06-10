#ifndef __GAME_SERVICE_ERROR__
#define __GAME_SERVICE_ERROR__

#include "../service/service_error.h"

namespace dooqu_server
{
	namespace plane
	{
		class service_error : public dooqu_server::service::service_error
		{
		public:
			static const int READY_MAP_TIME_OUT = 101;
		};
	}
}
#endif