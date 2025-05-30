#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termio.h>
#include <unistd.h>

#define buffer_size 1024
#define token_size 64
#define token_delimiter " \t\r\n\a"
#define conf_file "MOshell.conf"
#define history_file "MOshell.history"
#define max_lines 300

// We need to pass these args since we created an array of function pointers for quick access

int shell_cd(char **args);
int shell_help(char **args);
int shell_pwd(char **args);
int shell_launch(char **args);

int shell_exit(char **args)
{
        printf("Exiting MOshell.\n");
        return 0;
}
struct termios orig_termios;

// CTRL+C handler
void handle_signals(int sig)
{
        (void)sig;
        char response;
        printf("\nAre you sure you want to exit (Y/N)? ");
        int res = scanf(" %c", &response);
        while (getchar() != '\n')
                ; // flush stdin
        if (res == 1 && (response == 'Y' || response == 'y'))
                exit(0);
}

// Trim history lines to max_lines
void trim_history_lines(char **lines, int *count)
{
        if (*count <= max_lines)
                return;
        int remove_count = *count - max_lines;
        for (int i = 0; i < *count - remove_count; i++)
                lines[i] = lines[i + remove_count];
        *count = max_lines;
}

// Cache commands in history file (keep last max_lines)
void history_cache(char *command)
{
        FILE *fptr = fopen(history_file, "r");
        if (!fptr)
        {
                // History file does not exist, create and write new command
                fptr = fopen(history_file, "a");
                if (!fptr)
                {
                        fprintf(stderr, "Unable to open history file for writing\n");
                        return;
                }
                fprintf(fptr, "%s\n", command);
                fclose(fptr);
                return;
        }

        // Read existing lines
        char *lines[max_lines * 2];
        int count  = 0;
        size_t len = 0;
        ssize_t read;
        char *line = NULL;

        while ((read = getline(&line, &len, fptr)) != -1)
        {
                if (count < max_lines * 2)
                {
                        line[strcspn(line, "\r\n")] = 0;
                        lines[count++]              = strdup(line);
                }
        }
        free(line);
        fclose(fptr);

        // Add new command
        if (count == max_lines * 2)
        {
                free(lines[0]);
                memmove(lines, lines + 1, sizeof(char *) * (count - 1));
                count--;
        }
        lines[count++] = strdup(command);

        // Trim
        trim_history_lines(lines, &count);

        // Rewrite file
        fptr = fopen(history_file, "w");
        if (!fptr)
        {
                fprintf(stderr, "Unable to write history file\n");
                for (int i = 0; i < count; i++)
                        free(lines[i]);
                return;
        }
        for (int i = 0; i < count; i++)
        {
                fprintf(fptr, "%s\n", lines[i]);
                free(lines[i]);
        }
        fclose(fptr);
}

// Find first history entry starting with 'data'
char *tab_autocomplete(char *data)
{
        if (!data || strlen(data) == 0)
                return NULL;

        FILE *fd = fopen(history_file, "r");
        if (!fd)
                return NULL;

        char line[buffer_size];
        size_t len  = strlen(data);
        char *match = NULL;

        while (fgets(line, sizeof(line), fd))
        {
                size_t linelen = strlen(line);
                if (linelen > 0 && line[linelen - 1] == '\n')
                        line[linelen - 1] = '\0';

                if (strncmp(line, data, len) == 0)
                {
                        match = malloc(strlen(line) + 1);
                        if (match)
                                strcpy(match, line);
                        break;
                }
        }
        fclose(fd);
        return match;
}

// Built-in commands
char *builtin_str[]            = {"cd", "help", "pwd", "exit"};
int (*builtin_func[])(char **) = {&shell_cd, &shell_help, &shell_pwd, &shell_exit};
int num_builtins() { return sizeof(builtin_str) / sizeof(char *); }

