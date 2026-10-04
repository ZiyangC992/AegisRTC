#include "aegis/engine/session.hpp"

// Provides std::cerr
#include <iostream>
#include <chrono>

// Provides std::exception
#include <exception>

int main()
{
    try
    {
        aegis::engine::SessionConfig config{
            .width = 640U,
            .height = 480U,
            .frame_rate = 30U,
            .target_bitrate_kbps = 1500U,
            .key_frame_interval = 60U,

            // Use the deterministic encoder first.
            .encoder_backend =
                aegis::media::VideoEncoderBackend::
                    kSimulated,

	    .capturer_backend = 
		aegis::media::VideoCapturerBackend::
		    kDefault,

            .frame_count = 100U,
            .frame_interval =
                std::chrono::milliseconds{33}
        };

        aegis::engine::Session session{
            config
        };

        session.Run();
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Application failed: "
            << exception.what()
            << '\n';

        return 1;
    }

    return 0;
}
