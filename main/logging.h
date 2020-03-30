#define INFO_PRINTF(f_, ...)  puts("\033[32;42m"); printf((f_), ##__VA_ARGS__)
#define ERROR_PRINTF(f_, ...) puts("\033[31;40m"); printf((f_), ##__VA_ARGS__)
#define DEBUG_PRINTF(f_, ...) puts("\033[36;40m"); printf((f_), ##__VA_ARGS__)

