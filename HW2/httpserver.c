#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

struct proxy_handler {
    pthread_cond_t *cond;
    int write_fd;
    int read_fd;
    int *ok;

};

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 * You can change these functions to anything you want.
 * 
 * ATTENTION: Be careful to optimize your code. Judge is
 *            sesnsitive to time-out errors.
 */
void serve_file(int fd, char *path, struct stat *file_stat) {

    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", http_get_mime_type(path));
    char length[10];
    sprintf(length, "%ld", file_stat->st_size);
    http_send_header(fd, "Content-Length", length); // Change this too
    http_end_headers(fd);

    /* TODO: PART 1 Bullet 2 */

    FILE *file = fopen(path, "r");
    void *bytes_read = malloc(file_stat->st_size);
    fread(bytes_read, file_stat->st_size, 1, file);

    http_send_data(fd, bytes_read, file_stat->st_size);

    fclose(file);
    close(fd);
}


void serve_directory(int fd, char *path, DIR *dir) {
    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
    http_end_headers(fd);

    /* TODO: PART 1 Bullet 3,4 */
    struct dirent *ent;
    char *dir_list = malloc(strlen(path) + 100);
    dir_list[0] = 0;
    while ((ent = readdir(dir)) != NULL) {
        sprintf(dir_list, "%s<a href=/%s/%s%s>%s</a><br>\n", dir_list, path, ent->d_name,
                (ent->d_type == DT_DIR ? "/" : ""), ent->d_name);
    }
    http_send_string(fd, dir_list);
    closedir(dir);
    free(dir_list);
}

char *get_path_to_file() {
    char *path_to_file = malloc(sizeof(char) * (strlen(server_files_directory) + 1));
    strcpy(path_to_file, server_files_directory);
    return path_to_file;
}

char *get_file_absolute_path(char *rel_file_path) {
    char *path_to_file = get_path_to_file();
    char *file_path = malloc(sizeof(char) * (strlen(path_to_file) + strlen(rel_file_path) + 1));
    strcpy(file_path, server_files_directory);
    strcat(file_path, rel_file_path);
    return file_path;
}

char *get_directory_path(char *path) {
    char *directory_path = malloc(sizeof(char) * strlen(path) + 1);
    for (int i = 0; i < strlen(path) - 10; i++) {
        directory_path[i] = path[i];
    }
    return directory_path;
}

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 */
void handle_files_request(int fd) {

    struct http_request *request = http_request_parse(fd);

    if (request == NULL || request->path[0] != '/') {
        http_start_response(fd, 400);
        http_send_header(fd, "Content-Type", "text/html");
        http_end_headers(fd);
        close(fd);
        return;
    }

    if (strstr(request->path, "..") != NULL) {
        http_start_response(fd, 403);
        http_send_header(fd, "Content-Type", "text/html");
        http_end_headers(fd);
        close(fd);
        return;
    }

    /* Remove beginning `./` */
    char *path = malloc(2 + strlen(request->path) + 1);
    path[0] = '.';
    path[1] = '/';
    memcpy(path + 2, request->path, strlen(request->path) + 1);

    /*
     * TODO: First is to serve files. If the file given by `path` exists,
     * call serve_file() on it. Else, serve a 404 Not Found error below.
     *
     * TODO: Second is to serve both files and directories. You will need to
     * determine when to call serve_file() or serve_directory() depending
     * on `path`.
     *
     * Feel FREE to delete/modify anything on this function.
     */
    char *file_absolute_path = get_file_absolute_path(request->path);

    struct stat file_stat;
    stat(file_absolute_path, &file_stat);

    if (S_ISREG(file_stat.st_mode)) {
        serve_file(fd, file_absolute_path, &file_stat);
    } else if (S_ISDIR(file_stat.st_mode)) {
        strcat(file_absolute_path, "/");
        strcat(file_absolute_path, "index.html");
        int stat_result = stat(file_absolute_path, &file_stat);
        if (stat_result == 0) {
            serve_file(fd, file_absolute_path, &file_stat);
        } else {
            char *directory_path = get_directory_path(file_absolute_path);
            DIR *dir = opendir(directory_path);
            serve_directory(fd, directory_path, dir);
        }
    } else {
        http_start_response(fd, 404);
        http_send_header(fd, "Content-Type", "text/html");
        http_end_headers(fd);
    }

//    http_start_response(fd, 200);
//    http_send_header(fd, "Content-Type", "text/html");
//    http_end_headers(fd);
//    http_send_string(fd,
//                     "<center>"
//                     "<h1>Welcome to httpserver!</h1>"
//                     "<hr>"
//                     "<p>Nothing's here yet.</p>"
//                     "</center>");
    free(path);
    free(request);
    close(fd);
    return;
}

