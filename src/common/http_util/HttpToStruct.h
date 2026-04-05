#pragma once

#include "../protocol/password/ApplyPassword.h"
#include "../protocol/user/RegisterUser.h"
#include "../../server/transport/TransportTypes.h"

#include <memory>
#include <utility>

namespace server::http_util {

// Parse an HttpTextRequest into the provided protocol::Message (fills header and body fields).
// Returns true on success.
bool ToProtocolStruct(const transport::HttpTextRequest& httpText,
    protocol::Message* message);

// Extract type and JSON body from an HttpTextRequest.
std::pair<std::string, std::string> HttpToJson(const transport::HttpTextRequest& httpText);

}  // namespace server::http_util
