/*
 * Blind - A simple program to practice touch-typing in the terminal.
 *
 * Practice touch-typing speed, right on the terminal using either an existing
 * presets, or a provided text. Provided text can be either string argument or
 * a full text file from any kind. WPM and accuracy or errors, are calculated
 * and shown at the end.
 *
 * Keys:
 *   <RETURN>             ENTER to skip a line
 *   <CTRL-C>             EXIT at any time
 *
 * Options:
 *   -h, --help           display help message
 *   -v, --version        show version number
 *   -b, --block          block on wrong typing
 *   -r, --reset          reset the saved state
 *   -f, --file=FILE      practice on FILE lines
 *   -l, --start-line=N   file line number to start on
 *   -o, --output=FILE    specify FILE to store stats
 *       --show-actual    show the actual typed letter
 *       --allow-back     allow backspace for correction
 *       --alt-screen     run the program in alt-screen
 *
 * Arguments:
 *   STRING               one line of text. quoted or unquoted.
 *   NUMBER               Positive whole number (natural)
 *   FILE                 any file containing text
 *
 * Examples:
 *   blind -b
 *   blind "line to practice on"
 *   blind --file <path-to-file> -l 10
 *   blind --output=stats.csv
 *   blind --alt-screen --show-actual --allow-back -o /dev/null
 *
 * Default stats file: $XDG_DATA_HOME/DEFAULT_OUTPUT_PATH
 *
 * repo: https://github.com/shalom2552/blind.git
 * author: shalom2552
 * date: 2026-09-24
 */
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

const char* NAME;
#ifndef VERSION
#define VERSION "unknown"
#endif // VERSION

#define AWL                 5.0f // Average letters per word
#define MAX_TEXT_LINE_LEN   2048

#define DEFAULT_OUTPUT_PATH "/blind-tt/blind_stats.csv"
#define STATE_FILE_PATH     "/blind-tt/state.bin"

#define home()       getenv("HOME")
#define xdg_data()   getenv("XDG_DATA_HOME")
#define no_display() getenv("NDISPLAY")
#define display(...) do { if (!no_display()) printf(""__VA_ARGS__); fflush(stdout); } while (0)
#define DEBUG(...)   do { fprintf(stderr, "DEBUG: "__VA_ARGS__); puts(""); } while (0)

#define REDBG "\033[41m"
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
    OPT_ALT_SCREEN,
};

enum InputMode {
    DEFAULT_INPUT,
    STRING_INPUT,
    FILE_INPUT,
};

enum KEYS {
    KEY_ESC = 27,
    KEY_DEL = 127,
};

int input_mode = DEFAULT_INPUT;
int blocking_mode          = 0;
int allow_backspace_mode   = 0;
int show_actual_typed_mode = 0;
int hide_line_score        = 0;
int run_in_alt_screen      = 0;
int reset_state            = 0;
size_t file_start_line     = 1;

char* input_file_path   = NULL;
char* input_string_line = NULL;
char* output_file_path  = NULL;

int term_initialized = 0;
struct termios term;

struct {
    int wpm;
    int typed_errors;
    int lines_count;
    int lines_skipped;
    int total_letters;
    int correct_letters;
    double total_time;
    double accuracy;
} score = {0};

const size_t state_version = 3; // validation number
typedef struct {
    size_t state_version;
    size_t presets_idx;
    char   last_file[PATH_MAX];
    size_t line_of_last_file;
} State;
const State default_state = {
    .state_version     = state_version,
    .last_file         = "",
    .line_of_last_file = 1,
};
State state;

