#include "aegis/media/v4l2_video_capturer.hpp"

#include <linux/videodev2.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#include <cerrno>
#include <cstring>

namespace aegis::media {

struct MappedBuffer
{
	//Address returned by mmap()
	void* start{nullptr};
	
	//Size of the mapped memory region.
	std::size_t length{0};
};

struct V4L2VideoCapturer::Impl
{
    // Linux device path, for example /dev/video0.
    std::string device_path;

    // File descriptor returned by open().
    int file_descriptor{-1};

    // Requested capture configuration.
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t frame_rate{0};
	
    std::vector<MappedBuffer> buffers;

    // Indicates whether the device is currently open.
    bool opened{false};
    bool streaming{false};
};

namespace {

[[nodiscard]] std::string
SystemErrorMessage(
    const char* operation)
{
    std::ostringstream stream;

    stream
        << operation
        << " failed: "
        << std::strerror(errno);

    return stream.str();
}

// Retry ioctl when it is interrupted by a signal.
int ExecuteIoctl(
    int file_descriptor,
    unsigned long request,
    void* argument)
{
    int result = 0;

    do
    {
        result = ioctl(
            file_descriptor,
            request,
            argument
        );
    }
    while (result == -1 && errno == EINTR);

    return result;
}

}; // namespace

V4L2VideoCapturer::V4L2VideoCapturer(
    std::string device_path)
    : impl_(std::make_unique<Impl>())
{
    impl_->device_path =
        std::move(device_path);
}

V4L2VideoCapturer::~V4L2VideoCapturer()
{
    Close();
}

bool V4L2VideoCapturer::Open(
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t frame_rate)
{
    if (width == 0U ||
        height == 0U ||
        frame_rate == 0U)
    {
        throw std::invalid_argument(
            "V4L2 capture configuration is invalid."
        );
    }

    if (impl_->opened)
    {
        Close();
    }

    // Open the Linux video device.
    impl_->file_descriptor =
        open(
            impl_->device_path.c_str(),
            O_RDWR
        );

    if (impl_->file_descriptor == -1)
    {
        throw std::runtime_error(
            SystemErrorMessage(
                "open V4L2 device"
            )
        );
    }

    // Query basic device capabilities.
    v4l2_capability capability{};

    if (ExecuteIoctl(
            impl_->file_descriptor,
            VIDIOC_QUERYCAP,
            &capability) == -1)
    {
        Close();

        throw std::runtime_error(
            SystemErrorMessage(
                "VIDIOC_QUERYCAP"
            )
        );
    }

    // Verify that this device supports video capture.
    if ((capability.capabilities &
         V4L2_CAP_VIDEO_CAPTURE) == 0U)
    {
        Close();

        throw std::runtime_error(
            "V4L2 device does not support video capture."
        );
    }

    // Verify that the device supports streaming I/O.
    if ((capability.capabilities &
         V4L2_CAP_STREAMING) == 0U)
    {
        Close();

        throw std::runtime_error(
            "V4L2 device does not support streaming I/O."
        );
    }

    // Request the MJPEG pixel format and resolution.
    v4l2_format format{};
    format.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat =
        V4L2_PIX_FMT_MJPEG;
    format.fmt.pix.field =
        V4L2_FIELD_ANY;

    if (ExecuteIoctl(
            impl_->file_descriptor,
            VIDIOC_S_FMT,
            &format) == -1)
    {
        Close();

        throw std::runtime_error(
            SystemErrorMessage(
                "VIDIOC_S_FMT"
            )
        );
    }

    // Request the desired frame rate.
    v4l2_streamparm stream_parameter{};
    stream_parameter.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    stream_parameter.parm.capture.timeperframe.numerator =
        1;

    stream_parameter.parm.capture.timeperframe.denominator =
        frame_rate;

    if (ExecuteIoctl(
            impl_->file_descriptor,
            VIDIOC_S_PARM,
            &stream_parameter) == -1)
    {
        Close();

        throw std::runtime_error(
            SystemErrorMessage(
                "VIDIOC_S_PARM"
            )
        );
    }
    
    // ======================================================
    // Step 1: Request capture buffers from the V4L2 driver.
    // ======================================================
    
    //Request memory-mapped capture buffers from the driver.
    v4l2_requestbuffers request_buffers{};

    request_buffers.count = 4;
    request_buffers.type = 
	V4L2_BUF_TYPE_VIDEO_CAPTURE;
    request_buffers.memory = 
	V4L2_MEMORY_MMAP;

    if (ExecuteIoctl(
	    impl_->file_descriptor,
	    VIDIOC_REQBUFS,
	    &request_buffers) == -1)
    {
	Close();
	
	throw std::runtime_error(
	    SystemErrorMessage("VIDIOC_REQBUFS"));
    }

    if (request_buffers.count == 0U)
    {
	Close();

	throw std::runtime_error(
		"V4L2 driver returned no capture buffers."
	);
    }

// Create one MappedBuffer record for each driver buffer.
impl_->buffers.resize(
    request_buffers.count
);

// ======================================================
// Step 2 and Step 3:
// Query each buffer and map it into user space.
// ======================================================

for (std::size_t index = 0U;
     index < request_buffers.count;
     ++index)
{
    v4l2_buffer buffer{};

    buffer.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    // The buffer will be accessed through mmap().
    buffer.memory =
        V4L2_MEMORY_MMAP;

    buffer.index = static_cast<std::uint32_t>(index);

    if (ExecuteIoctl(
            impl_->file_descriptor,
            VIDIOC_QUERYBUF,
            &buffer) == -1)
    {
        Close();

        throw std::runtime_error(
            SystemErrorMessage(
                "VIDIOC_QUERYBUF"
            )
        );
    }
    
    // Map the driver's buffer into this process's address space.
    void* mapped_address =
        mmap(
            nullptr,                             // Let the OS choose the address
            buffer.length,                       // Size of the buffer
            PROT_READ | PROT_WRITE,              // Allow reading and writing
            MAP_SHARED,                          // Share memory with the driver
            impl_->file_descriptor,              // The opened video device
            buffer.m.offset                      // Offset of this buffer.
        );

    // mmap() returns MAP_FAILED when mmaping fails.
    if (mapped_address == MAP_FAILED)
    {
        Close();

        throw std::runtime_error(
            SystemErrorMessage(
                "mmap V4L2 buffer"
            )
        );
    }

    impl_->buffers[index] = MappedBuffer{
        .start = mapped_address,
        .length = buffer.length
    };
}

// ======================================================
// Step 4: Queue all mapped buffers to the driver.
// ======================================================
for (std::size_t index = 0U;
     index < impl_->buffers.size();
     ++index)
{
    v4l2_buffer buffer{};

    buffer.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;

    buffer.memory =
        V4L2_MEMORY_MMAP;

    buffer.index = static_cast<std::uint32_t>(index);

    if (ExecuteIoctl(
            impl_->file_descriptor,
            VIDIOC_QBUF,
            &buffer) == -1)
    {
        Close();

        throw std::runtime_error(
            SystemErrorMessage(
                "VIDIOC_QBUF"
            )
        );
    }
}

// ======================================================
// Step 5: Start the video stream.
// ======================================================
v4l2_buf_type buffer_type =
    V4L2_BUF_TYPE_VIDEO_CAPTURE;

if (ExecuteIoctl(
        impl_->file_descriptor,
        VIDIOC_STREAMON,
        &buffer_type) == -1)
{
    Close();

    throw std::runtime_error(
        SystemErrorMessage(
            "VIDIOC_STREAMON"
        )
    );
}

    impl_->streaming = true;

    // Save the requested configuration.
    impl_->width = width;
    impl_->height = height;
    impl_->frame_rate = frame_rate;
    impl_->opened = true;

    return true;
}

void V4L2VideoCapturer::Close() noexcept
{
    if (impl_ == nullptr)
    {
        return;
    }

    if (impl_->streaming)
    {
	v4l2_buf_type buffer_type = 
		V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	static_cast<void>(
		ExecuteIoctl(
			impl_->file_descriptor,
			VIDIOC_STREAMOFF,
			&buffer_type
		)
	);
	
	impl_->streaming = false;
    }

    for (const MappedBuffer& buffer :
	 impl_->buffers)
    {
	if(buffer.start != nullptr &&
	   buffer.length > 0U)
	{
		static_cast<void>(
			munmap(
				buffer.start,
				buffer.length
			)
		);
	}
    }

    impl_->buffers.clear();

    if (impl_->file_descriptor != -1)
    {
        close(
            impl_->file_descriptor
        );

        impl_->file_descriptor = -1;
    }

    impl_->opened = false;
}

std::optional<CapturedVideoFrame>
V4L2VideoCapturer::ReadFrame()
{
    if(!IsOpen())
    {
	return std::nullopt;
    }

    //Describe the type of buffer we want to dequeue.
    v4l2_buffer buffer{};
    
    buffer.type = 
      V4L2_BUF_TYPE_VIDEO_CAPTURE;
      
    buffer.memory = 
      V4L2_MEMORY_MMAP;
      
    if (ExecuteIoctl(
            impl_->file_descriptor,
            VIDIOC_DQBUF,
            &buffer) == -1)
    {
        if (errno == EAGAIN)
        {
            return std::nullopt;
        }
        
        throw std::runtime_error(
              SystemErrorMessage(
                "VIDIOC_DQBUF"
              ));
    }
    
    // Make sure the buffer index returned by the driver is valid.
    if (buffer.index >= impl_->buffers.size())
    {
        throw std::runtime_error(
              "V4L2 driver returned an invalid buffer index."
              );
    }
    
    const MappedBuffer& mapped_buffer = 
          impl_->buffers[buffer.index];
          
    if (buffer.bytesused > mapped_buffer.length)
    {
        throw std::runtime_error(
              "V4L2 driver returned an invalid frame size."
              );
    }
    
    //Copy the captured MJPEG bytes into a C++ vector.
    std::vector<std::uint8_t> frame_data(
        buffer.bytesused);
        
    if (buffer.bytesused > 0U)
    {
      std::memcpy(
          frame_data.data(),
          mapped_buffer.start,
          buffer.bytesused
          );
    }
    
    // Put the buffer back into the driver queue.
    //
    // The driver can reuse this buffer for a future frame.
    if (ExecuteIoctl(
            impl_->file_descriptor,
            VIDIOC_QBUF,
            &buffer) == -1)
    {
        Close();
        
        throw std::runtime_error(
              SystemErrorMessage(
                  "VIDIOC_QBUF"
              ));
    }
    
    // Convert the V4L2 timestamp to 100-nanosecond units.
    const std::int64_t timestamp_100ns = 
        static_cast<std::int64_t>(
            buffer.timestamp.tv_sec
        ) * 10'000'000LL
        +
        static_cast<std::int64_t>(
            buffer.timestamp.tv_usec
        ) * 10LL;
        
    // Build the project-level captured frame object.
    CapturedVideoFrame frame{
        .data = std::move(frame_data),
        .width = impl_->width,
        .height = impl_->height,
        .stride = 0U,
        .timestamp_100ns = timestamp_100ns,
        .subtype = VideoSubtype::kMjpg
    };
    
    return frame;
}
bool V4L2VideoCapturer::IsOpen()
    const noexcept
{
    return impl_ != nullptr &&
           impl_->opened;
}

} // namespace aegis::media
