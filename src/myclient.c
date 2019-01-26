/* 
 * MIT License
 * 
 * Copyright (c) 2019 Zi Jun, Xu
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_IP_ADDRESS_LEN 16
#define MAX_FAIL_TIME 10
#define MAX_BUFFER_SIZE 1024

int socketfd;
struct sockaddr_in socket_info;
char ipv4_address[MAX_IP_ADDRESS_LEN];
int port;
int is_connected;
int choice;
pthread_t thread_machine_no;

int check_ipv4_address(char *ipv4_addr)
{
    int dot_index[3] = {-1, -1, -1};
    int index = 0;
    int i;
    for (i = 0; i < strlen(ipv4_addr); i++)
    {
        if (ipv4_addr[i] == '.')
        {
            if (index == 3)
            {
                fprintf(stderr, "--------------------------------------------------------------------------------\n");
                fprintf(stderr, "| Error:                Wrong IPv4 address. Please enter again.                |\n");
                fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                return 0;
            }
            dot_index[index] = i;
            index++;
        }
        else if (isdigit(ipv4_addr[i]) == 0)
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:                Wrong IPv4 address. Please enter again.                |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
            return 0;
        }
    }

    for (i = 0; i < 3; i++)
    {
        if (dot_index[i] == -1)
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:                Wrong IPv4 address. Please enter again.                |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
            return 0;
        }
    }

    int count[4];
    memset(count, 0, sizeof(count));
    int digit;

    digit = dot_index[0] - 1;
    for (i = 0; i < dot_index[0]; i++)
    {
        count[0] += (ipv4_addr[i] - 48) * (int)pow(10, digit);
        digit--;
    }

    digit = dot_index[1] - dot_index[0] - 2;
    for (i = dot_index[0] + 1; i < dot_index[1]; i++)
    {
        count[1] += (ipv4_addr[i] - 48) * (int)pow(10, digit);
        digit--;
    }

    digit = dot_index[2] - dot_index[1] - 2;
    for (i = dot_index[1] + 1; i < dot_index[2]; i++)
    {
        count[2] += (ipv4_addr[i] - 48) * (int)pow(10, digit);
        digit--;
    }

    digit = strlen(ipv4_addr) - dot_index[2] - 2;
    for (i = dot_index[2] + 1; i < strlen(ipv4_addr); i++)
    {
        count[3] += (ipv4_addr[i] - 48) * (int)pow(10, digit);
        digit--;
    }

    for (i = 0; i < 4; i++)
    {
        if (count[i] >= 256)
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:                Wrong IPv4 address. Please enter again.                |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
            return 0;
        }
    }

    return 1;
}

void *receive_messages(void *arg)
{
    unsigned char receive_buffer[MAX_BUFFER_SIZE];
    char rcv_error_msg[] = "__*ERR__";
    char start_rcv_sent_msg[] = "~$^RCV*&";
    char mid_rcv_sent_msg[] = "__&RCV__";
    char end_rcv_sent_msg[] = "#*!RCV&$";

    /* Contiually receive messages. */
    while (recv(socketfd, receive_buffer, sizeof(receive_buffer), 0) != -1)
    {
        char tmp_buffer[MAX_BUFFER_SIZE];
        sscanf(receive_buffer, "%8s", tmp_buffer);

        int i;
        if (strcmp(tmp_buffer, rcv_error_msg) == 0)
        {
            for (i = 8; i < strlen(receive_buffer); i++)
            {
                tmp_buffer[i - 8] = receive_buffer[i];
            }
            tmp_buffer[i] = '\0';
            fprintf(stderr, "%s", tmp_buffer);
            continue;
        }
        else
        {
            for (i = 18; i < (strlen(receive_buffer) - 8); i++)
            {
                tmp_buffer[i - 18] = receive_buffer[i];
            }
            tmp_buffer[i] = '\0';

            if (receive_buffer[8] == '-' && receive_buffer[9] == '1')
            {
                fprintf(stdout, "%s", tmp_buffer);
            }
            else
            {
                int src_client = (receive_buffer[8] - 48) * 10 + (receive_buffer[9] - 48);
                int message_len = 30 + strlen(tmp_buffer);
                for (i = 0; i < message_len; i++)
                {
                    printf("-");
                }
                fprintf(stdout, "\n| Received message from %.2d: %s |\n", src_client, tmp_buffer);
                for (i = 0; i < message_len; i++)
                {
                    printf("-");
                }
                printf("\n\n");
            }
        }
    }

    /* If the program executes to this step, it indicates that the receiving step went wrong. */
    fprintf(stderr, "--------------------------------------------------------------------------------\n");
    fprintf(stderr, "| Error:                             Receive error.                            |\n");
    fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
    // Exit the thread.
    pthread_exit(NULL);
}