// Shorten long paths by replacing home with ~ and truncating middle components
char *shorten_part(char *path)
{
        static char shortened[1024];
        char *user = getenv("USER");
        if (!user)
                user = "user";

        char home_prefix[256];
        snprintf(home_prefix, sizeof(home_prefix), "/home/%s", user);
        size_t home_len = strlen(home_prefix);

        const char *relative_path = path;
        if (strncmp(path, home_prefix, home_len) == 0)
                relative_path = path + home_len;

        if (strlen(path) <= 30)
        {
                if (relative_path != path)
                        snprintf(shortened, sizeof(shortened), "~%s", relative_path);
                else
                        snprintf(shortened, sizeof(shortened), "%s", path);
                return shortened;
        }

        char temp[1024];
        snprintf(temp, sizeof(temp), "%s", relative_path);

        char *token;
        char *components[64];
        int count = 0;

        token = strtok(temp, "/");
        while (token != NULL && count < 64)
        {
                components[count++] = token;
                token               = strtok(NULL, "/");
        }

        shortened[0] = '\0';
        if (relative_path != path)
                strcat(shortened, "~");

        for (int i = 0; i < count; i++)
        {
                strcat(shortened, "/");
                if (i == count - 1)
                        strcat(shortened, components[i]);
                else
                        strncat(shortened, components[i], 1);
        }

        return shortened;
}

void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void enable_raw_mode()
{
        tcgetattr(STDIN_FILENO, &orig_termios);
        atexit(disable_raw_mode);

        struct termios raw = orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void clear_line(int prompt_len, int input_len)
{
        // Move to start, clear entire line
        printf("\r\033[K");
        // Print prompt again
        for (int i = 0; i < prompt_len; i++)
                putchar(' ');
        printf("\r");
}

// Print input buffer with cursor position and suggestion in grey
void print_input_with_suggestion(char *buffer, int length, int cursor, char *suggestion)
{
        // Clear line, print prompt and buffer
        printf("\r\033[K> ");
        fwrite(buffer, 1, length, stdout);

        // If suggestion exists and differs from buffer, print difference in grey
        if (suggestion && strncmp(suggestion, buffer, length) == 0 &&
            strlen(suggestion) > (size_t)length)
        {
                printf("\033[90m%s\033[0m", suggestion + length); // grey text
        }

        // Move cursor back to cursor position (offset + 2 for prompt "> ")
        printf("\r\033[%dC", cursor + 2);
        fflush(stdout);
}

// Reads command with tab-completion from history shown as grey suggestion
char *read_command()
{
        enable_raw_mode();

        int bufsize  = buffer_size;
        char *buffer = malloc(bufsize);
        if (!buffer)
        {
                fprintf(stderr, "Allocation error\n");
                exit(EXIT_FAILURE);
        }

        int length       = 0;
        int cursor       = 0;
        char *suggestion = NULL;

        printf("> ");
        fflush(stdout);

        while (1)
        {
                char c = getchar();

                if (c == 127 || c == '\b') // Backspace
                {
                        if (cursor > 0)
                        {
                                memmove(&buffer[cursor - 1], &buffer[cursor], length - cursor);
                                length--;
                                cursor--;
                                buffer[length] = '\0';
                        }
                }
                else if (c == '\t') // TAB key: accept suggestion
                {
                        if (suggestion)
                        {
                                size_t sug_len = strlen(suggestion);
                                if (sug_len >= (size_t)length)
                                {
                                        // Replace buffer with suggestion
                                        if (sug_len + 1 > (size_t)bufsize)
                                        {
                                                char *newbuf = realloc(buffer, sug_len + 1);
                                                if (!newbuf)
                                                        continue;
                                                buffer  = newbuf;
                                                bufsize = sug_len + 1;
                                        }
                                        strcpy(buffer, suggestion);
                                        length = (int)sug_len;
                                        cursor = length;
                                }
                                free(suggestion);
                                suggestion = NULL;
                        }
                }
                else if (c == '\n') // Enter
                {
                        buffer[length] = '\0';
                        printf("\n");
                        break;
                }
                else if (c == 27) // Escape sequences (arrows)
                {
                        char seq[3];
                        seq[0] = getchar();
                        seq[1] = getchar();
                        seq[2] = 0;

                        if (seq[0] == '[')
                        {
                                if (seq[1] == 'C') // Right arrow
                                {
                                        if (cursor < length)
                                                cursor++;
                                }
                                else if (seq[1] == 'D') // Left arrow
                                {
                                        if (cursor > 0)
                                                cursor--;
                                }
                        }
                        // Ignore other sequences
                }
                else if (c >= 32 && c <= 126) // Printable chars
                {
                        if (length + 1 >= bufsize)
                        {
                                bufsize *= 2;
                                char *newbuf = realloc(buffer, bufsize);
                                if (!newbuf)
                                        continue;
                                buffer = newbuf;
                        }

                        memmove(&buffer[cursor + 1], &buffer[cursor], length - cursor);
                        buffer[cursor] = c;
                        length++;
                        cursor++;
                        buffer[length] = '\0';
                }

                // Find new suggestion
                if (suggestion)
                {
                        free(suggestion);
                        suggestion = NULL;
                }

                if (length > 0)
                        suggestion = tab_autocomplete(buffer);

                print_input_with_suggestion(buffer, length, cursor, suggestion);
        }

        disable_raw_mode();
        if (suggestion)
                free(suggestion);
        return buffer;
}

char **split_line(char *line)
{
        int bufsize = token_size, position = 0;
        char **tokens = malloc(bufsize * sizeof(char *));
        char *token;

        if (!tokens)
        {
                fprintf(stderr, "Allocation error\n");
                exit(EXIT_FAILURE);
        }

        token = strtok(line, token_delimiter);
        while (token != NULL)
        {
                tokens[position++] = token;
                if (position >= bufsize)
                {
                        bufsize += token_size;
                        tokens = realloc(tokens, bufsize * sizeof(char *));
                        if (!tokens)
                        {
                                fprintf(stderr, "Allocation error\n");
                                exit(EXIT_FAILURE);
                        }
                }
                token = strtok(NULL, token_delimiter);
        }
        tokens[position] = NULL;
        return tokens;
}

int shell_cd(char **args)
{
        if (args[1] == NULL)
                fprintf(stderr, "MOshell: expected argument to \"cd\"\n");
        else
        {
                if (chdir(args[1]) != 0)
                        perror("MOshell");
        }
        return 1;
}

int shell_help(char **args)
{
        (void)args;
        printf("MOshell: The Minimalistic OS Shell\n");
        printf("Type program names and arguments, then press enter.\n");
        printf("Built-in commands:\n");

        for (int i = 0; i < num_builtins(); i++)
                printf("  %s\n", builtin_str[i]);

        printf("Use the man command for information on other programs.\n");
        return 1;
}

int shell_pwd(char **args)
{
        char cwd[1024];

        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
                printf("%s\n", cwd);
        }
        else
        {
                perror("getcwd() error");
                return 0;
        }

        return 1;
}

