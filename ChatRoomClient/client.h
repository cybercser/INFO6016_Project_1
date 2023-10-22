#pragma once

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>

#include <string>
#include <vector>

#include "buffer.h"
#include "message.h"

enum ClientState {
    kOFFLINE,  // offline (initial state)
    kONLINE,   // online (loged in server, but have not joined any room yet)
};

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
    void PrintRooms() const;
    void PrintUsers() const;

private:
    int Initialize(const std::string& host, uint16 port);
    int SendRequest(network::Message* msg);

    void HandleMessage(network::MessageType msgType, network::Buffer& recvBuf);

    int Shutdown();

private:
    // low-level network stuff
    SOCKET m_ConnectSocket = INVALID_SOCKET;
    struct addrinfo* m_AddrInfo = nullptr;

    // send/recv buffer
    static constexpr int kRECV_BUF_SIZE = 512;
    char m_RawRecvBuf[kRECV_BUF_SIZE];

    static constexpr int kSEND_BUF_SIZE = 512;
    network::Buffer m_SendBuf{kSEND_BUF_SIZE};

    // thread
    // std::thread m_RecvThread;

    // logic variables
    ClientState m_ClientState = ClientState::kOFFLINE;
    std::string m_UserName;
    std::vector<std::string> m_RoomNames;
    std::vector<std::string> m_UserNames;
};