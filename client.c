#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <magic.h>
#include <zlib.h>
#include "encode-decode.h"



#define PORT 8080
#define BUFFER_SIZE 32768 // almost 32kb
#define SERVER_PORT 8080



// below 3 function tells me which file type(image,video or normal file ) so that I can run the specialized compresssion algorithm

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





// Function to send file contents to the server (for uploads)
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
            usleep(10000);// IN MILLISECONDS 
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


void handling_command(int socket, const char *command)
{

    if(strncmp(command,"$CLEAR$",7) == 0)
    {
        system("clear");
        return;
    }

    char buffer[BUFFER_SIZE];
    memset(buffer,0,BUFFER_SIZE);


    if (strncmp(command, "$UPLOAD$", 8) == 0)
    {
        char *file_path = command + 8;

        FILE *file = fopen(file_path, "rb");
        if (file == NULL)
        {
            printf("File does not exist: %s\n", file_path);
            return;
        }


        char new_command[512]; // Ensure this is large enough
        char *dot = strrchr(file_path, '.'); // Find the last dot in the filename
        if (dot != NULL) {
            // If there is an extension
            snprintf(new_command, sizeof(new_command), "$UPLOAD$%.*s-compressed%s", 
                    (int)(dot - file_path), file_path, dot);
        } else {
            // If there is no extension, just append '-compressed'
            snprintf(new_command, sizeof(new_command), "$UPLOAD$%s-compressed", file_path);
        }



        
        // Send the full command to the server 
        send(socket, new_command, strlen(new_command), 0);

    
        // Wait for the verification msg from the server that it receives the command or not :
        memset(buffer,0,BUFFER_SIZE);
        recv(socket,buffer,sizeof(buffer),0);
        printf("%s \n",buffer);
        

       

        // Calculate the file size
        fseek(file, 0, SEEK_END);
        size_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        fclose(file);


       
        send(socket,&file_size,sizeof(file_size),0);
        read(socket, buffer, BUFFER_SIZE); // Wait for server to get the original size of the file 
        memset(buffer,0,BUFFER_SIZE);
        read(socket,buffer,BUFFER_SIZE);// Waiting for the server that the memory for the file is available or not


        if(strncmp(buffer, "Memory full!", 11) == 0)
        {
            printf("Server response: %s\n", buffer);
            return;
        }


         // Finding the type of the file(image,video,normal)

        int type = file_type(file_path);
        send(socket, &type, sizeof(type), 0);
        recv(socket, buffer, BUFFER_SIZE, 0); // Wait for server to get the type of the file
        memset(buffer, 0, BUFFER_SIZE);



        char* compressed_file = new_command + 8;

        size_t compressed_size = 0;

        if(type == 1)
        {   
            printf("It is an image but algorithm is not available \n");
            if(compress_file_zlib(file_path,compressed_file,&compressed_size) == 0)
            {
                printf("File compressed zlib successfully.\n");  
            }
            //base64_encode_file(file_path,compressed_file,&compressed_size);
            //printf("File compressed successfully.\n");
        }

        else if(type == 2)
        {   
            printf("It is a video but algorithm is not available \n");
            if(compress_file_zlib(file_path,compressed_file,&compressed_size) == 0)
            {
                printf("File compressed zlib successfully.\n");  
            }
            //base64_encode_file(file_path,compressed_file,&compressed_size);
            //printf("File compressed successfully.\n");
        }

        else
        {   
            if(compress_file_zlib(file_path,compressed_file,&compressed_size) == 0)
            {
                printf("File compressed zlib successfully.\n");  
            }
            //base64_encode_file(file_path,compressed_file,&compressed_size);
            //printf("File compressed successfully.\n");
        }



        // the compresion function gives the size of the compressed file now we just send it to the server:
        send(socket, &compressed_size, sizeof(compressed_size), 0);
        recv(socket, buffer, BUFFER_SIZE, 0); 
        memset(buffer, 0, BUFFER_SIZE);




        send_file_contents(socket, compressed_file,compressed_size);
        memset(buffer,0,BUFFER_SIZE);
        read(socket, buffer, BUFFER_SIZE);     // Wait for server response about file update
        printf("Server response: %s\n", buffer);
        remove(compressed_file);

        //printf("Upload \n");

    }

    else if(strncmp(command, "$DOWNLOAD$", 10) == 0)
    {
        // Send the full command to the server
        send(socket, command, strlen(command), 0);

        // Wait for the verification msg from the server
        recv(socket, buffer,BUFFER_SIZE, 0);
        printf("%s \n", buffer);

        // Waiting for server response that the file exists or not
        int rec = 0;
        memset(buffer, 0, BUFFER_SIZE);
        rec = recv(socket, buffer, BUFFER_SIZE, 0);

        if (rec <= 0) {
            printf("Issue with receiving bytes: ");
        }

        if (strncmp(buffer, "File does not exist\n", 21) == 0) {
            printf("Server response: %s\n", buffer);
            return;
        }

        send(socket, "I got it that file exists\n", 27, 0);

        // Now waiting for the file type(image,video or some random file)

        int file_type = 0;
        recv(socket,&file_type,sizeof(file_type),0);
        send(socket,"File type getted successfully\n",30,0);




        // Now waiting for the file size
        size_t file_size = 0;
        recv(socket, &file_size, sizeof(file_size), 0);
        send(socket, "File size is received\n", 22, 0);
        printf("After getting the file size\n");

        memset(buffer, 0, BUFFER_SIZE);

        // Now receiving the contents of the file
        char file_name[128] = {0};
        sscanf(command + 10, "%s", file_name);

        char file_dest[128] = {0};
        //sprintf(file_dest, "/home/ebad/Downloads/down%s", file_name); // Change this to a valid Windows path
        sprintf(file_dest, "%s%s%s","/home/ebad/Downloads/down/", file_name,"(compressed)"); // Extract file name
        FILE *dest_file = fopen(file_dest, "wb");

        char file_buffer[BUFFER_SIZE];
        memset(file_buffer, 0, BUFFER_SIZE);
        size_t bytes_received = 0;
        size_t total_byte_received = 0;
        printf("Before receiving the file in client\n");
        while (true)
        {
            int chuck_size = 0;
            while(chuck_size < BUFFER_SIZE)
            {
                bytes_received = recv(socket, file_buffer, BUFFER_SIZE, 0);
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

            int ack_sent = send(socket, "chunk received", 14, 0);
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
                printf("File size: %lu    Total_Bytes: %lu\n", file_size, total_byte_received);
            }
        }


        printf("File received.\n");

        fclose(dest_file);

        if (total_byte_received != file_size) {
            printf("Full file not received.\n");
        }
        

        // Now decompressing the file

        char original[256];
        sprintf(original, "%s%s","/home/ebad/Downloads/down/", file_name);


        if(file_type == 1)
        {
            printf("it is an image but algorithm is not available\n");
            if(decompress_file_zlib(file_dest,original) == 0)
            {
                printf("file decompressed zlib successfully \n");
            }
            else
            {
                 printf("file decompressed problem \n");
            }

            //base64_decode_file(file_dest,original);
            //printf("file decompressed successfully\n");

           
        }
        
        else if(file_type == 2)
        {
            printf("it is an video but algorithm is not available\n");
            if(decompress_file_zlib(file_dest,original) == 0)
            {
                printf("file decompressed zlib successfully \n");
            }
            else
            {
                printf("file decompressed problem \n");
            }

            //base64_decode_file(file_dest,original);
            //printf("file decompressed successfully\n");

           
        }

        else
        {
            if(decompress_file_zlib(file_dest,original) == 0)
            {
                printf("file decompressed zlib successfully \n");
            }
            else
            {
                printf("file decompressed problem \n");
            }

            //base64_decode_file(file_dest,original);
            //printf("file decompressed successfully\n");

           
        }


       
        
        send(socket, "I downloaded the file\n", 22, 0);
        remove(file_dest);
        
    }


   else
    { 
        // // Send the command to upload the file (including the file path)
        // send(socket, command, strlen(command), 0);

        // Wait for server's response before proceeding with the file upload

        // Send the full command to the server 
        send(socket, command, strlen(command), 0);

        
        // Wait for the verification msg from the server that it receives the command or not :
        recv(socket,buffer,sizeof(buffer),0);
        printf("%s \n",buffer);

        memset(buffer,0,BUFFER_SIZE);
        int rec = recv(socket, buffer, sizeof(buffer), 0);

        if(rec <= 0)
        {
            printf("Issue with receiving bytes in the last else : ");
        }

        send(socket,"Output received !\n",18,0);

        if(strncmp(buffer, "Closing the Client-socket\n", 26) == 0)
        {
            printf("Closing the Client-socket\n");
            close(socket);
            exit(1);
        }

        printf("Server response: %s\n", buffer);


    }   

}


