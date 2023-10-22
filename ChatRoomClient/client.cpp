#include "client.h"

#include <WS2tcpip.h>

#include <iostream>

using namespace network;

ChatRoomClient::ChatRoomClient(const std::string& host, uint16 port) {
    int result = Initialize(host, port);
    if (result == 0) {
        // m_RecvThread = std::thread{&ChatRoomClient::RecvResponse, this};
    }
}

ChatRoomClient::~ChatRoomClient() { Shutdown(); }

// Initialization includes:
// 1. Initialize Winsock: WSAStartup
// 2. getaddrinfo
// 3. create socket
// 4. connect
// 5. set non-blocking socket
int ChatRoomClient::Initialize(const std::string& host, uint16 port) {
    // Decalre adn initialize variables
    int result;
    WSADATA wsaData;
    m_ConnectSocket = INVALID_SOCKET;
    m_ClientState = ClientState::kOFFLINE;

    // 1. WSAStartup
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != NO_ERROR) {
        printf("WSAStartup failed with error %d\n", result);
        return result;
    } else {
        printf("WSAStartup OK!\n");
    }

    // 2. getaddrinfo
    // specifies the address family,
    // IP address, and port of the server to be connected to.
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;  // IPv4 #.#.#.#, AF_INET6 is IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    result = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &m_AddrInfo);
    if (result != 0) {
        printf("getaddrinfo failed with error: %d\n", result);
        WSACleanup();
        return result;
    } else {
        printf("getaddrinfo OK!\n");
    }

    // 3. Create a SOCKET for connecting to server [Socket]
    m_ConnectSocket = socket(m_AddrInfo->ai_family, m_AddrInfo->ai_socktype, m_AddrInfo->ai_protocol);
    if (m_ConnectSocket == INVALID_SOCKET) {
        printf("socket failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(m_AddrInfo);
        WSACleanup();
        return -1;
    } else {
        printf("socket OK!\n");
    }

    // 4. [Connect] to the server
    result = connect(m_ConnectSocket, m_AddrInfo->ai_addr, (int)m_AddrInfo->ai_addrlen);
    if (result == SOCKET_ERROR) {
        printf("connect failed with error: %d\n", WSAGetLastError());
        closesocket(m_ConnectSocket);
        freeaddrinfo(m_AddrInfo);
        WSACleanup();
        return result;
    } else {
        printf("connect OK!\n");
        m_ClientState = ClientState::kONLINE;
    }

    // 5. [ioctlsocket] input output control socket, makes it Non-blocking
    DWORD NonBlock = 1;
    result = ioctlsocket(m_ConnectSocket, FIONBIO, &NonBlock);
    if (result == SOCKET_ERROR) {
        printf("ioctlsocket to failed with error: %d\n", WSAGetLastError());
        closesocket(m_ConnectSocket);
        freeaddrinfo(m_AddrInfo);
        WSACleanup();
        return result;
    }

    return result;
}

// Send request to server
int ChatRoomClient::SendRequest(network::Message* msg) {
    int result = send(m_ConnectSocket, m_SendBuf.ConstData(), msg->header.packetSize, 0);
    if (result == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(m_ConnectSocket);
        WSACleanup();
        return result;
    } else {
        printf("sent msg #%d (%d bytes) to the server!\n", msg->header.messageType, result);
    }

    return result;
}

