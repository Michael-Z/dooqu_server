#ifndef __TICK_COUNT_H__
#define __TICK_COUNT_H__

#include <boost/noncopyable.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"

namespace dooqu_server
{
	namespace service
	{
		using namespace boost::posix_time;
		class tick_count : boost::noncopyable
		{
		protected:
			ptime start_time_;
		public:
			tick_count(){ this->restart(); }
			void restart(){ this->start_time_ = microsec_clock::local_time(); }
			void restart(long long millisecs){ this->start_time_ = microsec_clock::local_time(); this->start_time_ + milliseconds(millisecs); }
			long long elapsed(){ return (microsec_clock::local_time() - this->start_time_).total_milliseconds(); }
		};
	}
}
#endif
