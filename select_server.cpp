#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 10

int main()
{
    fd_set read_fds; // set of file descriptors to monitor for reading
    int master_socket, addrlen, new_socket, client_socket[MAX_CLIENTS], activity, i, valread, sd;
    int max_sd;
    struct sockaddr_in address;

    char buffer[1025]; // data buffer

    // create master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections
    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // set address and port for the server
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // bind the socket to the address and port
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // listen for incoming connections
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(address);
    std::cout << "Waiting for connections..." << std::endl;

    while (true)
    {
        // clear the set
        FD_ZERO(&read_fds);

        // add the master socket to the set
        FD_SET(master_socket, &read_fds);
        max_sd = master_socket;

        // add the client sockets to the set
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];

            // if valid socket descriptor then add to read list
            if (sd > 0)
            {
                FD_SET(sd, &read_fds);
            }

            // highest file descriptor number, need it for the select function
            if (sd > max_sd)
            {
                max_sd = sd;
            }
        }

        // wait for activity on any of the sockets
        activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            std::cout << "select error" << std::endl;
        }

        // if there is activity on the master socket, it means there is a new connection
        if (FD_ISSET(master_socket, &read_fds))
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            std::cout << "New connection, socket fd is " << new_socket << ", ip is " << inet_ntoa(address.sin_addr) << ", port is " << ntohs(address.sin_port) << std::endl;

            // add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }

        // handle the data
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];

            if (FD_ISSET(sd, &read_fds))
            {
                // check if it was for closing, and also read the incoming message
                if ((valread = read(sd, buffer, 1024)) == 0)
                {
                    // client disconnected
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    std::cout << "Host (fd = " << sd << ") disconnected, ip is " << inet_ntoa(address.sin_addr) << ", port is " << ntohs(address.sin_port) << std::endl;

                    // close the socket and mark it as 0 in the array for reuse
                    close(sd);
                    client_socket[i] = 0;
                }
                else
                {
                    // set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';
                    std::cout << "Received from fd " << sd << ": " << buffer << std::endl;

                    // echo back the message to the client
                    send(sd, buffer, strlen(buffer), 0);
                }
            }
        }
    }
    return 0;
}