void Connect()
{
    // Count the times that have been failed to connect.
    int fail_to_connect_times = 0;

    while (1)
    {
        /* Check the input IPv4 address is correct. */
        while (1)
        {
            printf("--------------------------------------------------------------------------------\n");
            printf("|                        Please input the IPv4 address:                        |\n");
            printf("--------------------------------------------------------------------------------\n\n");
            // printf("> ");
            scanf("%16s", ipv4_address);

            if (check_ipv4_address(ipv4_address) == 1)
            {
                break;
            }
        }

        /* Check the input port is correct. */
        while (1)
        {
            char string_port[6];
            printf("\n--------------------------------------------------------------------------------\n");
            printf("|                            Please input the port:                            |\n");
            printf("--------------------------------------------------------------------------------\n");
            printf("|                       Notice: The default port is 8888.                      |\n");
            printf("--------------------------------------------------------------------------------\n\n");
            // printf("> ");
            scanf("%5s", string_port);

            int i;
            int flag = 1;

            for (i = 0; i < strlen(string_port); i++)
            {
                if (isdigit(string_port[i]) == 0)
                {
                    printf("char = %c\n", string_port[i]);
                    flag = 0;
                    fprintf(stderr, "\n--------------------------------------------------------------------------------\n");
                    fprintf(stderr, "| Error:                    Wrong port. Please input again.                    |\n");
                    fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                    break;
                }
            }
            if (flag == 1)
            {
                // Change the string to int type and store to `port`.
                port = atoi(string_port);
                break;
            }
        }

        /* Create a socket with IPv4 and TCP attributes. */
        socketfd = socket(AF_INET, SOCK_STREAM, 0);
        // If the system fails to create a socket, exit the program with error status.
        if (socketfd == -1)
        {
            fprintf(stderr, "\n--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:       Failed to create a socket. Exit the program with status 1.      |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
            exit(1);
        }
        else
        {
            fprintf(stdout, "\n--------------------------------------------------------------------------------\n");
            fprintf(stdout, "| Info:                     Successfully create the socket.                    |\n");
            fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
        }

        /* Set up the Info of the connection. */
        // First, the Info struct is stored as 0.
        bzero(&socket_info, sizeof(socket_info));
        // Use IPv4 address.
        socket_info.sin_family = AF_INET;
        // IPv4 address.
        socket_info.sin_addr.s_addr = inet_addr(ipv4_address);
        // Change host endian to network compatible short integer.
        socket_info.sin_port = htons(port);

        /* Connect. */
        if (connect(socketfd, (struct sockaddr *)&socket_info, sizeof(socket_info)) == -1)
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:   Failed to connect. Please input IPv4 address and port from start.   |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");

            fail_to_connect_times++;
            if (fail_to_connect_times < MAX_FAIL_TIME)
            {
                continue;
            }
            else
            {
                fprintf(stderr, "--------------------------------------------------------------------------------\n");
                fprintf(stderr, "| Error:      It has been failed %d times. Exit the program with status 1.     |\n", MAX_FAIL_TIME);
                fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                exit(1);
            }
        }
        else
        {
            // The connection status is set to be connected.
            is_connected = 1;

            fprintf(stdout, "--------------------------------------------------------------------------------\n");
            fprintf(stdout, "| Info:                          Connect successfully.                         |\n");
            fprintf(stdout, "--------------------------------------------------------------------------------\n\n");

            // Create the child thread to receive messages.
            if (pthread_create(&thread_machine_no, NULL, receive_messages, NULL) != 0)
            {
                fprintf(stderr, "--------------------------------------------------------------------------------\n");
                fprintf(stderr, "| Error:     Failed to create child thread. Exit the program with status 1.    |\n");
                fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
                exit(1);
            }

            break;
        }
    }

    return;
}

void Disconnect()
{
    if (is_connected == 0)
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Warning:                       There is no connection.                       |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
    }
    else
    {
        /* Cancel the child thread of receiving messages, and close the socket. */
        if (pthread_cancel(thread_machine_no) != 0)
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:                    Failed to terminate the thread.                    |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
        }
        else
        {
            /* Wait the thread to terminate. */
            if (pthread_join(thread_machine_no, NULL) == 0)
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

        /* Close the socket. */
        if (close(socketfd) == -1)
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:                      Failed to close the socket.                      |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
        }
        else
        {
            fprintf(stdout, "--------------------------------------------------------------------------------\n");
            fprintf(stdout, "| Info:                     Successfully close the socket.                     |\n");
            fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
        }

        is_connected = 0;
    }
    return;
}

