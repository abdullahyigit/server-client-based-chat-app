#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define MAX_ROOMS 10
#define BUFFER_SZ 2048
#define NAME_SIZE 32

int listenfd;
SSL_CTX *ctx;

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    SSL *ssl;
    char name[32];
    int room;
} client_t;

typedef struct {
    char name[NAME_SIZE];
    client_t *clients[MAX_CLIENTS];
} room_t;

room_t rooms[MAX_ROOMS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static int uid = 10;

void init_rooms() {
    for (int i = 0; i < MAX_ROOMS; ++i) {
        sprintf(rooms[i].name, "Room %d", i + 1);
        memset(rooms[i].clients, 0, sizeof(rooms[i].clients));
    }
}

void sigint_handler(int sig) {
    printf("\nShutting down server...\n");

    close(listenfd);
    SSL_CTX_free(ctx);
    exit(0);
}

SSL_CTX* InitServerCTX(void) {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

void LoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile) {

    if (SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        abort();
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }
}

void send_message_to_room(char *s, int uid, int room) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (rooms[room].clients[i]) {
            if (rooms[room].clients[i]->uid != uid) {
                SSL_write(rooms[room].clients[i]->ssl, s, strlen(s));
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void close_client_connection(client_t *client) {
    close(client->sockfd);
    SSL_shutdown(client->ssl);
    SSL_free(client->ssl);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (rooms[client->room].clients[i] && rooms[client->room].clients[i]->uid == client->uid) {
            rooms[client->room].clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    free(client);
}

void *handle_client(void *arg) {
    char buff_out[BUFFER_SZ];
    char message[BUFFER_SZ + NAME_SIZE + 2];
    client_t *client = (client_t *)arg;
    int leave_flag = 0;

    int bytes = SSL_read(client->ssl, client->name, sizeof(client->name));
    if (bytes <= 0) {
        leave_flag = 1;
    } else {
        SSL_read(client->ssl, buff_out, sizeof(buff_out));
        int room = atoi(buff_out);
        client->room = room;

        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (!rooms[room].clients[i]) {
                rooms[room].clients[i] = client;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        sprintf(message, "%s has joined %s\n", client->name, rooms[room].name);
        printf("%s", message);
        send_message_to_room(message, client->uid, client->room);
    }

    while (!leave_flag) {
        memset(buff_out, 0, BUFFER_SZ);
        bytes = SSL_read(client->ssl, buff_out, BUFFER_SZ - 1);

        if (bytes > 0) {
            buff_out[bytes] = '\0';
            sprintf(message, "%s: %s\n", client->name, buff_out);
            printf("%s", message);
            send_message_to_room(message, client->uid, client->room);
        } else {
            leave_flag = 1;
        }
    }

    close_client_connection(client);
    sprintf(message, "%s has left %s\n", client->name, rooms[client->room].name);
    printf("%s", message);
    send_message_to_room(message, client->uid, client->room);
    return NULL;
}

int main() {
    struct sockaddr_in server_address;
    int opt = 1;

    signal(SIGINT, sigint_handler);

    SSL_library_init();
    ctx = InitServerCTX();
    LoadCertificates(ctx, "server-cert.pem", "server-key.pem");

    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8080);

    if (bind(listenfd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(listenfd, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("SERVER HAS STARTED...\n");

    init_rooms();

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        int clientfd = accept(listenfd, (struct sockaddr*)&client_address, &client_len);

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, clientfd);

        if (SSL_accept(ssl) == -1) {
            ERR_print_errors_fp(stderr);
            close(clientfd);
            SSL_free(ssl);
            continue;
        }

        client_t *client = (client_t*)malloc(sizeof(client_t));
        client->address = client_address;
        client->sockfd = clientfd;
        client->uid = uid++;
        client->ssl = ssl;
        client->room = -1;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void*)client);
    }

    close(listenfd);
    SSL_CTX_free(ctx);
    return 0;
}

