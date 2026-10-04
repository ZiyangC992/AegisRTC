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
	    
 	    .network_backend = 
		aegis::engine::NetworkBackend::kUdp,

	    .udp_local_address = 
		"192.168.233.129",

	    .udp_local_port = 
		9000,

	    .udp_remote_address = 
		"192.168.233.1",

	    .udp_remote_port = 
		9001,

            .frame_count = 10U,
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
