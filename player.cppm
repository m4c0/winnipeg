export module player;
import coro;
import hai;
import ffmod;
import siaudio;
import silog;

export struct player_promise {
  using coro = coro::t<player_promise>;

  ffmod::frame *value;
  bool failed;

  coro get_return_object() { return coro::from_promise(*this); }
  std::suspend_never initial_suspend() { return {}; }
  std::suspend_always final_suspend() noexcept { return {}; }
  std::suspend_always yield_value(ffmod::frame *v) {
    value = v;
    return {};
  }
  void unhandled_exception() {
    silog::log(silog::error, "uncaptured exception in player");
    failed = true;
  }
};
export class player {
  ffmod::fmt_ctx fmt_ctx{};
  ffmod::codec_ctx vdec_ctx{};
  ffmod::codec_ctx adec_ctx{};
  ffmod::frame frm{ffmod::av_frame_alloc()};
  siaudio::ring_buffered_stream m_audio{};
  int aidx;
  int vidx;
  bool m_stop;
  double m_start_ts{};

  void flush() {
    ffmod::avcodec_flush_buffers(vdec_ctx);
    ffmod::avcodec_flush_buffers(adec_ctx);
  }
  auto timestamp(unsigned idx) const noexcept {
    return ffmod::frame_timestamp(fmt_ctx, frm, idx);
  }

public:
  explicit player(const char *filename);

  void seek(double timestamp) {
    ffmod::avformat_seek_file(fmt_ctx, timestamp);
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

  fmt_ctx = ffmod::avformat_open_input(filename);

  // TODO: detect stream type instead of guessing
  // This allows usage of files with multiple streams (ex: OBS with two audio
  // tracks)

  vidx = ffmod::av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO);
  vdec_ctx = ffmod::avcodec_open_best(fmt_ctx, AVMEDIA_TYPE_VIDEO);

  aidx = ffmod::av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO);
  adec_ctx = ffmod::avcodec_open_best(fmt_ctx, AVMEDIA_TYPE_AUDIO);
}

player_promise::coro player::play() {
  auto pkt = ffmod::av_packet_alloc();

  while (true) {
    auto pref = ffmod::av_read_frame(fmt_ctx, pkt);
    if (!*pref)
      break;

    // From FFMPEG docs: "For video, the packet contains exactly one frame. For
    // audio, it contains an integer number of frames if each frame has a known
    // fixed size (e.g. PCM or ADPCM data). If the audio frames have a variable
    // size (e.g. MPEG audio), then it contains one frame."
    if ((*pkt)->stream_index == vidx) {
      ffmod::avcodec_send_packet(vdec_ctx, pkt);
      while (!m_stop) {
        auto fref = ffmod::avcodec_receive_frame(vdec_ctx, frm);
        if (timestamp(vidx) < m_start_ts) {
          break;
        }
        // output video frame
        co_yield &frm;
      }
    } else if ((*pkt)->stream_index == aidx) {
      ffmod::avcodec_send_packet(adec_ctx, pkt);
      while (!m_stop) {
        auto fref = ffmod::avcodec_receive_frame(vdec_ctx, frm);
        if (timestamp(vidx) < m_start_ts) {
          break;
        }

        // auto fmt = static_cast<AVSampleFormat>((*frm)->format);
        // assert(fmt == AV_SAMPLE_FMT_FLTP, "only float planar so far");

        // output audio frame
        auto *data = reinterpret_cast<float *>((*frm)->extended_data[0]);
        m_audio.push_frame(data, (*frm)->nb_samples);
        continue;
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
