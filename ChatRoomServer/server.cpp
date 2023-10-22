#include "server.h"

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdio.h>

using namespace network;

ChatRoomServer::ChatRoomServer(uint16 port) {
    // init chatroom logic stuff
    m_RoomNames.push_back("graphics");
    m_RoomNames.push_back("network");
    m_RoomNames.push_back("media");
    m_RoomNames.push_back("configuration");

    std::set<std::string> emptyUserSet{};
    m_RoomMap.insert(std::make_pair("graphics", emptyUserSet));
    m_RoomMap.insert(std::make_pair("network", emptyUserSet));
    m_RoomMap.insert(std::make_pair("media", emptyUserSet));
    m_RoomMap.insert(std::make_pair("configuration", emptyUserSet));

    // init networking stuff
    int result = Initialize(port);
    if (result != 0) {
        //
    }
}

ChatRoomServer::~ChatRoomServer() { Shutdown(); }

int ChatRoomServer::RunLoop() {
    FD_ZERO(&(m_Conn.activeSockets));  // Initialize the sets
    FD_ZERO(&(m_Conn.socketsReadyForReading));

    // Define timeout for select()
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000;  // 500 milliseconds, half a second

    int selectResult;

    // select work here
    for (;;) {
        // SocketsReadyForReading will be empty here
        FD_ZERO(&m_Conn.socketsReadyForReading);

        // Add all the sockets that have data ready to be recv'd
        // to the socketsReadyForReading
        //
        // socketsReadyForReading contains:
        //  ListenSocket
        //  All connectedClients' socket
        //
        // 1. Add the listen socket, to see if there are any new connections
        FD_SET(m_Conn.listenSocket, &m_Conn.socketsReadyForReading);

        // 2. Add all the connected sockets, to see if the is any information
        //    to be recieved from the connected clients.
        for (int i = 0; i < m_Conn.clients.size(); i++) {
            ClientInfo& client = m_Conn.clients[i];
            if (client.connected) {
                FD_SET(client.socket, &m_Conn.socketsReadyForReading);
            }
        }

        // Select will check all sockets in the SocketsReadyForReading set
        // to see if there is any data to be read on the socket.
        // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
        selectResult = select(0, &m_Conn.socketsReadyForReading, NULL, NULL, &tv);
        if (selectResult == 0) {
            // Time limit expired
            continue;
        }
        if (selectResult == SOCKET_ERROR) {
            printf("select failed with error: %d\n", WSAGetLastError());
            return selectResult;
        }

        // Check if our ListenSocket is set. This checks if there is a
        // new client trying to connect to the server using a "connect"
        // function call.
        if (FD_ISSET(m_Conn.listenSocket, &m_Conn.socketsReadyForReading)) {
            // [Accept]
            SOCKET clientSocket = accept(m_Conn.listenSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) {
                printf("accept failed with error: %d\n", WSAGetLastError());
            } else {
                printf("accept OK!\n");
                ClientInfo newClient;
                newClient.socket = clientSocket;
                newClient.connected = true;
                m_Conn.clients.push_back(newClient);
            }
        }

        // Check if any of the currently connected clients have sent data using send
        for (int i = 0; i < m_Conn.clients.size(); i++) {
            ClientInfo& client = m_Conn.clients[i];

            if (!client.connected) continue;

            if (FD_ISSET(client.socket, &m_Conn.socketsReadyForReading)) {
                // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-recv
                // result
                //		-1 : SOCKET_ERROR (More info received from WSAGetLastError() after)
                //		0 : client disconnected
                //		>0: The number of bytes received.
                ZeroMemory(&m_RawRecvBuf, kRECV_BUF_SIZE);
                int recvResult = recv(client.socket, m_RawRecvBuf, kRECV_BUF_SIZE, 0);

                if (recvResult < 0) {
                    printf("recv failed: %d\n", WSAGetLastError());
                    continue;
                }

                // Saving some time to not modify a vector while
                // iterating through it. Want remove the client
                // from the vector
                if (recvResult == 0) {
                    printf("client disconnected!\n");
                    client.connected = false;
                    continue;
                }

                printf("recv %d bytes from client.\n", recvResult);

                // We must receive 4 bytes before we know how long the packet actually is
                // We must receive the entire packet before we can handle the message.
                // Our protocol says we have a HEADER[pktsize, messagetype];
                m_RecvBuf.Set(m_RawRecvBuf, kRECV_BUF_SIZE);
                uint32_t packetSize = m_RecvBuf.ReadUInt32LE();
                MessageType messageType = static_cast<MessageType>(m_RecvBuf.ReadUInt32LE());

                if (m_RecvBuf.Size() >= packetSize) {
                    // We can finally handle our message
                    HandleMessage(messageType, client);
                }

                FD_CLR(client.socket, &m_Conn.socketsReadyForReading);
            }
        }
    }
}

