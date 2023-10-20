// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include "buffer.h"
#include "message.h"

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <string>

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "5555"

using namespace network;

void HandleMessage(MessageType msgType, Buffer& recvBuf);

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

    // [send] - C2S_LoginReqMsg
    C2S_LoginReqMsg loginReqMsg{"fan", "fan"};
    loginReqMsg.Serialize(sendBuf);

    printf("Logining to ChatRoom server . . . ");
    result = send(connectSocket, sendBuf.ConstData(), loginReqMsg.header.packetSize, 0);
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
        ZeroMemory(rawRecvBuf, kRECV_BUF_SIZE);
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

    // [send] - C2S_JoinRoomReqMsg
    C2S_JoinRoomReqMsg joinRoomMsg{"fan", "graphics"};
    joinRoomMsg.Serialize(sendBuf);

    printf("Joining room graphics_1 . . . ");
    result = send(connectSocket, sendBuf.ConstData(), joinRoomMsg.header.packetSize, 0);

    ZeroMemory(rawRecvBuf, kRECV_BUF_SIZE);
    result = recv(connectSocket, rawRecvBuf, kRECV_BUF_SIZE, 0);
    Buffer recvBuf{rawRecvBuf, kRECV_BUF_SIZE};
    uint32_t packetSize = recvBuf.ReadUInt32LE();
    MessageType messageType = static_cast<MessageType>(recvBuf.ReadUInt32LE());

    if (recvBuf.Size() >= packetSize) {
        HandleMessage(messageType, recvBuf);
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
                printf("Loign failed, status: %d\n", loginStatus);
            }
        }

        break;

        case MessageType::JoinRoomAck: {
            uint16 joinStatus = recvBuf.ReadUInt16LE();
            if (joinStatus == 200) {
                uint32 roomNameLength = recvBuf.ReadUInt32LE();
                std::string roomName = recvBuf.ReadString(roomNameLength);

                uint32 userListLength = recvBuf.ReadUInt32LE();
                std::vector<uint32> userNameLengths;
                for (size_t i = 0; i < userListLength; i++) {
                    userNameLengths.push_back(recvBuf.ReadUInt32LE());
                }

                printf("Users in Room %s:\n", roomName.c_str());
                std::vector<std::string> userNames;
                for (size_t i = 0; i < userListLength; i++) {
                    std::string userName = recvBuf.ReadString(userNameLengths[i]);
                    userNames.push_back(userName);
                    printf("%s\n", userName.c_str());
                }
            } else {
                printf("JoinRoom failed, status: %d\n", joinStatus);
            }
        } break;

        case MessageType::LeaveRoomAck:
            break;

        default:
            printf("Unknown message.\n");
            break;
    }
}