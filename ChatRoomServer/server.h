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

// the ChatRoom server
class ChatRoomServer {
public:
    explicit ChatRoomServer(uint16 port);
    ~ChatRoomServer();

    int RunLoop();

    // Responses
    int AckLogin(ClientInfo& client, network::MessageStatus status, const std::vector<std::string>& roomNames);
    int AckJoinRoom(ClientInfo& client, network::MessageStatus status, const std::string& roomName,
                    std::vector<std::string>& userNames);
    int BroadcastJoinRoom(ClientInfo& client, const std::set<std::string>& usersInRoom, const std::string& roomName,
                          const std::string& userName);
    int AckLeaveRoom(ClientInfo& client, network::MessageStatus status, const std::string& roomName,
                     const std::string& userName);
    int BroadcastLeaveRoom(ClientInfo& client, const std::set<std::string>& usersInRoom, const std::string& roomName,
                           const std::string& userName);
    int AckChatInRoom(ClientInfo& client, network::MessageStatus status, const std::string& roomName,
                      const std::string& userName);
    int BroadcastChatInRoom(ClientInfo& client, const std::set<std::string>& usersInRoom, const std::string& roomName,
                            const std::string& userName, const std::string& chat);

private:
    int Initialize(uint16 port);
    int SendResponse(ClientInfo& client, network::Message* msg);
    void HandleMessage(network::MessageType msgType, ClientInfo& client);
    void Shutdown();

private:
    // low-level network stuff
    ConnectionInfo g_Connection;

    // send/recv buffer
    static constexpr int kRECV_BUF_SIZE = 512;
    char m_RawRecvBuf[kRECV_BUF_SIZE];
    network::Buffer m_RecvBuf{kRECV_BUF_SIZE};

    static constexpr int kSEND_BUF_SIZE = 512;
    network::Buffer m_SendBuf{kSEND_BUF_SIZE};

    // Server cache
    std::map<std::string, ClientInfo*> m_ClientMap;          // userName (string) -> ClientInfo*
    std::map<std::string, std::set<std::string>> m_RoomMap;  // roomName (string) -> userNames (set of string)
    std::vector<std::string> m_RoomNames;                    // all the keys of m_RoomMap
};
