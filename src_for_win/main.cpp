#include <iostream>
#include <cstdio>
#include <queue>
#include <boost\asio.hpp>
#include <boost\thread.hpp>
#include <list>
#include <boost\thread\mutex.hpp>

#include "net\tcp_client.h"
#include "net\tcp_server.h"
#include "service\game_service.h"
#include "service\command_dispatcher.h"
#include "service\command.h"
#include <cstdarg>
#include "service\timer.h"
#include "service\http_request.h"
#include <string>
#include <boost\timer.hpp>
#include <assert.h>
#include "ddz\poker_parser.h"
#include <fstream>


using namespace boost::asio;


int main(int argc, char* argv[])
{
	{
		boost::asio::io_service ios;
		dooqu_server::service::game_service s(ios, 8080);

		s.create_game("ddz_0", "ddz", "ddz_zone1", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_1", "ddz", "ddz_zone1", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_2", "ddz", "ddz_zone1", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_3", "ddz", "ddz_zone1", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_4", "ddz", "ddz_zone1", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_5", "ddz", "ddz_zone1", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_6", "ddz", "ddz_zone1", 2000, "100", "10", "21000" ,NULL);

		s.create_game("ddz_7", "ddz", "ddz_zone2", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_8", "ddz", "ddz_zone2", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_9", "ddz", "ddz_zone2", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_10", "ddz", "ddz_zone2", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_11", "ddz", "ddz_zone2", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_12", "ddz", "ddz_zone2", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_13", "ddz", "ddz_zone2", 2000, "100", "10", "21000" ,NULL);

		s.create_game("ddz_14", "ddz", "ddz_zone3", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_15", "ddz", "ddz_zone3", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_16", "ddz", "ddz_zone3", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_17", "ddz", "ddz_zone3", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_18", "ddz", "ddz_zone3", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_19", "ddz", "ddz_zone3", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_20", "ddz", "ddz_zone3", 2000, "100", "10", "21000" ,NULL);


		s.create_game("ddz_21", "ddz", "ddz_zone4", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_22", "ddz", "ddz_zone4", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_23", "ddz", "ddz_zone4", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_24", "ddz", "ddz_zone4", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_25", "ddz", "ddz_zone4", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_26", "ddz", "ddz_zone4", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_27", "ddz", "ddz_zone4", 2000, "100", "10", "21000" ,NULL);

		s.create_game("ddz_28", "ddz", "ddz_zone5", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_29", "ddz", "ddz_zone5", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_30", "ddz", "ddz_zone5", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_31", "ddz", "ddz_zone5", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_32", "ddz", "ddz_zone5", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_33", "ddz", "ddz_zone5", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_34", "ddz", "ddz_zone5", 2000, "100", "10", "21000" ,NULL);


		s.create_game("ddz_35", "ddz", "ddz_zone6", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_36", "ddz", "ddz_zone6", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_37", "ddz", "ddz_zone6", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_38", "ddz", "ddz_zone6", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_39", "ddz", "ddz_zone6", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_40", "ddz", "ddz_zone6", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_41", "ddz", "ddz_zone6", 2000, "100", "10", "21000" ,NULL);

		s.create_game("ddz_42", "ddz", "ddz_zone7", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_43", "ddz", "ddz_zone7", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_44", "ddz", "ddz_zone7", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_45", "ddz", "ddz_zone7", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_46", "ddz", "ddz_zone7", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_47", "ddz", "ddz_zone7", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_48", "ddz", "ddz_zone7", 2000, "100", "10", "21000" ,NULL);


		s.create_game("ddz_49", "ddz", "ddz_zone8", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_50", "ddz", "ddz_zone8", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_51", "ddz", "ddz_zone8", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_52", "ddz", "ddz_zone8", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_53", "ddz", "ddz_zone8", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_54", "ddz", "ddz_zone8", 2000, "100", "10", "21000" ,NULL);
		s.create_game("ddz_55", "ddz", "ddz_zone8", 2000, "100", "10", "21000" ,NULL);
		


		s.start();		

		getchar();

		s.stop();

		printf("\ns.stop()");
	}

	getchar();

	return 1;
}
