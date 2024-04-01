
#ifndef DAKARA_CHECK_H
#define DAKARA_CHECK_H

#include <ffmpegaacsucks.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>

enum dakara_stream_result {
  OK,
  UNKNOWN_STREAM,
  LAVC_AAC_STREAM,
  TOO_MANY_AUDIO_STREAMS,
  TOO_MANY_VIDEO_STREAMS,
  TOO_MANY_SUBTITLE_STREAMS,
  ATTACHMENT_STREAM,
};

enum dakara_check_error_level {
  NONE,
  WARNING,
  ERROR,
};

struct dakara_check_report {
  char const *const message;
  enum dakara_check_error_level error_level;
};

struct dakara_check_report dakara_check_get_report(enum dakara_stream_result res);

struct dakara_check_results {
  enum dakara_stream_result *streams;
  unsigned int n_streams;
  bool passed;
};

struct dakara_check_results *dakara_check_results_new(void);

static inline void dakara_check_results_free(struct dakara_check_results *res) {
  if (res->streams != NULL)
    free(res->streams);
  free(res);
}

const char *dakara_check_version(void);

struct dakara_check_results *dakara_check(char *filepath, unsigned int external_sub_file);

void dakara_check_print_results(struct dakara_check_results *res, char *filepath);

int dakara_check_sub_file(char *filepath);

int dakara_check_external_sub_file_for(char *filepath);

#endif
