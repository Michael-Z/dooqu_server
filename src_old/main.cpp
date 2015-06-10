#include <iostream>
#include <cstdio>
#include <queue>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <list>
#include <boost/thread/mutex.hpp>

#include "net/tcp_client.h"
#include "net/tcp_server.h"
#include "service/game_service.h"
#include "ddz/ddz_plugin.h"
//#include "service/command_dispatcher.h"
//#include "service\command.h"
#include <cstdarg>
//#include "service/timer.h"
//#include "service/http_request.h"
#include <string>
#include <boost/timer.hpp>
#include <assert.h>
//#include "ddz/poker_parser.h"



using namespace boost::asio;

void print( void* p)
{
    printf("hello");
}


int main(int argc, char* argv[])
{
	int server_port = 8080;

	if (argc > 1)
	{
		server_port = std::atoi(argv[0]);
	}

    printf("\n[dooqu game server 5.0]");
	{
		boost::asio::io_service ios;
		dooqu_server::service::game_service service(ios, server_port);
		printf("\n[create service at tcp:%d]", server_port);


        for(int i = 0, j = 0, zone_id = 0; i <55; i++)
        {
            char curr_game_id[30];
            curr_game_id[sprintf(curr_game_id, "ddz_%d", i) + 1] = 0;

            char curr_zone_id[30];
            curr_zone_id[sprintf(curr_zone_id, "zone_%d", zone_id) + 1] = 0;          


            if((++j % 7) == 0)
            {
                j = 0;
                zone_id++;
            }

            dooqu_server::ddz::ddz_plugin* curr_plugin = new dooqu_server::ddz::ddz_plugin(&service, curr_game_id, curr_game_id, 2000, 100, 15, 21000);
            service.create_plugin(curr_plugin, curr_zone_id);

			printf("\ncreate game {%s}, at game_zone:{%s}", curr_game_id, curr_zone_id);
        }

		service.start();
		printf("\n[dooqu service start sucessfully.]");
		printf("\n[press anykey to stop service.]");

		getchar();

		service.stop();

		printf("\ns.stop()");

	}

	getchar();
	return 1;
}
