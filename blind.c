/*
 * Blind - A simple program to practice touch-typing in the terminal.
 *
 * Practice touch-typing speed, right on the terminal using either an existing presets, or a provided text.
 * Provided text can be either string argument or a full text file from any kind.
 * WPM and accuracy or errors are calculated and shown at the end of each line.
 *
 * Keys:
 *  <RETURN>             ENTER to skip a line
 *  <CTRL-C>             EXIT at any time
 *
 * Options:
 *  -h                   display help message
 *  -v                   show version number
 *  -b                   block on wrong typing
 *  -f=FILE              practice on FILE lines
 *  -l=N                 line number to start in the file
 *
 *  --help               display help message
 *  --block              block on wrong typing
 *  --version            show version number
 *  --file=FILE          practice on FILE lines
 *  --line-number=N      line number to start in the file
 *  --show-actual        show the actual typed letter
 *  --allow-back         allow backspace for correction
 *
 * Arguments:
 *  STRING               one line of text. quoted or unquoted.
 *  FILE                 any file containing text
 *
 * Example:
 *   blind -b
 *   blind \"line to practice on\"
 *   blind --file <path-to-file> -l 10
 *
 * author: shalom2552
 * date: 2026-09-24
 */
#include <assert.h>
#include <bits/getopt_core.h>
#include <complex.h>
#include <errno.h>
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

#define NAME    "blind"
#define VERSION "0.2.1"
#define MAX_TEXT_LINE_LEN 2048

#define DEBUG(...)   fprintf(stderr, "DEBUG: "__VA_ARGS__)
#define display(...) do { if (!getenv("NDISPLAY")) printf(""__VA_ARGS__); } while (0)

#define RED   "\033[31m"
#define GREEN "\033[32m"
#define UNDER "\033[4m"
#define DIM   "\033[2m"
#define BOLD  "\033[1m"
#define BACK  "\033[D"
#define RST   "\033[0m"

enum Options {
    OPT_SHOW_ACTUAL = 256,
    OPT_ALLOW_BACKSPACE,
};

enum InputMode {
    DEFAULT_INPUT,
    STRING_INPUT,
    FILE_INPUT
};

int input_mode = DEFAULT_INPUT;
int blocking_mode          = 0;
int allow_backspace_mode   = 0;
int show_actual_typed_mode = 0;
int hide_line_score        = 0;
int file_start_line        = 1;

char* input_file_path   = NULL;
char* input_string_line = NULL;

struct {
    int    count;
    int    wpm;
    int    errors;
    double accuracy;
} score = {0};

// https://clagnut.com/blog/2380
static char* Data[] = {
    "Who packed five dozen old quart jugs in my box?",
    "The quick brown fox jumped over the lazy dogs.",
    "Woven silk pyjamas exchanged for blue quartz.",
    "Brawny gods just flocked up to quiz and vex him.",
    "Twelve ziggurats quickly jumped a finch box.",
    "Prating jokers quizzically vexed me with fibs.",
    "Amazingly few discotheques provide jukeboxes.",
    "The quick onyx goblin jumps over the lazy dwarf.",
    "The lazy major was fixing Cupid’s broken quiver.",
    "My faxed joke won a pager in the cable TV quiz show.",
    "Jacky can now give six big tips from the old quiz.",
    0
};

void usage(void)
{
    fprintf(stderr, "blind: usage: blind [OPTIONS] [STRING|FILE]\n");
}

