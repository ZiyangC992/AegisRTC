// Windows Media Foundation camera access.
// This file initializes the framework, enumerates cameras,
// queries supported formats, and prepares future frame capture.

#include "aegis/media/mf_video_capturer.hpp"
#include "aegis/media/video_capturer.hpp"

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <wrl/client.h>
#include <mfreadwrite.h>
#include <mferror.h>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>
#include <optional>

namespace aegis::media {

struct MfVideoCapturer::Impl {
    //Represents the physical camera device
    Microsoft::WRL::ComPtr<IMFMediaSource> media_source;

    //Reads media samples from the media source
    Microsoft::WRL::ComPtr<IMFSourceReader> source_reader;

    //Current capture configuration
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t frame_rate{0};
    //Number of bytes between two consecutive image rows.
    std::uint32_t stride{0};

    //Current output pixel format
    VideoSubtype subtype{
        VideoSubtype::kUnknown
    };

    //Whether the camera is currently opened.
    bool opened{false};
};

MfVideoCapturer::MfVideoCapturer()
    : impl_(std::make_unique<Impl>()) {}

namespace {

// Convert an HRESULT value to a hexadecimal string.
//
// Example:
// 0x80070005
//
// HRESULT is the standard error-code type used by Windows APIs.
// Hexadecimal output is convenient when searching Microsoft documentation.
[[nodiscard]] std::string HResultToString(HRESULT result) {
    std::ostringstream stream;

    stream
        << "0x"
        << std::hex
        << std::uppercase
        << static_cast<unsigned long>(result);

    return stream.str();
}

// Check whether a Windows API call succeeded.
//
// If result indicates failure, throw std::runtime_error.
// operation describes the failed operation and helps debugging.
void ThrowIfFailed(
    HRESULT result,
    std::string_view operation
) {
    if (FAILED(result)) {
        throw std::runtime_error(
            std::string{operation} +
            " failed. HRESULT = " +
            HResultToString(result)
        );
    }
}

// GetAllocatedString uses CoTaskMemAlloc() internally.
//
// Therefore the returned memory must not be released with
// delete or free. It must be released with CoTaskMemFree().
struct CoTaskMemStringDeleter {
    void operator()(wchar_t* value) const noexcept {
        CoTaskMemFree(value);
    }
};

// MFEnumDeviceSources() returns an array of IMFActivate pointers.
//
// Two different resources must be released:
// 1. Every IMFActivate object requires Release().
// 2. The pointer array itself requires CoTaskMemFree().
struct ActivateArrayDeleter {
    std::uint32_t count{0};

    // Custom function-call operator used by std::unique_ptr.
    void operator()(IMFActivate** devices) const noexcept {
        if (devices == nullptr) {
            return;
        }

        for (
            std::uint32_t index = 0;
            index < count;
            ++index
        ) {
            if (devices[index] != nullptr) {
                devices[index]->Release();
            }
        }

        CoTaskMemFree(devices);
    }
};

// Read a string attribute from an IMFActivate object.
//
// Windows native APIs usually use UTF-16 strings represented by wchar_t.
// std::wstring is therefore a suitable C++ container.
[[nodiscard]] std::wstring ReadDeviceString(
    IMFActivate* device,
    const GUID& attribute_key
) {
    wchar_t* raw_value = nullptr;
    UINT32 character_count = 0;

    const HRESULT result =
        device->GetAllocatedString(
            attribute_key,
            &raw_value,
            &character_count
        );

    ThrowIfFailed(
        result,
        "IMFActivate::GetAllocatedString"
    );

    // unique_ptr normally calls delete.
    // The custom deleter changes the cleanup operation to
    // CoTaskMemFree(), which is required by this Windows API.
    const std::unique_ptr<
        wchar_t,
        CoTaskMemStringDeleter
    > value{raw_value};

    return std::wstring{
        value.get(),
        static_cast<std::size_t>(character_count)
    };
}

// Convert a Media Foundation subtype GUID to the project's enum.
[[nodiscard]] VideoSubtype ToVideoSubtype(
    const GUID& subtype
) noexcept {
    if (IsEqualGUID(subtype, MFVideoFormat_YUY2)) {
        return VideoSubtype::kYuy2;
    }

    if (IsEqualGUID(subtype, MFVideoFormat_NV12)) {
        return VideoSubtype::kNv12;
    }

    if (IsEqualGUID(subtype, MFVideoFormat_MJPG)) {
        return VideoSubtype::kMjpg;
    }

    if (IsEqualGUID(subtype, MFVideoFormat_RGB24)) {
        return VideoSubtype::kRgb24;
    }

    if (IsEqualGUID(subtype, MFVideoFormat_RGB32)) {
        return VideoSubtype::kRgb32;
    }

    if (IsEqualGUID(subtype, MFVideoFormat_I420)) {
        return VideoSubtype::kI420;
    }

    if (IsEqualGUID(subtype, MFVideoFormat_H264)) {
        return VideoSubtype::kH264;
    }

    return VideoSubtype::kUnknown;
}

} // namespace

std::vector<CameraDeviceInfo>
MfVideoCapturer::EnumerateDevices() {
    // ComPtr is Microsoft's smart pointer for COM interfaces.
    // When attributes leaves the scope, ComPtr automatically calls Release().
    Microsoft::WRL::ComPtr<IMFAttributes> attributes;

    // Create an attribute collection that can store one attribute.
    ThrowIfFailed(
        MFCreateAttributes(
            attributes.GetAddressOf(),
            1
        ),
        "MFCreateAttributes"
    );

    // Restrict enumeration to video capture devices.
    ThrowIfFailed(
        attributes->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        ),
        "IMFAttributes::SetGUID"
    );

