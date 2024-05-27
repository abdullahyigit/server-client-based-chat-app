#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFFER_SIZE 1024
#define NAME_SIZE 32
#define MAX_ROOMS 10

SSL *ssl;
char name[NAME_SIZE];
char room[NAME_SIZE];

SSL_CTX* InitSSL_CTX(void) {
    SSL_CTX *ctx;
    const SSL_METHOD *method;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method = TLS_client_method();
    ctx = SSL_CTX_new(method);

    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void str_trim_lf(char *arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void *send_msg_handler(void *arg) {
    char message[BUFFER_SIZE];

    while (1) {
        fgets(message, BUFFER_SIZE, stdin);
        str_trim_lf(message, BUFFER_SIZE);

        if (strcmp(message, "exit") == 0) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            exit(0);
        } else if (strlen(message) > 0) {
            if (SSL_write(ssl, message, strlen(message)) < 0) {
                perror("Send failed");
                exit(1);
            }
        }
    }
    return NULL;
}

void *receive_msg_handler(void *arg) {
    char message[BUFFER_SIZE];

    while (1) {
        int recv_length = SSL_read(ssl, message, BUFFER_SIZE - 1);
        if (recv_length <= 0) {
            if (recv_length == 0) {
                printf("Server disconnected.\n");
            } else {
                perror("Receive failed");
            }
            break;
        }

        message[recv_length] = '\0';
        printf("\r%s", message);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <IP> <Port>\n", argv[0]);
        exit(1);
    }

    SSL_CTX *ctx;
    int server_sock;
    struct sockaddr_in server_addr;
    pthread_t send_thread, receive_thread;

    SSL_library_init();
    ctx = InitSSL_CTX();

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    if (connect(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("Connect failed");
        exit(1);
    }

    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, server_sock);

    if (SSL_connect(ssl) == -1) {
        ERR_print_errors_fp(stderr);
    } else {
        printf("Connected to %s:%s with SSL encryption.\n", argv[1], argv[2]);
        fflush(stdout);
    }

    //printf("Username: ");
    fgets(name, NAME_SIZE, stdin);
    str_trim_lf(name, strlen(name));

    //printf("Enter room number (0-2): ");
    fgets(room, NAME_SIZE, stdin);
    str_trim_lf(room, strlen(room));

    int room_num = atoi(room);
    if (room_num < 0 || room_num >= MAX_ROOMS) {
        printf("Invalid room number. Exiting...\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(server_sock);
        exit(1);
    }

    if (SSL_write(ssl, name, strlen(name)) < 0) {
        perror("Send failed");
        exit(1);
    }
    if (SSL_write(ssl, room, strlen(room)) < 0) {
        perror("Send failed");
        exit(1);
    }

    pthread_create(&send_thread, NULL, send_msg_handler, NULL);
    pthread_create(&receive_thread, NULL, receive_msg_handler, NULL);
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    SSL_CTX_free(ctx);
    close(server_sock);
    return 0;
}
