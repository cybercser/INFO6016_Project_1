#pragma once

#include "common.h"

#include <string>
#include <vector>

// Message, aka. protocol

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

// Login req message
struct C2S_LoginReqMsg {
    PacketHeader header;
    uint32 userNameLength;
    std::string userName;
    uint32 passwordLength;
    std::string password;

    C2S_LoginReqMsg(const std::string& strUserName, const std::string& strPassword);
    void Serialize(Buffer& buf);
};

// Login ack message
struct S2C_LoginAckMsg {
    PacketHeader header;
    uint16 loginStatus;
    uint32 roomListLength;
    std::vector<uint32> roomNameLengths;
    std::vector<std::string> roomNames;

    S2C_LoginAckMsg(uint16 iStatus, const std::vector<std::string>& vecRoomNames);
    void Serialize(Buffer& buf);
};

// JoinRoom req message
struct C2S_JoinRoomReqMsg {
    PacketHeader header;
    uint32 roomNameLength;
    std::string roomName;
};

// JoinRoom ack message
struct S2C_JoinRoomAckMsg {
    PacketHeader header;
    uint32 roomNameLength;
    std::string roomName;
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
