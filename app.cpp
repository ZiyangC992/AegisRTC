#include "aegis/common.hpp"
#include "aegis/engine/session.hpp"

#include <windows.h>
#include <mfapi.h>

#include <iostream>


int main()
{
    const HRESULT com_result = 
        CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED
        );

    if (FAILED(com_result))
    {
        return 1;
    }

    const HRESULT mf_result = 
        MFStartup(
            MF_VERSION,
            MFSTARTUP_FULL
        );

    if (FAILED(mf_result))
    {
        CoUninitialize();
        return 1;
    }

    int exit_code = 0;

    try
    {

        aegis::engine::SessionConfig config{
            .width = 640U,
            .height = 480U,
            .frame_rate = 30U,
            .target_bitrate_kbps = 1500U,
            .key_frame_interval = 60U,

            .encoder_backend =
                aegis::media::VideoEncoderBackend::kFfmpeg,

            .capturer_backend =
                aegis::media::VideoCapturerBackend::
                    kDefault,

            .network_backend =
                aegis::engine::NetworkBackend::kUdp,

            .udp_local_address =
                "192.168.233.1",

            .udp_local_port =
                9001,

            .udp_remote_address =
                "192.168.233.129",

            .udp_remote_port =
                9000,
                
            .frame_count = 10U,
            .frame_interval =
                std::chrono::milliseconds{33},

            .bandwidth_bps = 1'500'000U,
            .network_base_delay =
                std::chrono::milliseconds{30},
            .network_jitter =
                std::chrono::milliseconds{5},
            .network_random_loss_rate = 0.05,
            .network_random_seed = 20260902U,

            .retransmission_cache_packets = 512U,
            .retransmission_cache_age =
                std::chrono::milliseconds{1000}};

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

        exit_code = 1;
    }

    MFShutdown();
    CoUninitialize();

    return exit_code;
}