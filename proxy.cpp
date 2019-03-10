#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <regex>
#include <ev.h>
#include <thread>
#include <fstream>

/*

- libev library used for the project;
- lines with perror function == possible mistakes;
- sql_line_detection_and_processing == parsing + async write to file;
- flow: client -> proxy -> SQL;

*/

#define PROXY_PORT 3008
#define SQL_SERVER_PORT 3007
#define BUFFER_SIZE 1024

struct my_io 
{
  struct 	ev_io io;
  int 		sql_socket_descriptor;
};

void  accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void  read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents);
void  sql_line_detection_and_processing(char *str);

class Proxy
{
	private:
		struct 	sockaddr_in proxy_addr;
		struct 	sockaddr_in sql_addr;
		struct 	my_io w_accept;
		struct 	ev_loop *loop;
		int 	proxy_socket_descriptor;
		int 	socket_for_connection_with_sql;
	public:
  		Proxy() { loop = ev_default_loop(0); };
  		~Proxy(){};

	void  create_proxy_socket(void) 
	{
		if( (proxy_socket_descriptor = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
		  perror("socket error");

		bzero(&proxy_addr, sizeof(proxy_addr));
		proxy_addr.sin_family = AF_INET;
		proxy_addr.sin_port = htons(PROXY_PORT);
		proxy_addr.sin_addr.s_addr = INADDR_ANY;

		// Bind socket to address
		if (bind(proxy_socket_descriptor, (struct sockaddr*) &proxy_addr, sizeof(proxy_addr)) != 0)
		  perror("bind error");

		// Start listing on the socket
		if (listen(proxy_socket_descriptor, 2) < 0)
		  perror("listen error");
	}

	void  create_connection_to_sql_server(void)
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

		w_accept.sql_socket_descriptor = socket_for_connection_with_sql;
	}

	void  proxy_main(void)
	{
		// Initialize and start a watcher to accepts client requests to the proxy

		ev_io_init(&w_accept.io, accept_cb, proxy_socket_descriptor, EV_READ);
		ev_io_start(loop, &w_accept.io);

		ev_loop(loop, 0);
	}
};


/* Accept client requests */
void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	struct 			sockaddr_in client_addr;
	socklen_t 		client_len = sizeof(client_addr);
	int 			client_sd;
	struct my_io 	*w_client = (struct my_io *)malloc(sizeof(struct my_io));
	struct my_io 	*w_ = (struct my_io*)watcher;

	w_client->sql_socket_descriptor = w_->sql_socket_descriptor;

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
	ev_io_init(&w_client->io, read_cb, client_sd, EV_READ);
	ev_io_start(loop, &w_client->io);
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
	}
	
	myfile.close();
}

/* Read client message */
void    read_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char    buffer[BUFFER_SIZE];
	ssize_t read; // <- read;

	bzero(buffer, sizeof(buffer));

	struct my_io *w_ = (struct my_io*)watcher;

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

	std::thread logs(sql_line_detection_and_processing, buffer);
	send(w_->sql_socket_descriptor, buffer, read, 0);

	bzero(buffer, read);
	logs.join();
}

int main(void)
{
	Proxy proxy;

	proxy.create_proxy_socket();

	proxy.create_connection_to_sql_server();

	proxy.proxy_main();

	return (0);
}
