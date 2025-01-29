#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BUFSZ 500

void usageExit(int argc, char **argv) {
    printf("Server usage: %s <v4|v6> <server port>\n", argv[0]);
    printf("Ex: %s v4 51511\n", argv[0]);
    printf("Ex: %s v6 51511\n", argv[0]);
    exit(EXIT_FAILURE);
}

void msgExit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int serverAddrInit(const char *proto, const char *portstr, struct sockaddr_storage *storage) {
    // AF_INET = IPv4, AF_INET6 = IPv6
    if(proto == NULL || portstr == NULL) return -1;

    uint16_t port = (uint16_t)atoi(portstr); // unsigned short, 16 bits
    if(port == 0) return -1;
    port = htons(port);

    memset(storage, 0, sizeof(*storage));
    if(strcmp(proto, "v4") == 0) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_addr.s_addr = INADDR_ANY; // qualquer endereço na interface de rede
        addr4->sin_port = port;
        return 0;
    } else if(strcmp(proto, "v6") == 0) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = in6addr_any;
        addr6->sin6_port = port;
        return 0;
    }
    else return -1;
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
        if(!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr, INET6_ADDRSTRLEN + 1))
            msgExit("ntop ipv6 failed");
        // network to host short
        port = ntohs(addr6->sin6_port);
    } else msgExit("addrtostr() failed, unknown protocol");

    if(str) snprintf(str, strsize, "IPv%d %s %hu", version, addrstr, port);
}