void *run_proxy_routine(void *ptr_to_proxy_handler) {
    struct proxy_handler *proxy_handler = ptr_to_proxy_handler;
    char read_bytes[10000];
    while (!*(proxy_handler->ok)) {
        int result = read(proxy_handler->read_fd, read_bytes, 10000);
        if (result <= 0) {
            *(proxy_handler->ok) = 1;
            pthread_cond_signal(proxy_handler->cond);
            return NULL;
        } else {
            read_bytes[result] = '\0';
            http_send_data(proxy_handler->write_fd, read_bytes, (size_t) result);
        }

    }
    return NULL;
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

    /*
    * The code below does a DNS lookup of server_proxy_hostname and
    * opens a connection to it. Please do not modify.
    */

    struct sockaddr_in target_address;
    memset(&target_address, 0, sizeof(target_address));
    target_address.sin_family = AF_INET;
    target_address.sin_port = htons(server_proxy_port);

    struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

    int target_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (target_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        close(fd);
        exit(errno);
    }

    if (target_dns_entry == NULL) {
        fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
        close(target_fd);
        close(fd);
        exit(ENXIO);
    }

    char *dns_address = target_dns_entry->h_addr_list[0];

    memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
    int connection_status = connect(target_fd, (struct sockaddr *) &target_address,
                                    sizeof(target_address));

    if (connection_status < 0) {
        /* Dummy request parsing, just to be compliant. */
        http_request_parse(fd);

        http_start_response(fd, 502);
        http_send_header(fd, "Content-Type", "text/html");
        http_end_headers(fd);
        http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
        close(target_fd);
        close(fd);
        return;

    }

    /*
    * TODO: Your solution for task 3 belongs here!
    */

    int server_read_fd = target_fd;
    int client_read_fd = fd;

    int server_write_fd = fd;
    int client_write_fd = target_fd;

    int ok = 0;

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    pthread_t server_thread;
    pthread_t client_thread;

    struct proxy_handler *server_over_proxy = malloc(sizeof(struct proxy_handler));
    struct proxy_handler *client_over_proxy = malloc(sizeof(struct proxy_handler));

    *server_over_proxy = (struct proxy_handler) {&cond, server_write_fd, server_read_fd, &ok};
    *client_over_proxy = (struct proxy_handler) {&cond, client_write_fd, client_read_fd, &ok};

    pthread_create(&client_thread, NULL, run_proxy_routine, client_over_proxy);
    pthread_create(&server_thread, NULL, run_proxy_routine, server_over_proxy);

    while (!ok) pthread_cond_wait(&cond, &mutex);
    
    pthread_cancel(client_thread);
    pthread_cancel(server_thread);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    close(target_fd);
    close(fd);
    free(client_over_proxy);
    free(server_over_proxy);
}


void *start_routine(void *handler) {

    void (*request_handler)(int) = handler;

    while (1) {
        int client_socket_number = wq_pop(&work_queue);
        request_handler(client_socket_number);
        close(client_socket_number);
    }
}

void init_thread_pool(int num_threads, void (*request_handler)(int)) {
    /*
     * TODO: Part of your solution for Task 2 goes here!
     */
    wq_init(&work_queue);
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, start_routine, request_handler);
    }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

    struct sockaddr_in server_address, client_address;
    size_t client_address_length = sizeof(client_address);
    int client_socket_number;

    *socket_number = socket(PF_INET, SOCK_STREAM, 0);
    if (*socket_number == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }

    int socket_option = 1;
    if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                   sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        exit(errno);
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);

    if (bind(*socket_number, (struct sockaddr *) &server_address,
             sizeof(server_address)) == -1) {
        perror("Failed to bind on socket");
        exit(errno);
    }

    if (listen(*socket_number, 1024) == -1) {
        perror("Failed to listen on socket");
        exit(errno);
    }

    printf("Listening on port %d...\n", server_port);

    init_thread_pool(num_threads, request_handler);

    while (1) {
        client_socket_number = accept(*socket_number,
                                      (struct sockaddr *) &client_address,
                                      (socklen_t *) &client_address_length);
        if (client_socket_number < 0) {
            perror("Error accepting socket");
            continue;
        }

        printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port);

        // TODO: Change me?
        if (num_threads == 0) {
            request_handler(client_socket_number);
            close(client_socket_number);
        } else {
            wq_push(&work_queue, client_socket_number);
        }

        printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port);
    }

    shutdown(*socket_number, SHUT_RDWR);
    close(*socket_number);
}

int server_fd;

void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    printf("Closing socket %d\n", server_fd);
    if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
    exit(0);
}

char *USAGE =
        "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
        "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);
    signal(SIGPIPE, SIG_IGN);

    /* Default settings */
    server_port = 8000;
    void (*request_handler)(int) = NULL;

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp("--files", argv[i]) == 0) {
            request_handler = handle_files_request;
            free(server_files_directory);
            server_files_directory = argv[++i];
            if (!server_files_directory) {
                fprintf(stderr, "Expected argument after --files\n");
                exit_with_usage();
            }
        } else if (strcmp("--proxy", argv[i]) == 0) {
            request_handler = handle_proxy_request;

            char *proxy_target = argv[++i];
            if (!proxy_target) {
                fprintf(stderr, "Expected argument after --proxy\n");
                exit_with_usage();
            }

            char *colon_pointer = strchr(proxy_target, ':');
            if (colon_pointer != NULL) {
                *colon_pointer = '\0';
                server_proxy_hostname = proxy_target;
                server_proxy_port = atoi(colon_pointer + 1);
            } else {
                server_proxy_hostname = proxy_target;
                server_proxy_port = 80;
            }
        } else if (strcmp("--port", argv[i]) == 0) {
            char *server_port_string = argv[++i];
            if (!server_port_string) {
                fprintf(stderr, "Expected argument after --port\n");
                exit_with_usage();
            }
            server_port = atoi(server_port_string);
        } else if (strcmp("--num-threads", argv[i]) == 0) {
            char *num_threads_str = argv[++i];
            if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
                fprintf(stderr, "Expected positive integer after --num-threads\n");
                exit_with_usage();
            }
        } else if (strcmp("--help", argv[i]) == 0) {
            exit_with_usage();
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }

    if (server_files_directory == NULL && server_proxy_hostname == NULL) {
        fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                        "                      \"--proxy [HOSTNAME:PORT]\"\n");
        exit_with_usage();
    }

    serve_forever(&server_fd, request_handler);

    return EXIT_SUCCESS;
}


