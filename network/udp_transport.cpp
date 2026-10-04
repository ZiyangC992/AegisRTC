#include "aegis/network/udp_transport.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace aegis::network
{

struct UdpTransport::Impl
{
    // UDP socket file descriptor.
    int socket_file_descriptor{-1};

    // Remote destination address.
    sockaddr_in remote_address{};
    
    NetworkTransportStats stats{};

    // Maximum UDP datagram size used by this transport.
    static constexpr std::size_t 
    kMaximumDatagramSize = 65'507U;
};

UdpTransport::UdpTransport(
    UdpTransportConfig config
)
    : impl_(std::make_unique<Impl>())
{
    // Create an IPv4 UDP socket.
    impl_->socket_file_descriptor =
        socket(
            AF_INET,
            SOCK_DGRAM,
            0
        );

    if (impl_->socket_file_descriptor == -1)
    {
        throw std::runtime_error(
            "Failed to create UDP socket: "
            + std::string(std::strerror(errno))
        );
    }

    // Configure the local address.
    sockaddr_in local_address{};
    local_address.sin_family =
        AF_INET;

    local_address.sin_port =
        htons(config.local_port);

    if (inet_pton(
            AF_INET,
            config.local_address.c_str(),
            &local_address.sin_addr
        ) != 1)
    {
        close(
            impl_->socket_file_descriptor
        );

        impl_->socket_file_descriptor = -1;

        throw std::invalid_argument(
            "Invalid local UDP address."
        );
    }

    // Bind the socket to the local address and port.
    if (bind(
            impl_->socket_file_descriptor,
            reinterpret_cast<const sockaddr*>(
                &local_address
            ),
            sizeof(local_address)
        ) == -1)
    {
        const std::string error_message =
            std::strerror(errno);

        close(
            impl_->socket_file_descriptor
        );

        impl_->socket_file_descriptor = -1;

        throw std::runtime_error(
            "Failed to bind UDP socket: "
            + error_message
        );
    }

    // Configure the remote destination address.
    impl_->remote_address.sin_family =
        AF_INET;

    impl_->remote_address.sin_port =
        htons(config.remote_port);

    if (inet_pton(
            AF_INET,
            config.remote_address.c_str(),
            &impl_->remote_address.sin_addr
        ) != 1)
    {
        close(
            impl_->socket_file_descriptor
        );

        impl_->socket_file_descriptor = -1;

        throw std::invalid_argument(
            "Invalid remote UDP address."
        );
    }

    // Make receive operations non-blocking.
    //
    // This allows ReceiveReady() to return immediately
    // when no UDP packet is currently available.
    const int current_flags =
        fcntl(
            impl_->socket_file_descriptor,
            F_GETFL,
            0
        );

    if (current_flags == -1 ||
        fcntl(
            impl_->socket_file_descriptor,
            F_SETFL,
            current_flags | O_NONBLOCK
        ) == -1)
    {
        close(
            impl_->socket_file_descriptor
        );

        impl_->socket_file_descriptor = -1;

        throw std::runtime_error(
            "Failed to configure non-blocking UDP socket: "
            + std::string(std::strerror(errno))
        );
    }
}

UdpTransport::~UdpTransport()
{
    if (impl_ != nullptr &&
        impl_->socket_file_descriptor != -1)
    {
        close(
            impl_->socket_file_descriptor
        );

        impl_->socket_file_descriptor = -1;
    }
}

bool UdpTransport::Send(
    std::vector<std::uint8_t> wire_data,
    aegis::TimePoint send_time
)
{
    // The timestamp is not required by UDP itself.
    static_cast<void>(send_time);

    if (wire_data.empty())
    {
        return false;
    }

const ssize_t sent_bytes =
    sendto(
        impl_->socket_file_descriptor,
        wire_data.data(),
        wire_data.size(),
        0,
        reinterpret_cast<const sockaddr*>(
            &impl_->remote_address
        ),
        sizeof(impl_->remote_address)
    );

if (sent_bytes < 0)
{
    ++impl_->stats.packets_dropped;
    return false;
}

if (static_cast<std::size_t>(sent_bytes) !=
    wire_data.size())
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

        const ssize_t received_bytes =
            recvfrom(
                impl_->socket_file_descriptor,
                buffer.data(),
                buffer.size(),
                MSG_DONTWAIT,
                nullptr,
                nullptr
            );

        if (received_bytes < 0)
        {
            // No more packets are currently available.
            if (errno == EAGAIN ||
                errno == EWOULDBLOCK)
            {
                break;
            }

            throw std::runtime_error(
                "Failed to receive UDP packet: "
                + std::string(std::strerror(errno))
            );
        }

        buffer.resize(
            static_cast<std::size_t>(
                received_bytes
            )
        );

        packets.push_back(
            ReceivedPacket{
                .wire_data = std::move(buffer),
                .receive_time = now
            }
        );
        
        ++impl_->stats.packets_delivered;

        impl_->stats.bytes_delivered +=
          static_cast<std::uint64_t>(
              received_bytes
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
    // UdpTransport does not maintain an application-level queue.
    return 0U;
}

} // namespace aegis::network