// https://clagnut.com/blog/2380
static char* Presets[] = {
    "Who packed five dozen old quart jugs in my box?",
    "The quick brown fox jumped over the lazy dogs.",
    "Woven silk pyjamas exchanged for blue quartz.",
    "Brawny gods just flocked up to quiz and vex him.",
    "Twelve ziggurats quickly jumped a finch box.",
    "Prating jokers quizzically vexed me with fibs.",
    "Amazingly few discotheques provide jukeboxes.",
    "The quick onyx goblin jumps over the lazy dwarf.",
    "The lazy major was fixing Cupid's broken quiver.",
    "My faxed joke won a pager in the cable TV quiz show.",
    "Jacky can now give six big tips from the old quiz.",
    "Just poets wax boldly as kings and queens march over fuzz.",
    "Quincy Pondexter blocked five jams against the Wizards!",
    "Crazy Frederick bought many very exquisite opal jewels.",
    "A quivering Texas zombie fought republic linked jewelry.",
    "A very big box sailed up then whizzed quickly from Japan.",
    "Grumpy wizards make toxic brew for the evil queen and jack.",
    "The job of waxing linoleum frequently peeves chintzy kids.",
    "Back in June we delivered oxygen equipment of the same size.",
    "Just keep examining every low bid quoted for zinc etchings.",
    "How razorback-jumping frogs can level six piqued gymnasts!",
    "A quick movement of the enemy will jeopardize six gunboats.",
    "All questions asked by five watched experts amaze the judge.",
    "Bobby Klun awarded Jayme sixth place for her very high quiz.",
    "The wizard quickly jinxed the gnomes before they vaporized.",
    "Zelda might fix the job growth plans very quickly on Monday.",
    "Zack Gappow saved the job requirement list for the six boys.",
    "Jackie will budget for the most expensive zoology equipment.",
    "Quirky spud boys can jam after zapping five worthy Polysixes.",
    "Jim quickly realized that the beautiful gowns are expensive.",
    0,
};

void version(void)
{
    fprintf(stderr, "%s %s\n", NAME, VERSION);
}

void usage(void)
{
    fprintf(stderr, "%s: usage: %s [OPTIONS] [STRING|FILE]\n", NAME, NAME);
}

void info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf(DIM"info: ");
    vprintf(fmt, args);
    puts(RST);
    va_end(args);
}

void help(void)
{
    fprintf(stderr,
        "A program to practice touch-typing in the terminal.\n"
        "\n"
        "  Practice touch-typing speed, right on the terminal\n"
        "  using either an existing presets, or a provided text.\n"
        "  Provided text can be either string argument or a full\n"
        "  text file from any kind. WPM and accuracy or errors, are\n"
        "  calculated and shown at the end.\n"
        "\n"
        "Keys:\n"
        "  <RETURN>             ENTER to skip a line\n"
        "  <CTRL-C>             EXIT at any time\n"
        "\n"
        "Options:\n"
        "  -h, --help           display help message\n"
        "  -v, --version        show version number\n"
        "  -b, --block          block on wrong typing\n"
        "  -r, --reset          reset the saved state\n"
        "  -f, --file=FILE      practice on FILE lines\n"
        "  -l, --start-line=N   file line number to start on\n"
        "  -o, --output=FILE    specify FILE to store stats\n"
        "      --show-actual    show the actual typed letter\n"
        "      --allow-back     allow backspace for correction\n"
        "      --alt-screen     run the program in alt-screen\n"
        "\n"
        "Arguments:\n"
        "  STRING               one line of text. quoted or unquoted.\n"
        "  NUMBER               Positive whole number (natural)\n"
        "  FILE                 any file containing text\n"
        "\n"
        "Examples:\n"
        "  blind -b\n"
        "  blind \"line to practice on\"\n"
        "  blind --file <path-to-file> -l 10\n"
        "  blind --output=stats.csv\n"
        "  blind --alt-screen --show-actual --allow-back -o /dev/null\n"
        "\n"
        "Default stats file: %s" DEFAULT_OUTPUT_PATH "\n"
        "\n"
        "Bugs can be reported on GitHub:\n"
        "https://github.com/shalom2552/blind.git\n"
        ,xdg_data()
    );
}

void print_result(int wpm, double accuracy, int errors)
{
    display(DIM"WPM:      "RST"%d\n"RST, wpm);
    display(DIM"Errors:   "RST"%s%d\n"RST, errors == 0 ? GREEN : RED, errors);
    if (!blocking_mode) {
        display(DIM"Accuracy: "RST"%s%.2f%%\n"RST, accuracy > 75 ? GREEN : RED, accuracy);
    }
}

void print_totals(void)
{
    if (run_in_alt_screen) display("\033[?1049l"); // exit alt-screen
    display(DIM"\n=============== Sumary ===============\n");
    print_result(score.wpm, score.accuracy, score.typed_errors);
    if (score.lines_count > 1) display(DIM"Lines:    "RST"%d\n", score.lines_count);
    if (score.lines_skipped) display(DIM"Skipped:  "RST"%d\n", score.lines_skipped);
    if (run_in_alt_screen) display("\033[?1049h"); // back to alt-screen
}

