#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 8888
#define MAX_NUM_CONNECTED 10
#define MAX_NUM_LISTEN 5
#define MAX_BUFFER_SIZE 1024

struct ClientData
{
    struct sockaddr_in socket_info;
    int socketfd;
    int client_no;
    int is_connected;
};

pthread_mutex_t mutex_connected_num;
pthread_mutex_t mutex_client[MAX_NUM_CONNECTED];
pthread_cond_t cond_check_connected;
pthread_t thread_server;
pthread_t thread_client[MAX_NUM_CONNECTED];

int server_socketfd;
struct sockaddr_in server_socket_info;
int connected_num;
struct ClientData client[MAX_NUM_CONNECTED];
int server_quit;

void show_all_clients_info(char *buffer)
{
    int i;
    int client_num = 0;
    char tmp_buffer[100];

    sprintf(buffer, "--------------------------------------------------------------------------------\n");
    sprintf(tmp_buffer, "| %10s%56s%10s |\n", "Client No.", "IPv4 address", "Port");
    strcat(buffer, tmp_buffer);
    strcat(buffer, "--------------------------------------------------------------------------------\n");
    for (i = 0; i < MAX_NUM_CONNECTED; i++)
    {
        pthread_mutex_lock(&mutex_client[i]);
        if (client[i].is_connected == 1)
        {
            sprintf(tmp_buffer, "| %.10d%56s%10d |\n", client[i].client_no, inet_ntoa(client[i].socket_info.sin_addr), client[i].socket_info.sin_port);
            strcat(buffer, tmp_buffer);
            client_num++;
        }
        pthread_mutex_unlock(&mutex_client[i]);
    }
    sprintf(tmp_buffer, "| %76s |\n", " ");
    strcat(buffer, tmp_buffer);
    // 69 = 80 - 5 - 1 - 4 - 1
    sprintf(tmp_buffer, "| total %d %68s |\n", client_num, " ");
    strcat(buffer, tmp_buffer);
    strcat(buffer, "--------------------------------------------------------------------------------\n\n");
    return;
}

