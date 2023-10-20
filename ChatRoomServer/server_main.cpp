// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include "buffer.h"
#include "message.h"

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "5555"

using namespace network;

struct ClientInformation {
    SOCKET socket;
    bool connected;
};

struct ServerInfo {
    struct addrinfo* info = nullptr;
    struct addrinfo hints;
    SOCKET listenSocket = INVALID_SOCKET;
    fd_set activeSockets;
    fd_set socketsReadyForReading;
    std::vector<ClientInformation> clients;
} g_ServerInfo;

int Initialize() {  // Initialization
    WSADATA wsaData;
    int result;

    FD_ZERO(&g_ServerInfo.activeSockets);
    FD_ZERO(&g_ServerInfo.socketsReadyForReading);

    printf("WSAStartup . . . ");
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed with error %d\n", result);
        return 1;
    } else {
        printf("Success!\n");
    }

    ZeroMemory(&g_ServerInfo.hints, sizeof(g_ServerInfo.hints));
    g_ServerInfo.hints.ai_family = AF_INET;        // IPV4
    g_ServerInfo.hints.ai_socktype = SOCK_STREAM;  // Stream
    g_ServerInfo.hints.ai_protocol = IPPROTO_TCP;  // TCP
    g_ServerInfo.hints.ai_flags = AI_PASSIVE;

    printf("Creating our AddrInfo . . . ");
    result = getaddrinfo(NULL, DEFAULT_PORT, &g_ServerInfo.hints, &g_ServerInfo.info);
    if (result != 0) {
        printf("getaddrinfo failed with error: %d\n", result);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
    }

    // Create our socket [Socket]
    printf("Creating our Listen Socket . . . ");
    g_ServerInfo.listenSocket =
        socket(g_ServerInfo.info->ai_family, g_ServerInfo.info->ai_socktype, g_ServerInfo.info->ai_protocol);
    if (g_ServerInfo.listenSocket == INVALID_SOCKET) {
        printf("Socket failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(g_ServerInfo.info);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
    }

    // 12,14,1,3:80				Address lengths can be different
    // 123,111,230,109:55555	Must specify the length
    // Bind our socket [Bind]
    printf("Calling Bind . . . ");
    result = bind(g_ServerInfo.listenSocket, g_ServerInfo.info->ai_addr, (int)g_ServerInfo.info->ai_addrlen);
    if (result == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(g_ServerInfo.info);
        closesocket(g_ServerInfo.listenSocket);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
    }

    // [Listen]
    printf("Calling Listen . . . ");
    result = listen(g_ServerInfo.listenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(g_ServerInfo.info);
        closesocket(g_ServerInfo.listenSocket);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
    }

    return 0;
}

void ShutDown() {
    // Close
    printf("Closing . . . \n");
    freeaddrinfo(g_ServerInfo.info);
    closesocket(g_ServerInfo.listenSocket);
    WSACleanup();
}

void HandleMessage(MessageType msgType, ClientInformation& client, Buffer& recvBuf, Buffer& sendBuf) {
    switch (msgType) {
        case MessageType::LoginReq: {
            // received C2S_LoginReqMsg
            uint32_t userNameLength = recvBuf.ReadUInt32LE();
            std::string userName = recvBuf.ReadString(userNameLength);
            uint32_t passwordLength = recvBuf.ReadUInt32LE();
            std::string password = recvBuf.ReadString(passwordLength);
            // TODO: user authentication
            printf("Authenticating user...\n");
            printf("User %s login successfully.\n", userName.c_str());

            // respond with S2C_LoginAckMsg
            std::vector<std::string> roomNames{"graphics_1", "network_programming"};
            S2C_LoginAckMsg response{200, roomNames};
            response.Serialize(sendBuf);
            // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-send
            int sendResult = send(client.socket, sendBuf.ConstData(), response.header.packetSize, 0);
            if (sendResult == SOCKET_ERROR) {
                printf("send failed with error %d\n", WSAGetLastError());
                WSACleanup();
                break;
            }
        }

        break;
        default:
            printf("Unknown message.\n");
            break;
    }
}