// Receive response from server
int ChatRoomClient::RecvResponse() {
    int result = 0;
    // the non-blocking version
    // remember to use ioctlsocket() to set the socket i/o mode first
    bool tryAgain = true;
    while (tryAgain) {
        ZeroMemory(m_RawRecvBuf, kRECV_BUF_SIZE);
        result = recv(m_ConnectSocket, m_RawRecvBuf, kRECV_BUF_SIZE, 0);
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
                closesocket(m_ConnectSocket);
                WSACleanup();
                return result;
            }
        } else {
            Buffer recvBuf{m_RawRecvBuf, kRECV_BUF_SIZE};
            uint32_t packetSize = recvBuf.ReadUInt32LE();
            MessageType messageType = static_cast<MessageType>(recvBuf.ReadUInt32LE());

            printf("recv msg #%d (%d bytes) from the server!\n", messageType, result);

            if (recvBuf.Size() >= packetSize) {
                HandleMessage(messageType, recvBuf);
            }

            tryAgain = false;
        }
    }
    // the blocking version
    //// Receive until the peer closes the connection
    // do {
    //     result = recv(m_ConnectSocket, m_RawRecvBuf, kRECV_BUF_SIZE, 0);
    //     if (result > 0) {
    //         printf("received: %d bytes\n", result);
    //         Buffer recvBuf{m_RawRecvBuf, kRECV_BUF_SIZE};
    //         uint32_t packetSize = recvBuf.ReadUInt32LE();
    //         MessageType messageType = static_cast<MessageType>(recvBuf.ReadUInt32LE());

    //        if (recvBuf.Size() >= packetSize) {
    //            HandleMessage(messageType, recvBuf);
    //        }
    //    } else if (result == 0) {
    //        printf("Connection closed\n");
    //    } else {
    //        printf("recv failed with error: %d\n", WSAGetLastError());
    //    }

    //} while (result > 0);
    return result;
}

// [send] C2S_LoginReqMsg
int ChatRoomClient::ReqLogin(const std::string& userName, const std::string& password) {
    m_UserName = userName;

    C2S_LoginReqMsg msg{userName, password};
    msg.Serialize(m_SendBuf);

    return SendRequest(&msg);
}

// [send] C2S_JoinRoomReqMsg
int ChatRoomClient::ReqJoinRoom(const std::string& roomName) {
    C2S_JoinRoomReqMsg msg{m_UserName, roomName};
    msg.Serialize(m_SendBuf);

    return SendRequest(&msg);
}

// [send] C2S_LeaveRoomReqMsg
int ChatRoomClient::ReqLeaveRoom(const std::string& roomName) {
    C2S_LeaveRoomReqMsg msg{roomName, m_UserName};
    msg.Serialize(m_SendBuf);

    return SendRequest(&msg);
}

// [send] C2S_ChatInRoomReqMsg
int ChatRoomClient::ReqChatInRoom(const std::string& roomName, const std::string chat) {
    C2S_ChatInRoomReqMsg msg{roomName, m_UserName, chat};
    msg.Serialize(m_SendBuf);

    return SendRequest(&msg);
}

void ChatRoomClient::PrintRooms() const {
    printf("----Rooms----\n");
    for (size_t i = 0; i < m_RoomNames.size(); i++) {
        printf("%d. %s\n", i + 1, m_RoomNames[i].c_str());
    }
    printf("-------------\n");
}

void ChatRoomClient::PrintUsers() const {
    printf("----Users----\n");
    for (size_t i = 0; i < m_UserNames.size(); i++) {
        printf("%d. %s\n", i + 1, m_UserNames[i].c_str());
    }
    printf("-------------\n");
}

