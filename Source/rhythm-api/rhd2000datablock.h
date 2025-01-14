#pragma once

#include <fmt/format.h>

#include <cstdint>
#include <span>
#include <vector>

#include "intan_chip.h"
#include "utils.h"

constexpr int SAMPLES_PER_DATA_BLOCK = 128;
constexpr int CHANNELS_PER_STREAM = 32;
constexpr std::uint64_t RHD2000_HEADER_MAGIC_NUMBER = 0xd7a22aaa38132a53;

template <typename Unit, typename amp = std::uint16_t>
std::size_t sample_size(int streams, int channels_per_stream, bool dio32)
{
    // magic number 8 bytes; time stamp 4 bytes;
    // (amp channels + 3 aux commands) * size of amp
    int sample_size = 8 + 4 + (streams * (channels_per_stream + 3) * sizeof(amp));
    // 0-3 filler words; ADCs 8 * 2 bytes; TTL in/out 16+16 bits or 32+32 bits
    if (dio32) {
        sample_size += ((streams + 2) % 4) * 2 + 16 + 8;
    } else {
        sample_size += (streams % 4) * 2 + 16 + 4;
    }
    return sample_size / sizeof(Unit);
}

template <typename Unit, typename amp = uint16_t>
std::size_t block_size(int samples, int streams, int channels_per_stream, bool dio32)
{
    return samples * sample_size<Unit, amp>(streams, channels_per_stream, dio32) / sizeof(Unit);
}

template <typename Unit, typename amp = std::uint16_t>
std::size_t max_sample_size(int streams)
{
    return sample_size<Unit, amp>(streams, CHANNELS_PER_STREAM, true);
}

template <std::uint64_t Magic,
          auto &as_ts,      // always uint32_t but keep for consistency
          auto &cast_ts_f,  // cast to desired type, OE use int64_t
          auto &as_amp, auto &cast_amp_f, auto &amp_index,  // deseralize and cast ampifier data
          auto &as_adc, auto &cast_adc_f, auto &adc_index, bool use_span = false>
class DataBlock
{
public:
    using ts_orig_t = decltype(as_ts(nullptr));
    using amp_orig_t = decltype(as_amp(nullptr));
    using adc_orig_t = decltype(as_adc(nullptr));
    using ts_t = decltype(cast_ts_f(as_ts(nullptr)));
    using amp_t = decltype(cast_amp_f(as_amp(nullptr)));
    using adc_t = decltype(cast_adc_f(as_adc(nullptr)));

    const int num_samples;
    const int num_streams;
    const int num_adc = 8;
    // const int num_aux = 3;
    const bool dio32;

    // TODO: upgrade to std::span when C++20 is more commonly available to avoid copying data
    std::vector<ts_t> timeStamp;
    std::vector<std::vector<std::uint16_t>> aux;
    std::conditional_t<use_span, std::span<amp_t>, std::vector<amp_t>> amp;
    std::conditional_t<use_span, std::span<adc_t>, std::vector<adc_t>> adc;
    std::vector<std::uint32_t> ttlIn;
    std::vector<std::uint32_t> ttlOut;

    template <typename Unit>
    std::size_t get_block_size() const
    {
        return block_size<Unit>(num_samples, num_streams, CHANNELS_PER_STREAM, dio32);
    }

    template <bool e = !use_span, typename = std::enable_if_t<e>>
    DataBlock(int numDataStreams, int num_samples = SAMPLES_PER_DATA_BLOCK, bool dio32 = false,
              const unsigned char *buffer = nullptr)
        : num_samples(num_samples),
          num_streams(numDataStreams),
          dio32(dio32),
          timeStamp(num_samples),
          amp(numDataStreams * CHANNELS_PER_STREAM * num_samples),
          adc(num_samples * 8),
          ttlIn(num_samples),
          ttlOut(num_samples)
    {
        for (int i = 0; i < 3; ++i) aux.emplace_back(numDataStreams * num_samples);

        if (buffer != nullptr) from_buffer(buffer);
    }

