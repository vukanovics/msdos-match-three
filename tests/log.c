#include "log.h"

#include <stdarg.h>
#include <stdio.h>

static FILE *log_file = NULL;

void test_log_open(const char *filename) { log_file = fopen(filename, "w"); }

void test_log_close(void) {
  if (log_file != NULL) {
    fclose(log_file);
    log_file = NULL;
  }
}

void test_log(const char *format, ...) {
  if (log_file == NULL) {
    return;
  }

  va_list args;
  va_start(args, format);
  vfprintf(log_file, format, args);
  va_end(args);

  // Flush after every entry so the log survives a test that hangs or crashes.
  fflush(log_file);
}
