#include "logger.h"
#include "server_app.h"
#include <iostream>
#include <string>

int main() {
    ServerApp app;
    app.init();
    app.run();
    return 0;
}