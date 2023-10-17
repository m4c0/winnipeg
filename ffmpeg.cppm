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

export module ffmpeg;
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

struct frame_size {
  int width;
  int height;
};
export struct coro {
  struct promise_type;
  using handle_t = std::coroutine_handle<promise_type>;

  struct promise_type {
    AVFrame *value;
    frame_size size;
    bool failed;

    coro get_return_object() { return {handle_t::from_promise(*this)}; }
    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    std::suspend_always yield_value(AVFrame *v) {
      value = v;
      return {};
    }
    std::suspend_never yield_value(frame_size s) {
      size = s;
      return {};
    }
    void unhandled_exception() { failed = true; }
  };

  handle_t h;

  auto failed() const noexcept { return h.promise().failed; }
  auto size() const noexcept { return h.promise().size; }

  explicit operator bool() const noexcept { return !h.done() && !failed(); }
  auto operator()() {
    h.resume();
    return h.promise().value;
  }
  ~coro() { h.destroy(); }
};

export coro play(const char *filename) {
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

  av_dump_format(*fmt_ctx, 0, filename, 0);

  // auto pix_fmt = (*vdec_ctx)->pix_fmt;
  auto w = (*vdec_ctx)->width;
  auto h = (*vdec_ctx)->height;
  co_yield frame_size{w, h};

  hai::holder<AVFrame, deleter> frm{av_frame_alloc()};
  hai::holder<AVPacket, deleter> pkt{av_packet_alloc()};

  unsigned vcount{};
  unsigned acount{};
  while (av_read_frame(*fmt_ctx, *pkt) >= 0) {
    hai::holder<AVPacket, unref> pref{*pkt};

    // From FFMPEG docs: "For video, the packet contains exactly one frame. For
    // audio, it contains an integer number of frames if each frame has a known
    // fixed size (e.g. PCM or ADPCM data). If the audio frames have a variable
    // size (e.g. MPEG audio), then it contains one frame."
    if ((*pkt)->stream_index == vidx) {
      vcount++;
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
      acount++;
    }
  }

  // TODO: flush decoders
  // avcodec_send_packet(*vdec_ctx, nullptr);
  // avcodec_send_packet(*adec_ctx, nullptr);
  // then send/read

  silog::log(silog::info, "yay V[%s/%d] A[%s/%d]", vdec->name, vcount,
             adec->name, acount);
}