
#ifndef DAKARA_CHECK_H
#define DAKARA_CHECK_H

#include <ffmpegaacsucks.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>
#include <stdint.h>

enum dakara_stream_result {
  OK,
  UNKNOWN_STREAM,
  LAVC_AAC_STREAM,
  TOO_MANY_AUDIO_STREAMS,
  TOO_MANY_VIDEO_STREAMS,
  INTERNAL_SUB_STREAM,
  ATTACHMENT_STREAM,
  GLOBAL_DURATION,
  NO_DURATION,
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

extern struct dakara_check_report dakara_results_error_reports[];

struct dakara_check_report dakara_check_get_report(enum dakara_stream_result res);

struct dakara_check_results {
  int64_t duration;
  enum dakara_stream_result *errors;
  unsigned int n_errors;
  bool passed;
};

struct dakara_check_results *dakara_check_results_new(void);

void dakara_check_results_free(struct dakara_check_results *res);

const char *dakara_check_version(void);

struct dakara_check_results *dakara_check(char *filepath);

struct dakara_check_results *dakara_check_avio(size_t buffer_size, void *readable,
                                               int (*read_packet)(void *, uint8_t *, int),
                                               int64_t (*seek)(void *, int64_t, int));

void dakara_check_print_results(struct dakara_check_results *res, char *filepath);

int dakara_check_sub_file(char *filepath);

int dakara_check_external_sub_file_for(char *filepath);

#endif