void *Client(void *argv)
{
    struct ClientData *client_i = (struct ClientData *)argv;

    unsigned char receive_buffer[MAX_BUFFER_SIZE];

    while (recv(client_i->socketfd, receive_buffer, MAX_BUFFER_SIZE, 0) != -1)
    {
        char buffer[MAX_BUFFER_SIZE];
        char tmp_buffer[MAX_BUFFER_SIZE];
        int need_to_send = 0;
        int is_from_others = 0;
        char disconnect[] = "__DISC__";
        char show_cur_time[] = "__TIME__";
        char show_ser_name[] = "__NAME__";
        char show_cli_info[] = "__INFO__";
        char start_send_msg[] = "!@#MSG$_";
        char mid_send_msg[] = "__DEST__";
        char end_send_msg[] = "_$&MSG^*";

        char rcv_error_msg[] = "__*ERR__";
        char start_rcv_sent_msg[] = "~$^RCV*&";
        char mid_rcv_sent_msg[] = "__&RCV__";
        char end_rcv_sent_msg[] = "#*!RCV&$";

        int dst_client = client_i->client_no;

        sscanf(receive_buffer, "%8s", tmp_buffer);

        if (strcmp(tmp_buffer, disconnect) == 0)
        {
            pthread_cond_signal(&cond_check_connected);
            break;
        }
        else if (strcmp(tmp_buffer, show_cur_time) == 0)
        {
            time_t time_now;
            struct tm *struct_time_now;
            time(&time_now);
            struct_time_now = localtime(&time_now);
            strcpy(buffer, asctime(struct_time_now));
            need_to_send = 1;
        }
        else if (strcmp(tmp_buffer, show_ser_name) == 0)
        {
            strcpy(buffer, "Machine name");
            need_to_send = 1;
        }
        else if (strcmp(tmp_buffer, show_cli_info) == 0)
        {
            show_all_clients_info(buffer);
            need_to_send = 1;
        }
        else if (strcmp(tmp_buffer, start_send_msg) == 0)
        {
            int i;
            for (i = 18; i < strlen(receive_buffer) - 8; i++)
            {
                buffer[i - 18] = receive_buffer[i];
            }
            buffer[i] = '\0';

            dst_client = (receive_buffer[8] - 48) * 10 + (receive_buffer[9] - 48);

            need_to_send = 1;
            is_from_others = 1;
        }

        if (need_to_send == 1)
        {
            pthread_mutex_lock(&mutex_client[client_i->client_no]);
            /* If the destination client is disconnected, send the error message back to source client. */
            if (client[dst_client].is_connected == 0)
            {
                sprintf(buffer, "--------------------------------------------------------------------------------\n");
                sprintf(buffer, "| Error:                    The destination is disconnected.                   |\n");
                sprintf(buffer, "--------------------------------------------------------------------------------\n\n");
                dst_client = client_i->client_no;
                strcpy(tmp_buffer, buffer);
                sprintf(buffer, "%s", rcv_error_msg);
                strcat(buffer, tmp_buffer);
            }
            else
            {
                strcpy(tmp_buffer, buffer);
                sprintf(buffer, "%s", start_rcv_sent_msg);
                char tmp_buf[3];
                if (is_from_others == 1)
                {
                    sprintf(tmp_buf, "%.2d", client_i->client_no);
                }
                else
                {
                    strcpy(tmp_buf, "-1");
                }
                strcat(buffer, tmp_buf);
                strcat(buffer, mid_rcv_sent_msg);
                strcat(buffer, tmp_buffer);
                strcat(buffer, end_rcv_sent_msg);
            }

            if (send(client[dst_client].socketfd, buffer, (strlen(buffer) + 1), 0) == -1)
            {
                fprintf(stderr, "--------------------------------------------------------------------------------\n");
                fprintf(stderr, "| Error:                               Send error.                             |\n");
                fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                pthread_mutex_unlock(&mutex_client[client_i->client_no]);
                continue;
            }
            else
            {
                fprintf(stdout, "--------------------------------------------------------------------------------\n");
                fprintf(stdout, "| Info:                           Send successfully.                           |\n");
                fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
                pthread_mutex_unlock(&mutex_client[client_i->client_no]);
            }
        }
    }

    pthread_mutex_lock(&mutex_client[client_i->client_no]);
    client_i->is_connected = 0;
    connected_num--;
    pthread_mutex_unlock(&mutex_client[client_i->client_no]);

    int socketfd = client_i->socketfd;
    if (close(socketfd) == -1)
    {
        fprintf(stderr, "--------------------------------------------------------------------------------\n");
        fprintf(stderr, "| Error:                    Failed to close the socket (%d).                    |\n", socketfd);
        fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
    }
    else
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Info:                   Successfully close the socket (%d).                   |\n", socketfd);
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
    }

    pthread_exit(NULL);
}

void disconnect_certain_client(void)
{
    if (connected_num == 0)
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Warning:                        There is NO connection.                      |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
        return;
    }

    int client_index;
    while (1)
    {
        printf("--------------------------------------------------------------------------------\n");
        printf("|     Please input the client to disconnect (the prefix 0s are NOT needed):    |\n");
        printf("--------------------------------------------------------------------------------\n\n");
        // printf("> ");
        scanf("%d", &client_index);

        if (client_index > (MAX_NUM_CONNECTED - 1))
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:            The client is not connected. Please input again.           |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
            continue;
        }
        else
        {
            pthread_mutex_lock(&mutex_client[client_index]);
            if (client[client_index].is_connected == 1)
            {
                /* Cancel the thread of client. */
                if (pthread_cancel(thread_client[client_index]) != 0)
                {
                    fprintf(stderr, "--------------------------------------------------------------------------------\n");
                    fprintf(stderr, "| Error:                    Failed to terminate the thread.                    |\n");
                    fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                }
                else
                {
                    /* Wait the thread to terminate. */
                    if (pthread_join(thread_client[client_index], NULL) == 0)
                    {
                        fprintf(stdout, "--------------------------------------------------------------------------------\n");
                        fprintf(stdout, "| Info:                   Successfully terminate the thread.                   |\n");
                        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
                    }
                    else
                    {
                        fprintf(stderr, "--------------------------------------------------------------------------------\n");
                        fprintf(stderr, "| Error:                    Failed to terminate the thread.                    |\n");
                        fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                    }
                }

                /* Close the socket of the client. */
                int socketfd = client[client_index].socketfd;
                if (close(socketfd) == -1)
                {
                    fprintf(stderr, "--------------------------------------------------------------------------------\n");
                    fprintf(stderr, "| Error:                    Failed to close the socket (%d).                    |\n", socketfd);
                    fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                }
                else
                {
                    fprintf(stdout, "--------------------------------------------------------------------------------\n");
                    fprintf(stdout, "| Info:                   Successfully close the socket (%d).                   |\n", socketfd);
                    fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
                }
            }
            else
            {
                fprintf(stderr, "--------------------------------------------------------------------------------\n");
                fprintf(stderr, "| Error:                      The client is not connected.                     |\n");
                fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
            }
            pthread_mutex_unlock(&mutex_client[client_index]);
            break;
        }
    }
    return;
}

