/*
 * blind - 
 * A program to practice blind-typing in the terminal.
 *
 *   Practice blind-typing speed, right on the terminal
 *   using either an existing presets, or a provided text.
 *   Provided text can be either string argument or a full
 *   text file from any kind.
 *   WPM and accuracy or errors are calculated and shown
 *   at the end of each line. Type <C-c> to exit at any time.
 *
 * Usage:
 *   blind [OPTIONS] [STRING|FILE]
 *
 * Options:
 *   -h            display help message
 *   -b            block on wrong typing
 *   -f=FILE       practice on FILE line
 *
 *   --help        display help message
 *   --block       block on wrong typing
 *   --file=FILE   practice on FILE line
 *   --show-actual show the actual typed letter
 *   --allow-back  allow backspace for correction
 *
 * STRING
 *   one line of text. quoted or unquoted.
 *
 * Example:
 *   blind -b
 *   blind \"line to practice on\"
 *   blind --file <path-to-file>
 *
 * author: shalom2552
 * date: 2026-09-24
 */
#include <assert.h>
#include <bits/getopt_core.h>
#include <getopt.h>
#include <signal.h>
#include <stdcountof.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define DEBUG(...)   fprintf(stderr, "DEBUG: "__VA_ARGS__)
#define display(...) do { if (!getenv("NDISPLAY")) printf(""__VA_ARGS__); } while (0)

#define MAX_TEXT_LEN 2048

#define RED   "\033[31m"
#define GREEN "\033[32m"
#define DIM   "\033[2m"
#define BOLD  "\033[1m"
#define UNDER "\033[4m"
#define RST   "\033[0m"
#define BACK  "\033[D"

enum Options {
    OPT_SHOW_ACTUAL = 256,
    OPT_ALLOW_BACKSPACE,
};

enum InputMode {
    DEFAULT_INPUT,
    TEXT_INPUT,
    FILE_INPUT
};

int input_mode = DEFAULT_INPUT;
int blocking_mode          = 0;
int allow_backspace_mode   = 0;
int show_actual_typed_mode = 0;
int hide_line_score        = 0;
char* input_file_path;

struct {
    int    count;
    int    wpm;
    int    errors;
    double accuracy;
} score = {0};

static char* Data[] = {
    "1. Crazy Fredrick bought many very exquisite opal rings in Zurich.",
    "2. Six big devils from Japan quickly forgot how to waltz together.",
    "3. The five boxing wizards jump quickly across the misty mountain ridge.",
    "4. Two driven jocks help fax my big quiz to the head office.",
    "5. Sphinx of black quartz, judge my vow under the starry sky.",
    "6. Quiet explorers venture through frozen tundra seeking lost artifacts.",
    "7. Cozy sphinx waves quart jug of bad milk at the traveler.",
    "8. The quick brown fox jumps over the lazy dog near the riverbank.",
    "9. How vexingly quick daft zebras jump across wide wooden fences.",
    "10. Five quacking zephyrs jolt my wax bed during the cold night.",
    "11. Jim quickly realized that the beautiful gown was expensive.",
    "12. Pack my box with five dozen liquor jugs before noon today.",
    "13. Jackdaws love my big sphinx of quartz sculpted from rare stone.",
    "14. Back in my quaint garden, jaunty zinnias vie for yellow spots.",
    "15. Complex algorithms solve difficult problems with surprising efficiency.",
    "16. Bright vixens jump, dozy fowl quack, and quiet sheep graze safely.",
    "17. A mad boxer shot a quick, gloved jab to the jaw of his foe.",
    "18. Grumpy wizards make toxic brew for the evil queen to drink.",
    "19. We promptly judged antique ivory buckles for the prize ceremony.",
    "20. Puzzled by the mysterious message, he searched for a hidden clue.",
    0
};

void usage(void)
{
    fprintf(stderr, "blind: usage: blind [OPTIONS] [STRING|FILE]");
}

void help(void)
{
    fprintf(stderr,
            "A program to practice blind-typing in the terminal.       \n"
            "                                                          \n"
            "  Practice blind-typing speed, right on the terminal      \n"
            "  using either an existing presets, or a provided text.   \n"
            "  Provided text can be either string argument or a full   \n"
            "  text file from any kind.                                \n"
            "  WPM and accuracy or errors are calculated and shown     \n"
            "  at the end of each line. Type <C-c> to exit at any time.\n"
            "                                                          \n"
            "Usage:                                                    \n"
            "  blind [OPTIONS] [STRING|FILE]                           \n"
            "                                                          \n"
            "Options:                                                  \n"
            "  -h            display help message                      \n"
            "  -b            block on wrong typing                     \n"
            "  -f=FILE       practice on FILE line                     \n"
            "                                                          \n"
            "  --help        display help message                      \n"
            "  --block       block on wrong typing                     \n"
            "  --file=FILE   practice on FILE line                     \n"
            "  --show-actual show the actual typed letter              \n"
            "  --allow-back  allow backspace for correction            \n"
            "                                                          \n"
            "STRING                                                    \n"
            "  one line of text. quoted or unquoted.                   \n"
            "                                                          \n"
            "Example:                                                  \n"
            "  blind -b                                                \n"
            "  blind \"line to practice on\"                           \n"
            "  blind --file <path-to-file>                             \n"
    );
}

