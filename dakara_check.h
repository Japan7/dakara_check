
#ifndef DAKARA_CHECK_H
#define DAKARA_CHECK_H

#include <ffmpegaacsucks.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>

enum dakara_stream_result {
  OK,
  UNHANDLED_STREAM,
  LAVC_AAC_STREAM,
  TOO_MANY_AUDIO_STREAMS,
  TOO_MANY_VIDEO_STREAMS,
  TOO_MANY_SUBTITLE_STREAMS,
  ATTACHMENT_STREAM,
};

static char const *const dakara_results_error_reports[] = {
    [OK] = "OK",
    [UNHANDLED_STREAM] = "Unhandled stream type",
    [LAVC_AAC_STREAM] = "Lavc/FFMPEG AAC stream",
    [TOO_MANY_AUDIO_STREAMS] = "Too many audio streams",
    [TOO_MANY_VIDEO_STREAMS] = "Too many video streams",
    [TOO_MANY_SUBTITLE_STREAMS] = "Internal subtitle track should be removed",
    [ATTACHMENT_STREAM] = "Attachment found (probably a font)",
};

struct dakara_check_results {
  enum dakara_stream_result *streams;
  unsigned int n_streams;
  bool passed;
};

const char *dakara_check_version();

struct dakara_check_results *dakara_check(char *filepath,
                                          int external_sub_file);

void dakara_check_results_free(struct dakara_check_results *res);

void dakara_check_print_results(struct dakara_check_results *res,
                                char *filepath);

int dakara_check_sub_file(char *filepath);

int dakara_check_external_sub_file_for(char *filepath);

#endif