int shell_launch(char **args)
{
        pid_t pid, wpid;
        int status;

        pid = fork();
        if (pid == 0)
        {
                // Child process
                if (execvp(args[0], args) == -1)
                {
                        perror("MOshell");
                }
                exit(EXIT_FAILURE);
        }
        else if (pid < 0)
        {
                perror("MOshell");
        }
        else
        {
                // Parent process
                do
                {
                        wpid = waitpid(pid, &status, WUNTRACED);
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }

        return 1;
}

int execute(char **args)
{
        if (args[0] == NULL)
                return 1;

        for (int i = 0; i < num_builtins(); i++)
        {
                if (strcmp(args[0], builtin_str[i]) == 0)
                        return (*builtin_func[i])(args);
        }

        return shell_launch(args);
}

void sigint_handler(int sig)
{
        (void)sig;
        printf("\nUse 'exit' command or Ctrl+D to quit the shell.\n> ");
        fflush(stdout);
}

int main(int argc, char **argv)
{
        (void)argc;
        (void)argv;

        signal(SIGINT, handle_signals);

        while (1)
        {
                char cwd[1024];
                if (getcwd(cwd, sizeof(cwd)) != NULL)
                {
                        char *prompt_path = shorten_part(cwd);
                        printf("\033[1;34m%s\033[0m $ ", prompt_path);
                }
                else
                {
                        printf("$ ");
                }

                char *line = read_command();

                if (line == NULL)
                {
                        printf("\n");
                        break; // EOF (Ctrl+D)
                }

                // Cache history
                if (strlen(line) > 0)
                        history_cache(line);

                char **args = split_line(line);

                int status = execute(args);

                free(args);
                free(line);

                if (status == 0)
                        break;
        }

        return EXIT_SUCCESS;
}
