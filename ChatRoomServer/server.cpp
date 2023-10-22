#include "server.h"

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdio.h>

using namespace network;

ChatRoomServer::ChatRoomServer(uint16 port) {
    m_RoomNames.push_back("graphics");
    m_RoomNames.push_back("network");

    int result = Initialize(port);
    if (result != 0) {
        //
    }
}

ChatRoomServer::~ChatRoomServer() { Shutdown(); }

int ChatRoomServer::RunLoop() {  // Define timeout for select()
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000;  // 500 milliseconds, half a second

    int selectResult;

    // select work here
    for (;;) {
        // SocketsReadyForReading will be empty here
        FD_ZERO(&g_Connection.socketsReadyForReading);

        // Add all the sockets that have data ready to be recv'd
        // to the socketsReadyForReading
        //
        // socketsReadyForReading contains:
        //  ListenSocket
        //  All connectedClients' socket
        //
        // 1. Add the listen socket, to see if there are any new connections
        FD_SET(g_Connection.listenSocket, &g_Connection.socketsReadyForReading);

        // 2. Add all the connected sockets, to see if the is any information
        //    to be recieved from the connected clients.
        for (int i = 0; i < g_Connection.clients.size(); i++) {
            ClientInfo& client = g_Connection.clients[i];
            if (client.connected) {
                FD_SET(client.socket, &g_Connection.socketsReadyForReading);
            }
        }

        // Select will check all sockets in the SocketsReadyForReading set
        // to see if there is any data to be read on the socket.
        // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
        selectResult = select(0, &g_Connection.socketsReadyForReading, NULL, NULL, &tv);
        if (selectResult == 0) {
            // Time limit expired
            continue;
        }
        if (selectResult == SOCKET_ERROR) {
            printf("select failed with error: %d\n", WSAGetLastError());
            return selectResult;
        }
        printf(".");

        // Check if our ListenSocket is set. This checks if there is a
        // new client trying to connect to the server using a "connect"
        // function call.
        if (FD_ISSET(g_Connection.listenSocket, &g_Connection.socketsReadyForReading)) {
            printf("\n");

            // [Accept]
            SOCKET clientSocket = accept(g_Connection.listenSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) {
                printf("accept failed with error: %d\n", WSAGetLastError());
            } else {
                printf("accept OK!\n");
                ClientInfo newClient;
                newClient.socket = clientSocket;
                newClient.connected = true;
                g_Connection.clients.push_back(newClient);
            }
        }

        // Check if any of the currently connected clients have sent data using send
        for (int i = 0; i < g_Connection.clients.size(); i++) {
            ClientInfo& client = g_Connection.clients[i];

            if (!client.connected) continue;

            if (FD_ISSET(client.socket, &g_Connection.socketsReadyForReading)) {
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
                    printf("Client disconnected!\n");
                    client.connected = false;
                    continue;
                }

                printf("Received %d bytes from the client!\n", recvResult);

                // We must receive 4 bytes before we know how long the packet actually is
                // We must receive the entire packet before we can handle the message.
                // Our protocol says we have a HEADER[pktsize, messagetype];
                Buffer recvBuf{m_RawRecvBuf, kRECV_BUF_SIZE};
                uint32_t packetSize = recvBuf.ReadUInt32LE();
                MessageType messageType = static_cast<MessageType>(recvBuf.ReadUInt32LE());

                if (recvBuf.Size() >= packetSize) {
                    // We can finally handle our message
                    HandleMessage(messageType, client, recvBuf, m_SendBuf);
                }

                FD_CLR(client.socket, &g_Connection.socketsReadyForReading);
            }
        }
    }
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

    FD_ZERO(&g_Connection.activeSockets);
    FD_ZERO(&g_Connection.socketsReadyForReading);

    // 1. WSAStartup
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed with error %d\n", result);
        return 1;
    } else {
        printf("WSAStartup OK!\n");
    }

    // 2. getaddrinfo
    ZeroMemory(&g_Connection.hints, sizeof(g_Connection.hints));
    g_Connection.hints.ai_family = AF_INET;        // IPV4
    g_Connection.hints.ai_socktype = SOCK_STREAM;  // Stream
    g_Connection.hints.ai_protocol = IPPROTO_TCP;  // TCP
    g_Connection.hints.ai_flags = AI_PASSIVE;

    result = getaddrinfo(NULL, std::to_string(port).c_str(), &g_Connection.hints, &g_Connection.info);
    if (result != 0) {
        printf("getaddrinfo failed with error: %d\n", result);
        WSACleanup();
        return result;
    } else {
        printf("getaddrinfo ok!\n");
    }

    // 3. Create our listen socket [Socket]
    g_Connection.listenSocket =
        socket(g_Connection.info->ai_family, g_Connection.info->ai_socktype, g_Connection.info->ai_protocol);
    if (g_Connection.listenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(g_Connection.info);
        WSACleanup();
        return result;
    } else {
        printf("socket OK!\n");
    }

    // 4. Bind our socket [Bind]
    // 12,14,1,3:80				Address lengths can be different
    // 123,111,230,109:55555	Must specify the length
    result = bind(g_Connection.listenSocket, g_Connection.info->ai_addr, (int)g_Connection.info->ai_addrlen);
    if (result == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(g_Connection.info);
        closesocket(g_Connection.listenSocket);
        WSACleanup();
        return result;
    } else {
        printf("bind OK!\n");
    }

    // 5. [Listen]
    result = listen(g_Connection.listenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(g_Connection.info);
        closesocket(g_Connection.listenSocket);
        WSACleanup();
        return result;
    } else {
        printf("listen OK!\n");
    }

    return result;
}