void print_result(int wpm, double accuracy, int errors) {
    display(DIM"WPM: "RST"%d\n"RST, wpm);
    if (blocking_mode) {
        display(DIM"Errors: "RST"%s%d\n"RST, errors == 0 ? GREEN : RED, errors);
    } else {
        display(DIM"Accuracy: "RST"%s%.2f%%\n"RST, accuracy > 75 ? GREEN : RED, accuracy);
    }
}

void print_totals(void) {
    display("\n");
    display(DIM"\n=============== Sumary ===============\n");
    display(DIM"Lines: "RST"%d\n", score.count);
    print_result(score.wpm, score.accuracy, score.errors);
}

void play(char* s) {
    tcflush(STDIN_FILENO, TCIFLUSH);           // flush the input buffer
    display(DIM"\n%s\r"RST, s); fflush(stdout); // print the text

    int cur     = 0;
    int correct = 0;
    int errors  = 0;
    struct timespec start, end;
    char* history = (char*) malloc(strlen(s));
    assert(history && "Buy more RAM");

    clock_gettime(CLOCK_MONOTONIC, &start);
    while (s[cur] != '\0')
    {
        char c = getchar();

        if (c == '\n') {
            break;

        } else if (c == 27) { // skip escape key
            getchar(); getchar(); continue;

        } else if (c == 127) { // backspace
            if (cur > 0 && allow_backspace_mode) {
                --cur;
                if (history[cur] == s[cur]) {
                    --correct;
                } else {
                    --errors;
                }
                display(BACK DIM"%c"BACK RST, s[cur]);
            }
            continue;
        }

        if (c == s[cur]) {
            ++correct;
            display(GREEN"%c"RST, c);
        } else {
            ++errors;
            if (blocking_mode) {
                continue;
            }
            display(RED"%c"RST, show_actual_typed_mode ? c : s[cur]);
        }

        fflush(stdout);
        history[cur++] = c;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    // update scores only if started this line
    if (cur > 0) {
        double time     = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
        double accuracy = (double)correct / strlen(s) * 100;
        int wpm         = (strlen(s) / 5.0) / (time / 60.0);

        score.wpm       = (score.wpm * score.count + wpm) / (score.count + 1);
        score.accuracy  = (score.accuracy * score.count + accuracy) / (score.count + 1);
        score.count     += 1;
        score.errors    += errors;
        if (!hide_line_score) {
            display("\n\n");
            print_result(wpm, accuracy, errors);
        }
    }
}

void run_presets(void) {
    int i = 0;
    while (Data[i] != 0) {
        play(Data[i]);
        i++;
    }
    print_totals();
}

void run_provided_text(int argc, char** argv)
{
    char text[MAX_TEXT_LEN] = "";
    while (optind < argc) {
        strcat(text, argv[optind++]);

        // add space if its not the last
        if (optind < argc) {
            strcat(text, " ");
        }
    }

    play(text);
}

void run_file_input(void)
{
    FILE*   finput;
    char*   line = NULL;
    size_t  size = 0;
    ssize_t nread;

    finput = fopen(input_file_path, "r");
    if (!finput) {
        perror("fopen");
        return;
    }

    while ((nread = getline(&line, &size, finput)) != -1) {
        *strchr(line, '\n') = '\0';
        play(line);
    }

    free(line);
    fclose(finput);
    print_totals();
}

struct termios term;

void init(void) {
    tcgetattr(STDIN_FILENO, &term);
    struct termios raw = term;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void cleanup(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void handle_siginit(int sig) {
    print_totals();
    (void)sig;
    exit(0);
}

void parse_args(int argc, char** argv)
{
    int c;
    for (;;) {
        int option_idx;
        static struct option lo[] = {
            { "help",        no_argument,       0, 'h'                 },
            { "block",       no_argument,       0, 'b'                 },
            { "file",        required_argument, 0, 'f'                 },
            { "show-actual", no_argument,       0, OPT_SHOW_ACTUAL     },
            { "allow-back",  no_argument,       0, OPT_ALLOW_BACKSPACE },
            {0}
        };

        c = getopt_long(argc, argv, "hbf:", lo, &option_idx);
        if (c == -1) break;

        switch (c) {
            case 'h':
                help();
                exit(0);

            case 'b':
                blocking_mode = 1;
                break;

            case 'f':
                input_mode = FILE_INPUT;
                input_file_path = optarg;
                hide_line_score = 1;
                break;

            case OPT_SHOW_ACTUAL:
                show_actual_typed_mode = 1;
                break;

            case OPT_ALLOW_BACKSPACE:
                allow_backspace_mode = 1;
                break;

            default:
                usage();
                exit(1);
        }
    }
}

int main(int argc, char** argv) {
    atexit(cleanup);
    signal(SIGINT, handle_siginit);
    init();

    parse_args(argc, argv);

    // assuming rest of arrgs are the text input
    if (optind < argc) {
        input_mode = TEXT_INPUT;
    }

    if (input_mode == DEFAULT_INPUT) {
        run_presets();
    } else if (input_mode == TEXT_INPUT) {
        run_provided_text(argc, argv);
    } else if (input_mode == FILE_INPUT) {
        run_file_input();
    }

    return 0;
}

