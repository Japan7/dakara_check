
#ifndef DAKARA_CHECK_H
#define DAKARA_CHECK_H

#include <ffmpegaacsucks.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>
#include <stdint.h>

struct dakara_check_results_errors_switches {
  // INFO: unknown stream type
  bool unknown_stream : 1;
  // WARNING: audio track encoded with LAVC AAC codec
  bool lavc_aac_stream : 1;
  // WARNING: too many video tracks
  bool too_many_video_streams : 1;
  // WARNING: too many audio tracks
  bool too_many_audio_streams : 1;
  // WARNING: internal sub track
  bool internal_sub_stream : 1;
  // WARNING: attachment found (probably a font)
  bool attachment_stream : 1;
  // INFO: using global duration of the file. detected duration may or may not be wrong.
  bool global_duration : 1;
  // WARNING: no detected duration
  bool no_duration : 1;
  // WARNING: missing video stream
  bool no_video_stream : 1;
  // ERROR: missing audio stream
  bool no_audio_stream : 1;
  // ERROR: failed to open file
  bool io_error : 1;
};

union dakara_check_results_report {
  struct dakara_check_results_errors_switches errors;
  // checks passed if 0, failed otherwise
  uint32_t passed;
};

typedef struct {
  uint32_t duration;
  union dakara_check_results_report report;
} dakara_check_results;

void dakara_check_results_init(dakara_check_results *res);

const char *dakara_check_version(void);

dakara_check_results *dakara_check(char *filepath, dakara_check_results *res);

dakara_check_results *dakara_check_avio(size_t buffer_size, void *readable,
                                        int (*read_packet)(void *, uint8_t *, int),
                                        int64_t (*seek)(void *, int64_t, int),
                                        dakara_check_results *res);

void dakara_check_print_results(dakara_check_results *res, char *filepath);

int dakara_check_external_sub_file_for(char *filepath);

#endif
