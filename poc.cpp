#pragma leco tool
#pragma leco add_include_dir "ffmpeg/include"
#pragma leco add_library "ffmpeg/lib/libavformat.dylib"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

import hai;
import silog;

struct deleter {
  void operator()(AVCodecContext *c) { avcodec_free_context(&c); }
  void operator()(AVFormatContext *c) { avformat_close_input(&c); }
};

int main(int argc, char **argv) {
  if (argc < 2) {
    silog::log(silog::error, "Missing filename");
    return 1;
  }
  const auto *filename = argv[1];

  hai::holder<AVFormatContext, deleter> fmt_ctx{};
  if (avformat_open_input(&*fmt_ctx, filename, nullptr, nullptr) < 0) {
    silog::log(silog::error, "Failed to read [%s]", filename);
    return 1;
  }
  if (avformat_find_stream_info(*fmt_ctx, nullptr) < 0) {
    silog::log(silog::error, "Could not find stream info");
    return 1;
  }

  auto vidx =
      av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  if (vidx < 0) {
    silog::log(silog::error, "Could not find video stream");
    return 1;
  }

  auto st = (*fmt_ctx)->streams[vidx];
  auto vdec = avcodec_find_decoder(st->codecpar->codec_id);
  if (!vdec) {
    silog::log(silog::error, "Could not find video codec");
    return 1;
  }

  hai::holder<AVCodecContext, deleter> vdec_ctx{avcodec_alloc_context3(vdec)};
  if (!*vdec_ctx) {
    silog::log(silog::error, "Could not allocate video codec context");
    return 1;
  }
  if (avcodec_parameters_to_context(*vdec_ctx, st->codecpar) < 0) {
    silog::log(silog::error, "Could not copy video codec parameters");
    return 1;
  }
  if (avcodec_open2(*vdec_ctx, vdec, nullptr) < 0) {
    silog::log(silog::error, "Could not open video codec");
    return 1;
  }

  silog::log(silog::info, "yay %s", vdec->name);
  return 0;
}
