#pragma leco tool
import ffmpeg;
import silog;

int main(int argc, char **argv) {
  if (argc < 2) {
    silog::log(silog::error, "Missing filename");
    return 1;
  }
  try {
    auto c = play(argv[1]);
    while (c) {
      auto frm = c();
      silog::log(silog::info, "%d", frm->width);
    }
    return 0;
  } catch (...) {
    return 1;
  }
}
