#include "../service/service_error.h"

namespace dooqu_server
{
	namespace ddz
	{
		class service_error : public dooqu_server::service::service_error
		{
		public:
			static const int GAME_IS_FULL = 20;
		};
	}
}