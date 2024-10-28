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
#include<magic.h>
#include <zlib.h>
#include"encode(compression).h"


#define PORT 8080
#define MAX_MEMORY 3000000000 // 3GB
#define BUFFER_SIZE 32768
#define CHUNK 131072  // Larger chunk size (256KB)


// Function to create a directory for the client if it doesn't exist

typedef struct
{
    char folder_path[256];
    size_t allocated_memory;
    size_t used_memory;
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
        if(bytes_send < BUFFER_SIZE)
        {
            usleep(10000);// IN MICROSECONDS 
            send(socket,"ENDING THE FILE",3,0);
        }
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


// compression algorithm for normal file :

int compress_file(const char *source, const char *destination)
{
    FILE *sourceFile = fopen(source, "rb");
    if (!sourceFile) {
        perror("Source file error");
        return -1;
    }

    FILE *destFile = fopen(destination, "wb");
    if (!destFile) {
        perror("Destination file error");
        fclose(sourceFile);
        return -1;
    }

    unsigned char *in = (unsigned char *)malloc(CHUNK);
    unsigned char *out = (unsigned char *)malloc(CHUNK);
    if (in == NULL || out == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(sourceFile);
        fclose(destFile);
        return -1;
    }

    z_stream strm = {0};
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
        fprintf(stderr, "deflateInit failed\n");
        free(in);
        free(out);
        fclose(sourceFile);
        fclose(destFile);
        return -1;
    }

    int flush;
    size_t bytesRead;
    do {
        bytesRead = fread(in, 1, CHUNK, sourceFile);
        if (ferror(sourceFile)) {
            deflateEnd(&strm);
            free(in);
            free(out);
            fclose(sourceFile);
            fclose(destFile);
            return -1;
        }
        flush = feof(sourceFile) ? Z_FINISH : Z_NO_FLUSH;

        strm.avail_in = bytesRead;
        strm.next_in = in;

        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            deflate(&strm, flush);
            size_t bytesWritten = CHUNK - strm.avail_out;
            fwrite(out, 1, bytesWritten, destFile);
        } while (strm.avail_out == 0);

    } while (flush != Z_FINISH);

    deflateEnd(&strm);
    free(in);
    free(out);
    fclose(sourceFile);
    fclose(destFile);

    return 0;
}


int decompress_file(const char *source, const char *destination)
{
    FILE *sourceFile = fopen(source, "rb");
    if (!sourceFile) {
        perror("Source file error");
        return -1;
    }

    FILE *destFile = fopen(destination, "wb");
    if (!destFile) {
        perror("Destination file error");
        fclose(sourceFile);
        return -1;
    }

    unsigned char *in = (unsigned char *)malloc(CHUNK);
    unsigned char *out = (unsigned char *)malloc(CHUNK);
    if (in == NULL || out == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(sourceFile);
        fclose(destFile);
        return -1;
    }

    z_stream strm = {0};
    if (inflateInit(&strm) != Z_OK) {
        fprintf(stderr, "inflateInit failed\n");
        free(in);
        free(out);
        fclose(sourceFile);
        fclose(destFile);
        return -1;
    }

    int ret;
    size_t bytesRead;
    do {
        bytesRead = fread(in, 1, CHUNK, sourceFile);
        if (ferror(sourceFile)) {
            inflateEnd(&strm);
            free(in);
            free(out);
            fclose(sourceFile);
            fclose(destFile);
            return -1;
        }

        strm.avail_in = bytesRead;
        strm.next_in = in;

        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;

            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                inflateEnd(&strm);
                free(in);
                free(out);
                fclose(sourceFile);
                fclose(destFile);
                return -1;
            }

            if (ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                fprintf(stderr, "Decompression error: %d\n", ret);
                inflateEnd(&strm);
                free(in);
                free(out);
                fclose(sourceFile);
                fclose(destFile);
                return -1;
            }

            size_t bytesWritten = CHUNK - strm.avail_out;
            fwrite(out, 1, bytesWritten, destFile);

        } while (strm.avail_out == 0);

    } while (ret != Z_STREAM_END);

    inflateEnd(&strm);
    free(in);
    free(out);
    fclose(sourceFile);
    fclose(destFile);

    return ret == Z_STREAM_END ? 0 : -1;
}




