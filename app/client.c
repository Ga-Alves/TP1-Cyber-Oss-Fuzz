#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BUFSZ 500

void usageExit(int argc, char **argv) {
    printf("Client usage: %s <server IP> <server port>\n", argv[0]);
    printf("Ex: %s 127.0.0.1 51511\n", argv[0]); // IPv4 loopback
    printf("Ex: %s ::1 51511\n", argv[0]); // IPv6 loopback
    exit(EXIT_FAILURE);
}

void msgExit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage) {
    // AF_INET = IPv4, AF_INET6 = IPv6
    if(addrstr == NULL || portstr == NULL) return -1;

    uint16_t port = (uint16_t)atoi(portstr); // unsigned short, 16 bits
    if(port == 0) return -1;

    port = htons(port); // converte para network byte order, host to network short

    struct in_addr inaddr4; // IPv4, 32 bits
    // inet presentation to network
    if(inet_pton(AF_INET, addrstr, &inaddr4)) {
        // converte para sockaddr_in (IPv4) e armazena em storage
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr = inaddr4;
        return 0;
    }

    struct in6_addr inaddr6; // IPv6, 128 bits
    if(inet_pton(AF_INET6, addrstr, &inaddr6)) {
        // converte para sockaddr_in6 (IPv6) e armazena em storage
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
        // addr6->sin6_addr = inaddr6; não funciona, pois inaddr6 é um array de 16 bytes
        // memcpy(destino, origem, tamanho)
        memcpy(&(addr6->sin6_addr), &inaddr6, sizeof(inaddr6));
        return 0;
    }

    return -1;
}

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize) {
    int version;
    char addrstr[INET6_ADDRSTRLEN + 1] = ""; // pode ser IPv4 ou IPv6
    uint16_t port;

    if(addr->sa_family == AF_INET) {
        version = 4;
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        // inet network to presentation
        if(!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr, INET6_ADDRSTRLEN + 1))
            msgExit("ntop ipv4 failed");
        // network to host short
        port = ntohs(addr4->sin_port);
    } else if(addr->sa_family == AF_INET6) {
        version = 6;
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
        // inet network to presentation
        // AF_INET6 or AF_INET?
        if(!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr, INET6_ADDRSTRLEN + 1))
            msgExit("ntop ipv6 failed");
        // network to host short
        port = ntohs(addr6->sin6_port);
    } else msgExit("addrtostr() failed, unknown protocol");

    if(str) snprintf(str, strsize, "IPv%d %s %hu", version, addrstr, port);
}


