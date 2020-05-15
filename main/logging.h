#define INFO_PRINTF(f_, ...)  printf(("\033[37m" f_ "\n"), ##__VA_ARGS__)
#define ERROR_PRINTF(f_, ...) printf(("\033[31m" f_ "\n"), ##__VA_ARGS__)
#define DEBUG_PRINTF(f_, ...) printf(("\033[36m" f_ "\n"), ##__VA_ARGS__)

