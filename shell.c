//Inclusão de bibliotecas
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

//Implementação de constantes, variáveis e estruturas
#define MAX_HISTORICO 100
#define MAX_TAMANHO_CMD 1024
#define MAX_ARGS 100

char *historico[MAX_HISTORICO];
int contador_historico = 0;

//Implementação da função de salvar no histórico
void salvar_no_historico(char *cmd) {
    if (contador_historico < MAX_HISTORICO) {
        historico[contador_historico++] = strdup(cmd);
    } else {
        free(historico[0]);
        for (int i = 1; i < MAX_HISTORICO; i++) {
            historico[i - 1] = historico[i];
        }
        historico[MAX_HISTORICO - 1] = strdup(cmd);
    }
}

//Implementação da função de printar o historico
void printar_historico() {
    for (int i = 0; i < contador_historico; i++) {
        printf("%d: %s\n", i + 1, historico[i]);
    }
}

//Implementação da função de execução de comandos
void executar_comando(char **args, int background) {
    pid_t pid = fork();
    if (pid == 0) {
        if (execvp(args[0], args) < 0) {
            perror("Falha na execução");
            exit(1);
        }
    } else if (pid > 0) {
        if (!background) {
            waitpid(pid, NULL, 0);
        }
    } else {
        perror("Falha no fork");
    }
}

//Implementação da função que trata redirecionamentos utilizando 'dup2' e 'open'.
void trata_redirecionamento(char **args) {
    int fd;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            args[i] = NULL;
            fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Falha na abertura");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        } else if (strcmp(args[i], "<") == 0) {
            args[i] = NULL;
            fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) {
                perror("Falha na abertura");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
}

//Implementação da função que lida com a criação de pipes e gerencia a execução de comandos conectados por pipes.
void trata_pipes(char *cmd) {
    int pipefd[2];
    pid_t pid;
    char *left_cmd[MAX_ARGS], *right_cmd[MAX_ARGS];
    char *left = strtok(cmd, "|");
    char *right = strtok(NULL, "|");

    if (right == NULL) {
        fprintf(stderr, "Erro: Comando de pipe inválido\n");
        return;
    }

    char *token = strtok(left, " ");
    int i = 0;
    while (token != NULL) {
        left_cmd[i++] = token;
        token = strtok(NULL, " ");
    }
    left_cmd[i] = NULL;

    token = strtok(right, " ");
    i = 0;
    while (token != NULL) {
        right_cmd[i++] = token;
        token = strtok(NULL, " ");
    }
    right_cmd[i] = NULL;

    if (pipe(pipefd) < 0) {
        perror("Falha no pipe");
        exit(1);
    }

    pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        trata_redirecionamento(left_cmd);
        if (execvp(left_cmd[0], left_cmd) < 0) {
            perror("Falha na execução");
            exit(1);
        }
    } else if (pid > 0) {
        close(pipefd[1]);
        pid = fork();
        if (pid == 0) {
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            trata_redirecionamento(right_cmd);
            if (execvp(right_cmd[0], right_cmd) < 0) {
                perror("Falha na execução");
                exit(1);
            }
        } else if (pid > 0) {
            close(pipefd[0]);
            waitpid(pid, NULL, 0);
        } else {
            perror("Falha no fork");
        }
        waitpid(pid, NULL, 0);
    } else {
        perror("Falha no fork");
    }
}

// TESTAR
void mudanca_diretorio(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: expected argument\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd failed");
        }
    }
}

// TESTAR
void mostrar_ajuda() {
    printf("Shell Commands:\n");
    printf("cd <dir> - Change directory\n");
    printf("historico - Display command historico\n");
    printf("exit - Exit the shell\n");
    printf("help - Display this help message\n");
}

//Implementação da função para analisar a linha de comando
void analise_comando(char *cmd) {
    char *args[MAX_ARGS];
    char *token = strtok(cmd, " ");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    int background = 0;
    if (i > 0 && strcmp(args[i - 1], "&") == 0) {
        background = 1;
        args[i - 1] = NULL;
    }

    if (args[0] == NULL) {
        return;
    }

    if (strcmp(args[0], "cd") == 0) {
        mudanca_diretorio(args);
    } else if (strcmp(args[0], "historico") == 0) {
        printar_historico();
    } else if (strcmp(args[0], "sair") == 0) {
        exit(0);
    } else if (strcmp(args[0], "ajuda") == 0) {
        mostrar_ajuda();
    } else if (strchr(cmd, '|') != NULL) {
        trata_pipes(cmd);
    } else {
        pid_t pid = fork();
        if (pid == 0) {
            trata_redirecionamento(args);
            if (execvp(args[0], args) < 0) {
                perror("Falha na execução");
                exit(1);
            }
        } else if (pid > 0) {
            if (!background) {
                waitpid(pid, NULL, 0);
            }
        } else {
            perror("Falha no fork");
        }
    }
}

//Implementação da função main e o loop de leitura de comandos
int main() {
    char cmd[MAX_TAMANHO_CMD];
    char *path = getenv("PATH");
    if (!path) {
        fprintf(stderr, "Erro: Variável de ambiente 'PATH' não foi setada\n");
        exit(1);
    }

    printf("Bem-vindo a Shell! Digite 'ajuda' para ver os comandos válidos.\n");

    while (1) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s> ", cwd);
        } else {
            perror("Falha no 'getcwd'");
            exit(1);
        }

        if (fgets(cmd, MAX_CMD_LENGTH, stdin) == NULL) {
            perror("Falha no 'fgets'");
            continue;
        }
        cmd[strcspn(cmd, "\n")] = 0; // Remove caractere de nova linha
        if (strlen(cmd) == 0) {
            continue;
        }
        salvar_no_historico(cmd);
        analise_comando(cmd);
    }

    for (int i = 0; i < contador_historico; i++) {
        free(historico[i]);
    }

    return 0;
}