void play(char* line)
{
    tcflush(STDIN_FILENO, TCIFLUSH); // flush the input buffer
    struct winsize w;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &w) == -1) { // get terminal size
        perror("ioctl");
        return;
    }

    // Convert UTF-8 input to wide characters for character-based typing.
    wchar_t text[MAX_TEXT_LINE_LEN];
    if (mbstowcs(text, line, MAX_TEXT_LINE_LEN) == (size_t)-1) {
        perror("mbstowcs");
        return;
    }
    text[MAX_TEXT_LINE_LEN - 1] = L'\0';

    if (wcslen(text) > w.ws_col) text[w.ws_col] = L'\0'; // trim the line
    display(DIM"\n%ls\r"RST, text);                  // print dimmed line

    int idx     = 0;
    int correct = 0;
    int errors  = 0;
    int total   = 0;
    struct timespec start, end;
    wchar_t* history = (wchar_t*)malloc(wcslen(text) * sizeof(wchar_t));
    assert(history && "Buy more RAM");

    wchar_t* line_end = wcschr(text, L'\n');
    if (line_end) *line_end = L'\0';

    // skip spaces and tabs prefix
    for (; iswspace(text[idx]); ++idx) {
        display(" ");
    }

    while (text[idx] != '\0') {

        // line end with space
        if (iswspace(text[idx]) && text[idx + 1] == L'\0') {
            ++idx; break;
        }

        // skip long spaces and tabs
        if (idx > 0 && iswspace(text[idx - 1]) && iswspace(text[idx])) {
            ++idx; display(" "); continue;
        }

        wint_t c_input = getwchar();
        if (c_input == WEOF) {
            free(history);
            perror("getwchar");
            return;
        }

        // start the clock on first type (after getchar)
        if (!total) clock_gettime(CLOCK_MONOTONIC, &start);

        if (c_input == L'\n') { // skip line on enter
            if ((size_t)idx != wcslen(text) - 1) {
                score.lines_skipped += 1;
                total = 0;
            }
            break;

        } else if (c_input == KEY_ESC) { // skip escape keys
            getwchar(); getwchar(); continue;

        } else if (c_input == KEY_DEL) { // backspace: backtrack
            if (idx > 0 && allow_backspace_mode) {
                --idx;
                --total;
                if (history[idx] == text[idx]) {
                    --correct;
                } else {
                    --errors;
                }
                display(BACK DIM"%lc"BACK RST, text[idx]);
            }
            continue;
        }

        if (c_input == text[idx]) {
            ++correct;
            display(GREEN"%lc"RST, c_input);
        } else {
            ++errors;
            if (blocking_mode) continue;
            char* color = (iswspace(text[idx]) || iswspace(c_input)) ? REDBG : RED;
            display("%s%lc"RST, color, show_actual_typed_mode ? c_input : text[idx]);
        }

        fflush(stdout);
        history[idx++] = c_input;
        ++total;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    // update scores if line started
    if (total > 1) {
        double time     = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
        double accuracy = (double)correct / total * 100;
        int wpm         = ((double)correct / AWL) / (time / 60.0f);

        score.lines_count     += 1;
        score.total_letters   += total;
        score.correct_letters += correct;
        score.typed_errors    += errors;
        score.total_time      += time;
        score.accuracy        =  (double)score.correct_letters / score.total_letters * 100;
        score.wpm             =  ((double)score.correct_letters / AWL) / (score.total_time / 60.0);

        if (!hide_line_score) {
            display("\n\n");
            print_result(wpm, accuracy, errors);
        }
    }
    free(history);
}

