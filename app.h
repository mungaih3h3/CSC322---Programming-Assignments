#ifndef APP_H_INCLUDED
#define APP_H_INCLUDED
#include <stdlib.h>
#include <string.h>
#include "catalog.h"
#include "util.h"
#include "handler.h"
#include "cross_platform_sockets.h"

void app(struct Catalog *catalog)
{
#if defined(_WIN32)
	WSADATA d;
	if (WSAStartup(MAKEWORD(2, 2), &d))
	{
		fprintf(stderr, "Failed to initialize.\n");
		return;
	}
#endif

	printf("Configuring local address...\n");
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo *bind_address;
	getaddrinfo(0, "8080", &hints, &bind_address);

	printf("Creating socket...\n");
	SOCKET socket_listen;
	socket_listen = socket(bind_address->ai_family,
						   bind_address->ai_socktype, bind_address->ai_protocol);

	if (!ISVALIDSOCKET(socket_listen))
	{
		fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
		return;
	}

	printf("Binding socket to local address...\n");
	if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen))
	{
		fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
		return;
	}
	freeaddrinfo(bind_address);

	printf("Listening...\n");
	if (listen(socket_listen, 10) < 0)
	{
		fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
		return;
	}

	fd_set master;
	FD_ZERO(&master);
	FD_SET(socket_listen, &master);
	SOCKET max_socket = socket_listen;

	printf("Waiting for connections...\n");

	while (1)
	{
		fd_set reads;
		reads = master;
		if (select(max_socket + 1, &reads, 0, 0, 0) < 0)
		{
			fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
			return;
		}
		SOCKET i;
		for (i = 1; i <= max_socket; ++i)
		{
			// printf("At socket: %d\n", i);
			if (FD_ISSET(i, &reads))
			{
				if (i == socket_listen)
				{
					struct sockaddr_storage client_address;
					socklen_t client_len = sizeof(client_address);
					SOCKET socket_client = accept(socket_listen,
												  (struct sockaddr *)&client_address,
												  &client_len);
					if (!ISVALIDSOCKET(socket_client))
					{
						fprintf(stderr, "accept() failed. (%d)\n",
								GETSOCKETERRNO());
						return;
					}
					FD_SET(socket_client, &master);
					if (socket_client > max_socket)
						max_socket = socket_client;
					char address_buffer[100];
					getnameinfo((struct sockaddr *)&client_address,
								client_len,
								address_buffer, sizeof(address_buffer), 0, 0,
								NI_NUMERICHOST);
					printf("New connection from %s\n", address_buffer);
				}
				else
				{
					char request[1024];
					int bytes_received = recv(i, request, 1024, 0);
					if (bytes_received < 1)
					{
						FD_CLR(i, &master);
						CLOSESOCKET(i);
						continue;
					}
					printf("Received %d bytes.\n", bytes_received);
					printf("%.*s", bytes_received, request);

					char args[20][20];
					split_str(request, ",", args);

					int selection = atoi(args[0]);

					char buf[BUFSIZ];
					memset(buf, 0, sizeof(buf));
					fflush(stdout);
					setbuf(stdout, buf);
					int code = handler(catalog, selection, args);
					fflush(stdout);
					setbuf(stdout, NULL);

					int bytes_sent = send(i, buf, strlen(buf), 0);
					printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(buf));
					if (code == -1)
					{
						break;
					}
				}
			}
		}
	}

	printf("Closing listening socket...\n");
	CLOSESOCKET(socket_listen);
#if defined(_WIN32)
	WSACleanup();
#endif
	printf("Finished.\n");
}

#endif