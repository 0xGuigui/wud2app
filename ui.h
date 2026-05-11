#pragma once

#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
#  define C_RESET  ""
#  define C_BOLD   ""
#  define C_RED    ""
#  define C_GREEN  ""
#  define C_YELLOW ""
#  define C_CYAN   ""
#  define C_DIM    ""
#else
#  define C_RESET  "\033[0m"
#  define C_BOLD   "\033[1m"
#  define C_RED    "\033[31m"
#  define C_GREEN  "\033[32m"
#  define C_YELLOW "\033[33m"
#  define C_CYAN   "\033[36m"
#  define C_DIM    "\033[2m"
#endif

#define UI_OK   C_GREEN  " [+]" C_RESET
#define UI_ERR  C_RED    " [!]" C_RESET
#define UI_STEP C_YELLOW "  => " C_RESET
#define UI_INFO C_CYAN   "  :: " C_RESET

static inline void ui_header(void)
{
    printf(C_BOLD C_CYAN "wud2app" C_RESET C_BOLD " v2.0"
           C_RESET "  -  Wii U disc extractor\n\n");
    printf(C_DIM "  original code by FIX94  -  updated by 0xGuigui" C_RESET "\n\n");
}

#define UI_BAR_WIDTH 32

static inline void ui_progress(const char *name, uint64_t done, uint64_t total)
{
    int filled = (total > 0) ? (int)((uint64_t)UI_BAR_WIDTH * done / total) : 0;
    printf("  %-22s " C_CYAN, name);
    for (int i = 0; i < filled; i++)          printf("█");
    printf(C_DIM);
    for (int i = filled; i < UI_BAR_WIDTH; i++) printf("░");
    printf(C_RESET "  %6.1f MiB\r", done / 1048576.0);
    fflush(stdout);
}

static inline void ui_progress_done(const char *name, uint64_t total)
{
    printf("  %-22s " C_CYAN, name);
    for (int i = 0; i < UI_BAR_WIDTH; i++) printf("█");
    printf(C_RESET "  %6.1f MiB" UI_OK "\n", total / 1048576.0);
}