int is_image(const char *file_type)
{
    return (strstr(file_type, "image/") != NULL);
}


// Function to check if the file is a video
int is_video(const char *file_type)
{
    return (strstr(file_type, "video/") != NULL);
}



int file_type(const char* filename)
{
    magic_t magic;

    // Initialize magic library
    magic = magic_open(MAGIC_MIME_TYPE); // MIME type detection
    if (magic == NULL)
    {
        printf("Error: Could not initialize magic library\n");
        return 1;
    }

    //const char* magic_db ="C:\\Users\\bilal\\vcpkg\\installed\\x64-windows\\share\\libmagic\\misc\\magic.mgc";

    // Load the default magic database
    if (magic_load(magic, NULL) != 0)
    {
        printf("Error: Could not load magic database - %s\n", magic_error(magic));
        magic_close(magic);
        return 1;
    }

    // Get MIME type of the file
    const char *file_type = magic_file(magic, filename);
    // if (file_type == NULL) {
    //     printf("Error: Could not determine file type - %s\n", magic_error(magic));
    //     magic_close(magic);
    //     return 1; // 
    // }

    printf("File type: %s\n", file_type);

    // Check if it's an image
    if (is_image(file_type)) {
        printf("The file is an image.\n");
        magic_close(magic);
        return 1;
    }
    // Check if it's a video
    else if (is_video(file_type)) {
        printf("The file is a video.\n");
        magic_close(magic);
        return 2;
    }
    // If not an image or video
    else {
        printf("The file is neither an image nor a video.\n");
        magic_close(magic);
        return 3;
    }
    
}








