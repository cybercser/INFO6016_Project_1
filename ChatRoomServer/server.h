#pragma once

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "buffer.h"
#include "message.h"

// Client socket info
struct ClientInfo {
    SOCKET socket;
    bool connected;
};

// All connection related info
struct ConnectionInfo {
    struct addrinfo* info = nullptr;
    struct addrinfo hints;
    SOCKET listenSocket = INVALID_SOCKET;
    fd_set activeSockets;
    fd_set socketsReadyForReading;
    std::vector<ClientInfo> clients;
};

class ChatRoomServer {
public:
    explicit ChatRoomServer(uint16 port);
    ~ChatRoomServer();

    int RunLoop();

private:
    int Initialize(uint16 port);
    int SendResponse(ClientInfo& client, network::Buffer& sendBuf, network::Message* msg);
    void HandleMessage(network::MessageType msgType, ClientInfo& client, network::Buffer& recvBuf,
                       network::Buffer& sendBuf);
    void Shutdown();

private:
    // low-level network stuff
    ConnectionInfo g_Connection;

    // send/recv buffer
    static constexpr int kRECV_BUF_SIZE = 512;
    char m_RawRecvBuf[kRECV_BUF_SIZE];

    static constexpr int kSEND_BUF_SIZE = 512;
    network::Buffer m_SendBuf{kSEND_BUF_SIZE};

    // Server cache
    std::vector<std::string> m_RoomNames;
    std::map<std::string, ClientInfo*> m_ClientMap;          // userName (string) -> ClientInfo*
    std::map<std::string, std::set<std::string>> m_RoomMap;  // roomName (string) -> userNames (set of string)
};