void disconnect_all_clients(void)
{
    if (connected_num == 0)
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Info:                           Quit successfully.                           |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
        return;
    }

    /* When quitting, check if all the clients have closed their sockets, and then cancel the threads of connected clients. */
    int i;
    if (server_quit == 1)
    {
        for (i = 0; i < MAX_NUM_CONNECTED; i++)
        {
            if (client[i].is_connected == 1)
            {
                /* Cancel the thread of client. */
                if (pthread_cancel(thread_client[i]) != 0)
                {
                    fprintf(stderr, "--------------------------------------------------------------------------------\n");
                    fprintf(stderr, "| Error:                    Failed to terminate the thread.                    |\n");
                    fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                }
                else
                {
                    /* Wait the thread to terminate. */
                    if (pthread_join(thread_client[i], NULL) == 0)
                    {
                        fprintf(stdout, "--------------------------------------------------------------------------------\n");
                        fprintf(stdout, "| Info:                   Successfully terminate the thread.                   |\n");
                        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
                    }
                    else
                    {
                        fprintf(stderr, "--------------------------------------------------------------------------------\n");
                        fprintf(stderr, "| Error:                    Failed to terminate the thread.                    |\n");
                        fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                    }
                }

                // Close the socket.
                int socketfd = client[i].socketfd;
                if (close(socketfd) == -1)
                {
                    fprintf(stderr, "--------------------------------------------------------------------------------\n");
                    fprintf(stderr, "| Error:                    Failed to close the socket (%d).                    |\n", socketfd);
                    fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                }
                else
                {
                    fprintf(stdout, "--------------------------------------------------------------------------------\n");
                    fprintf(stdout, "| Info:                   Successfully close the socket (%d).                   |\n", socketfd);
                    fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
                }
            }
        }
    }

    fprintf(stdout, "--------------------------------------------------------------------------------\n");
    fprintf(stdout, "| Info:                           Quit successfully.                           |\n");
    fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
    return;
}

