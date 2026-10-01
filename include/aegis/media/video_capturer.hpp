#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace aegis::media {

// Media Foundation video subtype.
enum class VideoSubtype : std::uint8_t {
    kUnknown = 0,
    kNv12,
    kYuy2,
    kMjpg,
    kRgb24,
    kRgb32,
    kI420,
    kH264,
};

// One captured video frame
struct CapturedVideoFrame {
    // Raw images bytes copied from Media Foundation
    std::vector<std::uint8_t> data;

    //Image resolution
    std::uint32_t width{0};
    std::uint32_t height{0};

    //Number of bytes occupied by one image row
    std::uint32_t stride{0};

    //Sample timestamp in 100-nanosecond units
    std::int64_t timestamp_100ns{0};

    //Pixel format of this frame
    VideoSubtype subtype {
        VideoSubtype::kUnknown
    };
};

class IVideoCapturer
{
public:
    virtual ~IVideoCapturer() = default;

    //Open the first available camera using the requested format.
    [[nodiscard]] virtual bool Open(
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t frame_rate
    ) = 0;

    //Read one captured frame.
    [[nodiscard]] virtual std::optional<
        CapturedVideoFrame
    > ReadFrame() = 0;

    //Close the capture device.
    virtual void Close() noexcept = 0;

    //Check whether the capture device is currently open.
    [[nodiscard]] virtual bool IsOpen() const noexcept = 0;
};

} //namespace aegis::media