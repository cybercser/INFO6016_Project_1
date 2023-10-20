#include "message.h"

#include "buffer.h"

namespace network {

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
    buf.Reset();
    buf.WriteUInt32LE(header.packetSize);
    buf.WriteUInt32LE(header.messageType);
    buf.WriteUInt32LE(userNameLength);
    buf.WriteString(userName, userNameLength);
    buf.WriteUInt32LE(passwordLength);
    buf.WriteString(password, passwordLength);
}

// Login ack message
S2C_LoginAckMsg::S2C_LoginAckMsg(uint16_t iStatus, const std::vector<std::string>& vecRoomNames)
    : loginStatus(iStatus) {
    roomListLength = vecRoomNames.size();
    roomNameLengths.clear();
    for (const std::string& roomName : vecRoomNames) {
        roomNameLengths.push_back(roomName.size());
        roomNames.push_back(roomName);
    }

    header.messageType = MessageType::LoginAck;
    header.packetSize = sizeof(network::PacketHeader);
    header.packetSize += sizeof(iStatus);
    header.packetSize += sizeof(roomListLength);
    header.packetSize += sizeof(uint32_t) * roomNameLengths.size();
    for (uint32_t len : roomNameLengths) {
        header.packetSize += len;
    }
}

void S2C_LoginAckMsg::Serialize(Buffer& buf) {
    buf.Reset();
    buf.WriteUInt32LE(header.packetSize);
    buf.WriteUInt32LE(header.messageType);
    buf.WriteUInt16LE(loginStatus);
    buf.WriteUInt32LE(roomListLength);
    for (size_t i = 0; i < roomListLength; i++) {
        buf.WriteUInt32LE(roomNameLengths[i]);
    }
    for (size_t i = 0; i < roomListLength; i++) {
        buf.WriteString(roomNames[i], roomNameLengths[i]);
    }
}
}  // end of namespace network
