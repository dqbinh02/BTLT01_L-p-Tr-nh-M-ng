#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

int set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        flags = 0;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("socket");
        return 1;
    }

    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    if (set_non_blocking(server_fd) == -1)
    {
        perror("fcntl");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, SOMAXCONN) == -1)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }

    struct epoll_event event;
    event.data.fd = server_fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
    {
        perror("epoll_ctl");
        close(server_fd);
        close(epoll_fd);
        return 1;
    }

    std::vector<char> buffer(BUFFER_SIZE);
    std::cout << "Epoll_server waiting for connections..." << std::endl;

    while (true)
    {
        struct epoll_event events[MAX_EVENTS];
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n == -1)
        {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < n; ++i)
        {
            if (events[i].data.fd == server_fd)
            {
                while (true)
                {
                    struct sockaddr_in client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                    if (client_fd == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        else
                        {
                            perror("accept");
                            break;
                        }
                    }

                    std::cout << "New client connected from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;

                    if (set_non_blocking(client_fd) == -1)
                    {
                        perror("fcntl");
                        close(client_fd);
                        continue;
                    }

                    event.data.fd = client_fd;
                    event.events = EPOLLIN | EPOLLET;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1)
                    {
                        perror("epoll_ctl");
                        close(client_fd);
                        continue;
                    }
                }
            }
            else
            {
                int client_fd = events[i].data.fd;
                while (true)
                {
                    ssize_t bytes_received = recv(client_fd, buffer.data(), buffer.size(), 0);
                    if (bytes_received == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        else
                        {
                            perror("recv");
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
                            close(client_fd);
                            continue;
                        }
                    }
                    else if (bytes_received == 0)
                    {
                        std::cout << "Client disconnected" << std::endl;
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
                        close(client_fd);
                        break;
                    }
                    else
                    {
                        // std::cout << "Received " << bytes_received << " bytes from client" << std::endl;
                        std::cout << "Received from client fd " << client_fd << " : ";
                        for (int m = 0;m< bytes_received;m++){
                            std::cout << buffer.data()[m];
                        }
                        std::cout << std::endl;

                        if (send(client_fd, buffer.data(), bytes_received, 0) == -1)
                        {
                            perror("send");
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
                            close(client_fd);
                            continue;
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}