#include <_types/_uint8_t.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "../pg/pg.h"

int main(int argc, char* argv[]) {
  pg_logger_t logger = {.level = PG_LOG_INFO};
  pg_array_t(uint8_t) file_data = {0};
  if (argc == 1) {
    // int fd = STDIN_FILENO;
    pg_log_fatal(&logger, EINVAL, "TODO read from stdin");
  } else if (argc == 2) {
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
      pg_log_fatal(&logger, errno, "Failed to open file %s: %s", argv[1],
                   strerror(errno));
    }
    int64_t ret = 0;
    if ((ret = pg_read_file(pg_heap_allocator(), argv[1], &file_data)) != 0) {
      pg_log_fatal(&logger, ret, "Failed to read file %s: %s", argv[1],
                   strerror(ret));
    }
  }

  pg_span_t input = {.data = (char*)file_data, .len = pg_array_len(file_data)};

  // Skip header, unneeded
  if (pg_span_starts_with(input, pg_span_make_c("CPU")))
    pg_span_skip_left_until_inclusive(&input, '\n');

  while (true) {
    pg_span_skip_left_until_inclusive(&input, ' ');

    // Skip CPU id column
    {
      pg_span_trim_left(&input);
      pg_span_skip_left_until_inclusive(&input, ' ');
    }

    // Skip ID column
    {
      pg_span_trim_left(&input);
      pg_span_skip_left_until_inclusive(&input, ' ');
    }

    // malloc/realloc/calloc/free
    pg_span_trim_left(&input);
    pg_span_t fn_leaf = input;
    pg_span_split(input, ' ', &fn_leaf, &input);
    pg_span_trim_left(&input);

    // timestamp
    pg_span_t ts = {0};
    pg_span_split(input, ' ', &ts, &input);
    pg_span_trim_left(&input);

    // arg0
    pg_span_t arg0 = {0};
    pg_span_split(input, ' ', &arg0, &input);
    pg_span_trim_left(&input);

    // arg1
    {
      bool more_chars = false;
      char c = pg_span_peek_left(input, &more_chars);
      if (!more_chars) break;
      if (c != '\n') {
        pg_span_t arg1 = {0};
        pg_span_split(input, '\n', &arg1, &input);
        pg_span_trim_left(&input);
      }
    }

    // Rest of stacktrace
    while (true) {
      bool more_chars = false;
      char c = pg_span_peek_left(input, &more_chars);
      if (!more_chars) break;
      if (c == '\n') {  // New frame
        // TODO
        return 1;
      }

      pg_span_trim_left(&input);
      pg_span_t fn = input;
      pg_span_split(input, ' ', &fn, &input);
      pg_span_trim_left(&input);
      pg_span_trim_right(&fn);
    }
  }
  return 0;
}