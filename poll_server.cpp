#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

int main()
{
    int server_fd, new_socket, client_sockets[MAX_CLIENTS], client_count = 0;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    char buffer[BUFFER_SIZE] = {0};

    // Create the server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        return 1;
    }

    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt failed");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    // Bind the socket to the server address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen failed");
        return 1;
    }

    // Add the server socket to the list of file descriptors to poll
    struct pollfd poll_fds[MAX_CLIENTS + 1]; // + 1 để chưa socket của server
    poll_fds[0].fd = server_fd;
    poll_fds[0].events = POLLIN;

    // struct pollfd {
    // int fd;          // Số hiệu file descriptor của socket
    // short events;    // Các sự kiện muốn theo dõi trên socket
    // short revents;   // Các sự kiện thực sự đã xảy ra trên socket
    // };

    // Loop to handle incoming connections and data
    std::cout << "Poll_server is waiting for connections..." << std::endl;
    while (1)
    {
        // Wait for activity on any of the sockets
        int ready = poll(poll_fds, client_count + 1, -1);
        if (ready < 0)
        {
            perror("poll failed");
            return 1;
        }

        // Check for activity on the server socket
        if (poll_fds[0].revents & POLLIN)
        {
            // Accept the incoming connection
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept failed");
                return 1;
            }

            // Add the new client socket to the list of file descriptors to poll
            if (client_count < MAX_CLIENTS)
            {
                client_sockets[client_count] = new_socket;
                poll_fds[client_count + 1].fd = new_socket;
                poll_fds[client_count + 1].events = POLLIN;
                client_count++;
                std::cout << "New client (fd =" << new_socket << " ) connected. Total clients: " << client_count << std::endl;
            }
            else
            {
                std::cout << "Max clients reached. Connection rejected." << std::endl;
                close(new_socket);
            }
        }

        // Check for activity on the client sockets
        for (int i = 0; i < client_count; i++)
        {
            if (poll_fds[i + 1].revents & POLLIN)
            {
                // Receive data from the client socket
                int valread = read(client_sockets[i], buffer, BUFFER_SIZE);
                if (valread <= 0)
                {
                    // Connection closed or error occurred, remove the client socket from the list of file descriptors to poll
                    close(client_sockets[i]);
                    poll_fds[i + 1].fd = -1;
                    poll_fds[i + 1].events = 0;
                    std::cout << "Client disconnected. Total clients: " << client_count - 1 << std::endl;
                    client_count--;
                }
                else
                {
                    // Print the received data
                    std::cout << " Received message from client fd = "<< poll_fds[i + 1].fd <<": " << buffer << std::endl;
                    // Echo the data back to the client
                    send(client_sockets[i], buffer, valread, 0);
                }
            }
        }
    }
    return 0;
}

// Giải thích code:

// - Đầu tiên, chúng ta tạo một TCP server socket bằng cách sử dụng hàm `socket()`
// và cấu hình socket bằng cách sử dụng `setsockopt()`.

// - Sau đó, chúng ta liên kết socket với địa chỉ server bằng cách sử dụng `bind()`.

// - Chúng ta lắng nghe kết nối đến từ client bằng cách sử dụng `listen()`.

// - Tiếp theo, chúng ta sử dụng `poll()` để đợi đồng thời cho các tín hiệu từ cả server và client.
// Chúng ta thêm server socket vào danh sách các file descriptor cần kiểm tra bằng cách
// thiết lập giá trị `poll_fds[0].fd = server_fd` và `poll_fds[0].events = POLLIN`.

// - Trong vòng lặp, chúng ta gọi `poll()` để đợi cho các tín hiệu từ các file descriptor trong danh sách `poll_fds`.

// - Nếu có kết nối mới, chúng ta sử dụng `accept()` để chấp nhận kết nối và thêm client socket mới vào danh sách các file descriptor cần kiểm tra
// bằng cách thiết lập `poll_fds[client_count + 1].fd = new_socket` và `poll_fds[client_count + 1].events = POLLIN`.

// - Nếu có dữ liệu được nhận từ client socket, chúng ta sử dụng `read()` để đọc dữ liệu và `send()` để gửi dữ liệu trả lời lại cho client.

// - Nếu có lỗi xảy ra hoặc kết nối bị đóng, chúng ta loại bỏ client socket khỏi danh sách
// các file descriptor cần kiểm tra bằng cách thiết lập `poll_fds[i + 1].fd = -1` và `poll_fds[i + 1].events = 0`.

// Cuối cùng, chúng ta đóng server socket và client socket khi hoàn thành.
