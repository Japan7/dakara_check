
#include <errno.h>
#include <ffmpegaacsucks.h>
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dakara_check.h"

#include "version.h"

const char *dakara_check_version(void) { return DAKARA_CHECK_VERSION; }

struct dakara_check_results *dakara_check_results_new(void) {
  struct dakara_check_results *res = malloc(sizeof(struct dakara_check_results));
  res->passed = true;
  res->n_streams = 0;
  res->streams = NULL;
  return res;
}

static void dakara_check_avf(AVFormatContext *s, struct dakara_check_results *res,
                             unsigned int external_sub_file, unsigned int external_audio_file) {
  unsigned int video_streams = 0;
  unsigned int audio_streams = 0;
  unsigned int sub_streams = 0;

  res->n_streams = s->nb_streams;
  res->streams = malloc(sizeof(char *) * res->n_streams);

  unsigned int ui;
  for (ui = 0; ui < res->n_streams; ui++)
    res->streams[ui] = OK;

  for (ui = 0; ui < s->nb_streams; ui++) {
    AVStream *st = s->streams[ui];
    AVCodecParameters *par = st->codecpar;

    switch (par->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
      if (video_streams++ > 0) {
        res->streams[ui] = TOO_MANY_VIDEO_STREAMS;
        res->passed = false;
      }
      break;
    case AVMEDIA_TYPE_AUDIO:
      // we allow 2 audio streams for instrumentals
      if (++audio_streams + external_audio_file > 2) {
        res->streams[ui] = TOO_MANY_AUDIO_STREAMS;
        res->passed = false;
      }
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      if ((++sub_streams + external_sub_file) > 1) {
        res->streams[ui] = TOO_MANY_SUBTITLE_STREAMS;
        res->passed = false;
      }
      break;
    case AVMEDIA_TYPE_ATTACHMENT:
      res->streams[ui] = ATTACHMENT_STREAM;
      res->passed = false;
      break;
    default:
      res->streams[ui] = UNKNOWN_STREAM;
      res->passed = false;
    }
  }

  struct ffaacsucks_result *ffaac_res = ffaacsucks_check_avfcontext(s);
  if (ffaac_res->n_streams > 0) {
    res->passed = false;
    for (int i = 0; i < ffaac_res->n_streams; i++)
      res->streams[ffaac_res->streams[i]] = LAVC_AAC_STREAM;
  }
  ffaacsucks_result_free(ffaac_res);
}

struct dakara_check_results *dakara_check_avio(size_t buffer_size, void *readable,
                                               int (*read_packet)(void *, uint8_t *, int),
                                               int64_t (*seek)(void *, int64_t, int),
                                               unsigned int external_sub_file,
                                               unsigned int external_audio_file) {
  AVFormatContext *fmt_ctx = NULL;
  AVIOContext *avio_ctx = NULL;
  struct dakara_check_results *res = dakara_check_results_new();

  uint8_t *avio_ctx_buffer = av_malloc(buffer_size);
  if (avio_ctx_buffer == NULL) {
    perror("could not allocate AVIO buffer");
    goto end;
  }

  avio_ctx = avio_alloc_context(avio_ctx_buffer, buffer_size, 0, readable, read_packet, NULL, seek);
  if (avio_ctx == NULL) {
    perror("could not allocate AVIO context");
    goto end;
  }

  fmt_ctx = avformat_alloc_context();
  if (fmt_ctx == NULL) {
    perror("could not allocate avformat context");
    goto end;
  }

  fmt_ctx->pb = avio_ctx;

  int ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
  if (ret < 0) {
    fprintf(stderr, "could not open avio input\n");
    goto end;
  }

  dakara_check_avf(fmt_ctx, res, external_sub_file, external_audio_file);

end:
  if (fmt_ctx != NULL)
    avformat_close_input(&fmt_ctx);

  if (avio_ctx != NULL) {
    av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);
  }

  return res;
}

struct dakara_check_results *dakara_check(char *filepath, unsigned int external_sub_file,
                                          unsigned int external_audio_file) {
  AVFormatContext *s = NULL;
  struct dakara_check_results *res = dakara_check_results_new();

  int ret = avformat_open_input(&s, filepath, NULL, NULL);
  if (ret < 0) {
    fprintf(stderr, "failed to load file %s: %s\n", filepath, strerror(errno));
    res->passed = false;
    return res;
  }

  dakara_check_avf(s, res, external_sub_file, external_audio_file);

  avformat_close_input(&s);
  avformat_free_context(s);

  return res;
}

void dakara_check_print_results(struct dakara_check_results *res, char *filepath) {
  unsigned int ui;
  for (ui = 0; ui < res->n_streams; ui++) {
    if (res->streams[ui] != OK) {
      struct dakara_check_report report = dakara_check_get_report(res->streams[ui]);
      printf("%s: Stream %d (error level %d): %s\n", filepath, ui, report.error_level,
             report.message);
    }
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
  unsigned int basepathlen = strlen(filepath);

  while (filebasepath[basepathlen - 1] != '.' && basepathlen > 0) {
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

static struct dakara_check_report dakara_results_error_reports[] = {
    [OK] = {"OK", NONE},
    [UNKNOWN_STREAM] = {"Unknown stream type", WARNING},
    [LAVC_AAC_STREAM] = {"Lavc/FFMPEG AAC stream", ERROR},
    [TOO_MANY_AUDIO_STREAMS] = {"Too many audio streams", ERROR},
    [TOO_MANY_VIDEO_STREAMS] = {"Too many video streams", ERROR},
    [TOO_MANY_SUBTITLE_STREAMS] = {"Internal subtitle track should be removed", ERROR},
    [ATTACHMENT_STREAM] = {"Attachment found (probably a font)", ERROR},
};

struct dakara_check_report dakara_check_get_report(enum dakara_stream_result res) {
  return dakara_results_error_reports[res];
}

void dakara_check_results_free(struct dakara_check_results *res) {
  if (res->streams != NULL)
    free(res->streams);
  free(res);
}
