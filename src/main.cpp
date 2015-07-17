#include <iostream>
#include <cstdio>
#include <queue>
#include <list>
#include <vector>
#include <cstdarg>
#include <string>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/timer.hpp>
#include <assert.h>
#include <crtdbg.h> 
#include "net/tcp_client.h"
#include "service/game_service.h"
#include "service/game_zone.h"
#include "service/command.h"
#include "ddz/ddz_plugin.h"
#include <boost/pool/pool.hpp>
#include "service\post_monitor.h"
#include "plane\plane_plugin.h"
//#include "net\threads_lock_status.h"

using namespace boost::asio;

#define SERVER_STATUS_DEBUG

#define SERVER_DEBUG_MODE
/*
1、、main thread: 1

2、io_service 线程: cpu + 1

3、io_service's timer: 1

4、game_zone's io_service thread; zone num * 1;
5、game_zone's io_service timer zone_num * 1;

6、http_request 会动态启动线程，无法控制 += 2
*/


inline void enable_mem_leak_check()
{
#ifdef WIN32
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif
}


int main(int argc, char* argv[])
{


	enable_mem_leak_check();

	{
		dooqu_server::service::game_service service(8000);

		//for (int i = 0; i < 2; i++)
		//{
		//	char curr_game_id[30] = { 0 };
		//	curr_game_id[sprintf(curr_game_id, "plane_%d", i)] = 0;

		//	char curr_game_title[60] = { 0 };
		//	curr_game_title[sprintf(curr_game_title, "飞机大战房间[%d]", i)] = 0;

		//	char curr_zone_id[30] = "zone_0";

		//	dooqu_server::plane::plane_plugin* plane_game = new dooqu_server::plane::plane_plugin(&service, curr_game_id, curr_game_title);

		//	service.create_plugin(plane_game, curr_zone_id);
		//}

		for (int i = 0, j = 0, zone_id = 0; i < 20; i++)
		{
			char curr_game_id[30];
			curr_game_id[sprintf(curr_game_id, "ddz_%d", i) + 1] = 0;

			char curr_zone_id[30];
			curr_zone_id[sprintf(curr_zone_id, "ddz_zone_%d", zone_id) + 1] = 0;

			if ((++j % 60) == 0)
			{
				j = 0;
				zone_id++;
			}
			dooqu_server::ddz::ddz_plugin* curr_plugin = new dooqu_server::ddz::ddz_plugin(&service, curr_game_id, curr_game_id, 2000, 100, 15, 21000);
			service.create_plugin(curr_plugin, curr_zone_id);
		}

		service.start();
		printf("please input the service command:\n");
		string read_line;
		std::getline(std::cin, read_line);


		while (std::strcmp(read_line.c_str(), "exit") != 0)
		{
			if (std::strcmp(read_line.c_str(), "onlines") == 0)
			{
				printf("current onlines is:\n");
				const std::map<char*, dooqu_server::service::game_plugin*, dooqu_server::service::char_key_op>* plugins = service.get_plugins();

				std::map<char*, dooqu_server::service::game_plugin*, dooqu_server::service::char_key_op>::const_iterator curr_plugin = plugins->begin();

				int online_total = 0;
				while (curr_plugin != plugins->end())
				{
					online_total += curr_plugin->second->clients()->size();
					printf("current game {%s}, onlines=%d\n", curr_plugin->second->game_id(), curr_plugin->second->clients()->size());
					++curr_plugin;
				}
				printf("all plugins online total=%d\n", online_total);
			}
			else if (std::strcmp(read_line.c_str(), "io") == 0)
			{
				printf("press any key to exit i/o monitor mode:\n");
				tcp_client::LOG_IO_DATA = true;
				getline(std::cin, read_line);
				tcp_client::LOG_IO_DATA = false;
				printf("i/o monitor is stoped.\n");
			}
			else if (std::strcmp(read_line.c_str(), "timers") == 0)
			{
				printf("press any key to exit timers monitor mode:\n");
				dooqu_server::service::game_zone::LOG_TIMERS_INFO = true;
				getline(std::cin, read_line);
				dooqu_server::service::game_zone::LOG_TIMERS_INFO = false;
				printf("timers monitor is stoped.\n");
			}
			else if (std::strcmp(read_line.c_str(), "thread") == 0)
			{
				for (dooqu_server::net::thread_status_map::iterator curr_thread_pair = service.threads_status()->begin();
					curr_thread_pair != service.threads_status()->end();
					++curr_thread_pair)
				{
					printf("thread:%d, %ld\n", (*curr_thread_pair).first, (*curr_thread_pair).second->elapsed());
				}
			}
			else if (std::strcmp(read_line.c_str(), "stop") == 0)
			{
				service.stop();
			}
			else if (std::strcmp(read_line.c_str(), "unload") == 0)
			{
				service.stop();
			}
			else if (std::strcmp(read_line.c_str(), "update") == 0)
			{
				service.tick_count_threads();
				boost::this_thread::sleep_for(boost::chrono::milliseconds(10));

				for (dooqu_server::net::thread_status_map::iterator curr_thread_pair = service.threads_status()->begin();
					curr_thread_pair != service.threads_status()->end();
					++curr_thread_pair)
				{
					printf("thread:%d, %ld\n", (*curr_thread_pair).first, (*curr_thread_pair).second->elapsed());
				}

				for (std::map<boost::thread::id, char*>::iterator e = thread_status::messages.begin();
					e != thread_status::messages.end();
					++e)
				{
					printf("thread id %d, %s\n", (*e).first, (*e).second);
				}
			}
			cin.clear();
			printf("please input the service command:\n");
			std::getline(std::cin, read_line);
		}
	}

	printf("server object is recyled, press any key to exit.");
	getchar();
	

	return 1;
}



