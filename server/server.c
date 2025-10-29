#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <strings.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_PATH 1024

char *root_dir;

void send_header(int client_socket, const char *status, const char *content_type, long content_length) {
    char header[BUFFER_SIZE];
    
    sprintf(header, "HTTP/1.1 %s\r\n", status);
    
    if (content_type) {
        sprintf(header + strlen(header), "Content-Type: %s\r\n", content_type);
    }
    
    if (content_length >= 0) {
        sprintf(header + strlen(header), "Content-Length: %ld\r\n", content_length);
    }
    
    sprintf(header + strlen(header), "Connection: close\r\n\r\n");
    
    send(client_socket, header, strlen(header), 0);
}

void send_error(int client_socket, const char *status, const char *message) {
    char body[BUFFER_SIZE];
    sprintf(body, "<html><head><title>%s</title></head><body><h1>%s</h1><p>%s</p></body></html>",
            status, status, message);
    
    send_header(client_socket, status, "text/html", strlen(body));
    send(client_socket, body, strlen(body), 0);
}

const char *get_mime_type(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "application/octet-stream";
    if (strcasecmp(dot, ".html") == 0) return "text/html";
    if (strcasecmp(dot, ".css") == 0) return "text/css";
    if (strcasecmp(dot, ".js") == 0) return "application/javascript";
    if (strcasecmp(dot, ".jpg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(dot, ".png") == 0) return "image/png";
    if (strcasecmp(dot, ".txt") == 0) return "text/plain";
    return "application/octet-stream";
}

void serve_file(int client_socket, const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        if (errno == EACCES) {
            send_error(client_socket, "403 Forbidden", "Você não tem permissão para acessar este arquivo.");
        } else {
            send_error(client_socket, "404 Not Found", "Arquivo não encontrado.");
        }
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    const char *mime = get_mime_type(filepath);
    send_header(client_socket, "200 OK", mime, file_size);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) < 0) {
            break;
        }
    }

    fclose(file);
}

void serve_directory_listing(int client_socket, const char *dir_path, const char *request_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        send_error(client_socket, "500 Internal Server Error", "Não foi possível ler o diretório.");
        return;
    }

    send_header(client_socket, "200 OK", "text/html; charset=UTF-8", -1);

    char buffer[BUFFER_SIZE];
    
    sprintf(buffer, "<html><head><title>Índice de %s</title></head>"
                    "<body><h1>Índice de %s</h1><hr><ul>",
            request_path, request_path);
    send(client_socket, buffer, strlen(buffer), 0);

    if (strcmp(request_path, "/") != 0) {
        sprintf(buffer, "<li><a href=\"../\">../</a></li>\n");
        send(client_socket, buffer, strlen(buffer), 0);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char entry_full_path[MAX_PATH];
        snprintf(entry_full_path, MAX_PATH, "%s/%s", dir_path, entry->d_name);
        
        struct stat entry_stat;
        const char *suffix = "";
        if (stat(entry_full_path, &entry_stat) == 0) {
            if (S_ISDIR(entry_stat.st_mode)) {
                suffix = "/";
            }
        }

        sprintf(buffer, "<li><a href=\"%s%s\">%s%s</a></li>\n",
                entry->d_name, suffix, entry->d_name, suffix);
        send(client_socket, buffer, strlen(buffer), 0);
    }

    sprintf(buffer, "</ul><hr></body></html>");
    send(client_socket, buffer, strlen(buffer), 0);

    closedir(dir);
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }

    char method[16], path[MAX_PATH], version[16];
    if (sscanf(buffer, "%15s %1023s %15s", method, path, version) != 3) {
        send_error(client_socket, "400 Bad Request", "Requisição mal formatada.");
        close(client_socket);
        return;
    }

    if (strcmp(method, "GET") != 0) {
        send_error(client_socket, "405 Method Not Allowed", "Este servidor aceita apenas requisições GET.");
        close(client_socket);
        return;
    }

    if (strstr(path, "..") != NULL) {
        send_error(client_socket, "403 Forbidden", "Acesso proibido: tentativa de 'directory traversal'.");
        close(client_socket);
        return;
    }

    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s%s", root_dir, path);

    struct stat path_stat;
    if (stat(full_path, &path_stat) != 0) {
        send_error(client_socket, "404 Not Found", "O recurso solicitado não foi encontrado.");
        close(client_socket);
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        
        if (path[strlen(path) - 1] != '/') {
            char new_location[MAX_PATH + 1];
            snprintf(new_location, MAX_PATH + 1, "%s/", path);
            
            char header[BUFFER_SIZE];
            sprintf(header, "HTTP/1.1 301 Moved Permanently\r\nLocation: %s\r\nConnection: close\r\n\r\n", new_location);
            send(client_socket, header, strlen(header), 0);
            
            close(client_socket);
            return;
        }

        char index_path[MAX_PATH];
        snprintf(index_path, MAX_PATH, "%s/index.html", full_path);

        if (access(index_path, F_OK) == 0) {
            serve_file(client_socket, index_path);
        } else {
            serve_directory_listing(client_socket, full_path, path);
        }

    } else if (S_ISREG(path_stat.st_mode)) {
        serve_file(client_socket, full_path);
    } else {
        send_error(client_socket, "403 Forbidden", "O tipo de recurso solicitado não é suportado.");
    }

    close(client_socket);
}

void handle_sigchld(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <diretorio_raiz>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s /home/flavio/meusite\n", argv[0]);
        exit(1);
    }
    
    root_dir = argv[1];
    
    struct stat dir_stat;
    if (stat(root_dir, &dir_stat) != 0 || !S_ISDIR(dir_stat.st_mode)) {
        perror("Diretório raiz inválido");
        exit(1);
    }

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    signal(SIGCHLD, handle_sigchld);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Erro ao criar socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro de bind");
        exit(1);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Erro de listen");
        exit(1);
    }

    printf("Servidor HTTP rodando na porta %d, servindo o diretório: %s\n", PORT, root_dir);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Erro de accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Erro no fork");
            close(client_socket);
        } else if (pid == 0) {
            close(server_socket);
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            printf("Conexão de %s... ", client_ip);
            fflush(stdout);

            handle_client(client_socket);
            
            printf("Conexão com %s fechada.\n", client_ip);
            exit(0);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}