    // Media Foundation returns an array of device activation objects.
    IMFActivate** raw_devices = nullptr;

    // Number of detected devices.
    UINT32 device_count = 0;

    ThrowIfFailed(
        MFEnumDeviceSources(
            attributes.Get(),
            &raw_devices,
            &device_count
        ),
        "MFEnumDeviceSources"
    );

    // Manage both the device objects and the returned pointer array.
    const std::unique_ptr<
        IMFActivate*,
        ActivateArrayDeleter
    > device_array{
        raw_devices,
        ActivateArrayDeleter{
            static_cast<std::uint32_t>(device_count)
        }
    };

    std::vector<CameraDeviceInfo> result;

    // Reserve memory to avoid repeated vector reallocations.
    result.reserve(
        static_cast<std::size_t>(device_count)
    );

    for (
        UINT32 index = 0;
        index < device_count;
        ++index
    ) {
        IMFActivate* const device =
            raw_devices[index];

        CameraDeviceInfo info;

        info.index =
            static_cast<std::uint32_t>(index);

        // Read the human-readable camera name.
        info.friendly_name =
            ReadDeviceString(
                device,
                MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME
            );

        // Read the stable Windows device identifier.
        info.symbolic_link =
            ReadDeviceString(
                device,
                MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK
            );

        result.push_back(std::move(info));
    }

    return result;
}

