#include "aegis/media/format_selector.hpp"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    using namespace aegis::media;

    const std::vector<CameraFormatInfo> formats{
        {
            .format_index = 0,
            .width = 640,
            .height = 480,
            .frame_rate_numerator = 30,
            .frame_rate_denominator = 1,
            .subtype = VideoSubtype::kYuy2
        },

        {
            .format_index = 1,
            .width = 640,
            .height = 480,
            .frame_rate_numerator = 30,
            .frame_rate_denominator = 1,
            .subtype = VideoSubtype::kMjpg
        },

        {
            .format_index = 2,
            .width = 1280,
            .height = 720,
            .frame_rate_numerator = 30,
            .frame_rate_denominator = 1,
            .subtype = VideoSubtype::kMjpg
        }
    };

    const auto selected = 
        SelectBestCameraFormat(
            formats,
            640,
            480,
            30
        );

    assert(selected.has_value());
    assert(selected->format_index == 1);
    assert(selected->subtype == VideoSubtype::kMjpg);

    const auto invalid = 
        SelectBestCameraFormat(
            formats,
            0,
            480,
            30
        );

    assert(!invalid.has_value());
    
    std::cout
        << "Format selector tests passed. \n" ;
    
    return 0;
}