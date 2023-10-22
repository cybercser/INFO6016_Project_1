#pragma once

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "buffer.h"
#include "message.h"

// the client state
enum ClientState {
    kOFFLINE,  // offline (initial state)
    kONLINE,   // online (loged in server)
};

// the ChatRoom client
class ChatRoomClient {
public:
    ChatRoomClient(const std::string& host, uint16 port);
    ~ChatRoomClient();

    int RecvResponse();

    // Requests
    int ReqLogin(const std::string& userName, const std::string& password);
    int ReqJoinRoom(const std::string& roomName);
    int ReqLeaveRoom(const std::string& roomName);
    int ReqChatInRoom(const std::string& roomName, const std::string chat);

    // Print
    void PrintRooms(const std::vector<std::string>& roomNames) const;
    void PrintUsersInRoom(const std::string& roomName) const;

private:
    int Initialize(const std::string& host, uint16 port);
    int SendRequest(network::Message* msg);

    void HandleMessage(network::MessageType msgType);

    int Shutdown();

private:
    // low-level network stuff
    SOCKET m_ConnectSocket = INVALID_SOCKET;
    struct addrinfo* m_AddrInfo = nullptr;

    // send/recv buffer
    static constexpr int kRECV_BUF_SIZE = 512;
    char m_RawRecvBuf[kRECV_BUF_SIZE];
    network::Buffer m_RecvBuf{kSEND_BUF_SIZE};

    static constexpr int kSEND_BUF_SIZE = 512;
    network::Buffer m_SendBuf{kSEND_BUF_SIZE};

    // logic variables
    ClientState m_ClientState = ClientState::kOFFLINE;
    std::string m_MyUserName;
    std::set<std::string> m_JoinedRoomNames;  // rooms already joined
    std::map<std::string, std::set<std::string>>
        m_JoinedRoomMap;  // roomName (string) -> userNames (set of string), only joined rooms
};