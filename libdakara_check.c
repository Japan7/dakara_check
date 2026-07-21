
#include <ass/ass.h>
#include <ass/ass_types.h>
#include <errno.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/defs.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dakara_check.h"

#include "version.h"

#include "defer.h"

constexpr char FFAAC_LAVC_SIGNATURE[] = "Lavc";
// size of FFAAC_LAVC_SIGNATURE_LEN without the trailing null character
constexpr int FFAAC_LAVC_SIGNATURE_LEN = sizeof(FFAAC_LAVC_SIGNATURE) - 1;

const char *dakara_check_version(void) { return DAKARA_CHECK_VERSION; }

struct lavc_version {
  bool error;
  bool islavc;
  long major;
  long minor;
  long micro;
};

struct lavc_version parse_lavc_version(char *comment) {
  struct lavc_version res;
  res.major = 0;
  res.minor = 0;
  res.micro = 0;

  res.islavc = strncmp(comment, FFAAC_LAVC_SIGNATURE, FFAAC_LAVC_SIGNATURE_LEN) == 0;
  if (!res.islavc) {
    return res;
  }

  // major
  char *start = &comment[FFAAC_LAVC_SIGNATURE_LEN];
  char *dotpos = strchr(start, '.');
  char *end = nullptr;
  res.major = strtol(start, &end, 10);
  if (dotpos != end) {
    fprintf(stderr, "failed to parse lavc major version: %s", comment);
    res.error = true;
    return res;
  }

  // minor
  start = dotpos + 1;
  dotpos = strchr(start, '.');
  res.minor = strtol(start, &end, 10);
  if (dotpos != end) {
    fprintf(stderr, "failed to parse lavc minor version: %s", comment);
    res.error = true;
    return res;
  }

  // micro
  start = dotpos + 1;
  char *endpos = strchr(start, 0);
  res.micro = strtol(start, &end, 10);
  if (endpos != end) {
    fprintf(stderr, "failed to parse lavc micro version: %s", comment);
    res.error = true;
  }

  return res;
}

bool dakara_check_ffaac_stream_packet(AVPacket *pkt) {
  int pkt_type, skip, namelen;

  uint8_t b = pkt->data[0];
  pkt_type = (b & 0xe0) >> 5;
  if (pkt_type != 6) {
    if (getenv("DAKARA_CHECK_DEBUG") != nullptr)
      fprintf(stderr,
              "unexpected packet type found for Lavc/FFMPEG AAC in stream %d "
              "(%d) %x\n",
              pkt->stream_index, pkt_type, b);
    return false;
  }

  namelen = (b & 0x1e) >> 1;

  if (namelen == 15)
    skip = 3;
  else
    skip = 2;

  char *comment = (char *)pkt->data + skip;
  struct lavc_version version = parse_lavc_version(comment);

  return version.islavc && version.major <= 63 && version.minor < 4;
}

void dakara_check_results_init(dakara_check_results *res) {
  res->duration = 0;
  struct dakara_check_report report = {0};
  res->report = report;
}

