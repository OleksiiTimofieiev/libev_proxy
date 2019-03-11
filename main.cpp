#include "proxy.hpp"

int		main(void)
{
	Proxy proxy;

	proxy.create_proxy_socket();

	proxy.create_connection_to_sql_server();

	proxy.init_and_start_watcher_for_the_client_connection_event();

	return (0);
}