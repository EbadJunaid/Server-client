// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h> // Required for working with directories
#include <time.h>
#include<stdbool.h>


#define PORT 8080
#define MAX_MEMORY 3000000000 // 3GB
#define BUFFER_SIZE 1024

// Function to create a directory for the client if it doesn't exist

typedef struct
{
    char folder_path[256];
    int allocated_memory;
    int used_memory;
    char email[128];
} ClientData;


void setup_client_folder(ClientData *client)
{
    // Create a folder for each client

    char *email_prefix = strtok(client->email, "@");
    
    // Set the folder path to be based on email prefix
    sprintf(client->folder_path, "clients-data/%s", email_prefix);


   // sprintf(client->folder_path, "client_%d", client_id);
    mkdir(client->folder_path, 0777); // Creates directory with read/write permissions
    client->allocated_memory = MAX_MEMORY;
    client->used_memory = 0;

    // Calculate the size of the folder (files in the folder)
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    // Open the client folder
    dir = opendir(client->folder_path);
    if (dir == NULL) {
        perror("Unable to open client folder");
        return;
    }

    // Loop through all the files in the folder
    while ((entry = readdir(dir)) != NULL) {
        // Skip the "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full path of the file
        char file_path[512];
        sprintf(file_path, "%s/%s", client->folder_path, entry->d_name);

        // Get the file's information
        if (stat(file_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            // Add the file size to the used memory
            client->used_memory += file_stat.st_size;
        }
    }

    closedir(dir);
}



void view_files(int client_socket, ClientData* client)
{
    struct dirent *entry;
    struct stat file_stat;
    char file_path[512];
    DIR *dir = opendir(client->folder_path);

    if (dir == NULL) {
        perror("Unable to open client folder");
        send(client_socket, "Unable to access your folder\n", 30, 0);
        return;
    }

    // Buffer to store response
    char response[2048];  // Increased size to accommodate more file data
    strcpy(response, "Your files:\n");

    // Traverse through all files in the folder
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full file path
        sprintf(file_path, "%s/%s", client->folder_path, entry->d_name);

        // Get file information
        if (stat(file_path, &file_stat) == 0) {
            // Get file size
            long file_size = file_stat.st_size;

            // Get modification time in 12-hour format (AM/PM)
            char time_str[50];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %I:%M:%S %p", localtime(&file_stat.st_mtime));

            // Prepare file info string
            char file_info[256];
            sprintf(file_info, "File: %s, Size: %ld bytes, Creation Time: %s\n", entry->d_name, file_size, time_str);

            // Append file info to response
            strcat(response, file_info);
        }
    }

    closedir(dir);

    // Send the response to the client
    send(client_socket, response, strlen(response), 0);
}



int check_user(const char *email, char *stored_password) 
{
    FILE *file = fopen("passwords.txt", "r");
    if (file == NULL) {
        return 0; // File doesn't exist or can't be opened
    }

    char file_email[128], file_password[128];
    while (fscanf(file, "%s %s", file_email, file_password) != EOF) {
        if (strcmp(email, file_email) == 0) {
            strcpy(stored_password, file_password);
            fclose(file);
            return 1; // User exists
        }
    }

    fclose(file);
    return 0; // User does not exist
}




// Function to register a new user
void register_user(int client_socket, ClientData *client, char *email, char *password)
{
    FILE *file = fopen("passwords.txt", "a");
    if (file == NULL) {
        send(client_socket, "Error: Cannot open passwords file\n", 35, 0);
        return;
    }

    char stored_password[128];
    
    if (check_user(email, stored_password) == 0)
    {
        fprintf(file, "%s %s\n", email, password);
        fclose(file);
        strcpy(client->email, email); // Save the email in the client data
        setup_client_folder(client); 
        send(client_socket, "Signup successful\n", 18, 0);
    }
    else
    {
        send(client_socket, "Email already exists Please Login\n", 34, 0);
    }

   
}