void handle_client(int* client_socket,int server_fd, ClientData* client)
{
    char buffer[BUFFER_SIZE] = {0}; 
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
        
        size_t original_file_size = 0;
        size_t file_size = 0;
        int file_type = 0; 
        recv(*client_socket,&original_file_size,sizeof(original_file_size),0);// receiving original file size 
        send(*client_socket, "size received\n", 14, 0);

        
        char file_path[512];
        sscanf(buffer + 8, "%s", file_path); // Extract file path from command


        if (client->used_memory + original_file_size > MAX_MEMORY) {
            char error_msg[128];
            sprintf(error_msg, "Memory full! You can upload only %ld bytes more\n", MAX_MEMORY - client->used_memory);
            send(*client_socket, error_msg, strlen(error_msg), 0);
            //fclose(file);
            return;
        }


        send(*client_socket,"File is Uploading",17,0);


        //getting the file type (image,video and normal)

        recv(*client_socket,&file_type,sizeof(file_type),0); 
        send(*client_socket, "File type getted successfully\n", 30, 0);



        // getting the size of the compressed file :

        usleep(10000);



        recv(*client_socket,&file_size,sizeof(file_size),0); 
        send(*client_socket, "compressed size received\n", 25, 0);



        //Allocate memory and save file in the client's folder
        char file_dest[512];
        sprintf(file_dest, "%s/%s", client->folder_path, strrchr(file_path, '/') + 1); // Extract file name
        FILE *dest_file = fopen(file_dest, "wb");

      
        char file_buffer[BUFFER_SIZE];
        size_t bytes_received;

        size_t total_byte_received = 0;
        printf("Before receiving the file in client\n");

        while (true)
        {
            int chuck_size = 0;
            while(chuck_size < BUFFER_SIZE)
            {
                bytes_received = recv(*client_socket, file_buffer, BUFFER_SIZE, 0);
                if((strcmp(file_buffer, "END") == 0))
                {
                    break;
                }
                if (bytes_received < 0)
                {
                    perror("Error receiving data");
                    fclose(dest_file);
                    return;
                }

                   // Write the received contents to the file
                fwrite(file_buffer, sizeof(char), bytes_received, dest_file);
                memset(file_buffer, 0, BUFFER_SIZE);  // Clear the buffer


                chuck_size += bytes_received;
            }
            

            total_byte_received += chuck_size;
            printf("Bytes received: %i\n", chuck_size);

            int ack_sent = send(*client_socket, "chunk received", 14, 0);
            if (ack_sent < 0) {
                perror("Error sending ACK");
                fclose(dest_file);
                return;
            }

         
            if (total_byte_received == file_size || total_byte_received> file_size) {
                printf("File download completed.\n");
                break;
            }
            else {
                printf("File size: %lu   Total_Bytes: %lu\n", file_size, total_byte_received);
            }
        }

        printf("File received.\n");

        
        fclose(dest_file);

        client->used_memory += original_file_size;

        if (total_byte_received != file_size)
        {
            printf("Full file not received.\n");
        }

       

        //Now decompressing the file

        char compressed[512];

        char* dot = strrchr(file_dest,'.');
        char* dash = strrchr(file_dest,'-');

        // Get the part before the last '-'
        int len_before_dash = dash - file_dest;
        char before_dash[512];
        strncpy(before_dash, file_dest, len_before_dash);
        before_dash[len_before_dash] = '\0'; // Null-terminate the string
        
        // Get the part after the last '.'
        char* after_dot;
        if(dot != NULL)
        {
            after_dot = strdup(dot);
            sprintf(compressed, "%s%s", before_dash, after_dot);
        }
        else
        {
            sprintf(compressed, "%s", before_dash);
        }
        // Duplicate the string from last '.'

        // Create a new string by merging the two
        
       
        





        if(file_type == 1)
        {
            printf("it is an image but algorithm is not available\n");
            // if(decompress_file(file_dest,compressed) == 0)
            // {
            //     printf("file decompressed successfully");
            // }
            // else
            // {
            //      printf("file decompressed problem");
            // }

            base64_decode_file(file_dest,compressed);
            printf("file decompressed successfully\n");

           
        }
        
        else if(file_type == 2)
        {
            printf("it is an video but algorithm is not available\n");
            // if(decompress_file(file_dest,compressed) == 0)
            // {
            //     printf("file decompressed successfully");
            // }
            // else
            // {
            //      printf("file decompressed problem");
            // }

            base64_decode_file(file_dest,compressed);
            printf("file decompressed successfully\n");

           
        }

        else
        {
            // if(decompress_file(file_dest,compressed) == 0)
            // {
            //     printf("file decompressed successfully");
            // }
            // else
            // {
            //      printf("file decompressed problem");
            // }

            base64_decode_file(file_dest,compressed);
            printf("file decompressed successfully\n");

           
        }
        

        send(*client_socket, "File uploaded successfully\n", 27, 0);
        remove(file_dest);

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




        int type = file_type(file_path);
        send(*client_socket, &type, sizeof(type), 0);
        recv(*client_socket, buffer, BUFFER_SIZE, 0); // Wait for server to get the type of the file
        memset(buffer, 0, BUFFER_SIZE);


       char compressed_file[512];
       snprintf(compressed_file, sizeof(compressed_file), "%s(compressed)", file_path);


        size_t file_size = 0;

        if(type == 1)
        {   
            printf("It is an image but algorithm is not available \n");
            //compress_file(file_path,compressed_file,&compressed_size);
            base64_encode_file(file_path,compressed_file,&file_size);
            printf("File compressed successfully.\n");
        }

        else if(type == 2)
        {   
            printf("It is a video but algorithm is not available \n");
            //compress_file(file_path,compressed_file,&compressed_size);
            base64_encode_file(file_path,compressed_file,&file_size);
            printf("File compressed successfully.\n");
        }

        else
        {   
            //compress_file(file_path,compressed_file,&compressed_size);
            base64_encode_file(file_path,compressed_file,&file_size);
            printf("File compressed successfully.\n");
        }


        // Calculating the size of the file :

        // fseek(file, 0, SEEK_END);
        // size_t file_size = ftell(file);
        // fseek(file, 0, SEEK_SET);

        
        // sending the file size to the client :
        memset(buffer, 0, BUFFER_SIZE);
        send(*client_socket,&file_size, sizeof(file_size), 0);
        recv(*client_socket,buffer,BUFFER_SIZE,0);
    
        // Now sending the file contents to the client with checks :

        printf("Before the send_file_contents in server\n");
        send_file_contents(*client_socket, compressed_file,file_size);
        printf("After the send_file_contents in server\n");
        printf("the file_size in the server is : %lu \n",file_size);

        
        recv(*client_socket,buffer,sizeof(buffer),0);
        remove(compressed_file);

        
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
    char buffer[BUFFER_SIZE] = {};


    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int buffer_size = BUFFER_SIZE;

    setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(BUFFER_SIZE));
    setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(BUFFER_SIZE));



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
