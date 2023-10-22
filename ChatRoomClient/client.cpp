#include "client.h"

#include <WS2tcpip.h>

#include <iostream>

using namespace network;

ChatRoomClient::ChatRoomClient(const std::string& host, uint16 port) {
    // init chatroom logic stuff
    m_JoinedRoomMap.clear();
    m_JoinedRoomNames.clear();

    // init networking stuff
    int result = Initialize(host, port);
    if (result == 0) {
        //
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
        printf("\tsent msg %d (%d bytes) to the server!\n", msg->header.messageType, result);
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
            m_RecvBuf.Set(m_RawRecvBuf, kRECV_BUF_SIZE);
            uint32_t packetSize = m_RecvBuf.ReadUInt32LE();
            MessageType messageType = static_cast<MessageType>(m_RecvBuf.ReadUInt32LE());

            printf("\trecv msg %d (%d bytes) from the server!\n", messageType, result);

            if (m_RecvBuf.Size() >= packetSize) {
                HandleMessage(messageType);
            }

            tryAgain = false;
        }
    }
    // the blocking version
    // Receive until the peer closes the connection
    // do {
    //    result = recv(m_ConnectSocket, m_RawRecvBuf, kRECV_BUF_SIZE, 0);
    //    if (result > 0) {
    //        printf("received: %d bytes\n", result);
    //        Buffer m_RecvBuf{m_RawRecvBuf, kRECV_BUF_SIZE};
    //        uint32_t packetSize = m_RecvBuf.ReadUInt32LE();
    //        MessageType messageType = static_cast<MessageType>(m_RecvBuf.ReadUInt32LE());

    //        if (m_RecvBuf.Size() >= packetSize) {
    //            HandleMessage(messageType);
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
    m_MyUserName = userName;

    C2S_LoginReqMsg msg{userName, password};
    msg.Serialize(m_SendBuf);

    return SendRequest(&msg);
}

// [send] C2S_JoinRoomReqMsg
int ChatRoomClient::ReqJoinRoom(const std::string& roomName) {
    C2S_JoinRoomReqMsg msg{m_MyUserName, roomName};
    msg.Serialize(m_SendBuf);

    return SendRequest(&msg);
}

// [send] C2S_LeaveRoomReqMsg
int ChatRoomClient::ReqLeaveRoom(const std::string& roomName) {
    C2S_LeaveRoomReqMsg msg{roomName, m_MyUserName};
    msg.Serialize(m_SendBuf);

    return SendRequest(&msg);
}

// [send] C2S_ChatInRoomReqMsg
int ChatRoomClient::ReqChatInRoom(const std::string& roomName, const std::string chat) {
    C2S_ChatInRoomReqMsg msg{roomName, m_MyUserName, chat};
    msg.Serialize(m_SendBuf);

    return SendRequest(&msg);
}

// print the rooms
void ChatRoomClient::PrintRooms(const std::vector<std::string>& roomNames) const {
    std::cout << "----Rooms----\n";
    for (size_t i = 0; i < roomNames.size(); i++) {
        std::cout << i + 1 << " " << roomNames[i] << std::endl;
    }
}

// print the users in a room
void ChatRoomClient::PrintUsersInRoom(const std::string& roomName) const {
    std::cout << "----Users in #" << roomName << "----\n";
    std::map<std::string, std::set<std::string>>::const_iterator cit = m_JoinedRoomMap.find(roomName);
    if (cit != m_JoinedRoomMap.end()) {
        const std::set<std::string>& usersInRoom = cit->second;
        size_t i = 1;
        for (const std::string& name : usersInRoom) {
            std::cout << i << " " << name << std::endl;
            i++;
        }
    }
}