void send_credentials(int socket, const char *command, const char *email, const char *password,bool* auth,char* message)
{
    char buffer[BUFFER_SIZE] = {0};
    char full_command[512];

    // Format the full command with email and password
    sprintf(full_command, "%s%s$%s", command, email, password);

    // Send the credentials(command) to the server
    send(socket, full_command, strlen(full_command), 0);

    // Wait for the verification msg from the server that it receives the command or not :
    recv(socket, buffer, sizeof(buffer), 0);
    memset(buffer,0,BUFFER_SIZE);

    // Now waiting for the server responce that the login or signup opeartion is done or not
    recv(socket, buffer, sizeof(buffer), 0);    

    if( (strncmp(buffer, "Login successful\n", 17) == 0) || (strncmp(buffer, "Signup successful\n", 18) == 0) )
    {   
        *auth = true;
        strcpy(message, buffer);
    }

    // else if(strncmp(buffer, "Signup successful\n", 18) == 0)
    // {  
    //     *auth = true;
    //     strcpy(message, buffer);
    // }

    printf("Server response: %s\n", buffer);
}



int main()
{
    int sock = 0;
    struct sockaddr_in server_addr;
    char command[512] = {};
    

    char email[128] = {};
    char password[128] = {};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error\n");
        return -1;
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Convert IP address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        printf("Invalid address or Address not supported\n");
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("Connection to server failed\n");
        return -1;
    }

    printf("Connected to server\n");
    

    // Ask the user whether to sign up or login

    bool auth = false;
    char message[128];

    while(auth == false)
    {

        printf("Do you want to signup or login? (signup/login): ");
        scanf("%s", command);


        // Ask for email and password


        // Send signup or login request to the server
        if (strcmp(command, "signup") == 0)
        {   
            printf("Enter your email: ");
            scanf("%s", email);
            printf("Enter your password: ");
            scanf("%s", password);
            send_credentials(sock, "$SIGNUP$", email, password,&auth,message);
        }
        else if (strcmp(command, "login") == 0)
        {
            
            printf("Enter your email: ");
            scanf("%s", email);
            printf("Enter your password: ");
            scanf("%s", password);
            send_credentials(sock, "$LOGIN$", email, password,&auth,message);
            
        }
    }

   system("clear");
   printf("Server response: %s\n", message);

    // Get the file path from the user
    while(1)
    {
        printf("Enter the Command: ");
        scanf("%s", command);

        // Send the file to the server
        handling_command(sock, command);
        memset(command,0,sizeof(command));
    }
   

    // Close the socket
    close(sock);
    return 0;
}
