
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

#define FFAAC_LAVC_SIGNATURE "Lavc"

const char *dakara_check_version(void) { return DAKARA_CHECK_VERSION; }

bool dakara_check_ffaac_stream_packet(AVPacket *pkt) {
  int pkt_type, skip, namelen;

  uint8_t b = pkt->data[0];
  pkt_type = (b & 0xe0) >> 5;
  if (pkt_type != 6) {
    if (getenv("DAKARA_CHECK_DEBUG") != NULL)
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
  return (strncmp(comment, FFAAC_LAVC_SIGNATURE, 4)) == 0;
}

void dakara_check_results_init(dakara_check_results *res) {
  res->duration = 0;
  res->report.passed = 0;
}

static void dakara_check_avf(AVFormatContext *s, dakara_check_results *res) {
  unsigned int video_streams = 0;
  unsigned int audio_streams = 0;
  unsigned int aac_streams = 0;

  int64_t duration = 0;

  // needed for mpeg-ts files
  if (avformat_find_stream_info(s, NULL) < 0) {
    perror("failed to find streams");
    res->report.errors.io_error = true;
    return;
  }

  for (unsigned int ui = 0; ui < s->nb_streams; ui++) {
    AVStream *st = s->streams[ui];
    AVCodecParameters *par = st->codecpar;

    switch (par->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
      if (st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
        break;
      }
      if (duration <= 0)
        duration = st->duration * st->time_base.num / st->time_base.den;
      if (video_streams++ > 0) {
        res->report.errors.too_many_video_streams = true;
      }
      st->discard = AVDISCARD_ALL;
      break;
    case AVMEDIA_TYPE_AUDIO:
      // we allow up to 1 audio streams in each file
      if (++audio_streams > 1) {
        res->report.errors.too_many_audio_streams = true;
      }
      if (st->codecpar->codec_id == AV_CODEC_ID_AAC) {
        aac_streams++;
      } else {
        st->discard = AVDISCARD_ALL;
      }
      break;
    case AVMEDIA_TYPE_SUBTITLE:
      res->report.errors.internal_sub_stream = 1;
      st->discard = AVDISCARD_ALL;
      break;
    case AVMEDIA_TYPE_ATTACHMENT:
      res->report.errors.attachment_stream = 1;
      st->discard = AVDISCARD_ALL;
      break;
    default:
      res->report.errors.unknown_stream = 1;
      st->discard = AVDISCARD_ALL;
    }
  }

  if (video_streams == 0)
    res->report.errors.no_video_stream = true;

  if (audio_streams == 0)
    res->report.errors.no_audio_stream = true;

  // if duration was not found in the streams
  if (duration <= 0) {
    if (s->duration > 0) {
      duration = s->duration / AV_TIME_BASE;
      res->report.errors.global_duration = true;
    } else {
      res->report.errors.no_duration = true;
    }
  }

  if (duration >= INT_MAX) {
    res->report.errors.no_duration = true;
  } else {
    res->duration = duration;
  }

  if (aac_streams > 0) {
    AVPacket *pkt = av_packet_alloc();

    while (aac_streams > 0) {
      int ret = av_read_frame(s, pkt);
      if (ret < 0) {
        perror("av_read_frame");
        res->report.errors.io_error = true;
        break;
      }

      if (s->streams[pkt->stream_index]->codecpar->codec_id != AV_CODEC_ID_AAC) {
        fprintf(stderr, "not aac stream\n");
      }

      if (dakara_check_ffaac_stream_packet(pkt))
        res->report.errors.lavc_aac_stream = true;

      s->streams[pkt->stream_index]->discard = AVDISCARD_ALL;
      aac_streams--;

      av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
  }
}

void dakara_check(char *filepath, dakara_check_results *res) {
  AVFormatContext *s = NULL;
  dakara_check_results_init(res);

  int ret = avformat_open_input(&s, filepath, NULL, NULL);
  if (ret < 0) {
    fprintf(stderr, "failed to load file %s: %s\n", filepath, strerror(errno));
    res->report.errors.io_error = true;
    return;
  }

  dakara_check_avf(s, res);

  avformat_close_input(&s);
  avformat_free_context(s);
}

void dakara_check_avio(size_t buffer_size, void *readable,
                       int (*read_packet)(void *, uint8_t *, int),
                       int64_t (*seek)(void *, int64_t, int), dakara_check_results *res) {
  AVFormatContext *fmt_ctx = NULL;
  AVIOContext *avio_ctx = NULL;
  dakara_check_results_init(res);

  uint8_t *avio_ctx_buffer = av_malloc(buffer_size);
  if (avio_ctx_buffer == NULL) {
    res->report.errors.io_error = true;
    perror("could not allocate AVIO buffer");
    goto end;
  }

  avio_ctx = avio_alloc_context(avio_ctx_buffer, buffer_size, 0, readable, read_packet, NULL, seek);
  if (avio_ctx == NULL) {
    res->report.errors.io_error = true;
    perror("could not allocate AVIO context");
    goto end;
  }

  fmt_ctx = avformat_alloc_context();
  if (fmt_ctx == NULL) {
    res->report.errors.io_error = true;
    perror("could not allocate avformat context");
    goto end;
  }

  fmt_ctx->pb = avio_ctx;

  int ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
  if (ret < 0) {
    res->report.errors.io_error = true;
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
}

char const *dakara_check_str_report(union dakara_check_results_report *report) {
  if (report->errors.lavc_aac_stream) {
    report->errors.lavc_aac_stream = false;
    return "file contains a LAVC AAC audio stream";
  }
  if (report->errors.attachment_stream) {
    report->errors.attachment_stream = false;
    return "found an attachment in the file (probably a font)";
  }
  if (report->errors.internal_sub_stream) {
    report->errors.internal_sub_stream = false;
    return "internal subtitle track should be removed";
  }
  if (report->errors.too_many_video_streams) {
    report->errors.too_many_video_streams = false;
    return "too many video tracks";
  }
  if (report->errors.no_video_stream) {
    report->errors.no_video_stream = false;
    return "no video track found";
  }
  if (report->errors.too_many_audio_streams) {
    report->errors.too_many_audio_streams = false;
    return "too many audio tracks";
  }
  if (report->errors.no_audio_stream) {
    report->errors.no_audio_stream = false;
    return "no audio track found";
  }
  if (report->errors.unknown_stream) {
    report->errors.unknown_stream = false;
    return "found an unknown track";
  }
  if (report->errors.no_duration) {
    report->errors.no_duration = false;
    return "failed to find file duration";
  }
  if (report->errors.global_duration) {
    report->errors.global_duration = false;
    return "using global file duration, may or may not be correct";
  }
  if (report->errors.io_error) {
    report->errors.io_error = false;
    return "failed to open file";
  }

  return NULL;
}

void dakara_check_print_results(dakara_check_results *res, char *filepath) {
  union dakara_check_results_report report = res->report;
  char const *msg;
  while ((msg = dakara_check_str_report(&report)) != NULL) {
    fprintf(stderr, "%s: %s\n", filepath, msg);
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

#define NBSP '\xa0'

/*
 * check events of the current track
 */
void dakara_check_subtitle_events(ASS_Track *track, dakara_check_sub_results *res) {
  unsigned long lyrics_len = strlen(res->lyrics) + 1;
  char *line = NULL;
  for (int i = 0; i < track->n_events; i++) {
    line = strdup(track->events[i].Text);
    if (line == NULL) {
      perror("failed to duplicate string");
      res->report.errors.io_error = true;
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
  res->report.passed = 0;
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
  dakara_check_sub_results *res = NULL;

  ASS_Library *library = ass_library_init();
  if (library == NULL) {
    perror("failed to allocate ASS library");
    return res;
  }

  ass_set_message_cb(library, dakara_check_ass_message_callback, NULL);

  ASS_Track *track = ass_read_file(library, filepath, "UTF-8");
  if (track == NULL) {
    perror("failed to read ASS track from file");
    goto end;
  }

  res = dakara_check_sub_results_init();
  if (res == NULL) {
    perror("failed to allocate memory for results");
  }

  dakara_check_subtitle_track(track, res);

  ass_free_track(track);
end:
  ass_library_done(library);
  return res;
}

dakara_check_sub_results *dakara_check_subtitle_memory(char *memory, size_t bufsize) {
  dakara_check_sub_results *res = NULL;

  ASS_Library *library = ass_library_init();
  if (library == NULL) {
    perror("failed to allocate ASS library");
    return res;
  }

  ass_set_message_cb(library, dakara_check_ass_message_callback, NULL);

  ASS_Track *track = ass_read_memory(library, memory, bufsize, "UTF-8");
  if (track == NULL) {
    perror("failed to read ASS track from buffer");
    goto end;
  }

  res = dakara_check_sub_results_init();
  if (res == NULL) {
    perror("failed to allocate memory for results");
  }

  dakara_check_subtitle_track(track, res);

  ass_free_track(track);
end:
  ass_library_done(library);
  return res;
}

void dakara_check_sub_results_free(dakara_check_sub_results *res) {
  if (res != NULL)
    free(res->lyrics);
  free(res);
}
