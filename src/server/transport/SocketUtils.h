#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#include <Ws2tcpip.h>
using NativeSocket = SOCKET;
#else
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
using NativeSocket = int;
#endif

#include <cstdint>

inline NativeSocket ToNative(std::intptr_t value) {
    return static_cast<NativeSocket>(value);
}
inline std::intptr_t ToStorage(NativeSocket value) {
    return static_cast<std::intptr_t>(value);
}
inline void CloseSocket(std::intptr_t socketValue) {
    const NativeSocket socket = ToNative(socketValue);
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

// platform invalid socket sentinel
#ifdef _WIN32
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;
#else
constexpr NativeSocket kInvalidSocket = -1;
#endif
