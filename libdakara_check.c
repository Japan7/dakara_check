
#include <ffmpegaacsucks.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "dakara_check.h"


#define UNHANDLED_STREAM_MSG "Unhandled stream type";
#define LAVC_AAC_STREAM_MSG "Lavc/FFMPEG AAC stream";
#define TOO_MANY_AUDIO_STREAMS_MSG "Too many audio streams";
#define TOO_MANY_VIDEO_STREAMS_MSG "Too many video streams";
#define TOO_MANY_SUBTITLE_STREAMS_MSG "Internal subtitle track should be removed";
#define ATTACHMENT_STREAM_MSG "Attachment found (probably a font)";


struct dakara_check_results *dakara_check(char *filepath, int external_sub_file) {
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
      if ((external_sub_file + sub_streams++) > 0) {
        res->streams[ui] = TOO_MANY_SUBTITLE_STREAMS_MSG;
        res->passed = false;
      }
      break;
    case AVMEDIA_TYPE_ATTACHMENT:
      res->streams[ui] = ATTACHMENT_STREAM_MSG;
      res->passed = false;
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
    printf("%s: %s\n", filepath, res->general);

  for (i = 0; i < res->n_streams; i++) {
    if (res->streams[i] != NULL)
      printf("%s: Stream %d: %s\n", filepath, i, res->streams[i]);
  }
}

int dakara_check_sub_file(char *filepath) {
  int ret;
  AVFormatContext *s = NULL;

  ret = avformat_open_input(&s, filepath, NULL, NULL);
  // TODO: could check that it actually contains a sub stream
  avformat_close_input(&s);
  avformat_free_context(s);

  return ret == 0;
}

int dakara_check_external_sub_file_for(char *filepath) {
  char *filebasepath = strdup(filepath);
  uint basepathlen = strlen(filepath);

  while (filebasepath[basepathlen-1] != '.' && basepathlen > 0) {
    basepathlen--;
  }
  filebasepath[basepathlen] = '\0';

  char sub_filepath[basepathlen + 4];
  // check .ass
  snprintf(sub_filepath, basepathlen + 4, "%sass", filebasepath);
  // return false if the original file is already the .ass
  if (strcmp(sub_filepath, filepath) == 0)
    return 0;

  if (dakara_check_sub_file(sub_filepath)) {
    free(filebasepath);
    return 1;
  }

  // check .ssa
  snprintf(sub_filepath, basepathlen + 4, "%sssa", filebasepath);
  // return false if the original file is already the .ssa
  if (strcmp(sub_filepath, filepath) == 0)
    return 0;

  free(filebasepath);
  return dakara_check_sub_file(sub_filepath);
}