// [send] S2C_LoginAckMsg
int ChatRoomServer::AckLogin(ClientInfo& client, MessageStatus status, const std::vector<std::string>& roomNames) {
    S2C_LoginAckMsg msg{MessageStatus::kSUCCESS, m_RoomNames};
    msg.Serialize(m_SendBuf);
    return SendResponse(client, &msg);
}

// [send] S2C_JoinRoomAckMsg
int ChatRoomServer::AckJoinRoom(ClientInfo& client, network::MessageStatus status, const std::string& roomName,
                                std::vector<std::string>& userNames) {
    S2C_JoinRoomAckMsg msg{static_cast<uint16>(status), roomName, userNames};
    msg.Serialize(m_SendBuf);
    return SendResponse(client, &msg);
}

// [send] S2C_JoinRoomNtfMsg
int ChatRoomServer::BroadcastJoinRoom(const std::set<std::string>& usersInRoom, const std::string& roomName,
                                      const std::string& userName) {
    for (const std::string& name : usersInRoom) {
        if (name != userName) {
            std::map<std::string, size_t>::iterator it = m_ClientMap.find(name);
            if (it != m_ClientMap.end()) {
                S2C_JoinRoomNtfMsg msg{roomName, userName};
                msg.Serialize(m_SendBuf);
                ClientInfo& client = m_Conn.clients.at(it->second);
                SendResponse(client, &msg);
            }
        }
    }
    return 0;
}

// [send] S2C_LeaveRoomAckMsg
int ChatRoomServer::AckLeaveRoom(ClientInfo& client, network::MessageStatus status, const std::string& roomName,
                                 const std::string& userName) {
    S2C_LeaveRoomAckMsg msg{static_cast<uint16>(status), roomName, userName};
    msg.Serialize(m_SendBuf);
    return SendResponse(client, &msg);
}

// [send] S2C_LeaveRoomNtfMsg
int ChatRoomServer::BroadcastLeaveRoom(const std::set<std::string>& usersInRoom, const std::string& roomName,
                                       const std::string& userName) {
    for (const std::string& name : usersInRoom) {
        std::map<std::string, size_t>::iterator it = m_ClientMap.find(name);
        if (it != m_ClientMap.end()) {
            S2C_LeaveRoomNtfMsg msg{roomName, userName};
            msg.Serialize(m_SendBuf);
            ClientInfo& client = m_Conn.clients.at(it->second);
            SendResponse(client, &msg);
        }
    }
    return 0;
}

// [send] S2C_ChatInRoomAckMsg
int ChatRoomServer::AckChatInRoom(ClientInfo& client, network::MessageStatus status, const std::string& roomName,
                                  const std::string& userName) {
    S2C_ChatInRoomAckMsg msg{MessageStatus::kSUCCESS, roomName, userName};
    msg.Serialize(m_SendBuf);
    return SendResponse(client, &msg);
}

// [send] S2C_ChatInRoomNtfMsg
int ChatRoomServer::BroadcastChatInRoom(const std::set<std::string>& usersInRoom, const std::string& roomName,
                                        const std::string& userName, const std::string& chat) {
    for (const std::string& name : usersInRoom) {
        std::map<std::string, size_t>::iterator it = m_ClientMap.find(name);
        if (it != m_ClientMap.end()) {
            S2C_ChatInRoomNtfMsg msg{roomName, userName, chat};
            msg.Serialize(m_SendBuf);
            ClientInfo& client = m_Conn.clients.at(it->second);
            SendResponse(client, &msg);
        }
    }
    return 0;
}

