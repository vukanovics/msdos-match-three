#ifndef TEST_LOG_H_INCLUDED
#define TEST_LOG_H_INCLUDED

// Test output is written to a file rather than stdout: the tests switch the
// display into Mode X, where text written to stdout is not visible.
void test_log_open(const char *filename);

void test_log_close(void);

void test_log(const char *format, ...);

#endif
