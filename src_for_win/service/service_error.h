#ifndef __SERVICE_SERVICE_ERROR_H__
#define __SERVICE_SERVICE_ERROR_H__

#include "..\net\service_error.h"

namespace dooqu_server
{
	namespace service
	{
		class service_error : public dooqu_server::net::service_error
		{
		public:
			static const int CLIENT_HAS_LOGINED = 11;
			static const int GAME_IS_FULL = 12;
			static const int GAME_NOT_EXISTED = 15;
			static const int TIME_OUT = 16;
			static const int SERVER_CLOSEING = 17;
			static const int ARGUMENT_ERROR = 18;
			static const int LOGIN_SERVICE_ERROR = 19;
		};
	}
}

#endif