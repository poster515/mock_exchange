#include "polling/TcpPollable.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h> // perror

namespace polling {
    TcpPollable::TcpPollable(TcpPollableParams&& params){

    }

    bool TcpPollable::Initialize() {
        int raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
        if (raw_socket < 0) {
            perror("Socket Error (Are you root?)");
            return false;
        }

        sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(8080); // Target port
        serverAddress.sin_addr.s_addr = INADDR_ANY; // Bind to any available local IP


        unsigned char buffer[65536];
        struct sockaddr_in saddr;
        socklen_t saddr_len = sizeof(saddr);

        if (bind(raw_socket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
            // Handle bind error (e.g., port already in use)
        }

        if (listen(raw_socket, 3) < 0) {
            perror("Listen failed");
            return false;
        }

        int flags = fcntl(raw_socket, F_GETFL, 0);
        if (fcntl(raw_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
            perror("non-blocking flag set failed");
            return false;
        }

        server_socket = pollfd{
            .fd = raw_socket,
            .events = POLLIN,
            .revents = POLLOUT
        };
        return true;
    }
    size_t TcpPollable::PollOnce() {
        // check interface for new connections - add to tracked connections
        // probably can do auth on this thread since any subsequent traffic will be polled by session mgmt thread.

        sockaddr incoming;
        socklen_t len {sizeof(incoming)};
        int new_fd = accept(server_socket.fd, (struct sockaddr*)&incoming, &len);
        const auto fd_read = poll(&server_socket, 1, 0 /* timeout */); // timeout of 0 returns immediately if nothing available.
        
        if (fd_read > 0 && server_socket.revents & POLLIN) {
            // get data off the socket and forward to logon handler
            // new_fd
        }
        return fd_read;
    }
    void TcpPollable::StopPolling() {
        close(server_socket.fd);
    }
}