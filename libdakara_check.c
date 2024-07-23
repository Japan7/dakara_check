
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
  if (res == NULL) {
    perror("failed to allocate dakara_check_results struct");
    return NULL;
  }
  res->passed = true;
  res->n_errors = 0;
  res->duration = 0;
  res->errors = NULL;
  return res;
}

bool dakara_check_results_add_error(struct dakara_check_results *res,
                                    enum dakara_stream_result err) {
  res->passed = false;
  res->errors = reallocarray(res->errors, res->n_errors++, sizeof(res->errors));
  if (res->errors == NULL) {
    perror("failed to allocate res->errors");
    return false;
  } else {
    res->errors[res->n_errors - 1] = err;
    return true;
  }
}

static void dakara_check_avf(AVFormatContext *s, struct dakara_check_results *res) {
  unsigned int video_streams = 0;
  unsigned int audio_streams = 0;

  for (unsigned int ui = 0; ui < s->nb_streams; ui++) {
    AVStream *st = s->streams[ui];
    AVCodecParameters *par = st->codecpar;

    switch (par->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
      res->duration = st->duration * st->time_base.num / st->time_base.den;
      if (video_streams++ > 0) {
        dakara_check_results_add_error(res, TOO_MANY_VIDEO_STREAMS);
      }
      break;
    case AVMEDIA_TYPE_AUDIO:
      // we allow up to 1 audio streams in each file
      if (++audio_streams > 1) {
        dakara_check_results_add_error(res, TOO_MANY_AUDIO_STREAMS);
      }
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      dakara_check_results_add_error(res, INTERNAL_SUB_STREAM);
      break;
    case AVMEDIA_TYPE_ATTACHMENT:
      dakara_check_results_add_error(res, ATTACHMENT_STREAM);
      break;
    default:
      dakara_check_results_add_error(res, UNKNOWN_STREAM);
    }
  }

  // if duration was not found in the streams
  if (res->duration <= 0) {
    if (s->duration > 0) {
      res->duration = s->duration / AV_TIME_BASE;
      dakara_check_results_add_error(res, GLOBAL_DURATION);
    } else {
      dakara_check_results_add_error(res, NO_DURATION);
    }
  }

  struct ffaacsucks_result *ffaac_res = ffaacsucks_check_avfcontext(s);
  if (ffaac_res->n_streams > 0) {
    res->passed = false;
    for (int i = 0; i < ffaac_res->n_streams; i++)
      dakara_check_results_add_error(res, LAVC_AAC_STREAM);
  }
  ffaacsucks_result_free(ffaac_res);
}

struct dakara_check_results *dakara_check(char *filepath) {
  AVFormatContext *s = NULL;
  struct dakara_check_results *res = dakara_check_results_new();
  if (res == NULL)
    return NULL;

  int ret = avformat_open_input(&s, filepath, NULL, NULL);
  if (ret < 0) {
    fprintf(stderr, "failed to load file %s: %s\n", filepath, strerror(errno));
    res->passed = false;
    return res;
  }

  dakara_check_avf(s, res);

  avformat_close_input(&s);
  avformat_free_context(s);

  return res;
}

struct dakara_check_results *dakara_check_avio(size_t buffer_size, void *readable,
                                               int (*read_packet)(void *, uint8_t *, int),
                                               int64_t (*seek)(void *, int64_t, int)) {
  AVFormatContext *fmt_ctx = NULL;
  AVIOContext *avio_ctx = NULL;
  struct dakara_check_results *res = dakara_check_results_new();
  if (res == NULL)
    return NULL;

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

  dakara_check_avf(fmt_ctx, res);

end:
  if (fmt_ctx != NULL)
    avformat_close_input(&fmt_ctx);

  if (avio_ctx != NULL) {
    av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);
  }

  return res;
}

void dakara_check_print_results(struct dakara_check_results *res, char *filepath) {
  for (unsigned int ui = 0; ui < res->n_errors; ui++) {
    if (res->errors[ui] != OK) {
      struct dakara_check_report report = dakara_check_get_report(res->errors[ui]);
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

struct dakara_check_report dakara_results_error_reports[] = {
    [OK] = {"OK", NONE},
    [UNKNOWN_STREAM] = {"Unknown stream type", WARNING},
    [LAVC_AAC_STREAM] = {"Lavc/FFMPEG AAC stream", ERROR},
    [TOO_MANY_AUDIO_STREAMS] = {"Too many audio streams", ERROR},
    [TOO_MANY_VIDEO_STREAMS] = {"Too many video streams", ERROR},
    [INTERNAL_SUB_STREAM] = {"Internal subtitle track should be removed", ERROR},
    [ATTACHMENT_STREAM] = {"Attachment found (probably a font)", ERROR},
    [GLOBAL_DURATION] =
        {"Video track has no duration, the detected duration may or may not be wrong", WARNING},
    [NO_DURATION] = {"No duration found for the file", ERROR}};

struct dakara_check_report dakara_check_get_report(enum dakara_stream_result res) {
  return dakara_results_error_reports[res];
}

void dakara_check_results_free(struct dakara_check_results *res) {
  if (res->errors != NULL)
    free(res->errors);
  free(res);
}
