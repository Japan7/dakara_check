
#ifndef DAKARA_CHECK_H
#define DAKARA_CHECK_H

#include <ffmpegaacsucks.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>

struct dakara_check_results {
  const char *general;
  const char **streams;
  unsigned int n_streams;
  bool passed;
};

const char *dakara_check_version();

struct dakara_check_results *dakara_check(char *filepath, int external_sub_file);

void dakara_check_results_free(struct dakara_check_results *res);

void dakara_check_print_results(struct dakara_check_results *res,
                                char *filepath);

int dakara_check_sub_file(char *filepath);

int dakara_check_external_sub_file_for(char *filepath);

#endif