static void dakara_check_avf(AVFormatContext *s, dakara_check_results *res) {
  unsigned int video_streams = 0;
  unsigned int audio_streams = 0;
  unsigned int aac_streams = 0;

  int64_t duration = 0;
  int64_t audio_duration = 0;

  // needed for mpeg-ts files
  if (avformat_find_stream_info(s, nullptr) < 0) {
    perror("failed to find streams");
    res->report.io_error = true;
    return;
  }

  for (unsigned int ui = 0; ui < s->nb_streams; ui++) {
    AVStream *st = s->streams[ui];
    AVCodecParameters *par = st->codecpar;

    switch (par->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
      if (video_streams++ > 0) {
        res->report.too_many_video_streams = true;
      }
      if (st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
        res->report.attached_cover_image = true;
        break;
      }
      if (duration <= 0)
        duration = st->duration * st->time_base.num / st->time_base.den;
      st->discard = AVDISCARD_ALL;
      break;
    case AVMEDIA_TYPE_AUDIO:
      // we allow up to 1 audio streams in each file
      if (++audio_streams > 1) {
        res->report.too_many_audio_streams = true;
      }
      if (audio_duration <= 0)
        audio_duration = st->duration * st->time_base.num / st->time_base.den;
      if (st->codecpar->codec_id == AV_CODEC_ID_AAC) {
        aac_streams++;
      } else {
        st->discard = AVDISCARD_ALL;
      }
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      res->report.internal_sub_stream = 1;
      st->discard = AVDISCARD_ALL;
      break;
    case AVMEDIA_TYPE_ATTACHMENT:
      res->report.attachment_stream = 1;
      st->discard = AVDISCARD_ALL;
      break;
    default:
      res->report.unknown_stream = 1;
      st->discard = AVDISCARD_ALL;
    }
  }

  if (video_streams == 0)
    res->report.no_video_stream = true;

  if (audio_streams == 0)
    res->report.no_audio_stream = true;

  // use audio stream duration when there is no video stream
  if (audio_duration > 0 && video_streams == 0) {
    duration = audio_duration;
  }

  // if duration was not found in the streams
  if (duration <= 0) {
    if (s->duration > 0) {
      duration = s->duration / AV_TIME_BASE;
      res->report.global_duration = true;
    } else {
      res->report.no_duration = true;
    }
  }

  if (duration >= INT_MAX) {
    res->report.no_duration = true;
  } else {
    res->duration = duration;
  }

  if (aac_streams > 0) {
    AVPacket *pkt = av_packet_alloc();
    defer { av_packet_free(&pkt); }

    while (aac_streams > 0) {
      int ret = av_read_frame(s, pkt);
      defer {
        s->streams[pkt->stream_index]->discard = AVDISCARD_ALL;
        av_packet_unref(pkt);
      }

      if (ret < 0) {
        perror("av_read_frame");
        res->report.io_error = true;
        break;
      }

      if (s->streams[pkt->stream_index]->codecpar->codec_id != AV_CODEC_ID_AAC) {
        continue;
      }

      if (dakara_check_ffaac_stream_packet(pkt))
        res->report.lavc_aac_stream = true;

      aac_streams--;
    }
  }
}

// ignore errors that are not relevant to instrumental tracks
void dakara_check_inst_ignored_reports(dakara_check_results *res) {
  res->report.no_video_stream = false;
  res->report.no_duration = false;
}

void dakara_check(char *filepath, dakara_check_results *res) {
  AVFormatContext *s = nullptr;
  dakara_check_results_init(res);

  int ret = avformat_open_input(&s, filepath, nullptr, NULL);
  if (ret < 0) {
    fprintf(stderr, "failed to load file %s: %s\n", filepath, strerror(errno));
    res->report.io_error = true;
    return;
  }
  defer {
    avformat_close_input(&s);
    avformat_free_context(s);
  }

  dakara_check_avf(s, res);
}

void dakara_check_inst(char *filepath, dakara_check_results *res) {
  dakara_check(filepath, res);
  dakara_check_inst_ignored_reports(res);
}

// ignore errors that are not relevant to instrumental tracks
void dakara_check_audio_ignored_reports(dakara_check_results *res) {
  res->report.attached_cover_image = false;
}

void dakara_check_audio(char *filepath, dakara_check_results *res) {
  dakara_check(filepath, res);
  dakara_check_audio_ignored_reports(res);
}

