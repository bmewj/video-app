#ifndef video_reader_hpp
#define video_reader_hpp

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <inttypes.h>
}

struct VideoReaderState {
    // Public things for other parts of the program to read from
    int width, height;
    AVRational time_base;

    // Private internal state
    AVFormatContext* av_format_ctx;
    AVCodecContext* av_codec_ctx;
    int video_stream_index;
    AVFrame* av_frame;
    AVPacket* av_packet;
    AVBufferRef* hw_device_ctx;
    AVBufferRef* hw_frames_ctx;
    AVFrame* hw_frame;
};

bool video_reader_open(VideoReaderState* state, const char* filename);
bool video_reader_read_frame(VideoReaderState* state, int64_t* pts);
bool video_reader_seek_frame(VideoReaderState* state, int64_t ts);
void video_reader_close(VideoReaderState* state);

#endif