std::vector<CameraFormatInfo>
MfVideoCapturer::EnumerateNativeFormats(
    std::uint32_t device_index
) {
    Microsoft::WRL::ComPtr<IMFAttributes> attributes;

    ThrowIfFailed(
        MFCreateAttributes(
            attributes.GetAddressOf(),
            1
        ),
        "MFCreateAttributes"
    );

    ThrowIfFailed(
        attributes->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        ),
        "IMFAttributes::SetGUID"
    );

    // Windows returns an array of camera activation objects.
    IMFActivate** raw_devices = nullptr;

    // Number of detected cameras.
    UINT32 device_count = 0;

    ThrowIfFailed(
        MFEnumDeviceSources(
            attributes.Get(),
            &raw_devices,
            &device_count
        ),
        "MFEnumDeviceSources"
    );

    const std::unique_ptr<
        IMFActivate*,
        ActivateArrayDeleter
    > device_array{
        raw_devices,
        ActivateArrayDeleter{
            static_cast<std::uint32_t>(device_count)
        }
    };

    // Check whether the requested device index is valid.
    if (device_index >= device_count) {
        throw std::out_of_range(
            "Camera device index is out of range."
        );
    }

    // IMFActivate is the activation entry point for a camera.
    // ActivateObject() returns the actual media source.
    //
    // IMFMediaSource represents the activated camera source.
    // It can be used to create an IMFSourceReader.
    Microsoft::WRL::ComPtr<IMFMediaSource> media_source;

    // GetAddressOf() allows a Windows API to write a new pointer.
    ThrowIfFailed(
        raw_devices[device_index]->ActivateObject(
            IID_PPV_ARGS(
                media_source.GetAddressOf()
            )
        ),
        "IMFActivate::ActivateObject"
    );

    if (media_source == nullptr) {
        throw std::runtime_error(
            "Activated camera media source is null."
        );
    }

    // Source Reader provides access to camera media samples.
    Microsoft::WRL::ComPtr<IMFSourceReader> source_reader;

    const HRESULT source_reader_result =
        MFCreateSourceReaderFromMediaSource(
            media_source.Get(),
            nullptr,
            source_reader.GetAddressOf()
        );

    if (FAILED(source_reader_result)) {
        // The media source has already been activated,
        // so it must be shut down before throwing.
        static_cast<void>(
            media_source->Shutdown()
        );

        ThrowIfFailed(
            source_reader_result,
            "MFCreateSourceReaderFromMediaSource"
        );
    }

    if (source_reader.Get() == nullptr) {
        // The API reported success but did not return a valid reader.
        static_cast<void>(
            media_source->Shutdown()
        );

        throw std::runtime_error(
            "Created source reader is null."
        );
    }

    std::vector<CameraFormatInfo> formats;

    // Enumerate every native format exposed by the camera.
    for (
        std::uint32_t format_index = 0;
        ;
        ++format_index
    ) {
        Microsoft::WRL::ComPtr<IMFMediaType> media_type;

        const HRESULT media_type_result =
            source_reader->GetNativeMediaType(
                static_cast<DWORD>(
                    MF_SOURCE_READER_FIRST_VIDEO_STREAM
                ),
                format_index,
                media_type.GetAddressOf()
            );

        if (media_type_result == MF_E_NO_MORE_TYPES) {
            break;
        }

        ThrowIfFailed(
            media_type_result,
            "IMFSourceReader::GetNativeMediaType"
        );

        if (media_type.Get() == nullptr) {
            throw std::runtime_error(
                "Native media type is null."
            );
        }

        UINT32 width = 0;
        UINT32 height = 0;

        ThrowIfFailed(
            MFGetAttributeSize(
                media_type.Get(),
                MF_MT_FRAME_SIZE,
                &width,
                &height
            ),
            "MFGetAttributeSize(MF_MT_FRAME_SIZE)"
        );

        UINT32 frame_rate_numerator = 0;
        UINT32 frame_rate_denominator = 0;

        ThrowIfFailed(
            MFGetAttributeRatio(
                media_type.Get(),
                MF_MT_FRAME_RATE,
                &frame_rate_numerator,
                &frame_rate_denominator
            ),
            "MFGetAttributeRatio(MF_MT_FRAME_RATE)"
        );

        if (frame_rate_denominator == 0) {
            throw std::runtime_error(
                "Native media type contains an invalid "
                "zero frame-rate denominator."
            );
        }

        GUID native_subtype{};

        ThrowIfFailed(
            media_type->GetGUID(
                MF_MT_SUBTYPE,
                &native_subtype
            ),
            "IMFMediaType::GetGUID"
        );

        const VideoSubtype subtype =
            ToVideoSubtype(native_subtype);

        CameraFormatInfo info{
            .format_index = format_index,
            .width = width,
            .height = height,
            .frame_rate_numerator = frame_rate_numerator,
            .frame_rate_denominator = frame_rate_denominator,
            .subtype = subtype
        };

        formats.push_back(info);
    }

    // Explicitly shut down the activated media source.
    static_cast<void>(
        media_source->Shutdown()
    );

    return formats;
}

[[nodiscard]] Microsoft::WRL::ComPtr<IMFMediaType>
CreateVideoMediaType(
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t frame_rate
) {
    Microsoft::WRL::ComPtr<IMFMediaType> media_type;

    ThrowIfFailed(
        MFCreateMediaType(
            media_type.GetAddressOf()
        ),
        "MFCreateMediaType"
    );

    //Describe this stream as video
    ThrowIfFailed(
        media_type->SetGUID(
            MF_MT_MAJOR_TYPE,
            MFMediaType_Video
        ),
        "IMFMediaType::SetGUID(MF_MT_MAJOR_TYPE)"
    );

    //Request NV12 as the output pixel format.

    ThrowIfFailed(
        media_type->SetGUID(
            MF_MT_SUBTYPE,
            MFVideoFormat_NV12
        ),
        "IMFMediaType::SetGUID(MF_MT_SUBTYPE)"
    );

    ThrowIfFailed(
        MFSetAttributeSize(
            media_type.Get(),
            MF_MT_FRAME_SIZE,
            width,
            height
        ),
        "MFSetAttributeSize(MF_MT_FRAME_SIZE)"
    );

    ThrowIfFailed(
        MFSetAttributeRatio(
            media_type.Get(),
            MF_MT_FRAME_RATE,
            frame_rate,
            1
        ),
        "MFSetAttributeRatio(MF_MT_FRAME_RATE)"
    );

    return media_type;
}

