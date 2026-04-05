#pragma once

#include <string>

#include "../../TransportTypes.h"

namespace server::transport::parsing {

bool ParseHttpText(const std::string& rawRequest, HttpTextRequest* request);

}  // namespace server::transport::parsing