// Send response to client
int ChatRoomServer::SendResponse(ClientInfo& client, network::Buffer& sendBuf, network::Message* msg) {
    // https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-send
    int sendResult = send(client.socket, sendBuf.ConstData(), msg->header.packetSize, 0);
    if (sendResult == SOCKET_ERROR) {
        printf("send failed with error %d\n", WSAGetLastError());
        WSACleanup();
    }
    return 0;
}

// Shutdown and cleanup
void ChatRoomServer::Shutdown() {
    printf("closing ...\n");
    freeaddrinfo(g_Connection.info);
    closesocket(g_Connection.listenSocket);
    WSACleanup();
}

// Handle received messages
void ChatRoomServer::HandleMessage(network::MessageType msgType, ClientInfo& client, network::Buffer& recvBuf,
                                   network::Buffer& sendBuf) {
    switch (msgType) {
        // received C2S_LoginReqMsg
        case MessageType::kLOGIN_REQ: {
            uint32_t userNameLength = recvBuf.ReadUInt32LE();
            std::string userName = recvBuf.ReadString(userNameLength);
            uint32_t passwordLength = recvBuf.ReadUInt32LE();
            std::string password = recvBuf.ReadString(passwordLength);

            printf("authenticating user...\n");
            // TODO: user authentication
            printf("user %s logged in.\n", userName.c_str());
            // update client map
            m_ClientMap.insert(std::make_pair(userName, &client));

            // respond with S2C_LoginAckMsg
            S2C_LoginAckMsg msg{MessageStatus::kSUCCESS, m_RoomNames};
            msg.Serialize(sendBuf);
            SendResponse(client, sendBuf, &msg);
        } break;

        // received C2S_JoinRoomReqMsg
        case MessageType::kJOIN_ROOM_REQ: {
            uint32_t userNameLength = recvBuf.ReadUInt32LE();
            std::string userName = recvBuf.ReadString(userNameLength);
            uint32_t roomNameLength = recvBuf.ReadUInt32LE();
            std::string roomName = recvBuf.ReadString(roomNameLength);

            // add the user to room
            std::map<std::string, std::set<std::string>>::iterator it = m_RoomMap.find(roomName);
            if (it != m_RoomMap.end()) {
                std::set<std::string>& usersInRoom = it->second;
                usersInRoom.insert(userName);

                // respond with S2C_JoinRoomAckMsg SUCCESS
                std::vector<std::string> userNames{usersInRoom.begin(), usersInRoom.end()};
                S2C_JoinRoomAckMsg msg{MessageStatus::kSUCCESS, roomName, userNames};
                msg.Serialize(sendBuf);
                SendResponse(client, sendBuf, &msg);

                // broadcast event with S2C_JoinRoomNtfMsg
                for (const std::string& name : usersInRoom) {
                    if (name != userName) {
                        std::map<std::string, ClientInfo*>::iterator it = m_ClientMap.find(name);
                        if (it != m_ClientMap.end()) {
                            S2C_JoinRoomNtfMsg msg{roomName, userName};
                            msg.Serialize(sendBuf);
                            SendResponse(*it->second, sendBuf, &msg);
                        }
                    }
                }

            } else {
                // respond with S2C_JoinRoomAckMsg FAILURE
                std::vector<std::string> placeHolder{};
                S2C_JoinRoomAckMsg msg{MessageStatus::kFAILURE, roomName, placeHolder};
                msg.Serialize(sendBuf);
                SendResponse(client, sendBuf, &msg);
            }

        } break;

        // received C2S_LeaveRoomReqMsg
        case MessageType::kLEAVE_ROOM_REQ: {
            uint32_t roomNameLength = recvBuf.ReadUInt32LE();
            std::string roomName = recvBuf.ReadString(roomNameLength);
            uint32_t userNameLength = recvBuf.ReadUInt32LE();
            std::string userName = recvBuf.ReadString(userNameLength);

            // remove the user from room
            std::map<std::string, std::set<std::string>>::iterator it = m_RoomMap.find(roomName);
            if (it != m_RoomMap.end()) {
                std::set<std::string>& usersInRoom = it->second;
                std::set<std::string>::iterator uit = usersInRoom.find(userName);
                if (uit != usersInRoom.end()) {
                    usersInRoom.erase(uit);
                }

                // respond with S2C_LeaveRoomAckMsg SUCCESS
                S2C_LeaveRoomAckMsg msg{MessageStatus::kSUCCESS, roomName, userName};
                msg.Serialize(sendBuf);
                SendResponse(client, sendBuf, &msg);

                // broadcast event with S2C_LeaveRoomNtfMsg
                for (const std::string& name : usersInRoom) {
                    std::map<std::string, ClientInfo*>::iterator it = m_ClientMap.find(name);
                    if (it != m_ClientMap.end()) {
                        S2C_LeaveRoomNtfMsg msg{roomName, userName};
                        msg.Serialize(sendBuf);
                        SendResponse(*it->second, sendBuf, &msg);
                    }
                }

            } else {
                // respond with S2C_LeaveRoomAckMsg FAILURE
                S2C_LeaveRoomAckMsg msg{MessageStatus::kFAILURE, roomName, userName};
                msg.Serialize(sendBuf);
                SendResponse(client, sendBuf, &msg);
            }

        } break;

        // received C2S_ChatInRoomReqMsg
        case MessageType::kCHAT_IN_ROOM_REQ: {
            uint32_t roomNameLength = recvBuf.ReadUInt32LE();
            std::string roomName = recvBuf.ReadString(roomNameLength);
            uint32_t userNameLength = recvBuf.ReadUInt32LE();
            std::string userName = recvBuf.ReadString(userNameLength);
            uint32_t chatLength = recvBuf.ReadUInt32LE();
            std::string chat = recvBuf.ReadString(chatLength);

            std::map<std::string, std::set<std::string>>::iterator it = m_RoomMap.find(roomName);
            if (it != m_RoomMap.end()) {
                // respond with S2C_ChatInRoomAckMsg SUCCESS
                S2C_ChatInRoomAckMsg msg{MessageStatus::kSUCCESS, roomName, userName};
                msg.Serialize(sendBuf);
                SendResponse(client, sendBuf, &msg);

                // broadcast event with S2C_ChatInRoomNtfMsg
                std::set<std::string>& usersInRoom = it->second;
                for (const std::string& name : usersInRoom) {
                    std::map<std::string, ClientInfo*>::iterator it = m_ClientMap.find(name);
                    if (it != m_ClientMap.end()) {
                        S2C_ChatInRoomNtfMsg msg{roomName, userName, chat};
                        msg.Serialize(sendBuf);
                        SendResponse(*it->second, sendBuf, &msg);
                    }
                }
            } else {
                // respond with S2C_ChatInRoomAckMsg FAILURE
                S2C_ChatInRoomAckMsg msg{MessageStatus::kFAILURE, roomName, userName};
                msg.Serialize(sendBuf);
                SendResponse(client, sendBuf, &msg);
            }

        } break;

        default:
            printf("Unknown message.\n");
            break;
    }
}
