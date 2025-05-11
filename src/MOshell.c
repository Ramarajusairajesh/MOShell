#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define buffer_size 1024
#define token_size 64
#define token_delimiter " \t\r\n\a"

int shell_cd(char **args);
int shell_help(char **args);
int shell_exit(char **args);
int shell_launch(char **args);

// Built-in commands
char *builtin_str[] = {"cd", "help", "exit"};

int (*builtin_func[])(char **) = {&shell_cd, &shell_help, &shell_exit};

int num_builtins() { return sizeof(builtin_str) / sizeof(char *); }

char *shorten_part(char *path)
{
        static char shortened[1024];
        char *user = getenv("USER");
        if (!user)
                user = "user"; // Fallback if USER is not set

        char home_prefix[256];
        snprintf(home_prefix, sizeof(home_prefix), "/home/%s", user);
        size_t home_len = strlen(home_prefix);

        // Handle replacement of /home/USER with ~
        const char *relative_path = path;
        if (strncmp(path, home_prefix, home_len) == 0)
        {
                relative_path = path + home_len;
        }

        // If the remaining path is short, return with ~ if applicable
        if (strlen(path) <= 30)
        {
                if (relative_path != path)
                {
                        snprintf(shortened, sizeof(shortened), "~%s", relative_path);
                }
                else
                {
                        snprintf(shortened, sizeof(shortened), "%s", path);
                }
                return shortened;
        }

        // Begin abbreviation
        char temp[1024];
        snprintf(temp, sizeof(temp), "%s", relative_path);

        char *token;
        char *last = NULL;
        char *components[64];
        int count = 0;

        token = strtok(temp, "/");
        while (token != NULL)
        {
                components[count++] = token;
                token               = strtok(NULL, "/");
        }

        shortened[0] = '\0';
        if (relative_path != path)
                strcat(shortened, "~");
        else
                strcat(shortened, "");

        for (int i = 0; i < count; i++)
        {
                strcat(shortened, "/");
                if (i == count - 1)
                {
                        strcat(shortened, components[i]); // last full
                }
                else
                {
                        strncat(shortened, components[i], 1); // just first char
                }
        }

        return shortened;
}

char *read_command(void)
{
        int bufsize  = buffer_size;
        int position = 0;
        int c;
        char *buffer = malloc(sizeof(char) * bufsize);

        if (!buffer)
        {
                fprintf(stderr, "Failed to allocate memory\n");
                exit(EXIT_FAILURE);
        }

        while (1)
        {
                c = getchar();

                if (c == EOF || c == '\n')
                {
                        buffer[position] = '\0';
                        return buffer;
                }
                else
                {
                        buffer[position] = c;
                }

                position++;

                if (position >= bufsize)
                {
                        bufsize += buffer_size;
                        buffer = realloc(buffer, bufsize);
                        if (!buffer)
                        {
                                fprintf(stderr, "Failed to reallocate memory\n");
                                exit(EXIT_FAILURE);
                        }
                }
        }
}

char **split_line(char *line)
{
        int bufsize = token_size, position = 0;
        char **tokens = malloc(sizeof(char *) * bufsize);
        char *token;

        if (!tokens)
        {
                fprintf(stderr, "Failed to allocate memory\n");
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
                                fprintf(stderr, "Failed to reallocate memory\n");
                                exit(EXIT_FAILURE);
                        }
                }

                token = strtok(NULL, token_delimiter);
        }
        tokens[position] = NULL;

        return tokens;
}

int execute_command(char **args)
{
        if (args[0] == NULL)
        {
                return 1;
        }

        for (int i = 0; i < num_builtins(); i++)
        {
                if (strcmp(args[0], builtin_str[i]) == 0)
                {
                        return (*builtin_func[i])(args);
                }
        }

        return shell_launch(args);
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
                        perror("Error executing command");
                }
                exit(EXIT_FAILURE);
        }
        else if (pid < 0)
        {
                // Error forking
                perror("Forking error");
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

int shell_cd(char **args)
{
        if (args[1] == NULL)
        {
                fprintf(stderr, "Expected argument to \"cd\"\n");
        }
        else
        {
                char *path = args[1];

                if (path[0] == '~')
                {
                        char *user = getenv("USER");
                        if (!user)
                        {
                                fprintf(stderr, "USER environment variable not set\n");
                                return 1;
                        }

                        char expanded_path[1024];
                        snprintf(expanded_path, sizeof(expanded_path), "/home/%s%s", user,
                                 path + 1);

                        if (chdir(expanded_path) != 0)
                        {
                                perror("Error changing directory");
                        }
                }
                else
                {
                        if (chdir(path) != 0)
                        {
                                perror("Error changing directory");
                        }
                }
        }
        return 1;
}

int shell_help(char **args)
{
        printf("MOShell: A basic shell implementation\n");
        printf("Built-in commands:\n");

        for (int i = 0; i < num_builtins(); i++)
        {
                printf("  %s\n", builtin_str[i]);
        }

        printf("Use the man command for info on other programs.\n");
        return 1;
}

int shell_exit(char **args) { return 0; }

void shell_loop(void)
{
        int status;
        char *line;
        char **args;
        char cwd[1024];
        char *current_path;

        do
        {
                if (getcwd(cwd, sizeof(cwd)) != NULL)
                {
                        current_path = shorten_part(cwd);
                        printf("%s~> ", current_path);
                }
                else
                {
                        perror("getcwd error");
                        exit(EXIT_FAILURE);
                }

                line   = read_command();
                args   = split_line(line);
                status = execute_command(args);

                free(line);
                free(args);
        } while (status);
}

int main()
{
        shell_loop();
        return EXIT_SUCCESS;
}
