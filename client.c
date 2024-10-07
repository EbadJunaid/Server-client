#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include<stdbool.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_PORT 8080


// Function to send file contents to the server (for uploads)
void send_file_contents(int socket, const char *file_path) 
{
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Error opening file for upload");
        return;
    }

    char file_buffer[BUFFER_SIZE];
    memset(file_buffer,0,BUFFER_SIZE);
    size_t bytes_read;

    // Read file in chunks and send it to the server
    while ((bytes_read = fread(file_buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
        send(socket, file_buffer, bytes_read, 0);
        memset(file_buffer, 0, BUFFER_SIZE);  // Clear the buffer
    }

    fclose(file);
    // After sending the file, send the "END" signal to mark completion
    //send(socket, "END", 3, 0);
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

    // // Send the full command to the server 
    // send(socket, command, strlen(command), 0);

    
    // // Wait for the verification msg from the server that it receives the command or not :
    // recv(socket,buffer,sizeof(buffer),0);
    // printf("%s \n",buffer);

    //Now sending of the command is done 

    if (strncmp(command, "$UPLOAD$", 8) == 0)
    {
        char *file_path = command + 8;

        FILE *file = fopen(file_path, "rb");
        if (file == NULL)
        {
            printf("File does not exist: %s\n", file_path);
            return;
        }

        
        // Send the full command to the server 
        send(socket, command, strlen(command), 0);

    
        // Wait for the verification msg from the server that it receives the command or not :
        recv(socket,buffer,sizeof(buffer),0);
        printf("%s \n",buffer);
        

        // send(socket, command, strlen(command), 0);  // Send the upload command with the file path
        // read(socket, buffer, BUFFER_SIZE);     // Wait for server response about command receiving
        //printf("%s\n", buffer);


        // Calculate the file size
        fseek(file, 0, SEEK_END);
        int file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        fclose(file);

       
        send(socket,&file_size,sizeof(file_size),0);
        read(socket, buffer, BUFFER_SIZE); // Wait for server to get the size of the file 
        memset(buffer,0,BUFFER_SIZE);
        read(socket,buffer,BUFFER_SIZE);


        if(strncmp(buffer, "Memory full!", 11) == 0)
        {
            printf("Server response: %s\n", buffer);
            return;
        }

        send_file_contents(socket, file_path);
        memset(buffer,0,BUFFER_SIZE);
        read(socket, buffer, BUFFER_SIZE);     // Wait for server response about file update
        printf("Server response: %s\n", buffer);

        //printf("Upload \n");

    }

    else if(strncmp(command, "$DOWNLOAD$", 10) == 0)
    {

        // Send the full command to the server 
        send(socket, command, strlen(command), 0);

    
        // Wait for the verification msg from the server that it receives the command or not :
        recv(socket,buffer,sizeof(buffer),0);
        printf("%s \n",buffer);


        // Waiting for server resposne that the file exists or not :
        int rec = 0;
        memset(buffer,0,BUFFER_SIZE);
        rec = recv(socket, buffer,BUFFER_SIZE, 0);

        if(rec <= 0)
        {
            printf("Issue with receiving bytes : ");
        }

        if(strncmp(buffer, "File does not exist\n", 21) == 0)
        {
            printf("Server response: %s\n", buffer);
            return;
        }

        send(socket,"I got it that files exists\n",27,0);

        // Now waiting for the file size :

        size_t file_size = 0;
        recv(socket, &file_size, sizeof(file_size), 0);
        send(socket,"File size is received\n",22,0);
        printf("After getting the file size\n");

        memset(buffer,0,BUFFER_SIZE);    

        // Now receiving the contents of the file :

        char file_name[128] = {0};
        sscanf(command + 10, "%s", file_name);

        char file_dest[128] = {0};
        sprintf(file_dest, "%s/%s","/home/ebad/Downloads/down", file_name); // Extract file name
        FILE *dest_file = fopen(file_dest, "wb");

        char file_buffer[BUFFER_SIZE];
        memset(file_buffer, 0, BUFFER_SIZE); 
        size_t bytes_received = 0;
        size_t total_byte_received = 0;
        printf("Before the receiving of the file in client\n");
        while (true)
        {
            
            bytes_received = recv(socket, file_buffer, BUFFER_SIZE,0);
            if (bytes_received < 0)
            {
                perror("Error receiving data");
                fclose(dest_file);
                return;
            }

            total_byte_received += bytes_received;
            printf("the bytes received : %lu \n",bytes_received);

            int ack_sent = send(socket,"chunk received",14, 0);
            if (ack_sent < 0)
            {
                perror("Error sending ACK");
                fclose(dest_file);
                return;
            }

            // Write the received contents to the file
            fwrite(file_buffer, sizeof(char), bytes_received, dest_file);
            memset(file_buffer, 0, BUFFER_SIZE);  // Clear the buffer

            if(total_byte_received == file_size )
            {
                printf("%s","Insider the terminating condition of client \n");
                break;
            }
            else
            {
                printf("File size : %lu    Total_Bytes : %lu\n",file_size,total_byte_received);
            }

           
        }
        printf("After the receiving of the file in client\n");
       
        fclose(dest_file);

        if(total_byte_received != file_size)
        {
            printf("Full file is not received :\n");
        }

        send(socket,"I downloaded the file \n",20,0);

        //recv(socket, buffer, sizeof(buffer), 0);
        printf("File downloaded successfully\n");
        return;

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