void SendMessages(void)
{
    unsigned char send_buffer[MAX_BUFFER_SIZE];
    char disconnect[] = "__DISC__";
    char show_cur_time[] = "__TIME__";
    char show_ser_name[] = "__NAME__";
    char show_cli_info[] = "__INFO__";
    char start_send_msg[] = "!@#MSG$_";
    char mid_send_msg[] = "__DEST__";
    char end_send_msg[] = "_$&MSG^*";

    /* Check if there is a connection. */
    if (is_connected == 0)
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Warning:            There is NO connection. Please connect first.            |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
        return;
    }

    char tmp_buffer[MAX_BUFFER_SIZE];
    int dst_client;
    /* Send differnet messages due to the user's choice. */
    switch (choice)
    {
    case 2:
        sscanf(disconnect, "%s", send_buffer);
        break;
    case 3:
        sscanf(show_cur_time, "%s", send_buffer);
        break;
    case 4:
        sscanf(show_ser_name, "%s", send_buffer);
        break;
    case 5:
        sscanf(show_cli_info, "%s", send_buffer);
        break;
    case 6:
        printf("--------------------------------------------------------------------------------\n");
        printf("|                   Please input the client you want to send:                  |");
        printf("--------------------------------------------------------------------------------\n\n");
        scanf("%d", &dst_client);
        strcat(send_buffer, start_send_msg);
        sprintf(tmp_buffer, "%.2d", dst_client);
        strcat(send_buffer, tmp_buffer);

        printf("--------------------------------------------------------------------------------\n");
        printf("|                  Please input the message you want to send:                  |\n");
        printf("--------------------------------------------------------------------------------\n\n");
        // printf("> ");
        // 997 = 1024 - 8 * 3 - 2 - 1, since the header is 8-bit, the no. of destination client is 2-bit, and the null character is 1-bit.
        scanf("%997s", tmp_buffer);
        strcat(send_buffer, mid_send_msg);
        strcat(send_buffer, tmp_buffer);
        strcat(send_buffer, end_send_msg);
        break;
    }

    /* Send the messages. */
    if (send(socketfd, send_buffer, (strlen(send_buffer) + 1), 0) == -1)
    {
        fprintf(stderr, "--------------------------------------------------------------------------------\n");
        fprintf(stderr, "| Error:                              Send error.                              |\n");
        fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
    }
    else
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Info:                           Send successfully.                           |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
    }

    return;
}

void Quit()
{
    /* If there is a connection, then cancel the child thread. */
    if (is_connected == 1)
    {
        /* Cancel the child thread of receiving messages, and close the socket. */
        if (pthread_cancel(thread_machine_no) != 0)
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:                    Failed to terminate the thread.                    |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
        }
        else
        {
            /* Wait the thread to terminate. */
            if (pthread_join(thread_machine_no, NULL) == 0)
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

        /* Close the socket. */
        if (close(socketfd) == -1)
        {
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:                      Failed to close the socket.                      |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
        }
        else
        {
            fprintf(stdout, "--------------------------------------------------------------------------------\n");
            fprintf(stdout, "| Info:                     Successfully close the socket.                     |\n");
            fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
        }
    }
    else
    {
        fprintf(stdout, "--------------------------------------------------------------------------------\n");
        fprintf(stdout, "| Info:                           Quit successfully.                           |\n");
        fprintf(stdout, "--------------------------------------------------------------------------------\n\n");
    }
    return;
}

void UserInterface(void)
{
    choice = 0;
    printf("--------------------------------------------------------------------------------\n");
    printf("|                                     Menu                                     |\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("| Service <1>:                             Connect                             |\n");
    printf("| Service <2>:                            Disconnect                           |\n");
    printf("| Service <3>:                        Show current time                        |\n");
    printf("| Service <4>:                      Show the server's name                     |\n");
    printf("| Service <5>:              Show the Info of each connected client             |\n");
    printf("| Service <6>:    Send messages (Must know the No. of the sent client first)   |\n");
    printf("| Service <7>:                               Quit                              |\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("| Notice: You can <Quit> without <Disconnect> in advance.                      |\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("\n");
    printf("--------------------------------------------------------------------------------\n");
    printf("|             Please input one service (ONLY the number) each time:            |\n");
    printf("--------------------------------------------------------------------------------\n\n");

    while (choice != 7)
    {
        // printf("> ");
        scanf("%d", &choice);
        printf("\n");

        switch (choice)
        {
        case 1:
            Connect();
            break;
        case 2:
            Disconnect();
            SendMessages();
            break;
        case 3:
        case 4:
        case 5:
        case 6:
            SendMessages();
            break;
        case 7:
            Quit();
            break;
        default:
            fprintf(stderr, "--------------------------------------------------------------------------------\n");
            fprintf(stderr, "| Error:      The service that you input is incorrect. Please input again.     |\n");
            fprintf(stderr, "--------------------------------------------------------------------------------\n\n");
            break;
        }
    }
    return;
}

int main(void)
{
    // Initially, the connection status is set to be disconnected.
    is_connected = 0;

    UserInterface();
    return 0;
}