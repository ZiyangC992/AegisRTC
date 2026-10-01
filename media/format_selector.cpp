#include "aegis/media/format_selector.hpp"

#include <optional>
#include <cstdint>
#include <vector>
#include <limits>

namespace aegis::media {
    
namespace {

//Calculate the absolute difference between two unsigned values
//without unsigned integer underflow
[[nodiscard]] std::int64_t AbsoluteDifference(
    std::uint32_t left,
    std::uint32_t right
) noexcept {
    const std::int64_t signed_left = 
        static_cast<std::int64_t>(left);

    const std::int64_t signed_right = 
        static_cast<std::int64_t>(right);

    const std::int64_t difference = 
        signed_left - signed_right;
    
    return difference > 0 
        ? difference 
        : -difference;
}

[[nodiscard]] std::int64_t SubtypeScore(
    VideoSubtype subtype
) noexcept {
    switch(subtype) {
        case VideoSubtype::kMjpg:
            return 50;

        case VideoSubtype::kNv12:
            return 40;
        
        case VideoSubtype::kYuy2:
            return 20;
        
        case VideoSubtype::kH264:
        case VideoSubtype::kRgb24:
        case VideoSubtype::kI420:
        case VideoSubtype::kRgb32:
            return 10;
        
        case VideoSubtype::kUnknown:
            return -100'000;
    }

    return -100'000;

} 
} // namespace

[[nodiscard]] std::optional<CameraFormatInfo>
SelectBestCameraFormat(
        const std::vector<CameraFormatInfo>& formats,
        std::uint32_t target_width,
        std::uint32_t target_height,
        std::uint32_t target_frame_rate
    ) {

        if(
            target_width == 0U ||
            target_height == 0U ||
            target_frame_rate == 0U
        ) return std::nullopt;

        std::optional<CameraFormatInfo>
        best_format = std::nullopt;

        std::int64_t best_score = 
            std::numeric_limits<std::int64_t>::lowest();

        for(const CameraFormatInfo& format : formats){
            if(
                format.width == 0U ||
                format.height == 0U ||
                format.frame_rate_numerator == 0U ||
                format.frame_rate_denominator ==0U
            ) continue;
                
            std::uint32_t actual_frame_rate = 
            format.frame_rate_numerator / format.frame_rate_denominator;

            if(actual_frame_rate == 0U)
                continue;

            std::int64_t score = 0;
            
            score -= AbsoluteDifference(format.width,target_width) * 10;
            score -= AbsoluteDifference(format.height,target_height) * 10;
            score -= AbsoluteDifference(actual_frame_rate,target_frame_rate) * 100;
            
            //Exact resolution match is strongly preferred
            if(
                format.width == target_width &&
                format.height == target_height
            ) {
                score += 10'000;
            }
            
            //Exact frame-rate match is also preferred
            if(actual_frame_rate == target_frame_rate){
                score += 1'000;
            }
            
            score += SubtypeScore(format.subtype);

            if(
                !best_format.has_value() ||
                score > best_score)
            {
                best_score = score;
                best_format = format;
            }
        }

        return best_format;
    }

} // namespace aegis::media