void dakara_check_avio(size_t buffer_size, void *readable,
                       int (*read_packet)(void *, uint8_t *, int),
                       int64_t (*seek)(void *, int64_t, int), dakara_check_results *res) {
  AVFormatContext *fmt_ctx = nullptr;
  AVIOContext *avio_ctx = nullptr;
  dakara_check_results_init(res);

  uint8_t *avio_ctx_buffer = av_malloc(buffer_size);
  if (avio_ctx_buffer == nullptr) {
    res->report.io_error = true;
    perror("could not allocate AVIO buffer");
  }

  avio_ctx =
      avio_alloc_context(avio_ctx_buffer, buffer_size, 0, readable, read_packet, nullptr, seek);
  if (avio_ctx == nullptr) {
    res->report.io_error = true;
    perror("could not allocate AVIO context");
    av_freep(avio_ctx_buffer);
  }
  defer {
    av_freep(&avio_ctx->buffer);
    avio_context_free(&avio_ctx);
  }

  fmt_ctx = avformat_alloc_context();
  if (fmt_ctx == nullptr) {
    res->report.io_error = true;
    perror("could not allocate avformat context");
  }
  defer { avformat_free_context(fmt_ctx); }

  fmt_ctx->pb = avio_ctx;

  int ret = avformat_open_input(&fmt_ctx, nullptr, NULL, NULL);
  if (ret < 0) {
    res->report.io_error = true;
    fprintf(stderr, "could not open avio input\n");
  }
  defer { avformat_close_input(&fmt_ctx); }

  dakara_check_avf(fmt_ctx, res);
}

void dakara_check_inst_avio(size_t buffer_size, void *readable,
                            int (*read_packet)(void *, uint8_t *, int),
                            int64_t (*seek)(void *, int64_t, int), dakara_check_results *res) {
  dakara_check_avio(buffer_size, readable, read_packet, seek, res);
  dakara_check_inst_ignored_reports(res);
}

void dakara_check_audio_avio(size_t buffer_size, void *readable,
                             int (*read_packet)(void *, uint8_t *, int),
                             int64_t (*seek)(void *, int64_t, int), dakara_check_results *res) {
  dakara_check_avio(buffer_size, readable, read_packet, seek, res);
  dakara_check_audio_ignored_reports(res);
}

bool dakara_check_passed(struct dakara_check_report report) {
  return !(report.unknown_stream || report.lavc_aac_stream || report.too_many_video_streams ||
           report.too_many_audio_streams || report.internal_sub_stream ||
           report.attachment_stream || report.global_duration || report.no_duration ||
           report.no_video_stream || report.no_audio_stream || report.io_error);
}

/*
 * Return diagnostics for errors detected in the file.
 * Order should follow error level (DC_ERROR > DC_WARNING > DC_INFO)
 */
struct dakara_check_diagnostic dakara_check_get_diagnostic(struct dakara_check_report *report) {
  struct dakara_check_diagnostic diagnostic;
  diagnostic.error_level = DC_ERROR;

  if (report->io_error) {
    report->io_error = false;
    diagnostic.report_id = DC_IO_ERROR;
    diagnostic.message = "Failed to decode file.";
    return diagnostic;
  }

  if (report->attached_cover_image) {
    report->attached_cover_image = false;
    diagnostic.report_id = DC_ATTACHED_COVER;
    diagnostic.message = "Found a cover image in file, it should be removed.";
    return diagnostic;
  }

  if (report->internal_sub_stream) {
    report->internal_sub_stream = false;
    diagnostic.report_id = DC_INTERNAL_SUBS;
    diagnostic.message = "Internal subtitle track should be removed from the file";
    return diagnostic;
  }

  if (report->too_many_video_streams) {
    report->too_many_video_streams = false;
    diagnostic.report_id = DC_TOO_MANY_VIDEO_STREAMS;
    diagnostic.message = "This file contains more than one video track. There should only be one.";
    return diagnostic;
  }

  if (report->no_video_stream) {
    report->no_video_stream = false;
    diagnostic.report_id = DC_NO_VIDEO_STREAM;
    diagnostic.message = "No video track found when there should be one.";
    return diagnostic;
  }

  if (report->too_many_audio_streams) {
    report->too_many_audio_streams = false;
    diagnostic.report_id = DC_TOO_MANY_AUDIO_STREAMS;
    diagnostic.message =
        "Found more than two audio tracks. There should be at most two audio tracks in a file.";
    return diagnostic;
  }

  if (report->no_audio_stream) {
    report->no_audio_stream = false;
    diagnostic.report_id = DC_NO_AUDIO_STREAM;
    diagnostic.message = "No audio track found when there should be one.";
    return diagnostic;
  }

  if (report->no_duration) {
    report->no_duration = false;
    diagnostic.report_id = DC_NO_DURATION_FOUND;
    diagnostic.message = "Failed to find file duration.";
    return diagnostic;
  }

