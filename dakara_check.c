
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dakara_check.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: dakara_check <FILE>\n");
    return EXIT_FAILURE;
  }

  if (strcmp(argv[1], "--version") == 0) {
    printf("dakara_check %s\n", dakara_check_version());
    return EXIT_SUCCESS;
  }

  int external_sub_file = dakara_check_external_sub_file_for(argv[1]);
  if (external_sub_file == 0) {
    printf("%s: no external sub file found\n", argv[1]);
  }

  dakara_check_results res;
  dakara_check(argv[1], &res);

  printf("%s duration: %d\n", argv[1], res.duration);
  if (res.report.passed == 0) {
    return EXIT_SUCCESS;
  }

  dakara_check_print_results(&res, argv[1]);
  return EXIT_FAILURE;
}