// Function to handle login
int login_user(int client_socket, ClientData *client, char *email, char *password)
{
    char stored_password[128];
    
    if (!check_user(email, stored_password)) {
        send(client_socket, "Email not found\n", 16, 0);
        return 0;
    }

    if (strcmp(stored_password, password) == 0) {
        strcpy(client->email, email); // Save email in client data after successful login
        setup_client_folder(client);
        send(client_socket, "Login successful\n", 17, 0);
        return 1;
    } else {
        send(client_socket, "Incorrect password\n", 19, 0);
        return 0;
    }
}



void send_file_contents(int socket, const char *file_path,int file_size) 
{
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("Error opening file for download");
        return;
    }

    char file_buffer[BUFFER_SIZE];
    memset(file_buffer, 0, BUFFER_SIZE);
    size_t bytes_read;

    char ack_msg[14] = {0};

    // Read file in chunks and send it to the server
    printf("In the send_file_contents in server\n");
    int total_sending_bytes = 0;
    while ((bytes_read = fread(file_buffer, sizeof(char), BUFFER_SIZE, file)) > 0)
    {
        int bytes_send = send(socket, file_buffer, bytes_read, 0);
        if (bytes_send < 0)
        {
            perror("Error sending data");
            fclose(file);
            return;
        }
        printf("Bytes_send is : %i \n",bytes_send);

        recv(socket,ack_msg,sizeof(ack_msg),0);

        if (ack_msg <= 0 || strncmp(ack_msg,"chunk received", 14) != 0)
        {
            // If ACK not received, resend the chunk
            fseek(file, -bytes_read, SEEK_CUR);  // Go back to resend the same chunk
            printf("Incomplete chuck sported \n");
        }
        else
        {
            printf("Chunk sent successfully\n");
        }

        total_sending_bytes += bytes_send;
        if(total_sending_bytes == file_size)
        {
            printf("Full file is sended succesffuly\n");
        }
        memset(file_buffer, 0, BUFFER_SIZE);  // Clear the buffer
    }
    //printf("%c",'b');

    fclose(file);

    if(total_sending_bytes == file_size)
    {
        printf("Full file is sended succesffuly and loop is terminated\n");
    }


    if(total_sending_bytes != file_size)
    {
        printf("Full file is not sended \n");
    }   

}


