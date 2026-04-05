#pragma once

#include <map>
#include <string>

namespace server::transport {

struct HttpTextRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    // identifier of the connection/socket this request was received on
    std::intptr_t connection = -1;
};

}  // namespace server::transport