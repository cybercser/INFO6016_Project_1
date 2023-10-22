#include "client.h"

#define DEFAULT_PORT 5555

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

int main(int argc, char** argv) {
    ChatRoomClient client{"127.0.0.1", DEFAULT_PORT};

    client.ReqLogin("Fan", "Fanshawe");
    client.RecvResponse();

    client.ReqJoinRoom("graphics");
    client.RecvResponse();

    client.ReqJoinRoom("network");
    client.RecvResponse();

    client.ReqLeaveRoom("graphics");
    client.RecvResponse();

    client.ReqChatInRoom("network", "socket programming is hardcore.");
    client.RecvResponse();

    client.ReqLeaveRoom("network");
    client.RecvResponse();

    return 0;
}