// Handle received messages
void ChatRoomClient::HandleMessage(network::MessageType msgType) {
    switch (msgType) {
        // login ACK
        case MessageType::kLOGIN_ACK: {
            uint16 status = m_RecvBuf.ReadUInt16LE();
            if (status == MessageStatus::kSUCCESS) {
                uint32 roomListLength = m_RecvBuf.ReadUInt32LE();
                std::vector<uint32> roomNameLengths;
                for (size_t i = 0; i < roomListLength; i++) {
                    roomNameLengths.push_back(m_RecvBuf.ReadUInt32LE());
                }

                std::vector<std::string> roomNames;
                for (size_t i = 0; i < roomListLength; i++) {
                    std::string roomName = m_RecvBuf.ReadString(roomNameLengths[i]);
                    roomNames.push_back(roomName);
                }

                PrintRooms(roomNames);
            } else {
                m_ClientState = ClientState::kOFFLINE;
                printf("loign failed, status: %d\n", status);
            }
        } break;

        // join room ACK
        case MessageType::kJOIN_ROOM_ACK: {
            uint16 status = m_RecvBuf.ReadUInt16LE();
            if (status == MessageStatus::kSUCCESS) {
                uint32 roomNameLength = m_RecvBuf.ReadUInt32LE();
                std::string roomName = m_RecvBuf.ReadString(roomNameLength);

                uint32 userListLength = m_RecvBuf.ReadUInt32LE();
                std::vector<uint32> userNameLengths;
                for (size_t i = 0; i < userListLength; i++) {
                    userNameLengths.push_back(m_RecvBuf.ReadUInt32LE());
                }

                std::set<std::string> userNames;
                for (size_t i = 0; i < userListLength; i++) {
                    std::string userName = m_RecvBuf.ReadString(userNameLengths[i]);
                    userNames.insert(userName);
                }
                // update JoinedRoomNames & JoinedRoomMap
                m_JoinedRoomNames.insert(roomName);
                std::map<std::string, std::set<std::string>>::iterator it = m_JoinedRoomMap.find(roomName);
                if (it != m_JoinedRoomMap.end()) {
                    (it->second).merge(userNames);
                } else {
                    m_JoinedRoomMap.insert(std::make_pair(roomName, userNames));
                }

                printf("join room #%s OK\n", roomName.c_str());
                printf("joined rooms: ");
                for (const std::string& room : m_JoinedRoomNames) {
                    std::cout << room << " ";
                }
                printf("\n");

                PrintUsersInRoom(roomName);
            } else {
                m_ClientState = ClientState::kOFFLINE;
                printf("join room failed, status: %d\n", status);
            }
        } break;

        // join room NTF
        case MessageType::kJOIN_ROOM_NTF: {
            uint32 roomNameLength = m_RecvBuf.ReadUInt32LE();
            std::string roomName = m_RecvBuf.ReadString(roomNameLength);
            uint32 userNameLength = m_RecvBuf.ReadUInt32LE();
            std::string userName = m_RecvBuf.ReadString(userNameLength);

            printf("'%s' has joined room #%s\n", userName.c_str(), roomName.c_str());
            // update JoinedRoomMap
            std::map<std::string, std::set<std::string>>::iterator it = m_JoinedRoomMap.find(roomName);
            if (it != m_JoinedRoomMap.end()) {
                (it->second).insert(userName);
            }
            PrintUsersInRoom(roomName);
        } break;

        // leave room ACK
        case MessageType::kLEAVE_ROOM_ACK: {
            uint16 status = m_RecvBuf.ReadUInt16LE();
            if (status == MessageStatus::kSUCCESS) {
                uint32 roomNameLength = m_RecvBuf.ReadUInt32LE();
                std::string roomName = m_RecvBuf.ReadString(roomNameLength);
                uint32 userNameLength = m_RecvBuf.ReadUInt32LE();
                std::string userName = m_RecvBuf.ReadString(userNameLength);

                // update JoinedRoomNames & JoinedRoomMap
                m_JoinedRoomNames.erase(roomName);
                std::map<std::string, std::set<std::string>>::iterator it = m_JoinedRoomMap.find(roomName);
                if (it != m_JoinedRoomMap.end()) {
                    (it->second).erase(userName);
                }

                printf("leaved room #%s OK\n", roomName.c_str());
                printf("joined rooms: ");
                for (const std::string& room : m_JoinedRoomNames) {
                    std::cout << room << " ";
                }
                printf("\n");
            } else {
                m_ClientState = ClientState::kOFFLINE;
                printf("leave room failed, status: %d\n", status);
            }
        } break;

        // leave room NTF
        case MessageType::kLEAVE_ROOM_NTF: {
            uint32 roomNameLength = m_RecvBuf.ReadUInt32LE();
            std::string roomName = m_RecvBuf.ReadString(roomNameLength);
            uint32 userNameLength = m_RecvBuf.ReadUInt32LE();
            std::string userName = m_RecvBuf.ReadString(userNameLength);

            printf("'%s' has left room #%s\n", userName.c_str(), roomName.c_str());
            // update JoinedRoomMap
            std::map<std::string, std::set<std::string>>::iterator it = m_JoinedRoomMap.find(roomName);
            if (it != m_JoinedRoomMap.end()) {
                (it->second).erase(userName);
            }
            PrintUsersInRoom(roomName);
        } break;

        // chat in room ACK
        case MessageType::kCHAT_IN_ROOM_ACK: {
            uint16 status = m_RecvBuf.ReadUInt16LE();
            if (status == MessageStatus::kSUCCESS) {
                uint32 roomNameLength = m_RecvBuf.ReadUInt32LE();
                std::string roomName = m_RecvBuf.ReadString(roomNameLength);
                uint32 userNameLength = m_RecvBuf.ReadUInt32LE();
                std::string userName = m_RecvBuf.ReadString(userNameLength);

                printf("chat OK.\n");
            } else {
                m_ClientState = ClientState::kOFFLINE;
                printf("chat failed, status: %d\n", status);
            }
        } break;

        // chat in room NTF
        case MessageType::kCHAT_IN_ROOM_NTF: {
            uint32 roomNameLength = m_RecvBuf.ReadUInt32LE();
            std::string roomName = m_RecvBuf.ReadString(roomNameLength);
            uint32 userNameLength = m_RecvBuf.ReadUInt32LE();
            std::string userName = m_RecvBuf.ReadString(userNameLength);
            uint32 chatLength = m_RecvBuf.ReadUInt32LE();
            std::string chat = m_RecvBuf.ReadString(chatLength);

            printf("'%s' - #%s: %s\n", userName.c_str(), roomName.c_str(), chat.c_str());
        } break;

        default:
            printf("unknown message.\n");
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
