#ifndef __THREADS_LOCK_STATUS__
#define __THREADS_LOCK_STATUS__

#include <map>
#include <boost\thread.hpp>

class thread_status
{
public:
	static std::map<boost::thread::id, char*> messages;

	static inline void log(char* message)
	{
		messages[boost::this_thread::get_id()] = message;
	}
};


std::map<boost::thread::id, char*> thread_status::messages;



#endif;