void handle_client(int* client_socket,int server_fd, ClientData* client)
{
    char buffer[1024] = {0}; 
    int receive = 0;
    receive = recv(*client_socket, buffer, sizeof(buffer), 0);

    if(receive <= 0)
    {
        printf("Command received failed\n");
        send(*client_socket,"Command received failed",23,0);
    }

    send(*client_socket,"Command received successfully\n",30,0);
    printf("Command received \n");



    // Command format: $UPLOAD$file_path
    if (strncmp(buffer, "$UPLOAD$", 8) == 0)
    {
        //send(*client_socket, "command received\n", 17, 0);
        int file_size = 0;
        recv(*client_socket,&file_size,sizeof(file_size),0);
        send(*client_socket, "size received\n", 14, 0);

        
        char file_path[512];
        sscanf(buffer + 8, "%s", file_path); // Extract file path from command


        if (client->used_memory + file_size > MAX_MEMORY) {
            char error_msg[128];
            sprintf(error_msg, "Memory full! You can upload only %ld bytes more\n", MAX_MEMORY - client->used_memory);
            send(*client_socket, error_msg, strlen(error_msg), 0);
            //fclose(file);
            return;
        }


        send(*client_socket,"File is Uploading",17,0);

        //Allocate memory and save file in the client's folder
        char file_dest[512];
        sprintf(file_dest, "%s/%s", client->folder_path, strrchr(file_path, '/') + 1); // Extract file name
        FILE *dest_file = fopen(file_dest, "wb");

      
        char file_buffer[BUFFER_SIZE];
        size_t bytes_received;
        while ((bytes_received = recv(*client_socket, file_buffer, BUFFER_SIZE, 0)) > 0)
        {
           
            if(bytes_received < BUFFER_SIZE )
            {
                fwrite(file_buffer, sizeof(char), bytes_received, dest_file);
                memset(file_buffer, 0, BUFFER_SIZE);  // Clear the buffer
                fflush(dest_file);
                break;
            }

            // Write the received contents to the file
            fwrite(file_buffer, sizeof(char), bytes_received, dest_file);
            memset(file_buffer, 0, BUFFER_SIZE);  // Clear the buffer
        }
        
        fclose(dest_file);

        client->used_memory += file_size;
        send(*client_socket, "File uploaded successfully\n", 27, 0);
        printf("Upload \n");

    }


   else if (strncmp(buffer, "$VIEW$", 6) == 0)
    {
        view_files(*client_socket, client); // Call the view_files function to display all files in the client's folder
        recv(*client_socket,buffer,sizeof(buffer),0); // confirmation msg that the client received the output
    }


    else if (strncmp(buffer, "$DOWNLOAD$", 10) == 0)
    {
        char file_name[512] = {0};
        sscanf(buffer + 10, "%s", file_name); // Extract file name from command

        char file_path[512] = {0};
        sprintf(file_path, "%s/%s", client->folder_path, file_name); // Full file path in client's folder


        // Check if the file exists
        FILE *file = fopen(file_path, "rb");
        if (file == NULL) {
            send(*client_socket, "File does not exist\n", 21, 0);
            return;
        }

        send(*client_socket, "File exists in the server\n", 26, 0);
        memset(buffer, 0, BUFFER_SIZE);
        recv(*client_socket,buffer,BUFFER_SIZE,0); // verification msg 

        // Calculating the size of the file :

        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        
        // sending the file size to the client :
        memset(buffer, 0, BUFFER_SIZE);
        send(*client_socket,&file_size, sizeof(file_size), 0);
        recv(*client_socket,buffer,BUFFER_SIZE,0);
    
        // Now sending the file contents to the client with checks :

        printf("Before the send_file_contents in server\n");
        send_file_contents(*client_socket, file_path,file_size);
        printf("After the send_file_contents in server\n");
        printf("the file_size in the server is : %lu \n",file_size);
        recv(*client_socket,buffer,sizeof(buffer),0);
    }


    else if(strncmp(buffer, "$SIGNUP$", 8) == 0)
    {
        char email[128], password[128];
        sscanf(buffer + 8, "%[^$]$%s", email, password);   // Extract the password and email 
        register_user(*client_socket, client, email, password);
    }


    else if (strncmp(buffer, "$LOGIN$", 7) == 0)
    {
        char email[128], password[128];
        sscanf(buffer + 7, "%[^$]$%s", email, password); // Extract email and password
        if (!login_user(*client_socket, client, email, password))
        {
            return; // Exit if login fails
        }
    }
    
    
    else if(strncmp(buffer, "$EXIT$", 6) == 0)
    {
        usleep(0.5);
        send(*client_socket,"Closing the Client-socket\n",26,0);
        close(*client_socket);
        *client_socket = -1;
        recv(*client_socket,buffer,sizeof(buffer),0);   // confirmation msg that the client received the output

    }

    else
    {
       usleep(10000);
       send(*client_socket, "Command not found \n", 17, 0);
       recv(*client_socket,buffer,sizeof(buffer),0);   // confirmation msg that the client received the output
    }

}


int main()
{
    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_id = 0;
    char buffer[1024] = {};


    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    // Accept incoming client connections
    if ((client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
        perror("Accept failed");
        //continue;
    }




    ClientData client;



    // Continuous loop to receive commands from the client
    while (true)
    {
           
        handle_client(&client_socket,server_fd,&client);

        if(client_socket < 0)
        {
            printf("the client socket is closed \n");
            system("clear");
            printf("Now establing a new connection\n");
            if ((client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len)) < 0)
            {
                perror("Accept failed");
            }

        }
    }

    // Closing the connection
    close(server_fd);
    return 0;
}
