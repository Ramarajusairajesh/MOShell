#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#define buffer_size 1024
#define token_size 65
#define token_delimiter "\t\r\n\a"

int shell_cd(char **args);
int shell_help(char **args);
int shell_exit(char **args);
int shell_launch(char **args);
char *buildin_str[] = {"cd", "help", "exit"};

int num_buildins() { return sizeof(buildin_str) / sizeof(char *); }
char *read_command(void)
{
        int bufsize = buffer_size;
        int c, position = 0;
        char *buffer = malloc(sizeof(char) * bufsize);
        if (!buffer)
        {
                fprintf(stderr, "Failed to allocate memory");
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
                                fprintf(stderr, "Failed to reallocate memory");
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
                fprintf(stderr, "Failed to allocate memory");
                exit(EXIT_FAILURE);
        }
        token = strtok(line, token_delimiter); // should return entire string ?
        while (token != NULL)
        {
                tokens[position] = token;
                position++;
                if (position >= bufsize)
                {
                        bufsize += token_size;
                        tokens = realloc(tokens, bufsize);
                        if (!tokens)
                        {
                                fprintf(stderr, "Failed to reallocated memory");
                                exit(EXIT_FAILURE);
                        }
                }
                token = strtok(NULL, token_delimiter); // Why? Does this just return NULL?
        }
        tokens[position] = NULL;
        return tokens;
}

int (*buildin_fun[])(char **) = {&shell_cd, &shell_help, &shell_exit};
int execute_commad(char **args)
{
        int i;
        if (args[0] == NULL)
        {
                return 1;
        }
        for (i = 0; i < num_buildins(); i++)
        {
                if (strcmp(args[0], buildin_str[i]) == 0)
                {
                        return (*buildin_fun[i])(args);
                }
        }

        return shell_launch(args);
}

int shell_launch(char **args)
{
        pid_t pid, wpid;
        int status;
        pid = fork();
        if (pid == 0) // child process running and healthy
        {
                if (execvp(args[0], args) == -1)
                {
                        perror("Error with shell");
                }
                exit(EXIT_FAILURE);
        }
        else if (pid < 0)
        {
                perror("Error with shell");
        }
        else
        {
                do
                {
                        wpid = waitpid(pid, &status, WUNTRACED);
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }

        return 1;
}
// cd ,help and exit functions
int shell_cd(char **args)
{
        if (args[1] == NULL)
        {
                fprintf(stderr, "Specify a direcotry");
        }
        else
        {
                if (chdir(args[1]) != 0)
                {
                        perror("Error changing directory");
                }
        }
        return 1;
}

int shell_help(char **args)
{
        int i;
        printf("MOShell , run  like a normal shell\n");
        printf("The following are build in \n");
        for (i = 0; i < num_buildins(); i++)
        {
                printf("%s \n", buildin_str[i]);
        }
        return 1; // status value
}

int shell_exit(char **args) { return 0; }

void shell_loop(void)
{
        int status;
        char *line, **args;
        do
        {
                printf(">");
                line   = read_command();
                args   = split_line(line);
                status = execute_commad(args);
                free(line);
                free(args);

        } while (status);
}

int main()
{
        shell_loop();
        return EXIT_SUCCESS;
}
