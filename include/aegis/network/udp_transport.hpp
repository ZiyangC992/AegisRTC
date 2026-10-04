#pragma once

#include "aegis/network/network_transport.hpp"

#include <memory>
#include <cstdint>
#include <string>
#include <vector>

namespace aegis::network
{

// Configuration for a UDP transport.
struct UdpTransportConfig
{
    // Local address used for bind().
    std::string local_address{"127.0.0.1"};

    // Local UDP port.
    std::uint16_t local_port{9000};

    // Remote destination address.
    std::string remote_address{"127.0.0.1"};

    // Remote destination port.
    std::uint16_t remote_port{9001};
};

// Real UDP transport implementation.
class UdpTransport final
    : public INetworkTransport
{
public:
    explicit UdpTransport(
        UdpTransportConfig config
    );

    ~UdpTransport() override;

    UdpTransport(
        const UdpTransport&
    ) = delete;

    UdpTransport& operator=(
        const UdpTransport&
    ) = delete;

    // Send one serialized packet through UDP.
    [[nodiscard]] bool Send(
        std::vector<std::uint8_t> wire_data,
        aegis::TimePoint send_time
    ) override;

    // Receive all currently available UDP packets.
    [[nodiscard]] std::vector<ReceivedPacket>
    ReceiveReady(
        aegis::TimePoint now
    ) override;
    
    // Return UDP transport statistics.
    [[nodiscard]] const NetworkTransportStats&
    stats() const noexcept override;

    // Return the number of packets waiting internally.
    //
    // UDP uses the operating system receive queue,
    // so this transport does not maintain its own queue.
    [[nodiscard]] std::size_t
    queued_packets() const noexcept override;
    
private:
    struct Impl;

    std::unique_ptr<Impl> impl_;
};

} // namespace aegis::network
