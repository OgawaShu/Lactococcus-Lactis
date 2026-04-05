// forward to common implementation
#include "StructToHttp.h"

namespace server::http_util {

server::transport::HttpTextRequest ToHttpText(
    const server::transport::HttpTextRequest& httpTextRequest,
    const protocol::Message& message) {
    server::transport::HttpTextRequest result = httpTextRequest;
    result.headers["protocol"] = message.MessageTypeName();
    result.body = message.ToJson();
    return result;
}

}  // namespace server::http_util
