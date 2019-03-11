#ifndef PROXY_HPP
#define PROXY_HPP

#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <regex>
#include <ev.h>
#include <future>
#include <thread>
#include <fstream>

#define PROXY_PORT 		3008
#define SQL_SERVER_PORT 3007
#define BUFFER_SIZE 	1024

struct my_io 
{
  struct 	ev_io 	io;
  int 				sql_socket_descriptor;
};

class Proxy
{
	private:
		struct 	sockaddr_in proxy_addr;
		struct 	sockaddr_in sql_addr;
		struct 	my_io 		w_accept_connection_to_proxy;
		struct 	ev_loop 	*loop;
		int 				proxy_socket_descriptor;
		int 				socket_for_connection_with_sql;
	public:
  		Proxy();
  		~Proxy();

		void  	create_proxy_socket(void);
		void  	create_connection_to_sql_server(void);
		void  	init_and_start_watcher_for_the_client_connection_event(void);
};

#endif