
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dakara_check.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <FILE>\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (strcmp(argv[1], "--version") == 0) {
    printf("dakara_check %s\n", dakara_check_version());
    return EXIT_SUCCESS;
  }

  dakara_check_sub_results *res = dakara_check_subtitle_file(argv[1]);
  if (res != NULL) {
    printf("%s\n", res->lyrics);
  }

  dakara_check_sub_results_free(res);

  return EXIT_SUCCESS;
}
