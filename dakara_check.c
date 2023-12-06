
#include <ffmpegaacsucks.h>
#include <libavutil/avutil.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *UNHANDLED_STREAM_MSG = "Unhandled stream type";
static const char *LAVC_AAC_STREAM_MSG = "Lavc/FFMPEG AAC stream";
static const char *TOO_MANY_AUDIO_STREAMS_MSG = "Too many audio streams";
static const char *TOO_MANY_VIDEO_STREAMS_MSG = "Too many video streams";
static const char *TOO_MANY_SUBTITLE_STREAMS_MSG = "Too many video streams";

struct dakara_check_results {
  const char *general;
  const char **streams;
  unsigned int n_streams;
  bool passed;
};

struct dakara_check_results *dakara_check(char *filepath) {
  AVFormatContext *s = NULL;
  int ret, video_streams, audio_streams, sub_streams;
  int i;
  uint ui;
  struct ffaacsucks_result *ffaac_res;
  struct dakara_check_results *res;

  res = malloc(sizeof(struct dakara_check_results));
  res->passed = true;
  res->n_streams = 0;
  res->streams = NULL;
  res->general = NULL;

  ret = avformat_open_input(&s, filepath, NULL, NULL);
  if (ret < 0) {
    fprintf(stderr, "failed to load file %s: %s\n", filepath, strerror(errno));
    res->passed = false;
    return res;
  }

  video_streams = 0;
  audio_streams = 0;
  sub_streams = 0;
  res->n_streams = s->nb_streams;
  res->streams = malloc(sizeof(char *) * res->n_streams);
  for (ui = 0; ui < res->n_streams; ui++)
    res->streams[ui] = NULL;

  for (ui = 0; ui < s->nb_streams; ui++) {
    AVStream *st = s->streams[ui];
    AVCodecParameters *par = st->codecpar;

    switch (par->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
      if (video_streams++ > 0) {
        res->streams[ui] = TOO_MANY_VIDEO_STREAMS_MSG;
        res->passed = false;
      }
      break;
    case AVMEDIA_TYPE_AUDIO:
      // we allow 2 audio streams for instrumentals
      if (audio_streams++ > 1) {
        res->streams[ui] = TOO_MANY_AUDIO_STREAMS_MSG;
        res->passed = false;
      }
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      if (sub_streams++ > 0) {
        res->streams[ui] = TOO_MANY_SUBTITLE_STREAMS_MSG;
        res->passed = false;
      }
      break;
    default:
      res->streams[ui] = UNHANDLED_STREAM_MSG;
      res->passed = false;
    }
  }

  ffaac_res = ffaacsucks_check_avfcontext(s, filepath);
  if (ffaac_res->n_streams > 0) {
    res->passed = false;
    for (i = 0; i < ffaac_res->n_streams; i++)
      res->streams[ffaac_res->streams[i]] = LAVC_AAC_STREAM_MSG;
  }
  ffaacsucks_result_free(ffaac_res);

  avformat_close_input(&s);
  avformat_free_context(s);

  return res;
}

void dakara_check_results_free(struct dakara_check_results *res) {
  if (res->streams != NULL)
    free(res->streams);
  free(res);
}

void dakara_check_print_results(struct dakara_check_results *res,
                                char *filepath) {
  uint i;
  if (res->general != NULL)
    fprintf(stderr, "%s: %s\n", filepath, res->general);

  for (i = 0; i < res->n_streams; i++) {
    if (res->streams[i] != NULL)
      fprintf(stderr, "%s: Stream %d: %s\n", filepath, i, res->streams[i]);
  }
}

int main(int argc, char *argv[]) {
  struct dakara_check_results *res;

  if (argc < 2) {
    fprintf(stderr, "usage: dakara_check <FILE>\n");
    return EXIT_FAILURE;
  }

  res = dakara_check(argv[1]);
  if (res->passed) {
    dakara_check_results_free(res);
    return EXIT_SUCCESS;
  }

  dakara_check_print_results(res, argv[1]);
  dakara_check_results_free(res);
  return EXIT_FAILURE;
}
