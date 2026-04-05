// forward to common implementation
#include "HttpToStruct.h"

// existing implementation files remain for compatibility; the common version is used by targets
namespace server::http_util {

std::pair<std::string, std::string> HttpToJson(const transport::HttpTextRequest& httpText) {
    std::string type;
    if (httpText.headers.count("protocol")) type = httpText.headers.at("protocol");
    return {type, httpText.body};
}

bool ToProtocolStruct(const transport::HttpTextRequest& httpText, protocol::Message* message_ptr) {
    if (message_ptr == nullptr) return false;
    if (httpText.headers.count("protocol") == 0) return false;
    return message_ptr->FromJson(httpText.body);
}

} // namespace server::http_util

