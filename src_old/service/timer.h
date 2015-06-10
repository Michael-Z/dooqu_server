#ifndef __TIMER_H__
#define __TIMER_H__

#include <boost/noncopyable.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

namespace dooqu_server
{
	namespace service
	{
		using namespace boost::posix_time;
		class timer : boost::noncopyable
		{
		protected:
			ptime start_time_;
		public:
			timer(){this->restart();}
			void restart(){this->start_time_ = microsec_clock::local_time();}
			void restart(long long millisecs){this->start_time_ = microsec_clock::local_time();}
			long long elapsed(){return (microsec_clock::local_time() - this->start_time_).total_milliseconds();}
		};
	}
}
#endif
