#pragma once

#include <string>

#include "../protocol/Message.h"
#include "../../server/transport/TransportTypes.h"

namespace server::http_util {

server::transport::HttpTextRequest ToHttpText(
    const server::transport::HttpTextRequest& httpTextRequest,
    const protocol::Message& message);

}  // namespace server::http_util
