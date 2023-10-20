#pragma once

#include "common.h"

#include <string>
#include <vector>

namespace network {
// forward decalaration
class Buffer;

// Naming convention:
// Req = request, usually the client request the server for something
// Ack = acknowledge, usually the server answer the client's request
// Ntf = notify, usually the server actively notify the clients
// C2S = client to server
// S2C = server to client

// The message type (protocol unique id)
enum MessageType { LoginReq, LoginAck, JoinRoomReq, JoinRoomAck, LeaveRoomReq, LeaveRoomAck, SendMessageToRoom };

// The fixed-length packet header
struct PacketHeader {
    uint32 packetSize;
    uint32 messageType;
};

// the Message (aka. protocol) base class
struct Message {
    PacketHeader header;
    virtual void Serialize(Buffer& buf);
};

// Login req message
struct C2S_LoginReqMsg : public Message {
    uint32 userNameLength;
    std::string userName;
    uint32 passwordLength;
    std::string password;

    C2S_LoginReqMsg(const std::string& strUserName, const std::string& strPassword);
    void Serialize(Buffer& buf) override;
};

// Login ack message
struct S2C_LoginAckMsg : public Message {
    uint16 loginStatus;
    uint32 roomListLength;
    std::vector<uint32> roomNameLengths;
    std::vector<std::string> roomNames;

    S2C_LoginAckMsg(uint16 iStatus, const std::vector<std::string>& vecRoomNames);
    void Serialize(Buffer& buf) override;
};

// JoinRoom req message
struct C2S_JoinRoomReqMsg : public Message {
    uint32 userNameLength;
    std::string userName;
    uint32 roomNameLength;
    std::string roomName;

    C2S_JoinRoomReqMsg(const std::string& strUserName, const std::string& strRoomName);
    void Serialize(Buffer& buf) override;
};

// JoinRoom ack message
struct S2C_JoinRoomAckMsg : public Message {
    uint16 joinStatus;
    uint32 roomNameLength;
    std::string roomName;
    uint32 userListLength;
    std::vector<uint32> userNameLengths;
    std::vector<std::string> userNames;

    S2C_JoinRoomAckMsg(uint16 iStatus, const std::string& strRoomName, const std::vector<std::string>& vecUserNames);
    void Serialize(Buffer& buf) override;
};

// LeaveRoom req message
struct C2S_LeaveRoomReqMsg {
    PacketHeader header;
};

// LeaveRoom ack message
struct C2S_LeaveRoomAckMsg {
    PacketHeader header;
};

// Chat req message
struct C2S_ChatReqMsg {
    PacketHeader header;
    uint32 chatLength;
    std::string chat;
};

// Chat ack message
struct C2S_ChatAckMsg {
    PacketHeader header;
    uint32 chatLength;
    std::string chat;
};

// Chat ntf message, to broadcast the chat in a room
struct C2S_ChatNtfMsg {
    PacketHeader header;
    uint32 userNameLength;
    std::string userName;
    uint32 chatLength;
    std::string chat;
};

}  // end of namespace network
