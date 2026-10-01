#pragma once

#include "aegis/media/video_capturer.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace aegis::media {

// Basic information about one camera device.
struct CameraDeviceInfo {
    // Index in the current enumeration result.
    std::uint32_t index{0};

    // Human-readable camera name returned by Windows.
    std::wstring friendly_name;

    // Stable Windows device identifier.
    std::wstring symbolic_link;
};

// One native format supported by a camera.
struct CameraFormatInfo {
    // Index returned by IMFSourceReader::GetNativeMediaType.
    std::uint32_t format_index{0};

    // Video resolution in pixels.
    std::uint32_t width{0};
    std::uint32_t height{0};

    // Frame rate represented as numerator / denominator.
    //
    // For example:
    // 30 / 1 = 30 FPS
    std::uint32_t frame_rate_numerator{0};
    std::uint32_t frame_rate_denominator{1};

    // Media Foundation video subtype.
    VideoSubtype subtype{
        VideoSubtype::kUnknown
    };
};

// Camera enumerator based on Windows Media Foundation.
//
// The current stage supports:
// 1. Enumerating camera devices.
// 2. Enumerating native formats of a selected camera.
class MfVideoCapturer final
    : public IVideoCapturer {
public:

    MfVideoCapturer();

    MfVideoCapturer(
        const MfVideoCapturer&
    ) = delete;

    MfVideoCapturer& operator=(
        const MfVideoCapturer&
    ) = delete;

    ~MfVideoCapturer();

    // Windows-specific overload.
    // Open the selected camera by device index.
    [[nodiscard]] bool Open(
        std::uint32_t device_index,
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t frame_rate);

    // Cross-platform interface overload.
    // Open the first available camera.
    [[nodiscard]] bool Open(
        std::uint32_t width,
        std::uint32_t height,
        std::uint32_t frame_rate) override;
        
    //Read one frame from the camera
    [[nodiscard]] std::optional<
        CapturedVideoFrame
    > ReadFrame() override;

    //Close the camera and release all resources
    void Close() noexcept override;

    //Check whether the camera is currently opened
    [[nodiscard]] bool IsOpen() const noexcept override;

    // Enumerate all available video capture devices.
    //
    // Returns an empty vector when no camera is found.
    //
    // Throws std::runtime_error when a Media Foundation
    // operation fails.
    [[nodiscard]] static std::vector<CameraDeviceInfo>
    EnumerateDevices();

    // Enumerate all native formats supported by one camera.
    //
    // device_index is the index returned by EnumerateDevices().
    //
    // This function may throw when:
    // - device_index is out of range;
    // - a Windows API call fails;
    // - the camera cannot be activated;
    // - the source reader cannot be created;
    // - required media attributes are missing.
    [[nodiscard]] static std::vector<CameraFormatInfo>
    EnumerateNativeFormats(
        std::uint32_t device_index
    );

private:
    //Forward declaration
    //The complete implementation is defined in the .cpp file.
    struct Impl;

    //PImpl object that stores Media Foundation resources.
    std::unique_ptr<Impl> impl_;
};

} // namespace aegis::media