// Initialization includes:
// 1. Initialize Winsock: WSAStartup
// 2. getaddrinfo
// 3. create socket
// 4. bind
// 5. listen
int ChatRoomServer::Initialize(uint16 port) {
    // Decalre adn initialize variables
    WSADATA wsaData;
    int result;

    FD_ZERO(&m_Conn.activeSockets);
    FD_ZERO(&m_Conn.socketsReadyForReading);

    // 1. WSAStartup
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed with error %d\n", result);
        return 1;
    } else {
        printf("WSAStartup OK!\n");
    }

    // 2. getaddrinfo
    ZeroMemory(&m_Conn.hints, sizeof(m_Conn.hints));
    m_Conn.hints.ai_family = AF_INET;        // IPV4
    m_Conn.hints.ai_socktype = SOCK_STREAM;  // Stream
    m_Conn.hints.ai_protocol = IPPROTO_TCP;  // TCP
    m_Conn.hints.ai_flags = AI_PASSIVE;

    result = getaddrinfo(NULL, std::to_string(port).c_str(), &m_Conn.hints, &m_Conn.info);
    if (result != 0) {
        printf("getaddrinfo failed with error: %d\n", result);
        WSACleanup();
        return result;
    } else {
        printf("getaddrinfo ok!\n");
    }

    // 3. Create our listen socket [Socket]
    m_Conn.listenSocket =
        socket(m_Conn.info->ai_family, m_Conn.info->ai_socktype, m_Conn.info->ai_protocol);
    if (m_Conn.listenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(m_Conn.info);
        WSACleanup();
        return result;
    } else {
        printf("socket OK!\n");
    }

    // 4. Bind our socket [Bind]
    // 12,14,1,3:80				Address lengths can be different
    // 123,111,230,109:55555	Must specify the length
    result = bind(m_Conn.listenSocket, m_Conn.info->ai_addr, (int)m_Conn.info->ai_addrlen);
    if (result == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(m_Conn.info);
        closesocket(m_Conn.listenSocket);
        WSACleanup();
        return result;
    } else {
        printf("bind OK!\n");
    }

    // 5. [Listen]
    result = listen(m_Conn.listenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(m_Conn.info);
        closesocket(m_Conn.listenSocket);
        WSACleanup();
        return result;
    } else {
        printf("listen OK!\n");
    }

    return result;
}

// Send response to client
int ChatRoomServer::SendResponse(ClientInfo& client, network::Message* msg) {
    // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-send
    int sendResult = send(client.socket, m_SendBuf.ConstData(), msg->header.packetSize, 0);
    if (sendResult == SOCKET_ERROR) {
        printf("send failed with error %d\n", WSAGetLastError());
        WSACleanup();
    }
    return 0;
}

// Shutdown and cleanup
void ChatRoomServer::Shutdown() {
    printf("closing ...\n");
    freeaddrinfo(m_Conn.info);
    closesocket(m_Conn.listenSocket);
    WSACleanup();
}