int main(int argc, char **argv) {
    if(argc < 3 || argc > 3) usageExit(argc, argv);


    // estrutura que armazena endereço ipv4 ou ipv6
    struct sockaddr_storage storage;
    if (addrparse(argv[1], argv[2], &storage) != 0) usageExit(argc, argv);

    int sock;
    //IPv4 ou IPv6, TCP, IP
    sock = socket(storage.ss_family, SOCK_STREAM, 0);
    if(sock < 0) msgExit("socket() failed");

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if(connect(sock, addr, sizeof(storage)) != 0) msgExit("connect() failed");

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("Connected to %s\n", addrstr);

    // inicializa buffer com máximo de 500 bytes
    char buffer[BUFSZ];
    memset(buffer, 0, BUFSZ);

    char *dot = NULL;
    char *extension = NULL;

    // array de extensões válidas para os arquivos
    char *valid_extensions[] = {".txt", ".c", ".cpp", ".py", ".tex", ".java"};
    // string que representa o nome do último arquivo válido selecionado
    char *selected_file = NULL;
    // string que representa a mensagem "<nomearquivo><conteudo>\exit"
    char message[BUFSZ];
    // activeFile indica se há algum arquivo selecionado no momento
    // to send file indica se é para enviar um arquivo selecionado ou não
    int activeFile = 0, toSendFile = 0;

    int count = 0;
    while(1) {
        // lê do teclado e armazena em buffer
        memset(buffer, 0, BUFSZ);
        fgets(buffer, BUFSZ-1, stdin);
        
        // se os primeiros 12 caracteres do buffer forem select file 
        if(strncmp(buffer, "select file ", 12) == 0) {
            // recupera o nome do arquivo
            char *aux = NULL;
            aux = strtok(buffer, " "); // select
            aux = strtok(NULL, " ");   // file
            aux = strtok(NULL, "\n");  // nome do arquivo
            if(aux == NULL || strcmp(buffer, "select file ") == 0) {
                printf("no file selected!\n");
                continue;
            }
            char *file_name = malloc(strlen(aux) * sizeof(char));
            strcpy(file_name, aux);

            // checa se o arquivo existe
            if(access(file_name, F_OK) != 0) {
                printf("%s does not exist\n", file_name);
                free(file_name);
                continue;
            }

            // acessa o campo da extensão do nome do arquivo
            dot = strrchr(file_name, '.');
            if (dot == NULL) {
                printf("%s is not valid!\n", file_name);
                free(file_name);
                continue;
            }
            extension = strdup(dot);

            // checa se é uma extensão válida iterando sobre o array de strings de extensões válidas
            for(int i = 0; i < 6; i++) {
                if(strcmp(extension, valid_extensions[i]) == 0) {
                    printf("%s selected\n", file_name);
                    selected_file = strdup(file_name);
                    activeFile = 1;
                    break;
                }
                if(i == 5) printf("%s not valid!\n", file_name);
            }
            free(file_name);
            continue;
        } 
        else if(strcmp(buffer, "send file\n") == 0) { // caso seja exatamente send file
            // checa se um arquivo foi selecionado para enviar
            if(activeFile == 0) {
                printf("no file selected!\n");
                continue;
            }
            else {
                if(access(selected_file, F_OK) != 0) { // checa se um arquivo não foi deletado entre um select e um send file
                    printf("%s does not exist\n", selected_file);
                    activeFile = 0;
                    // free(selected);
                    continue;
                }
                // lê os conteúdos do arquivo
                FILE *f = fopen(selected_file, "r");
                char content[BUFSZ];
                char buf;

                memset(content, 0, BUFSZ);
                memset(message, 0, BUFSZ);

                // leitura char a char
                while(1) {
                    buf = getc(f);
                    if(buf == EOF) break;
                    char c = (char)buf;
                    strncat(content, &c, 1);
                }
                fclose(f);

                // monta a mensagem e manda para o servidor
                strcat(content, "\\end"); // Comentar essa parte para testar msg error receiving file
                strcpy(message, selected_file);
                // strcat(message, " "); seria muito mais fácil se tivesse esse espaço
                strcat(message, content);
                // marca que pode enviar o arquivo e, como não há continue, segue direto para enviá-lo abaixo
                toSendFile = 1;
            }
        }
        // envia mensagem <nomearquivo><conteudo><\end> para o servidor, count conta os bytes enviados
        if(toSendFile == 1) {
            toSendFile = 0;
            count = send(sock, message, strlen(message)+1, 0);
            if(count != strlen(message)+1) msgExit("send() failed, msg size mismatch");
        }
        // pedido para desconexão
        else if(strncmp(buffer, "exit", 4) == 0) {
            // coloca '\end' no fim da mensagem de exit
            if(strlen(buffer) == 5 && buffer[strlen(buffer) - 1] == '\n') {
                buffer[strlen(buffer) - 1] = '\\';
                strcat(buffer, "end");
            }
            else strcat(buffer, "\\end");
            count = send(sock, buffer, strlen(buffer)+1, 0);
            if(count != strlen(buffer)+1) msgExit("send() failed, msg size mismatch");
        }
        // comando inválido
        else {
            sprintf(buffer, "invalid command\\end");
            count = send(sock, buffer, strlen(buffer)+1, 0);
            if(count != strlen(buffer)+1) msgExit("send() failed, msg size mismatch");
        }

        // recebe mensagem do servidor e coloca em buffer em ordem
        // variavel total é necessaria pois podemos não recebemos tudo de uma vez
        memset(buffer, 0, BUFSZ);
        unsigned total = 0;
        while(1) {
            count = recv(sock, buffer + total, BUFSZ - total, 0);
            const char *last_four = &buffer[strlen(buffer)-4];
                if(strcmp(last_four, "\\end") == 0) { // se os últimos 4 caracteres são "\end", podemos parar de ler
                buffer[strlen(buffer)-4] = '\0';
                break;
            }
            if (count == 0) {
                break;
            }
            total += count;
        }

        // comando inválido/desconhecido
        if(strcmp(buffer, "disconnect") == 0) {
            printf("disconnected due to incorrect command\n");
            memset(buffer, 0, BUFSZ);
            close(sock);
            exit(1);
        }
        // resposta do servidor ao envio de exit pelo cliente
        else if(strcmp(buffer, "connection closed") == 0) {
            printf("connection closed\n");
            memset(buffer, 0, BUFSZ);
            close(sock);
            exit(1);
        }
        // confirmação (ou falha) de arquivo recebido ou sobrescrito
        printf("%s", buffer);
    }

    close(sock);
    exit(EXIT_SUCCESS);
}