bool MfVideoCapturer::IsOpen() const noexcept {
    return impl_->opened;
}

bool MfVideoCapturer::Open(
    std::uint32_t device_index,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t frame_rate
) {

    if(width == 0U) {
        throw std::invalid_argument(
            "Camera width must be greater than zero."
        );
    }

    if(height == 0U) {
        throw std::invalid_argument(
            "Camera height must be greater than zero."
        );
    }

    if(frame_rate == 0U) {
        throw std::invalid_argument(
            "Camera frame rate must be greater than zero."
        );
    }

    //Close the previous camera before opening a new one
    Close();

    //This function requires COM and Media Foundation
    //to be initialized by the caller
    Microsoft::WRL::ComPtr<IMFAttributes> attributes;

    ThrowIfFailed(
        MFCreateAttributes(
            attributes.GetAddressOf(),
            1
        ),
        "MFCreateAttributes"
    );

    //Restrict device enumeration to video capture devices
    ThrowIfFailed(
        attributes->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        ),
        "IMFAttributes::SetGUID"
    );

    IMFActivate** raw_device = nullptr;

    UINT32 device_count = 0;

    ThrowIfFailed(
        MFEnumDeviceSources(
            attributes.Get(),
            &raw_device,
            &device_count
        ),
        "MFEnumDeviceSources"
    );

    const std::unique_ptr<
        IMFActivate*,
        ActivateArrayDeleter
    > device_array(
        raw_device,
        ActivateArrayDeleter{
            static_cast<std::uint32_t>(device_count)
        }
    );
        
    if(device_index >= device_count){
        throw std::out_of_range(
            "Camera device index is out of range."
        );
    }

    //Select the request camera.
    IMFActivate* const selected_device = 
        raw_device[device_index];
    
    //Activate the selected camera as an IMFMediaSource
    Microsoft::WRL::ComPtr<IMFMediaSource> media_source;

    ThrowIfFailed(
        selected_device->ActivateObject(
            IID_PPV_ARGS(
                media_source.GetAddressOf()
            )
        ),
        "IMFActivate::ActivateObject"
    );

    //Create a source reader for the media source
    Microsoft::WRL::ComPtr<IMFSourceReader> source_reader;

    ThrowIfFailed(
        MFCreateSourceReaderFromMediaSource(
            media_source.Get(),
            nullptr,
            source_reader.GetAddressOf()
        ),
        "MFCreateSourceFromMediaSource"
    );
    
    //Create the requested output media type
    Microsoft::WRL::ComPtr<IMFMediaType> requested_type = 
        CreateVideoMediaType(
            width,
            height,
            frame_rate
        );
    
    //Apply the requested media type to the first video stream
    ThrowIfFailed(
        source_reader->SetCurrentMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            nullptr,
            requested_type.Get()
        ),
        "IMFSourceReader::SetCurrentMediaType"
    );

    Microsoft::WRL::ComPtr<IMFMediaType> actual_type;

    ThrowIfFailed(
        source_reader->GetCurrentMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            actual_type.GetAddressOf()
        ),
        "IMFSourceReader::GetCurrentMediaType"
    );

    GUID actual_subtype{};

    ThrowIfFailed(
        actual_type->GetGUID(
            MF_MT_SUBTYPE,
            &actual_subtype
        ),
        "IMFMediaType::GetGUID(MF_MT_SUBTYPE)"
    );

    UINT32 actual_width = 0;
    UINT32 actual_height = 0;
    UINT32 actual_frame_rate_numerator = 0;
    UINT32 actual_frame_rate_denominator = 1;

    ThrowIfFailed(
        MFGetAttributeSize(
            actual_type.Get(),
            MF_MT_FRAME_SIZE,
            &actual_width,
            &actual_height
        ),
        "MFGetAttributeSize(MF_MT_FRAME_SIZE)"
    );

    ThrowIfFailed(
        MFGetAttributeRatio(
            actual_type.Get(),
            MF_MT_FRAME_RATE,
            &actual_frame_rate_numerator,
            &actual_frame_rate_denominator
        ),
        "MFGetAttributeRatio(MF_MT_FRAME_RATE)"
    );

    UINT32 actual_frame_rate = 
        actual_frame_rate_numerator / actual_frame_rate_denominator;
    
    const VideoSubtype selected_subtype = 
        ToVideoSubtype(actual_subtype);

    UINT32 actual_stride = 
        MFGetAttributeUINT32(
            actual_type.Get(),
            MF_MT_DEFAULT_STRIDE,
            width
        );

    //Store the requested capture configuration
    impl_->media_source = std::move(media_source);
    impl_->source_reader = std::move(source_reader);

    impl_->width = actual_width;
    impl_->height = actual_height;
    impl_->frame_rate = actual_frame_rate;
    impl_->stride = actual_stride;
    impl_->subtype = selected_subtype;

    //The real camera initialization will be implemented later.
    //For now, mark the capturer as logically opened.
    impl_->opened = true;

    return true;
} 