void help(void)
{
    fprintf(stderr,
        "A program to practice touch-typing in the terminal.         \n"
        "                                                            \n"
        "  Practice touch-typing speed, right on the terminal        \n"
        "  using either an existing presets, or a provided text.     \n"
        "  Provided text can be either string argument or a full     \n"
        "  text file from any kind. WPM and accuracy or errors are   \n"
        "  calculated and shown at the end of each line.             \n"
        "                                                            \n"
        "Keys:                                                       \n"
        " <RETURN>             ENTER to skip a line                  \n"
        " <CTRL-C>             EXIT at any time                      \n"
        "                                                            \n"
        "Options:                                                    \n"
        " -h                   display help message                  \n"
        " -v                   show version number                   \n"
        " -b                   block on wrong typing                 \n"
        " -f=FILE              practice on FILE lines                \n"
        " -l=N                 line number to start in the file      \n"
        "                                                            \n"
        " --help               display help message                  \n"
        " --block              block on wrong typing                 \n"
        " --version            show version number                   \n"
        " --file=FILE          practice on FILE lines                \n"
        " --line-number=N      line number to start in the file      \n"
        " --show-actual        show the actual typed letter          \n"
        " --allow-back         allow backspace for correction        \n"
        "                                                            \n"
        "Arguments:                                                  \n"
        " STRING               one line of text. quoted or unquoted. \n"
        " FILE                 any file containing text              \n"
        "                                                            \n"
        "Example:                                                    \n"
        "  blind -b                                                  \n"
        "  blind \"line to practice on\"                             \n"
        "  blind --file <path-to-file> -l 10                         \n"
    );
}

void print_result(int wpm, double accuracy, int errors)
{
    display(DIM"WPM: "RST"%d\n"RST, wpm);
    if (blocking_mode) {
        display(DIM"Errors: "RST"%s%d\n"RST, errors == 0 ? GREEN : RED, errors);
    } else {
        display(DIM"Accuracy: "RST"%s%.2f%%\n"RST, accuracy > 75 ? GREEN : RED, accuracy);
    }
}

void print_totals(void)
{
    display("\n");
    display(DIM"\n=============== Sumary ===============\n");
    display(DIM"Lines: "RST"%d\n", score.count);
    print_result(score.wpm, score.accuracy, score.errors);
}

