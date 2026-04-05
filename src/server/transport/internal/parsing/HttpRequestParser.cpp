#include "HttpRequestParser.h"

#include <sstream>

namespace server::transport::parsing {

bool ParseHttpText(const std::string& rawRequest, HttpTextRequest* request) {
    if (request == nullptr) { return false; }
    const std::size_t headerEnd = rawRequest.find("\r\n\r\n");
    if (headerEnd == std::string::npos) { return false; }
    const std::string headerPart = rawRequest.substr(0, headerEnd);
    request->body = rawRequest.substr(headerEnd + 4);

    std::istringstream stream(headerPart);
    std::string line;
    if (!std::getline(stream, line)) { return false; }
    if (!line.empty() && line.back() == '\r') { line.pop_back(); }

    std::istringstream requestLine(line);
    requestLine >> request->method >> request->path >> request->version;
    if (request->method.empty() || request->path.empty() || request->version.empty()) { return false; }

    request->headers.clear();
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        if (line.empty()) { break; }
        const std::size_t separator = line.find(':');
        if (separator == std::string::npos) { continue; }
        const std::string key = line.substr(0, separator);
        const std::string value = separator + 1 < line.size() ? line.substr(separator + 1) : std::string();
        request->headers[key] = value;
    }

    return true;
}

}  // namespace server::transport::parsing