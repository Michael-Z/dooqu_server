#ifndef __NET_SERVICE_ERROR_H__
#define __NET_SERVICE_ERROR_H__

namespace dooqu_server
{
	namespace net
	{
		class service_error
		{
		public:
			static const int CLIENT_EXIT = 0;
			static const int CLIENT_NET_ERROR = 1 ;
			static const int DATA_OUT_OF_BUFFER = 2;
			static const int OK = 99;
		};
	}
}

#endif