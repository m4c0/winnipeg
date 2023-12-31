module;
#pragma leco add_include_dir "ffmpeg/include"
#pragma leco add_library_dir "ffmpeg/lib"
#pragma leco add_library "avcodec"
#pragma leco add_library "avformat"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
}

export module player;
import audio;
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
  using coro = coro::t<player_promise>;

  AVFrame *value;
  bool failed;

  coro get_return_object() { return coro::from_promise(*this); }
  std::suspend_never initial_suspend() { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  std::suspend_always yield_value(AVFrame *v) {
    value = v;
    return {};
  }
  void unhandled_exception() {
    silog::log(silog::error, "uncaptured exception in player");
    failed = true;
  }
};
export class player {
  hai::holder<AVFormatContext, deleter> fmt_ctx{};
  hai::holder<AVCodecContext, deleter> vdec_ctx{};
  hai::holder<AVCodecContext, deleter> adec_ctx{};
  hai::holder<AVFrame, deleter> frm{av_frame_alloc()};
  audio m_audio;
  int aidx;
  int vidx;
  bool m_stop;
  double m_start_ts{};

  void flush() {
    avcodec_flush_buffers(*vdec_ctx);
    avcodec_flush_buffers(*adec_ctx);
  }
  auto timestamp(int idx) const noexcept {
    auto st = (*fmt_ctx)->streams[idx];
    auto t = static_cast<double>((*frm)->pts);
    auto tb = st->time_base;
    return t * av_q2d(tb);
  }

public:
  explicit player(const char *filename);

  void seek(double timestamp) {
    auto vtb = static_cast<int>(timestamp * static_cast<double>(AV_TIME_BASE));
    assert_p(avformat_seek_file(*fmt_ctx, -1, INT64_MIN, vtb, vtb, 0),
             "Failed to seek");
    m_audio.stop();
    m_audio.flush();
    m_audio.start();
    m_start_ts = timestamp;
  }
  void stop() { m_stop = true; }

  auto width() { return (*vdec_ctx)->width; }
  auto height() { return (*vdec_ctx)->height; }

  auto timestamp() const noexcept { return timestamp(vidx); }

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
               "Error sending video packet to decode");
      while (!m_stop) {
        auto res = avcodec_receive_frame(*vdec_ctx, *frm);
        if (res >= 0 && timestamp(vidx) >= m_start_ts) {
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
      assert_p(avcodec_send_packet(*adec_ctx, *pkt),
               "Error sending audio packet to decode");
      while (!m_stop) {
        auto res = avcodec_receive_frame(*adec_ctx, *frm);
        if (res >= 0 && timestamp(aidx) >= m_start_ts) {
          hai::holder<AVFrame, unref> fref{*frm};

          auto fmt = static_cast<AVSampleFormat>((*frm)->format);
          assert(fmt == AV_SAMPLE_FMT_FLTP, "only float planar so far");

          // output audio frame
          auto *data = reinterpret_cast<float *>((*frm)->extended_data[0]);
          m_audio.push_frame(data, (*frm)->nb_samples);
          continue;
        }
        if (res == AVERROR_EOF || AVERROR(EAGAIN))
          break;
        assert_p(res, "Error decoding audio frame");
      }
    }

    if (m_stop) {
      m_stop = false;
      co_yield nullptr;
    }
  }

  flush();
  silog::log(silog::info, "Movie ended");
}
