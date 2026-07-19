
#ifndef DAKARA_CHECK_H
#define DAKARA_CHECK_H

#include <ass/ass.h>
#include <ass/ass_types.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const char *dakara_check_version(void);

enum dakara_check_report_type : uint8_t {
  DC_DONE,
  DC_IO_ERROR,
  DC_NO_VIDEO_STREAM,
  DC_NO_AUDIO_STREAM,
  DC_TOO_MANY_VIDEO_STREAMS,
  DC_TOO_MANY_AUDIO_STREAMS,
  DC_INTERNAL_SUBS,
  DC_ATTACHMENT_STREAM,
  DC_UNKNOWN_STREAM,
  DC_NO_DURATION_FOUND,
  DC_GLOBAL_DURATION,
  DC_LAVC_AAC_STREAM,
  DC_ATTACHED_COVER,
};

enum dakara_check_error_level : uint8_t {
  // Information: something worth noting but should not cause issues with playback of the file.
  DC_INFO,
  // Warning: May or may not cause issues with playback of the file.
  DC_WARNING,
  // Error: Will most likely cause issues with playback of the file.
  DC_ERROR,
};

struct dakara_check_diagnostic {
  enum dakara_check_report_type report_id;
  enum dakara_check_error_level error_level;
  const char *message;
};

struct dakara_check_report {
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
  // ERROR: found cover image
  bool attached_cover_image : 1;
};

typedef struct dakara_check_results {
  uint32_t duration;
  struct dakara_check_report report;
} dakara_check_results;

void dakara_check_results_init(dakara_check_results *res);

bool dakara_check_passed(struct dakara_check_report err);

// check file from filepath
void dakara_check(char *filepath, dakara_check_results *res);

// check instrumental file from filepath
void dakara_check_inst(char *filepath, dakara_check_results *res);

// check audio only file from filepath
void dakara_check_audio(char *filepath, dakara_check_results *res);

// check file from AVIO
void dakara_check_avio(size_t buffer_size, void *readable,
                       int (*read_packet)(void *, uint8_t *, int),
                       int64_t (*seek)(void *, int64_t, int), dakara_check_results *res);

// check instrumental track from AVIO
void dakara_check_inst_avio(size_t buffer_size, void *readable,
                            int (*read_packet)(void *, uint8_t *, int),
                            int64_t (*seek)(void *, int64_t, int), dakara_check_results *res);

// check audio only file from AVIO
void dakara_check_audio_avio(size_t buffer_size, void *readable,
                             int (*read_packet)(void *, uint8_t *, int),
                             int64_t (*seek)(void *, int64_t, int), dakara_check_results *res);

struct dakara_check_diagnostic dakara_check_get_diagnostic(struct dakara_check_report *report);
void dakara_check_print_diagnostics(struct dakara_check_report report, char *filepath);

int dakara_check_external_sub_file_for(char *filepath);

struct dakara_check_sub_report {
  bool io_error : 1;
};

bool dakara_check_sub_passed(struct dakara_check_sub_report);

struct dakara_check_diagnostic
dakara_check_sub_get_diagnostic(struct dakara_check_sub_report *report);
void dakara_check_sub_print_diagnostics(struct dakara_check_sub_report report, char *filepath);

typedef struct {
  struct dakara_check_sub_report report;
  char *lyrics;
} dakara_check_sub_results;

/*
 * Check a subtitle file for errors
 */
dakara_check_sub_results *dakara_check_subtitle_file(char *filepath);

dakara_check_sub_results *dakara_check_subtitle_memory(char *memory, size_t bufsize);

void dakara_check_sub_results_free(dakara_check_sub_results *res);

#endif
