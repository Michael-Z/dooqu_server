#ifndef __NET_SERVICE_ERROR_H__
#define __NET_SERVICE_ERROR_H__

namespace dooqu_server
{
	namespace net
	{
		class service_error
		{
		public:
			//NET ERROR��ָ�ͻ��˵������Ͽ�
			static const int CLIENT_NET_ERROR = 0;
			///
			static const int CLIENT_EXIT = 1;			
			static const int DATA_OUT_OF_BUFFER = 2;
			static const int SERVER_CLOSEING = 3;
			static const int OK = 99;
		};
	}
}

#endif