#pragma once

// Message, aka. protocol

namespace network
{
	enum MessageType
	{
		JoinRoom,
		LeaveRoom,
		SendMessageToRoom
	};

	// The fixed-length packet header
	struct PacketHeader
	{
		uint32_t packetSize;
		uint32_t messageType;
	};

	// Chat message
	struct ChatMessage
	{
		PacketHeader header;
		uint32_t chatLength;
		std::string chat;
	};

	// JoinRoom message
	struct JoinRoomMessage
	{
		PacketHeader header;
		uint32_t roomNameLength;
		std::string roomName;
	};
} // end of namespace network
