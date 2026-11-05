#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define HEART "\xE2\x99\xA5 "
#define HEART_WIDTH 2

static struct termios original_terminal;
static int terminal_configured = 0;
static volatile sig_atomic_t running = 1;

static void restore_terminal(void)
{
    if (terminal_configured) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal);
        terminal_configured = 0;
    }

    fputs("\033[0m\033[?25h\n", stdout);
    fflush(stdout);
}

static void handle_signal(int signal_number)
{
    (void)signal_number;
    running = 0;
}

static void draw_screen(unsigned long frame)
{
    struct winsize size;
    int rows = 24;
    int columns = 80;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 &&
        size.ws_row > 0 && size.ws_col > 0) {
        rows = size.ws_row;
        columns = size.ws_col;
    }

    printf("\033[2J\033[H");

    const char *title = "Gioia";
    int title_padding = (columns - (int)strlen(title)) / 2;
    if (title_padding < 0) {
        title_padding = 0;
    }

    printf("%*s\033[1;93m%s\033[0m\n", title_padding, "", title);

    for (int row = 1; row < rows; row++) {
        for (int column = 0; column + HEART_WIDTH <= columns; column += HEART_WIDTH) {
            unsigned long color = (frame + (unsigned long)row +
                                   (unsigned long)(column / HEART_WIDTH)) % 3;
            const char *colors[] = {"91", "95", "31"};

            printf("\033[%sm%s\033[0m", colors[color], HEART);
        }
        putchar('\n');
    }

    fflush(stdout);
}

int main(void)
{
    struct termios raw_terminal;
    struct sigaction action;
    unsigned long frame = 0;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "Esegui questo programma in un terminale.\n");
        return EXIT_FAILURE;
    }

    if (tcgetattr(STDIN_FILENO, &original_terminal) == -1) {
        perror("tcgetattr");
        return EXIT_FAILURE;
    }

    raw_terminal = original_terminal;
    raw_terminal.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw_terminal.c_cc[VMIN] = 0;
    raw_terminal.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_terminal) == -1) {
        perror("tcsetattr");
        return EXIT_FAILURE;
    }
    terminal_configured = 1;
    atexit(restore_terminal);

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    fputs("\033[?25l", stdout);

    while (running) {
        struct pollfd input = { .fd = STDIN_FILENO, .events = POLLIN };
        int ready = poll(&input, 1, 120);

        if (ready > 0 && (input.revents & POLLIN)) {
            char key;
            if (read(STDIN_FILENO, &key, 1) == 1 && (key == 'q' || key == 'Q')) {
                break;
            }
        } else if (ready == -1 && errno != EINTR) {
            perror("poll");
            break;
        }

        draw_screen(frame++);
    }

    return EXIT_SUCCESS;
}