void *Server(void *argv)
{
    int choice = 0;
    printf("--------------------------------------------------------------------------------\n");
    printf("|                                     Menu                                     |\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("| Service <1>:              Show the Info of each connected client             |\n");
    printf("| Service <2>:                       Disconnect a client                       |\n");
    printf("| Service <3>:                               Quit                              |\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("|            Please input one operation (ONLY the number) each time:           |\n");
    printf("--------------------------------------------------------------------------------\n\n");

    while (choice != 3)
    {
        // printf("> ");
        scanf("%d", &choice);
        printf("\n");

        char buffer[MAX_BUFFER_SIZE];
        switch (choice)
        {
        case 1:
            show_all_clients_info(buffer);
            printf("%s", buffer);
            break;
        case 2:
            disconnect_certain_client();
            break;
        case 3:
            server_quit = 1;
            disconnect_all_clients();
            break;
        default:
            break;
        }
    }

    // Exit the thread.
    pthread_exit(NULL);
}

void Connect(void)
{
    // Create the thread of the server.
    pthread_create(&thread_server, NULL, Server, NULL);

    /* Create a socket with IPv4 and TCP attributes. */
    server_socketfd = socket(AF_INET, SOCK_STREAM, 0);
    // If the system fails to create a socket, exit the program with error status.
    if (server_socketfd == -1)
    {
        fprintf(stderr, "--------------------------------------------------------------------------------\n");
        fprintf(stderr, "| Error:       Failed to create a socket. Exit the program with status 1.      |\n");
        fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
        exit(1);
    }
    else
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Info:                     Successfully create the socket.                    |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
    }

    // Set the socket with the non-block attribute.
    fcntl(server_socketfd, F_SETFL, O_NONBLOCK);

    /* Set up the Info of the server. */
    // First, the Info struct is stored as 0.
    bzero(&server_socket_info, sizeof(server_socket_info));
    // Use IPv4 address.
    server_socket_info.sin_family = AF_INET;
    // Any IPv4 address.
    server_socket_info.sin_addr.s_addr = htonl(INADDR_ANY);
    // Change host endian to network compatible short integer.
    server_socket_info.sin_port = htons(PORT);

    /* Bind the server's address with the socket. */
    if (bind(server_socketfd, (struct sockaddr *)&server_socket_info, sizeof(server_socket_info)) == -1)
    {
        fprintf(stderr, "--------------------------------------------------------------------------------\n");
        fprintf(stderr, "| Error:       Failed to bind the socket. Exit the program with status 1.      |\n");
        fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
        exit(1);
    }
    else
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Info:                      Successfully bind the socket.                     |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
    }

    /* Listen the socket and set the maximal length of listening queue. */
    if (listen(server_socketfd, MAX_NUM_LISTEN) == -1)
    {
        fprintf(stderr, "--------------------------------------------------------------------------------\n");
        fprintf(stderr, "| Error:      Failed to listen the socket. Exit the program with status 1.     |\n");
        fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
        exit(1);
    }
    else
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Info:                     Successfully listen the socket.                    |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
    }

    while (1)
    {
        // After quit request from the thread of server, leaves this loop.
        if (server_quit == 1)
        {
            return;
        }

        /* Check if the the number of connected clients is over the limit. */
        pthread_mutex_lock(&mutex_connected_num);
        if (connected_num >= MAX_NUM_CONNECTED)
        {
            pthread_cond_wait(&cond_check_connected, &mutex_connected_num);
        }
        pthread_mutex_unlock(&mutex_connected_num);

        /* Find the smallest unused client_no. */
        int i;
        for (i = 0; i < MAX_NUM_CONNECTED; i++)
        {
            pthread_mutex_lock(&mutex_client[i]);
            if (client[i].is_connected == 0)
            {
                pthread_mutex_unlock(&mutex_client[i]);
                break;
            }
            else
            {
                pthread_mutex_unlock(&mutex_client[i]);
                i++;
            }
        }

        /* Accept. */
        int client_socketfd;
        struct sockaddr_in client_socket_info;
        int addr_size = sizeof(struct sockaddr_in);
        client_socketfd = accept(server_socketfd, (struct sockaddr *)&client_socket_info, &addr_size);
        if (client_socketfd == -1)
        {
            sleep(1);
            continue;
        }
        else
        {
            printf("\n");
            fprintf(stdout, "--------------------------------------------------------------------------------\n");
            fprintf(stdout, "| Info:                           Successfully accept.                         |\n");
            fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
        }

        /* Set up the Info of the client. */
        pthread_mutex_lock(&mutex_client[i]);
        client[i].socket_info = client_socket_info;
        client[i].socketfd = client_socketfd;
        client[i].client_no = i;
        client[i].is_connected = 1;
        pthread_mutex_unlock(&mutex_client[i]);

        // Create the thread of the client.
        pthread_create(&thread_client[i], NULL, Client, &client[i]);

        pthread_mutex_lock(&mutex_connected_num);
        connected_num++;
        pthread_mutex_unlock(&mutex_connected_num);
    }

    return;
}

int main(void)
{
    /* Initialize the mutex and condition variable.*/
    pthread_mutex_init(&mutex_connected_num, NULL);
    int i;
    for (i = 0; i < MAX_NUM_CONNECTED; i++)
    {
        pthread_mutex_init(&mutex_client[i], NULL);
    }
    pthread_cond_init(&cond_check_connected, NULL);

    // Initially, the number of connected clients is set to 0.
    connected_num = 0;

    // Initially, the server is set to be not to quit.
    server_quit = 0;

    /* Initially, all clients are disconnected. */
    for (i = 0; i < MAX_NUM_CONNECTED; i++)
    {
        client[i].is_connected = 0;
    }

    Connect();

    /* Destroy the mutex and condition varaiable. */
    pthread_mutex_destroy(&mutex_connected_num);
    for (i = 0; i < MAX_NUM_CONNECTED; i++)
    {
        pthread_mutex_destroy(&mutex_client[i]);
    }
    pthread_cond_destroy(&cond_check_connected);

    return 0;
}