int main(int argc, char **argv) {
    if(argc < 3 || argc > 3) usageExit(argc, argv);

    struct sockaddr_storage storage;
    if (serverAddrInit(argv[1], argv[2], &storage) != 0) usageExit(argc, argv);

    int sock;
    //IPv4 ou IPv6, TCP, IP
    sock = socket(storage.ss_family, SOCK_STREAM, 0);
    if(sock < 0) msgExit("socket() failed");

    int enable = 1;
    // Reusar porta sem atraso
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0)
        msgExit("setsockopt() failed");

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    // bind
    if(bind(sock, addr, sizeof(storage)) != 0) msgExit("bind() failed");

    // listen, 10 = número máximo de conexões pendentes para tratamento
    if(listen(sock, 10) != 0) msgExit("listen() failed");

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("[log] Bound to %s, waiting connections\n", addrstr);

    // mesmo raciocínio do array de extensões válidas do cliente
    char *valid_extensions[] = {"cpp", "txt", "c", "py", "tex", "java"};

    while(1) {
        struct sockaddr_storage clientStorage;
        struct sockaddr *clientSockaddr = (struct sockaddr *)(&clientStorage);
        socklen_t clientAddrLen = sizeof(clientStorage);

        // accept, Socket que conversa com cliente
        printf("[log] Waiting for new client\n");
        int clientSocket = accept(sock, clientSockaddr, &clientAddrLen);
        if(clientSocket == -1) {
            close(sock);
            msgExit("accept() failed");
        }

        char clientAddrStr[BUFSZ];
        addrtostr(clientSockaddr, clientAddrStr, BUFSZ);
        printf("[log] connected from %s\n", clientAddrStr);

        // string na stack do programa para receber os caracteres do cliente
        char buffer[BUFSZ];
        while(1) {
            memset(buffer, 0, BUFSZ);
            size_t bytesReceived = recv(clientSocket, buffer, BUFSZ, 0);
            int size = strlen(buffer);
            
            if(bytesReceived == 0) break; // sai do loop do cliente atual para esperar um novo caso nada é enviado (conexão fechada)
            if(bytesReceived == -1) { // erro no recv em si
                close(clientSocket);
                close(sock);
                msgExit("recv() failed");
            }
            if(strcmp(buffer, "exit\\end") == 0) { // cliente solicita desconexão e ambas as conexões são fechadas
                // printa "connection closed" na saída padrão e no buffer para enviar para o cliente
                printf("connection closed\n");
                sprintf(buffer, "connection closed");
                bytesReceived = send(clientSocket, buffer, strlen(buffer)+1, 0);
                if(bytesReceived != strlen(buffer)+1) msgExit("send() failed");
                // encerramento das conexões
                close(clientSocket);
                close(sock);
                exit(1);
            }
            else if(strcmp(buffer, "invalid command\\end") == 0) {
                // comando inválido: envia "disconnect" para o cliente, que trata isso e apenas o cliente é desconectado
                sprintf(buffer, "disconnect");
                bytesReceived = send(clientSocket, buffer, strlen(buffer)+1, 0);
                if(bytesReceived != strlen(buffer)+1) msgExit("send() failed");
                break; // sai do loop interno para receber outro cliente
            }
            else if(size >= 4) { // size >= 4 pois strlen("\end") = 4
                // pega somente o nome do arquivo, sem o .extensão
                char *aux = NULL;
                aux = strtok(buffer, ".");
                char *file_name = malloc(BUFSZ * sizeof(char));
                strcpy(file_name, aux);
                
                // pega o resto da string recebida e a copia para contents
                char *rest = strtok(NULL, "");
                char *contents = malloc(BUFSZ * sizeof(char));
                strcpy(contents, rest);
                char dot = '.';

                // faz o parse da extensão e do conteúdo do arquivo
                int extension_name_size = 0;
                for(int i = 0; i < 6; i++) {
                    extension_name_size = strlen(valid_extensions[i]);
                    // compara os primeiros extension_name_size caracteres do resto da mensagem com a extensão
                    //  da atual iteração do array. Se der match, é a extensão correta
                    if(strncmp(rest, valid_extensions[i], extension_name_size) == 0) {
                        strncat(file_name, &dot, 1);
                        strcat(file_name, valid_extensions[i]);
                        // atualiza a posição do ponteiro para pular a extensão e 
                        // o coloca no início do conteúdo de texto
                        contents += extension_name_size;
                        // remove o "\end" ao final, para sobrar somente o conteúdo de texto
                        contents[strlen(contents)-4] = '\0';
                        break;
                    }
                }

                // se caso a mensagem fora enviada sem o "\end", printar error receiving file
                // o cliente atual não é desconectado por conta disso e o servidor aguarda
                // uma nova mensagem
                const char *last_four = &buffer[size-4];
                if(strcmp(last_four, "\\end") != 0) {
                    memset(buffer, 0, BUFSZ);
                    sprintf(buffer, "error receiving file %s\n\\end", file_name);
                    bytesReceived = send(clientSocket, buffer, strlen(buffer)+1, 0);
                    if(bytesReceived != strlen(buffer)+1) msgExit("send() failed");

                    free(file_name);
                    continue;
                }

                FILE *fp;
                if(access(file_name, F_OK) == 0) { // se o arquivo já existe no diretório, reescreva-o
                    fp = fopen(file_name, "w");
                    int i = 0;
                    while (contents[i] != '\0') {
                        // printa char a achar no arquivo
                        fputc(contents[i], fp);
                        i++;
                    }
                    fclose(fp);

                    memset(buffer, 0, BUFSZ);
                    sprintf(buffer, "file %s overwritten\n\\end", file_name); // msg de confirmação
                    bytesReceived = send(clientSocket, buffer, strlen(buffer)+1, 0);
                    if(bytesReceived != strlen(buffer)+1) msgExit("send() failed");
                }
                else { // caso não esteja no diretório, crie o arquivo e escreva contents nele
                    fp = fopen(file_name, "w");
                    int i = 0;
                    while (contents[i] != '\0') {
                        fputc(contents[i], fp);
                        i++;
                    }
                    fclose(fp);

                    memset(buffer, 0, BUFSZ);
                    sprintf(buffer, "file %s received\n\\end", file_name); // msg de confirmação
                    bytesReceived = send(clientSocket, buffer, strlen(buffer)+1, 0);
                    if(bytesReceived != strlen(buffer)+1) msgExit("send() failed");
                }
                free(file_name);
                // libera a memória alocada, ajustando o ponteiro de acordo com o deslocamento feito na linha 178
                free(contents - extension_name_size);
            }
        }
        // fecha a conexão do cliente caso haja um break no loop interno do servidor (comando inválido do cliente)
        close(clientSocket);
    }
    close(sock);
    exit(EXIT_SUCCESS);
}