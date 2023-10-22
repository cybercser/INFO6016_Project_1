#include "server.h"

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT 5555

int main(int argc, char** argv) {
    ChatRoomServer server{DEFAULT_PORT};
    server.RunLoop();
    return 0;
}