  diagnostic.error_level = DC_WARNING;

  if (report->lavc_aac_stream) {
    report->lavc_aac_stream = false;
    diagnostic.report_id = DC_LAVC_AAC_STREAM,
    diagnostic.message =
        "File contains a LAVC AAC audio stream from a version of FFmpeg that is known to cause "
        "audio issues. Reencode the stream in opus or update your FFmpeg version.";
    return diagnostic;
  }

  if (report->global_duration) {
    report->global_duration = false;
    diagnostic.report_id = DC_GLOBAL_DURATION;
    diagnostic.message = "Using global file duration, may or may not be correct";
    return diagnostic;
  }

  diagnostic.error_level = DC_INFO;

  if (report->unknown_stream) {
    report->unknown_stream = false;
    diagnostic.report_id = DC_UNKNOWN_STREAM;
    diagnostic.message = "Found an unknown track type in the file.";
    return diagnostic;
  }

  if (report->attachment_stream) {
    report->attachment_stream = false;
    diagnostic.report_id = DC_ATTACHMENT_STREAM,
    diagnostic.message =
        "Found an attachment in the file (probably a font). Remove the attachment from the file as "
        "it should not be necessary. Fonts should be uploaded to Karaberus.";
    return diagnostic;
  }

  diagnostic.report_id = DC_DONE;
  diagnostic.message = "";
  return diagnostic;
}

static inline char *get_error_level_prefix(enum dakara_check_error_level level) {
  if (level == DC_ERROR)
    return "ERROR";

  if (level == DC_WARNING)
    return "WARNING";

  if (level == DC_INFO)
    return "INFO";

  abort();
}

static inline void print_diagnostic(struct dakara_check_diagnostic diagnostic, char *filepath) {
  fprintf(stderr, "%s: [%s] %s\n", filepath, get_error_level_prefix(diagnostic.error_level),
          diagnostic.message);
}

void dakara_check_print_diagnostics(struct dakara_check_report report, char *filepath) {
  struct dakara_check_diagnostic diagnostic;
  while ((diagnostic = dakara_check_get_diagnostic(&report)).report_id != DC_DONE) {
    print_diagnostic(diagnostic, filepath);
  }
}