void run_presets(void)
{
    if (!Presets[state.presets_idx]) state.presets_idx = 0;
    if (state.presets_idx) {
        info("continuing at preset %d (reset with -r)", state.presets_idx + 1);
    }
    size_t idx = state.presets_idx;
    while (Presets[idx] != 0) {
        play(Presets[idx++]);
        state.presets_idx = idx;
        display("\n");
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
    size_t  line_c = 1;
    size_t  size   = 0;
    ssize_t nread;

    // store/restore saved file info
    char fullpath[PATH_MAX];
    if (!realpath(input_file_path, fullpath)) {
        fprintf(stderr, "%s: %s: %s\n", NAME, strerror(errno), input_file_path);
        return;
    }
    // if this is the same file from last run, restore its line number
    if (!strcmp(state.last_file, fullpath)) {
        if (file_start_line == 1 && state.line_of_last_file > 1) {
            file_start_line = state.line_of_last_file;
            info("continuing at line %d (reset with -r)", file_start_line);
        }
    }
    strcpy(state.last_file, fullpath); // save last_file

    finput = fopen(input_file_path, "r");
    if (!finput) {
        fprintf(stderr, "%s: %s: %s\n", NAME, strerror(errno), input_file_path);
        return;
    }

    // play each line of the file
    while ((nread = getline(&line, &size, finput)) != -1) {
        state.line_of_last_file = line_c;
        if (line_c < file_start_line) {
            ++line_c;
            continue;
        }

        // only run regular lines (ending with '\n')
        char* eol = strchr(line, '\n');
        if (eol) {
            *eol = '\0';
            play(line);
        }
        ++line_c;
    }

    free(line);
    fclose(finput);
    print_totals();
}

int ensure_dir_exists(const char* path)
{
    if (path == NULL || strlen(path) >= PATH_MAX) return -1;

    char dir_path[PATH_MAX];
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    char* last_slash = strrchr(dir_path, '/');
    if (last_slash == NULL) return 0;
    if (last_slash == dir_path) return 0;
    *last_slash = '\0';

    for (char* p = dir_path + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }

    if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

FILE* open_xdg_data_file(const char* path, const char* modes)
{
    char* xdg_data = xdg_data();
    char* fallback = "";
    char* slash    = "";
    if (!xdg_data || !*xdg_data) {
        fallback = home();
        xdg_data = "/.local/share";
        slash = "/";
        if (!fallback || !*fallback) return NULL;
    }
    char file_path[PATH_MAX];
    snprintf(file_path, PATH_MAX - 1, "%s%s%s%s",
             fallback, slash, xdg_data, path);
    if (ensure_dir_exists(file_path)) return NULL;
    return fopen(file_path, modes);
}

void write_summary_to_file(void)
{
    if (!output_file_path) output_file_path = DEFAULT_OUTPUT_PATH;
    FILE* output_file = open_xdg_data_file(output_file_path, "a");
    if (!output_file) {
        fprintf(stderr, "%s: could not save result score: %s: %s",
                NAME, output_file_path, strerror(errno));
        return;
    }

    long file_size = ftell(output_file);
    if (file_size < 0) {
        fprintf(stderr, "%s: cannot get output file size for '%s': %s\n",
                NAME, output_file_path, strerror(errno));
        fclose(output_file);
        return;
    }

    if (file_size == 0 &&
        fprintf(output_file, "timestamp,mode,lines,lines_skipped,total_letters,correct_letters,errors,wpm,accuracy\n") < 0) {
        fprintf(stderr, "%s: cannot write CSV header to '%s': %s\n",
                NAME, output_file_path, strerror(errno));
        fclose(output_file);
        return;
    }

    const char* mode;
    switch (input_mode) {
        case STRING_INPUT:  mode = "text";    break;
        case FILE_INPUT:    mode = "file";    break;
        case DEFAULT_INPUT: mode = "presets"; break;
        default:            mode = "unknown"; break;
    }

    const char* cur_locale = setlocale(LC_ALL, NULL);
    if (cur_locale == NULL) {
        fprintf(stderr, "%s: cannot query current locale: %s\n", NAME, strerror(errno));
        fclose(output_file);
        return;
    }

    char* saved_cur_locale = NULL;
    if (cur_locale) {
        saved_cur_locale = strdup(cur_locale);
        if (saved_cur_locale == NULL) {
            fprintf(stderr, "%s: cannot save current locale: %s\n", NAME, strerror(errno));
            fclose(output_file);
            return;
        }
    }

    if (setlocale(LC_NUMERIC, "C") == NULL) {
        fprintf(stderr, "%s: cannot set numeric locale to C: %s\n", NAME, strerror(errno));
        free(saved_cur_locale);
        fclose(output_file);
        return;
    }

    int fprintf_res = fprintf(output_file, "%lld,%s,%d,%d,%d,%d,%d,%d,%.2f\n",
                (long long)time(NULL), mode, score.lines_count,
                score.lines_skipped, score.total_letters, score.correct_letters,
                score.typed_errors, score.wpm, score.accuracy);

    if (saved_cur_locale != NULL) {
        setlocale(LC_ALL, saved_cur_locale);
        free(saved_cur_locale);
    }

    if (fprintf_res < 0) {
        fprintf(stderr, "%s: cannot write session statistics to '%s': %s\n",
                NAME, output_file_path, strerror(errno));
        fclose(output_file);
        return;
    }

    if (fclose(output_file) != 0) {
        fprintf(stderr, "%s: cannot close output file '%s': %s\n",
                NAME, output_file_path, strerror(errno));
    }
}

void load_state(void)
{
    FILE* state_file = open_xdg_data_file(STATE_FILE_PATH, "rb");
    if (!state_file) return;
    if (fread(&state, sizeof(State), 1, state_file) != 1
            || state.state_version != state_version
            || reset_state == 1) {
        state = default_state;
    }
    fclose(state_file);
}

void save_state(void)
{
    FILE* state_file = open_xdg_data_file(STATE_FILE_PATH, "wb");
    if (!state_file) {
        fprintf(stderr, "%s: could not save state file: %s\n", NAME, strerror(errno));
        return;
    }
    fwrite(&state, sizeof(State), 1, state_file);
    fclose(state_file);
}

void init(void)
{
    load_state();
    atexit(save_state);
    if (term_initialized) return;
    if (!no_display()) {
        tcgetattr(STDIN_FILENO, &term);
        struct termios raw = term;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        if (run_in_alt_screen) display("\033[?1049h\033[0H");
    } else {
        DEBUG("running in NDISPLAY mode\n");
    }
    term_initialized = 1;
}

void cleanup(void)
{
    if (term_initialized && !no_display()) {
        if (run_in_alt_screen) display("\033[?1049l");
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
    }
}

void handle_siginit(int sig)
{
    print_totals();
    (void)sig;
    exit(0);
}

void parse_args(int argc, char** argv)
{
    char* laststash = strrchr(*argv, '/');
    NAME = laststash ? laststash + 1 : *argv;

    int c;
    for (;;) {
        int option_idx;
        static struct option lo[] = {
            { "help",        no_argument,       0, 'h'                 },
            { "version",     no_argument,       0, 'v'                 },
            { "block",       no_argument,       0, 'b'                 },
            { "reset",       no_argument,       0, 'r'                 },
            { "file",        required_argument, 0, 'f'                 },
            { "start-line",  required_argument, 0, 'l'                 },
            { "output",      optional_argument, 0, 'o'                 },
            { "allow-back",  no_argument,       0, OPT_ALLOW_BACKSPACE },
            { "show-actual", no_argument,       0, OPT_SHOW_ACTUAL     },
            { "alt-screen",  no_argument,       0, OPT_ALT_SCREEN      },
            {0},
        };

        c = getopt_long(argc, argv, "hvbrf:l:o:", lo, &option_idx);
        if (c == -1) break;

        switch (c) {
            case 'h':
                help();
                exit(0);

            case 'v':
                version();
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

            case 'o':
                output_file_path = optarg;
                break;

            case 'r':
                reset_state = 1;
                break;

            case OPT_ALLOW_BACKSPACE:
                allow_backspace_mode = 1;
                break;

            case OPT_SHOW_ACTUAL:
                show_actual_typed_mode = 1;
                break;

            case OPT_ALT_SCREEN:
                run_in_alt_screen = 1;
                break;

            default:
                usage();
                exit(1);
        }
    }
}

int main(int argc, char** argv)
{
    parse_args(argc, argv);
    atexit(cleanup);
    atexit(write_summary_to_file);
    signal(SIGINT, handle_siginit);
    init();

    if (setlocale(LC_ALL, "") == NULL) {
        perror("setlocale");
        return 1;
    }

    // HACK: assuming rest of args are the text input if no input mode provided
    //          this allows to run unquoted text: `blind line to practice`
    if (optind < argc && input_mode == DEFAULT_INPUT) {
        input_mode = STRING_INPUT;
    }

    switch (input_mode) {
        case STRING_INPUT : run_provided_text(argc, argv); break;
        case FILE_INPUT   : run_file_input()             ; break;
        case DEFAULT_INPUT: run_presets()                ; break;
        default: return 1;
    }

    return 0;
}

//
// NOTE: FEATURES
//
//      NOTE: 40, small
//          hide cursor and blink current word.
//
//      NOTE: 30, medium
//          caculate mid line scrors (WPM is the issue, how to update mid line?)
//          or document that mid line enter not save stats.
//