    template <bool e = use_span, typename = std::enable_if_t<e>>
    DataBlock(amp_t *amp_begin, adc_t *adc_begin, int numDataStreams,
              int num_samples = SAMPLES_PER_DATA_BLOCK, bool dio32 = false,
              const unsigned char *buffer = nullptr)
        : amp(amp_begin, numDataStreams * CHANNELS_PER_STREAM * num_samples),
          adc(adc_begin, num_samples * 8),
          num_samples(num_samples),
          dio32(dio32),
          timeStamp(num_samples),
          num_streams(numDataStreams),
          ttlIn(num_samples),
          ttlOut(num_samples)
    {
        for (int i = 0; i < 3; ++i) aux.emplace_back(numDataStreams * num_samples);

        if (buffer != nullptr) from_buffer(buffer);
    }


    bool from_buffer(const unsigned char *buffer)
    {
        using namespace utils::endian;
        for (int t = 0; t < num_samples; ++t) {
            if (little2host64(buffer) != Magic) {
                return false;
            }
            buffer += 8;
            timeStamp[t] = cast_ts_f(as_ts(buffer));
            buffer += sizeof(ts_orig_t);
            // Read auxiliary results
            for (int channel = 0; channel < 3; ++channel) {
                for (int stream = 0; stream < num_streams; ++stream) {
                    aux[channel][stream * num_samples + t] = little2host16(buffer);
                    buffer += 2;
                }
            }
            // Read amplifier channels
            for (int channel = 0; channel < CHANNELS_PER_STREAM; ++channel) {
                for (int stream = 0; stream < num_streams; ++stream) {
                    amp[amp_index(num_samples, CHANNELS_PER_STREAM, num_streams, t, channel,
                                  stream)] = cast_amp_f(as_amp(buffer));
                    buffer += sizeof(amp_orig_t);
                }
            }
            // skip filler words in each data stream
            buffer += 2 * ((num_streams + dio32 * 2) % 4);
            // Read from ADCs
            for (int i = 0; i < num_adc; ++i) {
                adc[adc_index(num_samples, num_adc, t, i)] = cast_adc_f(as_adc(buffer));
                buffer += sizeof(adc_orig_t);
            }
            // Read TTL input and output values
            if (dio32) {
                ttlIn[t] = little2host32(buffer);
                buffer += 4;

                ttlOut[t] = little2host32(buffer);
                buffer += 4;

            } else {
                ttlIn[t] = little2host16(buffer);
                buffer += 2;

                ttlOut[t] = little2host16(buffer);
                buffer += 2;
            }
        }

        return true;
    }
};

inline long long cast_ts(std::uint32_t x)
{
    return x;
}


// Original Order
inline int amp_index_time_channel_stream(int samples, int channels, int streams, int t, int c,
                                         int s)
{
    return t * channels * streams + c * streams + s;
}
// Transposed, Stream x Channel x Time
inline int amp_index_stream_channel_time(int samples, int channels, int streams, int t, int c,
                                         int s)
{
    return t + c * samples + s * samples * channels;
}

// Original Order
inline int adc_index_time_channel(int samples, int channels, int t, int c)
{
    return t * channels + c;
}
// Transposed, Channel x Time
inline int adc_index_channel_time(int samples, int channels, int t, int c)
{
    return t + c * samples;
}

// clang-format off
using Rhd2000DataBlock = DataBlock<RHD2000_HEADER_MAGIC_NUMBER,
    utils::endian::little2host32, cast_ts,
    utils::endian::little2host16, IntanChip::amp2uV, amp_index_stream_channel_time,
    utils::endian::little2host16, IntanChip::adc2V, adc_index_channel_time>;
using Rhd2000DataBlockView = DataBlock<RHD2000_HEADER_MAGIC_NUMBER,
    utils::endian::little2host32, cast_ts,
    utils::endian::little2host16, IntanChip::amp2uV, amp_index_stream_channel_time,
    utils::endian::little2host16, IntanChip::adc2V, adc_index_channel_time,
    true>;
// clang-format on