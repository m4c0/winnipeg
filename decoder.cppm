export module decoder;
import ffmod;
import silog;
import sith;

class decoder : public sith::thread {
  ffmod::fmt_ctx m_ctx;

public:
  explicit decoder(const char *filename)
      : m_ctx{ffmod::avformat_open_input(filename)} {}

  void run() override {
    auto decs = ffmod::avcodec_open_all(m_ctx);
    silog::assert((*decs[0])->codec_type == AVMEDIA_TYPE_VIDEO,
                  "invalid movie format");
    silog::assert((*decs[1])->codec_type == AVMEDIA_TYPE_AUDIO,
                  "invalid movie format");

    auto pkt = ffmod::av_packet_alloc();
    auto frm = ffmod::av_frame_alloc();
    while (!interrupted()) {
      auto pkt_ref = ffmod::av_read_frame(m_ctx, pkt);
      if (!*pkt_ref)
        break;

      auto idx = (*pkt)->stream_index;
      auto &dec = decs[idx];

      ffmod::avcodec_send_packet(dec, pkt);
      while (!interrupted()) {
        auto frm_ref = ffmod::avcodec_receive_frame(dec, frm);
        if (!*frm_ref)
          break;

        if ((*dec)->codec_type == AVMEDIA_TYPE_VIDEO) {
          // ffmod::copy_frame_yuv(frm, y, u, v);
        } else if ((*dec)->codec_type == AVMEDIA_TYPE_AUDIO) {
          // auto num_samples = (*frm)->nb_samples;
          // auto sample_rate = (*frm)->sample_rate;
          // auto *data = reinterpret_cast<float *>((*frm)->extended_data[0]);
        }
      }
    }
  }
};