// Handle received messages
void ChatRoomClient::HandleMessage(network::MessageType msgType, network::Buffer& recvBuf) {
    switch (msgType) {
        // login ACK
        case MessageType::kLOGIN_ACK: {
            uint16 status = recvBuf.ReadUInt16LE();
            if (status == MessageStatus::kSUCCESS) {
                uint32 roomListLength = recvBuf.ReadUInt32LE();
                std::vector<uint32> roomNameLengths;
                for (size_t i = 0; i < roomListLength; i++) {
                    roomNameLengths.push_back(recvBuf.ReadUInt32LE());
                }

                m_RoomNames.clear();
                for (size_t i = 0; i < roomListLength; i++) {
                    std::string roomName = recvBuf.ReadString(roomNameLengths[i]);
                    m_RoomNames.push_back(roomName);
                }

                PrintRooms();
            } else {
                m_ClientState = ClientState::kOFFLINE;
                printf("Loign failed, status: %d\n", status);
            }
        } break;

        // join room ACK
        case MessageType::kJOIN_ROOM_ACK: {
            uint16 status = recvBuf.ReadUInt16LE();
            if (status == MessageStatus::kSUCCESS) {
                uint32 roomNameLength = recvBuf.ReadUInt32LE();
                std::string roomName = recvBuf.ReadString(roomNameLength);

                uint32 userListLength = recvBuf.ReadUInt32LE();
                std::vector<uint32> userNameLengths;
                for (size_t i = 0; i < userListLength; i++) {
                    userNameLengths.push_back(recvBuf.ReadUInt32LE());
                }

                m_UserNames.clear();
                for (size_t i = 0; i < userListLength; i++) {
                    std::string userName = recvBuf.ReadString(userNameLengths[i]);
                    m_UserNames.push_back(userName);
                }

                PrintUsers();
            } else {
                m_ClientState = ClientState::kOFFLINE;
                printf("JoinRoom failed, status: %d\n", status);
            }
        } break;

        // join room NTF
        case MessageType::kJOIN_ROOM_NTF: {
            uint32 roomNameLength = recvBuf.ReadUInt32LE();
            std::string roomName = recvBuf.ReadString(roomNameLength);
            uint32 userNameLength = recvBuf.ReadUInt32LE();
            std::string userName = recvBuf.ReadString(userNameLength);

            printf("\'%s\' has joined room [%s]", userName.c_str(), roomName.c_str());
        } break;

        // leave room ACK
        case MessageType::kLEAVE_ROOM_ACK: {
            uint16 status = recvBuf.ReadUInt16LE();
            if (status == MessageStatus::kSUCCESS) {
            } else {
                m_ClientState = ClientState::kOFFLINE;
                printf("LeaveRoom failed, status: %d\n", status);
            }
        } break;

        // leave room NTF
        case MessageType::kLEAVE_ROOM_NTF: {
            uint32 roomNameLength = recvBuf.ReadUInt32LE();
            std::string roomName = recvBuf.ReadString(roomNameLength);
            uint32 userNameLength = recvBuf.ReadUInt32LE();
            std::string userName = recvBuf.ReadString(userNameLength);

            printf("\'%s\' has joined room [%s]", userName.c_str(), roomName.c_str());
        } break;

        // chat in room ACK
        case MessageType::kCHAT_IN_ROOM_ACK: {
            uint16 status = recvBuf.ReadUInt16LE();
            if (status == MessageStatus::kSUCCESS) {
            } else {
                m_ClientState = ClientState::kOFFLINE;
                printf("ChatInRoom failed, status: %d\n", status);
            }
        } break;

        // chat in room NTF
        case MessageType::kCHAT_IN_ROOM_NTF: {
            uint32 roomNameLength = recvBuf.ReadUInt32LE();
            std::string roomName = recvBuf.ReadString(roomNameLength);
            uint32 userNameLength = recvBuf.ReadUInt32LE();
            std::string userName = recvBuf.ReadString(userNameLength);
            uint32 chatLength = recvBuf.ReadUInt32LE();
            std::string chat = recvBuf.ReadString(chatLength);

            printf("\'%s\' in room [%s] says: %s", userName.c_str(), roomName.c_str(), chat.c_str());
        } break;

        default:
            printf("Unknown message.\n");
            break;
    }
}

// Shutdown and cleanup include:
// 1. shutdown socket
// 2. close socket
// 3. freeaddrinfo
// 4. WSACleanup
int ChatRoomClient::Shutdown() {
    int result = shutdown(m_ConnectSocket, SD_SEND);
    if (result == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        return 1;
    } else {
        printf("shutdown OK!\n");
    }

    printf("closing socket ... \n");
    closesocket(m_ConnectSocket);
    freeaddrinfo(m_AddrInfo);
    WSACleanup();

    return result;
}
