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

inline void assert(bool cond, const char *msg) {
  silog::assert(cond, msg);
  if (!cond)
    throw 0;
}
inline int assert_p(int i, const char *msg) {
  assert(i >= 0, msg);
  return i;
}

void run(const char *filename) {
  silog::log(silog::info, "Processing [%s]", filename);

  hai::holder<AVFormatContext, deleter> fmt_ctx{};
  assert_p(avformat_open_input(&*fmt_ctx, filename, nullptr, nullptr),
           "Failed to read input file");
  assert_p(avformat_find_stream_info(*fmt_ctx, nullptr),
           "Could not find stream info");

  // TODO: detect stream type instead of guessing
  // This allows usage of files with multiple streams (ex: OBS with two audio
  // tracks)

  auto vidx = assert_p(
      av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0),
      "Could not find video stream");
  auto vst = (*fmt_ctx)->streams[vidx];
  assert(vst, "Missing video stream");
  auto vdec = avcodec_find_decoder(vst->codecpar->codec_id);
  assert(vdec, "Could not find video codec");

  hai::holder<AVCodecContext, deleter> vdec_ctx{avcodec_alloc_context3(vdec)};
  assert(*vdec_ctx, "Could not allocate video codec context");
  assert_p(avcodec_parameters_to_context(*vdec_ctx, vst->codecpar),
           "Could not copy video codec parameters");
  assert_p(avcodec_open2(*vdec_ctx, vdec, nullptr),
           "Could not open video codec");

  auto aidx =
      av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
  assert_p(aidx, "Could not find video stream");
  auto ast = (*fmt_ctx)->streams[aidx];
  assert(ast, "Missing audio stream");
  auto adec = avcodec_find_decoder(ast->codecpar->codec_id);
  assert(adec, "Could not find audio codec");

  hai::holder<AVCodecContext, deleter> adec_ctx{avcodec_alloc_context3(adec)};
  assert(*adec_ctx, "Could not allocate video codec context");
  assert_p(avcodec_parameters_to_context(*adec_ctx, ast->codecpar),
           "Could not copy video codec parameters");
  assert_p(avcodec_open2(*adec_ctx, adec, nullptr),
           "Could not open video codec");

  silog::log(silog::info, "yay V[%s] A[%s]", vdec->name, adec->name);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    silog::log(silog::error, "Missing filename");
    return 1;
  }
  try {
    run(argv[1]);
    return 0;
  } catch (...) {
    return 1;
  }
}