// Handle received messages
void ChatRoomServer::HandleMessage(network::MessageType msgType, ClientInfo& client) {
    switch (msgType) {
        // received C2S_LoginReqMsg
        case MessageType::kLOGIN_REQ: {
            uint32_t userNameLength = m_RecvBuf.ReadUInt32LE();
            std::string userName = m_RecvBuf.ReadString(userNameLength);
            uint32_t passwordLength = m_RecvBuf.ReadUInt32LE();
            std::string password = m_RecvBuf.ReadString(passwordLength);

            printf("authenticating user...\n");
            // TODO: user authentication
            printf("'%s' has logged in.\n", userName.c_str());
            // update client map
            for (size_t i = 0; i < m_Conn.clients.size(); i++) {
                if (m_Conn.clients[i].socket == client.socket) {
                    m_ClientMap.insert(std::make_pair(userName, i));
                    break;
                }
            }

            // respond with S2C_LoginAckMsg
            AckLogin(client, MessageStatus::kSUCCESS, m_RoomNames);
        } break;

        // received C2S_JoinRoomReqMsg
        case MessageType::kJOIN_ROOM_REQ: {
            uint32_t userNameLength = m_RecvBuf.ReadUInt32LE();
            std::string userName = m_RecvBuf.ReadString(userNameLength);
            uint32_t roomNameLength = m_RecvBuf.ReadUInt32LE();
            std::string roomName = m_RecvBuf.ReadString(roomNameLength);

            printf("'%s' has joined #%s.\n", userName.c_str(), roomName.c_str());

            // add the user to room
            std::map<std::string, std::set<std::string>>::iterator it = m_RoomMap.find(roomName);
            if (it != m_RoomMap.end()) {
                std::set<std::string>& usersInRoom = it->second;
                usersInRoom.insert(userName);

                // respond with S2C_JoinRoomAckMsg SUCCESS
                std::vector<std::string> userNames{usersInRoom.begin(), usersInRoom.end()};
                AckJoinRoom(client, MessageStatus::kSUCCESS, roomName, userNames);

                // broadcast event with S2C_JoinRoomNtfMsg
                BroadcastJoinRoom(usersInRoom, roomName, userName);
            } else {
                // respond with S2C_JoinRoomAckMsg FAILURE
                std::vector<std::string> placeHolder{};
                AckJoinRoom(client, MessageStatus::kFAILURE, roomName, placeHolder);
            }

        } break;

        // received C2S_LeaveRoomReqMsg
        case MessageType::kLEAVE_ROOM_REQ: {
            uint32_t roomNameLength = m_RecvBuf.ReadUInt32LE();
            std::string roomName = m_RecvBuf.ReadString(roomNameLength);
            uint32_t userNameLength = m_RecvBuf.ReadUInt32LE();
            std::string userName = m_RecvBuf.ReadString(userNameLength);

            printf("'%s' has left #%s.\n", userName.c_str(), roomName.c_str());

            // remove the user from room
            std::map<std::string, std::set<std::string>>::iterator it = m_RoomMap.find(roomName);
            if (it != m_RoomMap.end()) {
                std::set<std::string>& usersInRoom = it->second;
                std::set<std::string>::iterator uit = usersInRoom.find(userName);
                if (uit != usersInRoom.end()) {
                    usersInRoom.erase(uit);
                }

                // respond with S2C_LeaveRoomAckMsg SUCCESS
                AckLeaveRoom(client, MessageStatus::kSUCCESS, roomName, userName);

                // broadcast event with S2C_LeaveRoomNtfMsg
                BroadcastLeaveRoom(usersInRoom, roomName, userName);
            } else {
                // respond with S2C_LeaveRoomAckMsg FAILURE
                AckLeaveRoom(client, MessageStatus::kFAILURE, roomName, userName);
            }

        } break;

        // received C2S_ChatInRoomReqMsg
        case MessageType::kCHAT_IN_ROOM_REQ: {
            uint32_t roomNameLength = m_RecvBuf.ReadUInt32LE();
            std::string roomName = m_RecvBuf.ReadString(roomNameLength);
            uint32_t userNameLength = m_RecvBuf.ReadUInt32LE();
            std::string userName = m_RecvBuf.ReadString(userNameLength);
            uint32_t chatLength = m_RecvBuf.ReadUInt32LE();
            std::string chat = m_RecvBuf.ReadString(chatLength);

            printf("'%s' - #%s: %s.\n", userName.c_str(), roomName.c_str(), chat.c_str());

            std::map<std::string, std::set<std::string>>::iterator it = m_RoomMap.find(roomName);
            if (it != m_RoomMap.end()) {
                // respond with S2C_ChatInRoomAckMsg SUCCESS
                AckChatInRoom(client, MessageStatus::kSUCCESS, roomName, userName);

                // broadcast event with S2C_ChatInRoomNtfMsg
                std::set<std::string>& usersInRoom = it->second;
                BroadcastChatInRoom(usersInRoom, roomName, userName, chat);

            } else {
                // respond with S2C_ChatInRoomAckMsg FAILURE
                AckChatInRoom(client, MessageStatus::kFAILURE, roomName, userName);
            }

        } break;

        default:
            printf("unknown message.\n");
            break;
    }
}