int main(int argc, char** argv) {
    // Initialization
    int result = Initialize();
    if (result != 0) {
        return result;
    }

    // Define timeout for select()
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000;  // 500 milliseconds, half a second

    int selectResult;
    constexpr int kRECV_BUF_SIZE = 512;
    char rawRecvBuf[kRECV_BUF_SIZE];

    constexpr int kSEND_BUF_SIZE = 512;
    Buffer sendBuf{kSEND_BUF_SIZE};

    // select work here
    for (;;) {
        // SocketsReadyForReading will be empty here
        FD_ZERO(&g_ServerInfo.socketsReadyForReading);

        // Add all the sockets that have data ready to be recv'd
        // to the socketsReadyForReading
        //
        // socketsReadyForReading contains:
        //  ListenSocket
        //  All connectedClients' socket
        //
        // 1. Add the listen socket, to see if there are any new connections
        FD_SET(g_ServerInfo.listenSocket, &g_ServerInfo.socketsReadyForReading);

        // 2. Add all the connected sockets, to see if the is any information
        //    to be recieved from the connected clients.
        for (int i = 0; i < g_ServerInfo.clients.size(); i++) {
            ClientInformation& client = g_ServerInfo.clients[i];
            if (client.connected) {
                FD_SET(client.socket, &g_ServerInfo.socketsReadyForReading);
            }
        }

        // Select will check all sockets in the SocketsReadyForReading set
        // to see if there is any data to be read on the socket.
        // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
        selectResult = select(0, &g_ServerInfo.socketsReadyForReading, NULL, NULL, &tv);
        if (selectResult == 0) {
            // Time limit expired
            continue;
        }
        if (selectResult == SOCKET_ERROR) {
            printf("select() failed with error: %d\n", WSAGetLastError());
            return 1;
        }
        printf(".");

        // Check if our ListenSocket is set. This checks if there is a
        // new client trying to connect to the server using a "connect"
        // function call.
        if (FD_ISSET(g_ServerInfo.listenSocket, &g_ServerInfo.socketsReadyForReading)) {
            printf("\n");

            // [Accept]
            printf("Calling Accept . . . ");
            SOCKET clientSocket = accept(g_ServerInfo.listenSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) {
                printf("Accept failed with error: %d\n", WSAGetLastError());
            } else {
                printf("Success!\n");
                ClientInformation newClient;
                newClient.socket = clientSocket;
                newClient.connected = true;
                g_ServerInfo.clients.push_back(newClient);
            }
        }

        // Check if any of the currently connected clients have sent data using send
        for (int i = 0; i < g_ServerInfo.clients.size(); i++) {
            ClientInformation& client = g_ServerInfo.clients[i];

            if (!client.connected) continue;

            if (FD_ISSET(client.socket, &g_ServerInfo.socketsReadyForReading)) {
                // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-recv
                // result
                //		-1 : SOCKET_ERROR (More info received from WSAGetLastError() after)
                //		0 : client disconnected
                //		>0: The number of bytes received.
                ZeroMemory(&rawRecvBuf, kRECV_BUF_SIZE);
                int recvResult = recv(client.socket, rawRecvBuf, kRECV_BUF_SIZE, 0);

                if (recvResult < 0) {
                    printf("recv failed: %d\n", WSAGetLastError());
                    continue;
                }

                // Saving some time to not modify a vector while
                // iterating through it. Want remove the client
                // from the vector
                if (recvResult == 0) {
                    printf("Client disconnected!\n");
                    client.connected = false;
                    continue;
                }

                printf("Received %d bytes from the client!\n", recvResult);

                // We must receive 4 bytes before we know how long the packet actually is
                // We must receive the entire packet before we can handle the message.
                // Our protocol says we have a HEADER[pktsize, messagetype];
                Buffer recvBuf{rawRecvBuf, kRECV_BUF_SIZE};
                uint32_t packetSize = recvBuf.ReadUInt32LE();
                MessageType messageType = static_cast<MessageType>(recvBuf.ReadUInt32LE());

                if (recvBuf.Size() >= packetSize) {
                    // We can finally handle our message
                    HandleMessage(messageType, client, recvBuf, sendBuf);
                }

                FD_CLR(client.socket, &g_ServerInfo.socketsReadyForReading);
            }
        }
    }

    ShutDown();
    return 0;
}