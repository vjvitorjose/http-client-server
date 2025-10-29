#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 4096

void parse_url(char *url, char *host, int *port, char *path) {
    char *url_no_http = url + 7;
    char *path_start = strchr(url_no_http, '/');
    
    if (path_start == NULL) {
        strcpy(path, "/");
        strcpy(host, url_no_http);
    } else {
        strcpy(path, path_start);
        int host_len = path_start - url_no_http;
        strncpy(host, url_no_http, host_len);
        host[host_len] = '\0';
    }

    char *port_ptr = strchr(host, ':');
    if (port_ptr != NULL) {
        *port = atoi(port_ptr + 1);
        *port_ptr = '\0';
    } else {
        *port = 80;
    }
}

char *get_filename_from_path(char *path) {
    char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) return path;
    if (last_slash[1] == '\0') return NULL;
    return last_slash + 1;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <URL>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s http://www.exemplo.com/arquivo.txt\n", argv[0]);
        exit(1);
    }

    char *full_url = argv[1];
    if (strncmp(full_url, "http://", 7) != 0) {
        fprintf(stderr, "Erro: A URL deve começar com \"http://\"\n");
        exit(1);
    }

    char host[256];
    char path[1024];
    int port;

    parse_url(full_url, host, &port, path);

    char *filename_from_url = get_filename_from_path(path);
    char output_filename[256];
    char request_path[1024];
    
    int is_directory_request = (filename_from_url == NULL || strlen(filename_from_url) == 0);
    int trying_index = 0;

    if (is_directory_request) {
        strcpy(output_filename, "index.html");
        strcpy(request_path, path);
        if (request_path[strlen(request_path) - 1] != '/') {
            strcat(request_path, "/");
        }
        strcat(request_path, "index.html");
        printf("Pasta detectada. Tentando baixar: %s\n", request_path);
        trying_index = 1;
    } else {
        strcpy(output_filename, filename_from_url);
        strcpy(request_path, path);
    }

    struct hostent *server;
    struct sockaddr_in serv_addr;
    server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "Erro: Host não encontrado (%s)\n", host);
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

RequestAttempt:
    printf("Host: %s\nPorta: %d\nCaminho da Requisição: %s\nSalvando como: %s\n\n",
           host, port, request_path, output_filename);

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao abrir socket");
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Erro ao conectar");
        close(sockfd);
        exit(1);
    }

    char request[2048];
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", 
            request_path, host);

    if (send(sockfd, request, strlen(request), 0) < 0) {
        perror("Erro ao enviar requisição");
        close(sockfd);
        exit(1);
    }

    FILE *fp = fopen(output_filename, "wb");
    if (fp == NULL) {
        perror("Erro ao criar arquivo de saída");
        close(sockfd);
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    int bytes_received;
    int first_chunk = 1;
    char *body_start = NULL;

    while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        if (first_chunk) {
            buffer[bytes_received] = '\0';
            int http_status = 0;
            if (strncmp(buffer, "HTTP/", 5) == 0) {
                sscanf(buffer, "HTTP/1.%*d %d", &http_status);
            }

            if (http_status < 200 || http_status >= 300) {
                fprintf(stderr, "Erro HTTP: Recebido status %d\n", http_status);

                if (http_status == 404 && trying_index) {
                    fprintf(stderr, "index.html não encontrado (404). Tentando listagem de diretório...\n\n");
                    
                    fclose(fp);
                    remove(output_filename);
                    close(sockfd);
                    
                    strcpy(output_filename, "listing.html");
                    strcpy(request_path, path);
                    trying_index = 0; 
                    
                    goto RequestAttempt;
                }

                fprintf(stderr, "Decisão: O recurso solicitado (%s) não foi encontrado.\n", request_path);
                fclose(fp);
                remove(output_filename);
                close(sockfd);
                exit(1);
            }

            body_start = strstr(buffer, "\r\n\r\n");
            if (body_start != NULL) {
                body_start += 4;
                int header_len = body_start - buffer;
                int body_len_in_chunk = bytes_received - header_len;
                fwrite(body_start, 1, body_len_in_chunk, fp);
            }
            first_chunk = 0;
        } else {
            fwrite(buffer, 1, bytes_received, fp);
        }
    }

    if (bytes_received < 0) {
        perror("Erro ao receber dados");
    }

    printf("Download concluído: %s\n", output_filename);

    fclose(fp);
    close(sockfd);

    return 0;
}