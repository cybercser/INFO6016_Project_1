// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "5555"

#include "buffer.h"
#include "message.h"

#include <string>

using namespace network;

void HandleMessage(MessageType msgType, Buffer& recvBuf) {
    switch (msgType) {
        case MessageType::LoginAck: {
            uint16 loginStatus = recvBuf.ReadUInt16LE();
            if (loginStatus == 200) {
                uint32 roomListLength = recvBuf.ReadUInt32LE();
                std::vector<uint32> roomNameLengths;
                for (size_t i = 0; i < roomListLength; i++) {
                    roomNameLengths.push_back(recvBuf.ReadUInt32LE());
                }

                printf("Rooms:\n");
                std::vector<std::string> roomNames;
                for (size_t i = 0; i < roomListLength; i++) {
                    std::string roomName = recvBuf.ReadString(roomNameLengths[i]);
                    roomNames.push_back(roomName);
                    printf("%s\n", roomName.c_str());
                }
            } else {
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
    WSADATA wsaData;
    int result;

    printf("WSAStartup . . . ");
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed with error %d\n", result);
        return 1;
    } else {
        printf("Success!\n");
    }

    struct addrinfo* info = nullptr;
    struct addrinfo* ptr = nullptr;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4 #.#.#.#, AF_INET6 is IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    printf("Creating our AddrInfo . . . ");
    result = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &info);
    if (result != 0) {
        printf("getaddrinfo failed with error: %d\n", result);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
    }

    // Create our connection [Socket]
    SOCKET connectSocket;
    printf("Creating our Connection Socket . . . ");
    connectSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (connectSocket == INVALID_SOCKET) {
        printf("socket failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(info);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
    }

    // [Connect] to the server
    printf("Connect to the server . . . ");
    result = connect(connectSocket, info->ai_addr, (int)info->ai_addrlen);
    if (result == SOCKET_ERROR) {
        printf("Failed to connect to the server with error: %d\n", WSAGetLastError());
        closesocket(connectSocket);
        freeaddrinfo(info);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
    }

    // send/recv buffer
    constexpr int kRECV_BUF_SIZE = 512;
    char rawRecvBuf[kRECV_BUF_SIZE];

    constexpr int kSEND_BUF_SIZE = 512;
    Buffer sendBuf{kSEND_BUF_SIZE};

    C2S_LoginReqMsg msg{"fan", "fan"};
    msg.Serialize(sendBuf);

    // [send]
    printf("Sending message to the server . . . ");
    result = send(connectSocket, sendBuf.ConstData(), msg.header.packetSize, 0);
    if (result == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
        printf("Sent %d bytes to the server!\n", result);
    }

    bool tryAgain = true;

    while (tryAgain) {
        result = recv(connectSocket, rawRecvBuf, kRECV_BUF_SIZE, 0);
        // Expected result values:
        // 0 = closed connection, disconnection
        // >0 = number of bytes received
        // -1 = SOCKET_ERROR
        //
        // NonBlocking recv, it will immediately return
        // result will be -1
        if (result == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                // printf("WouldBlock!\n");
                tryAgain = true;
            } else {
                printf("recv failed with error: %d\n", WSAGetLastError());
                closesocket(connectSocket);
                WSACleanup();
                return 1;
            }
        } else {
            printf("Success!\n");
            printf("recv %d bytes from the server!\n", result);

            Buffer recvBuf{rawRecvBuf, kRECV_BUF_SIZE};
            uint32_t packetSize = recvBuf.ReadUInt32LE();
            MessageType messageType = static_cast<MessageType>(recvBuf.ReadUInt32LE());

            if (recvBuf.Size() >= packetSize) {
                HandleMessage(messageType, recvBuf);
            }

            tryAgain = false;
        }
    }

    // Close
    printf("Shutdown . . . ");
    result = shutdown(connectSocket, SD_SEND);
    if (result == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    } else {
        printf("Success!\n");
    }

    printf("Closing . . . \n");
    closesocket(connectSocket);
    WSACleanup();

    return 0;
}