bool MfVideoCapturer::Open(
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t frame_rate)
{
    const auto devices = 
        EnumerateDevices();

    if (devices.empty())
    {
        return false;
    }

    return Open(
        devices.front().index,
        width,
        height,
        frame_rate
    );
}

std::optional<CapturedVideoFrame>
MfVideoCapturer::ReadFrame() {
    if(!impl_->opened){
        throw std::logic_error(
            "Camera is not open."
        );
    }

    //The real IMFSourceReader::ReadSample()
    DWORD stream_index = 0;
    DWORD stream_flags = 0;
    LONGLONG timestamp_100ns = 0;

    Microsoft::WRL::ComPtr<IMFSample> sample;

    const HRESULT result = 
        impl_->source_reader->ReadSample(
           static_cast<DWORD> (MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            &stream_index,
            &stream_flags,
            &timestamp_100ns,
            sample.GetAddressOf()
        );
    
    ThrowIfFailed(
        result,
        "IMFSourceReader::ReadSample"
    );

    if((stream_flags & MF_SOURCE_READERF_ERROR) != 0U) {
        throw std::runtime_error(
            "Source reader reported a stream error."
        );
    }
    
    if((stream_flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0U) {
        //The stream has reached its end
        return std::nullopt;
    }

    if(sample == nullptr) {
        //No sample is available at this moment
        return std::nullopt;
    }

    //Convert the sample into one contiguous media buffer
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;

    ThrowIfFailed(
        sample->ConvertToContiguousBuffer(
            buffer.GetAddressOf()
        ),
        "IMFSample::ConvertToContiguousBuffer"
    );

    BYTE* raw_data = nullptr;
    DWORD max_length = 0;
    DWORD current_length = 0;

    ThrowIfFailed(
        buffer->Lock(
            &raw_data,
            &max_length,
            &current_length
        ),
        "IMFMediaBuffer::Lock"
    );  

    if (raw_data == nullptr || current_length == 0U) {
        //The buffer does not contain valid image data
        static_cast<void>(
            buffer->Unlock()
        );

        return std::nullopt;
    }

    std::vector<std::uint8_t> frame_data;

    frame_data.resize(
        static_cast<std::size_t>(current_length)
    );

    if(current_length > 0U) {
        std::memcpy(
            frame_data.data(),
            raw_data,
            static_cast<std::size_t>(current_length)
        );
    }

    ThrowIfFailed(
        buffer->Unlock(),
        "IMFMediaBuffer::Unlock"
    );

    CapturedVideoFrame frame{
        .data = std::move(frame_data),
        .width = impl_->width,
        .height = impl_->height,
        .stride = impl_->stride,
        .timestamp_100ns = 
            static_cast<std::int64_t>(timestamp_100ns),
        .subtype = impl_->subtype
    };

    return frame;
}

MfVideoCapturer::~MfVideoCapturer() {
    Close();
}

void MfVideoCapturer::Close() noexcept {
    if(!impl_->opened)   
        return;
    
    //Shutdown the media source before releasing it
    if(impl_->media_source) {
        static_cast<void>(
            impl_->media_source->Shutdown()
        );
    }

    //Release COM objects through ComPtr.
    impl_->source_reader.Reset();
    impl_->media_source.Reset();

    //Clear the cached configuration
    impl_->width = 0;
    impl_->height = 0;
    impl_->frame_rate = 0;
    impl_->stride = 0;
    impl_->subtype = VideoSubtype::kUnknown;

    impl_->opened = false;
}

} // namespace aegis::media