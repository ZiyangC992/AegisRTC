#pragma once

#include "aegis/media/video_capturer.hpp"

#include <cstdint>
#include <optional>
#include <memory>
#include <string>

namespace aegis::media
{

class V4L2VideoCapturer final
    : public IVideoCapturer
{
public:
    //Create a V4L2 capturer for a linux video device.
    explicit V4L2VideoCapturer(
	std::string device_path = "/dev/video0"
    );

    V4L2VideoCapturer(
	const V4L2VideoCapturer&
    ) = delete;
    
    V4L2VideoCapturer& operator=(
	const V4L2VideoCapturer&
    ) = delete;

    ~V4L2VideoCapturer() override;

    //Open the camera with the requested format.
    [[nodiscard]] bool Open(
	std::uint32_t width,
	std::uint32_t height,
	std::uint32_t frame_rate
    ) override;

    //Read one captured frame.
    [[nodiscard]] std::optional<
	CapturedVideoFrame
    > ReadFrame() override;

    //Close the V4L2 device.
    void Close() noexcept override;

    //Check whether the device is currently open.
    [[nodiscard]] bool
    IsOpen() const noexcept override;

private:
    //Hide Linux-specific implementation details.
    struct Impl;

    //Store the implementation using the PImpl pattern.
    std::unique_ptr<Impl> impl_;
};

} //namespace aegis::media
