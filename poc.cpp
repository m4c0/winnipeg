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

  silog::log(silog::info, "yay %p", *fmt_ctx);
  return 0;
}
