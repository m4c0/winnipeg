module;
#pragma leco add_include_dir "ffmpeg/include"
#pragma leco add_library "ffmpeg/lib/libavcodec.dylib"
#pragma leco add_library "ffmpeg/lib/libavformat.dylib"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

export module player;
import coro;
import hai;
import silog;

struct deleter {
  void operator()(AVCodecContext *c) { avcodec_free_context(&c); }
  void operator()(AVFormatContext *c) { avformat_close_input(&c); }
  void operator()(AVFrame *c) { av_frame_free(&c); }
  void operator()(AVPacket *c) { av_packet_free(&c); }
};
struct unref {
  void operator()(AVFrame *c) { av_frame_unref(c); }
  void operator()(AVPacket *c) { av_packet_unref(c); }
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

export struct player_promise {
  using coro = ::coro<player_promise>;

  AVFrame *value;
  bool failed;

  coro get_return_object() { return coro::from_promise(*this); }
  std::suspend_never initial_suspend() { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  std::suspend_always yield_value(AVFrame *v) {
    value = v;
    return {};
  }
  void unhandled_exception() { failed = true; }
};
export class player {
  hai::holder<AVFormatContext, deleter> fmt_ctx{};
  hai::holder<AVCodecContext, deleter> vdec_ctx{};
  hai::holder<AVCodecContext, deleter> adec_ctx{};
  hai::holder<AVFrame, deleter> frm{av_frame_alloc()};
  int aidx;
  int vidx;

public:
  explicit player(const char *filename);

  void seek(double timestamp) {
    auto vst = (*fmt_ctx)->streams[vidx];
    auto tb = static_cast<int>(timestamp / av_q2d(vst->time_base));
    assert_p(avformat_seek_file(*fmt_ctx, vidx, tb - 1, tb, tb + 1, 0),
             "Failed to seek");
  }

  auto width() { return (*vdec_ctx)->width; }
  auto height() { return (*vdec_ctx)->height; }

  auto timestamp() {
    auto vst = (*fmt_ctx)->streams[vidx];
    auto t = static_cast<double>((*frm)->pts);
    auto tb = vst->time_base;
    return t * av_q2d(tb);
  }

  player_promise::coro play();
};

player::player(const char *filename) {
  silog::log(silog::info, "Processing [%s]", filename);

  assert_p(avformat_open_input(&*fmt_ctx, filename, nullptr, nullptr),
           "Failed to read input file");
  assert_p(avformat_find_stream_info(*fmt_ctx, nullptr),
           "Could not find stream info");

  // TODO: detect stream type instead of guessing
  // This allows usage of files with multiple streams (ex: OBS with two audio
  // tracks)

  vidx = assert_p(
      av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0),
      "Could not find video stream");
  auto vst = (*fmt_ctx)->streams[vidx];
  assert(vst, "Missing video stream");
  auto vdec = avcodec_find_decoder(vst->codecpar->codec_id);
  assert(vdec, "Could not find video codec");

  *vdec_ctx = avcodec_alloc_context3(vdec);
  assert(*vdec_ctx, "Could not allocate video codec context");
  assert_p(avcodec_parameters_to_context(*vdec_ctx, vst->codecpar),
           "Could not copy video codec parameters");
  assert_p(avcodec_open2(*vdec_ctx, vdec, nullptr),
           "Could not open video codec");

  aidx = av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
  assert_p(aidx, "Could not find video stream");
  auto ast = (*fmt_ctx)->streams[aidx];
  assert(ast, "Missing audio stream");
  auto adec = avcodec_find_decoder(ast->codecpar->codec_id);
  assert(adec, "Could not find audio codec");

  *adec_ctx = avcodec_alloc_context3(adec);
  assert(*adec_ctx, "Could not allocate video codec context");
  assert_p(avcodec_parameters_to_context(*adec_ctx, ast->codecpar),
           "Could not copy video codec parameters");
  assert_p(avcodec_open2(*adec_ctx, adec, nullptr),
           "Could not open video codec");

  av_dump_format(*fmt_ctx, 0, filename, 0);
}

player_promise::coro player::play() {
  hai::holder<AVPacket, deleter> pkt{av_packet_alloc()};

  while (av_read_frame(*fmt_ctx, *pkt) >= 0) {
    hai::holder<AVPacket, unref> pref{*pkt};

    // From FFMPEG docs: "For video, the packet contains exactly one frame. For
    // audio, it contains an integer number of frames if each frame has a known
    // fixed size (e.g. PCM or ADPCM data). If the audio frames have a variable
    // size (e.g. MPEG audio), then it contains one frame."
    if ((*pkt)->stream_index == vidx) {
      assert_p(avcodec_send_packet(*vdec_ctx, *pkt),
               "Error sending packet to decode");
      while (true) {
        auto res = avcodec_receive_frame(*vdec_ctx, *frm);
        if (res >= 0) {
          hai::holder<AVFrame, unref> fref{*frm};
          // output video frame
          co_yield *frm;
          continue;
        }
        if (res == AVERROR_EOF || AVERROR(EAGAIN))
          break;
        assert_p(res, "Error decoding video frame");
      }
    } else if ((*pkt)->stream_index == aidx) {
    }
  }

  // TODO: flush decoders
  // avcodec_send_packet(*vdec_ctx, nullptr);
  // avcodec_send_packet(*adec_ctx, nullptr);
  // then send/read

  silog::log(silog::info, "Movie ended");
}
