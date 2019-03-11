#include "proxy.hpp"

void  	accept_event_callback(struct ev_loop *loop, struct ev_io *watcher, int revents);
void  	read_event_callback(struct ev_loop *loop, struct ev_io *watcher, int revents);
void  	sql_line_detection_and_processing(char *str);

/*
*********************************** constructors ***************************************************
*/

Proxy::Proxy() 	{ loop = ev_default_loop(0); }
Proxy::~Proxy()	{ };

/*
*********************************** proxy socket creation and initializatioin **********************
*/

void  Proxy::create_proxy_socket(void) 
{
	if((proxy_socket_descriptor = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	  perror("socket error");

	bzero(&proxy_addr, sizeof(proxy_addr));

	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_port = htons(PROXY_PORT);
	proxy_addr.sin_addr.s_addr = INADDR_ANY;

	// Bind socket to the address
	if (bind(proxy_socket_descriptor, (struct sockaddr*) &proxy_addr, sizeof(proxy_addr)) != 0)
	  perror("bind error");

	// Start listing on the socket
	if (listen(proxy_socket_descriptor, 2) < 0)
	  perror("listen error");
}

/*
************************* creation of the socket for the targeted connection ***********************
*/

void  Proxy::create_connection_to_sql_server(void)
{
	// Create client socket
	if( (socket_for_connection_with_sql = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	  perror("socket creation error");

	bzero(&sql_addr, sizeof(sql_addr));

	sql_addr.sin_family = AF_INET;
	sql_addr.sin_port = htons(SQL_SERVER_PORT);
	sql_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Connect to server socket
	if(connect(socket_for_connection_with_sql, (struct sockaddr *)&sql_addr, sizeof sql_addr) < 0)
	  perror("connection error");

	w_accept_connection_to_proxy.sql_socket_descriptor = socket_for_connection_with_sql;
}

/*
************************* initialization of the libev_loop *****************************************
*/

void  Proxy::init_and_start_watcher_for_the_client_connection_event(void)
{
	// Initialize and start a watcher to accepts client requests to the proxy
	ev_io_init(&w_accept_connection_to_proxy.io, accept_event_callback, proxy_socket_descriptor, EV_READ);
	ev_io_start(loop, &w_accept_connection_to_proxy.io);

	ev_loop(loop, 0);
}

/*
********************************* callbacks for the events *****************************************

/* Accept client requests */
void	accept_event_callback(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	struct 			sockaddr_in client_addr;
	socklen_t 		client_len = sizeof(client_addr);
	int 			client_sd;
	struct my_io 	*w_client = (struct my_io *)malloc(sizeof(struct my_io));
	struct my_io 	*ptr_to_get_sql_socket_descriptor = (struct my_io*)watcher;

	w_client->sql_socket_descriptor = ptr_to_get_sql_socket_descriptor->sql_socket_descriptor;

	if(EV_ERROR & revents)
	{
	  // any invalid event;
	  perror("got invalid event");
	  return;
	}

	// Accept client request
	client_sd = accept(watcher->fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_sd < 0)
	{
	  // client has not been accepted;
	  perror("accept error");
	  return;
	}

	printf("Successfully connected with client.\n");
	
	// Initialize and start watcher to read client requests
	ev_io_init(&w_client->io, read_event_callback, client_sd, EV_READ);
	ev_io_start(loop, &w_client->io);
}

/* Read client message */
void    read_event_callback(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char    buffer[BUFFER_SIZE];
	ssize_t read; // <- read;

	bzero(buffer, sizeof(buffer));

	struct my_io *ptr_to_get_sql_socket_descriptor = (struct my_io*)watcher;

	if(EV_ERROR & revents)
	{
	  perror("got invalid event");
	  return;
	}

	// Receive message from client socket
	read = recv(watcher->fd, buffer, BUFFER_SIZE, 0);

	if(read < 0)
	{
	  perror("read error");
	  return ;
	}

	if (read == 0)
	{
	  // Stop and free watcher if client socket is closing
	  ev_io_stop(loop,watcher);
	  free(watcher);
	  perror("peer might closing");
	  return ;
	}

	auto future = std::async(std::launch::async, &sql_line_detection_and_processing, buffer);

	send(ptr_to_get_sql_socket_descriptor->sql_socket_descriptor, buffer, read, 0);
}

void    sql_line_detection_and_processing(char *str)
{
	std::string s(str);
	std::ofstream myfile;

	std::smatch match;

	const std::regex rgx("(SQL:)(\".*\")");

	if (std::regex_match(s, match, rgx))
	{
		myfile.open("log.txt", std::ios::app);
		myfile << match[2] << std::endl;
		myfile.close();
	}
}
