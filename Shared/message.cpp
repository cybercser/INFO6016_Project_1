#include "message.h"

#include "buffer.h"

namespace network {

void Message::Serialize(Buffer& buf) {
    buf.Reset();
    buf.WriteUInt32LE(header.packetSize);
    buf.WriteUInt32LE(header.messageType);
}

// Login req message
C2S_LoginReqMsg::C2S_LoginReqMsg(const std::string& strUserName, const std::string& strPassword)
    : userName(strUserName), password(strPassword) {
    userNameLength = userName.size();
    passwordLength = password.size();

    header.messageType = network::MessageType::LoginReq;
    header.packetSize = sizeof(network::PacketHeader);
    header.packetSize += sizeof(userNameLength) + userNameLength;
    header.packetSize += sizeof(passwordLength) + passwordLength;
}

void C2S_LoginReqMsg::Serialize(Buffer& buf) {
    Message::Serialize(buf);

    buf.WriteUInt32LE(userNameLength);
    buf.WriteString(userName, userNameLength);
    buf.WriteUInt32LE(passwordLength);
    buf.WriteString(password, passwordLength);
}

// Login ack message
S2C_LoginAckMsg::S2C_LoginAckMsg(uint16_t iStatus, const std::vector<std::string>& vecRoomNames)
    : loginStatus(iStatus) {
    roomListLength = vecRoomNames.size();
    for (const std::string& roomName : vecRoomNames) {
        roomNameLengths.push_back(roomName.size());
        roomNames.push_back(roomName);
    }

    header.messageType = MessageType::LoginAck;
    header.packetSize = sizeof(network::PacketHeader);
    header.packetSize += sizeof(loginStatus);
    header.packetSize += sizeof(roomListLength);
    header.packetSize += sizeof(uint32_t) * roomNameLengths.size();
    for (uint32_t len : roomNameLengths) {
        header.packetSize += len;
    }
}

void S2C_LoginAckMsg::Serialize(Buffer& buf) {
    Message::Serialize(buf);

    buf.WriteUInt16LE(loginStatus);
    buf.WriteUInt32LE(roomListLength);
    for (size_t i = 0; i < roomListLength; i++) {
        buf.WriteUInt32LE(roomNameLengths[i]);
    }
    for (size_t i = 0; i < roomListLength; i++) {
        buf.WriteString(roomNames[i], roomNameLengths[i]);
    }
}

// JoinRoom req message
C2S_JoinRoomReqMsg::C2S_JoinRoomReqMsg(const std::string& strUserName, const std::string& strRoomName)
    : userName(strUserName), roomName(strRoomName) {
    userNameLength = userName.size();
    roomNameLength = roomName.size();

    header.messageType = network::MessageType::JoinRoomReq;
    header.packetSize = sizeof(network::PacketHeader);
    header.packetSize += sizeof(userNameLength) + userNameLength;
    header.packetSize += sizeof(roomNameLength) + roomNameLength;
}

void C2S_JoinRoomReqMsg::Serialize(Buffer& buf) {
    Message::Serialize(buf);

    buf.WriteUInt32LE(userNameLength);
    buf.WriteString(userName, userNameLength);
    buf.WriteUInt32LE(roomNameLength);
    buf.WriteString(roomName, roomNameLength);
}

// JoinRoom ack message
S2C_JoinRoomAckMsg::S2C_JoinRoomAckMsg(uint16 iStatus, const std::string& strRoomName,
                                       const std::vector<std::string>& vecUserNames)
    : roomName(strRoomName), joinStatus(iStatus) {
    roomNameLength = strRoomName.size();
    userListLength = vecUserNames.size();
    for (const std::string& userName : vecUserNames) {
        userNameLengths.push_back(userName.size());
        userNames.push_back(userName);
    }

    header.messageType = MessageType::JoinRoomAck;
    header.packetSize = sizeof(network::PacketHeader);
    header.packetSize += sizeof(joinStatus);
    header.packetSize += sizeof(roomNameLength) + roomNameLength;
    header.packetSize += sizeof(userListLength);
    header.packetSize += sizeof(uint32_t) * userNameLengths.size();
    for (uint32_t len : userNameLengths) {
        header.packetSize += len;
    }
}

void S2C_JoinRoomAckMsg::Serialize(Buffer& buf) {
    Message::Serialize(buf);

    buf.WriteUInt16LE(joinStatus);
    buf.WriteUInt32LE(roomNameLength);
    buf.WriteString(roomName, roomNameLength);
    buf.WriteUInt32LE(userListLength);
    for (size_t i = 0; i < userListLength; i++) {
        buf.WriteUInt32LE(userNameLengths[i]);
    }
    for (size_t i = 0; i < userListLength; i++) {
        buf.WriteString(userNames[i], userNameLengths[i]);
    }
}
}  // end of namespace network
