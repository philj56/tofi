#ifndef LOG_H
#define LOG_H

void log_error(const char *const fmt, ...);
void log_warning(const char *const fmt, ...);
void log_debug(const char *const fmt, ...);
void log_info(const char *const fmt, ...);

#endif /* LOG_H */