void play(char* s)
{
    tcflush(STDIN_FILENO, TCIFLUSH);           // flush the input buffer
    display(DIM"\n%s\r"RST, s); fflush(stdout); // print the text

    int idx     = 0;
    int correct = 0;
    int errors  = 0;
    int total   = 0;
    struct timespec start, end;
    char* history = (char*) malloc(strlen(s));
    assert(history && "Buy more RAM");

    char* c = strchr(s, '\n');
    if (c) *c = '\0';

    // trim spaces and tabs prefix
    for (; s[idx] == ' '; ++idx) {
        display(" ");
    }

    while (s[idx] != '\0')
    {
        // line end with space
        if (s[idx] == ' ' && s[idx + 1] == '\0') { getchar(); break; }
        // trim long spaces and tabs
        while (idx > 0 && s[idx - 1] == ' ' && s[idx] == ' ') {
            display(" ");
            ++idx;
        }

        char c = getchar();

        // start the clock on first type (after getchar)
        if (!total) clock_gettime(CLOCK_MONOTONIC, &start);

        if (c == '\n') { // skip line on enter
            break;

        } else if (c == 27) { // skip escape key
            getchar(); getchar(); continue;

        } else if (c == 127) { // backspace - backtrack
            if (idx > 0 && allow_backspace_mode) {
                --idx;
                --total;
                if (history[idx] == s[idx]) {
                    --correct;
                } else {
                    --errors;
                }
                display(BACK DIM"%c"BACK RST, s[idx]);
            }
            continue;
        }

        if (c == s[idx]) {
            ++correct;
            display(GREEN"%c"RST, c);
        } else {
            ++errors;
            if (blocking_mode) {
                continue;
            }
            display(RED"%c"RST, show_actual_typed_mode ? c : s[idx]);
        }

        fflush(stdout);
        history[idx++] = c;
        ++total;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    // update scores if line started
    if (total > 0) {
        double time     = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
        double accuracy = (double)correct / strlen(s) * 100;
        int wpm         = ((double)total / 5.0) / (time / 60.0);

        score.wpm       = (score.wpm * score.count + wpm) / (score.count + 1);
        score.accuracy  = (score.accuracy * score.count + accuracy) / (score.count + 1);
        score.errors    += errors;
        score.count     += 1;
        if (!hide_line_score) {
            display("\n\n");
            print_result(wpm, accuracy, errors);
        }
    }
}

void run_presets(void)
{
    int i = 0;
    while (Data[i] != 0) {
        play(Data[i++]);
    }
    print_totals();
}

void run_provided_text(int argc, char** argv)
{
    // play rest of cmdline args as text
    char text[MAX_TEXT_LINE_LEN] = "";
    while (optind < argc) {
        strcat(text, argv[optind++]);

        // seprate arguments by space
        if (optind < argc) {
            strcat(text, " ");
        }
    }

    play(text);
}

void run_file_input(void)
{
    FILE*   finput;
    char*   line   = NULL;
    int     line_c = 1;
    size_t  size   = 0;
    ssize_t nread;

    finput = fopen(input_file_path, "r");
    if (!finput) {
        fprintf(stderr, "fopen: %s: %s\n", strerror(errno), input_file_path);
        return;
    }

    // play each line of the file
    while ((nread = getline(&line, &size, finput)) != -1) {
        if (++line_c < file_start_line) continue;

        // only run regular lines (ending with '\n')
        char* c = strchr(line, '\n');
        if (c) {
            *c = '\0';
            play(line);
        }
    }

    free(line);
    fclose(finput);
    print_totals();
}

struct termios term;

void init(void)
{
    tcgetattr(STDIN_FILENO, &term);
    struct termios raw = term;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void cleanup(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
}

void handle_siginit(int sig)
{
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
            { "version",     no_argument,       0, 'v'                 },
            { "block",       no_argument,       0, 'b'                 },
            { "file",        required_argument, 0, 'f'                 },
            { "start-line",  required_argument, 0, 'l'                 },
            { "show-actual", no_argument,       0, OPT_SHOW_ACTUAL     },
            { "allow-back",  no_argument,       0, OPT_ALLOW_BACKSPACE },
            {0}
        };

        c = getopt_long(argc, argv, "hbvf:l:", lo, &option_idx);
        if (c == -1) break;

        switch (c) {
            case 'h':
                help();
                exit(0);

            case 'v':
                display("%s: %s\n", NAME, VERSION);
                exit(0);

            case 'b':
                blocking_mode = 1;
                break;

            case 'l':
                file_start_line = atoi(optarg);
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

int main(int argc, char** argv)
{
    atexit(cleanup);
    signal(SIGINT, handle_siginit);
    init();

    parse_args(argc, argv);

    // HACK: assuming rest of args are the text input if no input mode provided
    //          this allows to run unquoted text: `blind line to practice`
    //          and also easier to parse over a string input flag value which couses bugs
    if (optind < argc && input_mode == DEFAULT_INPUT) {
        input_mode = STRING_INPUT;
    }

    if (input_mode == DEFAULT_INPUT) {
        run_presets();
    } else if (input_mode == STRING_INPUT) {
        run_provided_text(argc, argv);
    } else if (input_mode == FILE_INPUT) {
        run_file_input();
    }

    return 0;
}

// FIX: BUGS:
//
//      BUG: 70, medium
//          in file mode with blank lines, we get 0 WPM after <C-c>
//          suspect - indented lines count as started and caculate to the total scores
//          fix: skip if line not started even in auto indent
//              also, dont caculate totals on empty lines
//
//      BUG: 40, medium
//          on single char opt with arugments the '=' is left in the optval
//
//
// NOTE: FEATURES:
//
//      NOTE: 70, easy
//          add total skiped to total scores in file mode.
//
//      NOTE: 40, small
//          hide cursor and blink current word.
//
//      NOTE: 30, medium
//          caculate mid line scrors (WPM is the issue, how to update mid line?)
//          or document that mid line enter not save stats.
//