int dakara_check_sub_file(char *filepath) {
  int ret;
  AVFormatContext *s = nullptr;

  ret = avformat_open_input(&s, filepath, nullptr, NULL);
  defer {
    avformat_close_input(&s);
    avformat_free_context(s);
  }

  // TODO: could check that it actually contains a sub stream

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

#define NBSP '\xa0'

/*
 * check events of the current track
 */
void dakara_check_subtitle_events(ASS_Track *track, dakara_check_sub_results *res) {
  unsigned long lyrics_len = strlen(res->lyrics) + 1;
  char *line = nullptr;
  for (int i = 0; i < track->n_events; i++) {
    line = strdup(track->events[i].Text);
    if (line == nullptr) {
      perror("failed to duplicate string");
      res->report.io_error = true;
      return;
    }

    unsigned long write_head = 0;
    bool tags = false;
    bool drawing = false;
    bool escaped = false;

    for (unsigned long read_head = 0; line[read_head] != '\0'; read_head++) {
      if (tags) {
        switch (line[read_head]) {
        case '}':
          tags = false;
          break;
        case '\\':
          if (line[read_head + 1] == 'p' && line[read_head + 2] <= '9' &&
              line[read_head + 2] >= '0') {
            // \p0, disables \pn is valid and enables drawing mode
            drawing = line[read_head + 2] != '0';
          }
          break;
        }
        if (line[read_head] == '}') {
          tags = false;
        }
      } else if (!drawing) {
        if (escaped) {
          escaped = false;
          switch (line[read_head]) {
          case 'n':
          case 'N':
            line[write_head++] = '\n';
            break;
          case 'h':
            line[write_head++] = NBSP;
            break;
          case '{':
          case '}':
            line[write_head++] = line[read_head];
            break;
          default:
            line[write_head++] = '\\';
            line[write_head++] = line[read_head];
          }
        } else {
          switch (line[read_head]) {
          case '{':
            tags = true;
            break;
          case '\\':
            escaped = true;
            break;
          default:
            if (read_head != write_head) {
              line[write_head] = line[read_head];
            }
            write_head++;
          }
        }
      }
    }
    line[write_head] = '\0';

    lyrics_len += write_head + 1;
    res->lyrics = realloc(res->lyrics, sizeof(char) * (lyrics_len));

    if (res->lyrics[0] != '\0')
      strcat(res->lyrics, "\n");
    strcat(res->lyrics, line);

    free(line);
  }
}

void dakara_check_subtitle_track(ASS_Track *track, dakara_check_sub_results *res) {
  dakara_check_subtitle_events(track, res);
}

dakara_check_sub_results *dakara_check_sub_results_init(void) {
  dakara_check_sub_results *res = malloc(sizeof(dakara_check_sub_results));
  res->report.io_error = 0;
  res->lyrics = malloc(1);
  res->lyrics[0] = '\0';

  return res;
}

// don’t show info logs
#define DAKARA_CHECK_ASS_LOG_LEVEL 2

void dakara_check_ass_message_callback(int level, const char *fmt, va_list va, void *data) {
  (void)data;
  if (level > DAKARA_CHECK_ASS_LOG_LEVEL) {
    return;
  }
  printf("[ass] ");
  vfprintf(stderr, fmt, va);
  printf("\n");
}

dakara_check_sub_results *dakara_check_subtitle_file(char *filepath) {
  dakara_check_sub_results *res = nullptr;

  ASS_Library *library = ass_library_init();
  if (library == nullptr) {
    perror("failed to allocate ASS library");
    return res;
  }
  defer { ass_library_done(library); }

  ass_set_message_cb(library, dakara_check_ass_message_callback, nullptr);

  ASS_Track *track = ass_read_file(library, filepath, "UTF-8");
  if (track == nullptr) {
    perror("failed to read ASS track from file");
    return res;
  }
  defer { ass_free_track(track); }

  res = dakara_check_sub_results_init();
  if (res == nullptr) {
    perror("failed to allocate memory for results");
    return res;
  }

  dakara_check_subtitle_track(track, res);

  return res;
}

dakara_check_sub_results *dakara_check_subtitle_memory(char *memory, size_t bufsize) {
  dakara_check_sub_results *res = nullptr;

  ASS_Library *library = ass_library_init();
  if (library == nullptr) {
    perror("failed to allocate ASS library");
    return res;
  }
  defer { ass_library_done(library); }

  ass_set_message_cb(library, dakara_check_ass_message_callback, nullptr);

  ASS_Track *track = ass_read_memory(library, memory, bufsize, "UTF-8");
  if (track == nullptr) {
    perror("failed to read ASS track from buffer");
    return res;
  }
  defer { ass_free_track(track); }

  res = dakara_check_sub_results_init();
  if (res == nullptr) {
    perror("failed to allocate memory for results");
    return res;
  }

  dakara_check_subtitle_track(track, res);

  return res;
}

void dakara_check_sub_results_free(dakara_check_sub_results *res) {
  if (res != nullptr)
    free(res->lyrics);
  free(res);
}

struct dakara_check_diagnostic
dakara_check_sub_get_diagnostic(struct dakara_check_sub_report *report) {
  struct dakara_check_diagnostic diagnostic;
  diagnostic.error_level = DC_ERROR;

  if (report->io_error) {
    diagnostic.report_id = DC_IO_ERROR;
    diagnostic.message = "Failed to parse subtitle file.";
    return diagnostic;
  }

  diagnostic.error_level = DC_INFO;

  diagnostic.report_id = DC_DONE;
  diagnostic.message = "";
  return diagnostic;
}

void dakara_check_sub_print_diagnostics(struct dakara_check_sub_report report, char *filepath) {
  struct dakara_check_diagnostic diagnostic;
  while ((diagnostic = dakara_check_sub_get_diagnostic(&report)).report_id != DC_DONE) {
    print_diagnostic(diagnostic, filepath);
  }
}
