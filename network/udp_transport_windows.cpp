#include "aegis/network/udp_transport.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#pragma comment(lib, "Ws2_32.lib")

namespace aegis::network
{

struct UdpTransport::Impl
{
    // Windows UDP socket handle.
    SOCKET socket_handle{INVALID_SOCKET};

    // Remote destination address.
    sockaddr_in remote_address{};

    // Runtime transport statistics.
    NetworkTransportStats stats{};

    // Maximum UDP datagram size.
    static constexpr std::size_t
        kMaximumDatagramSize = 65'507U;
};

namespace
{

std::runtime_error
WinsockError(
    const char* operation
)
{
    return std::runtime_error(
        std::string(operation)
        + " failed. Winsock error code: "
        + std::to_string(
            WSAGetLastError()
        )
    );
}

} // namespace

UdpTransport::UdpTransport(
    UdpTransportConfig config
)
    : impl_(std::make_unique<Impl>())
{
    // Initialize the Winsock library.
    WSADATA winsock_data{};

    if (WSAStartup(
            MAKEWORD(2, 2),
            &winsock_data
        ) != 0)
    {
        throw std::runtime_error(
            "WSAStartup failed."
        );
    }

    // Create an IPv4 UDP socket.
    impl_->socket_handle =
        socket(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP
        );

    if (impl_->socket_handle ==
        INVALID_SOCKET)
    {
        WSACleanup();

        throw WinsockError(
            "socket"
        );
    }

    sockaddr_in local_address{};

    local_address.sin_family =
        AF_INET;

    local_address.sin_port =
        htons(config.local_port);

    if (InetPtonA(
            AF_INET,
            config.local_address.c_str(),
            &local_address.sin_addr
        ) != 1)
    {
        closesocket(
            impl_->socket_handle
        );

        impl_->socket_handle =
            INVALID_SOCKET;

        WSACleanup();

        throw std::invalid_argument(
            "Invalid local UDP address."
        );
    }

    // Bind the socket to the local address.
    if (bind(
            impl_->socket_handle,
            reinterpret_cast<const sockaddr*>(
                &local_address
            ),
            sizeof(local_address)
        ) == SOCKET_ERROR)
    {
        const auto error =
            WinsockError(
                "bind"
            );

        closesocket(
            impl_->socket_handle
        );

        impl_->socket_handle =
            INVALID_SOCKET;

        WSACleanup();

        throw error;
    }

    impl_->remote_address.sin_family =
        AF_INET;

    impl_->remote_address.sin_port =
        htons(config.remote_port);

    if (InetPtonA(
            AF_INET,
            config.remote_address.c_str(),
            &impl_->remote_address.sin_addr
        ) != 1)
    {
        closesocket(
            impl_->socket_handle
        );

        impl_->socket_handle =
            INVALID_SOCKET;

        WSACleanup();

        throw std::invalid_argument(
            "Invalid remote UDP address."
        );
    }

    // Configure non-blocking receive behavior.
    u_long non_blocking_mode = 1U;

    if (ioctlsocket(
            impl_->socket_handle,
            FIONBIO,
            &non_blocking_mode
        ) == SOCKET_ERROR)
    {
        const auto error =
            WinsockError(
                "ioctlsocket"
            );

        closesocket(
            impl_->socket_handle
        );

        impl_->socket_handle =
            INVALID_SOCKET;

        WSACleanup();

        throw error;
    }
}

UdpTransport::~UdpTransport()
{
    if (impl_ != nullptr &&
        impl_->socket_handle !=
            INVALID_SOCKET)
    {
        closesocket(
            impl_->socket_handle
        );

        impl_->socket_handle =
            INVALID_SOCKET;
    }

    // Release the Winsock library.
    WSACleanup();
}

bool UdpTransport::Send(
    std::vector<std::uint8_t> wire_data,
    aegis::TimePoint send_time
)
{
    // The timestamp is used by the common interface,
    // but UDP itself does not need it.
    static_cast<void>(send_time);

    if (wire_data.empty())
    {
        return false;
    }

    const int sent_bytes =
        sendto(
            impl_->socket_handle,
            reinterpret_cast<const char*>(
                wire_data.data()
            ),
            static_cast<int>(
                wire_data.size()
            ),
            0,
            reinterpret_cast<const sockaddr*>(
                &impl_->remote_address
            ),
            sizeof(impl_->remote_address)
        );

    if (sent_bytes == SOCKET_ERROR)
    {
        ++impl_->stats.packets_dropped;
        return false;
    }

    if (static_cast<std::size_t>(
            sent_bytes
        ) != wire_data.size())
    {
        ++impl_->stats.packets_dropped;
        return false;
    }

    ++impl_->stats.packets_sent;

    impl_->stats.bytes_sent +=
        static_cast<std::uint64_t>(
            wire_data.size()
        );

    return true;
}

std::vector<ReceivedPacket>
UdpTransport::ReceiveReady(
    aegis::TimePoint now
)
{
    std::vector<ReceivedPacket> packets;

    while (true)
    {
        std::vector<std::uint8_t> buffer(
            Impl::kMaximumDatagramSize
        );

        const int received_bytes =
            recvfrom(
                impl_->socket_handle,
                reinterpret_cast<char*>(
                    buffer.data()
                ),
                static_cast<int>(
                    buffer.size()
                ),
                0,
                nullptr,
                nullptr
            );

        if (received_bytes ==
            SOCKET_ERROR)
        {
            const int error_code =
                WSAGetLastError();

            if (error_code ==
                    WSAEWOULDBLOCK ||
                error_code ==
                    WSAEINPROGRESS)
            {
                break;
            }

            throw WinsockError(
                "recvfrom"
            );
        }

        buffer.resize(
            static_cast<std::size_t>(
                received_bytes
            )
        );

        ++impl_->stats.packets_delivered;

        impl_->stats.bytes_delivered +=
            static_cast<std::uint64_t>(
                received_bytes
            );

        packets.push_back(
            ReceivedPacket{
                .wire_data = std::move(buffer),
                .receive_time = now
            }
        );
    }

    return packets;
}

const NetworkTransportStats&
UdpTransport::stats() const noexcept
{
    return impl_->stats;
}

std::size_t
UdpTransport::queued_packets() const noexcept
{
    // The operating system owns the UDP receive queue.
    return 0U;
}

} // namespace aegis::network