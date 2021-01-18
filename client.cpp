// Client side C/C++ program to demonstrate Socket programming 
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h>
#include <bits/stdc++.h> 
#include <openssl/sha.h>
// #define SERVER_PORT 49152 // the port client will be connecting to
// #define CLIENT_PORT 8000
#define CHUNKSIZE 512

using namespace std;

struct download_info{
    string serverip;
    int serverport;
    int startpos;
    int endpos;
    // int numofchnks;
    string fname;
    string dest;
    string srcPath;
    long filesize;
};

typedef struct download_info downloadInfo;

string computeHash(string fname){
    FILE *fp=fopen(fname.c_str(),"r");
    if(fp==NULL){
        perror("File open");
        exit(1);
    }
    char data[CHUNKSIZE];
    SHA_CTX shactx;
    unsigned char digest[SHA_DIGEST_LENGTH];
    char output[70];
    string storedHash="";
    size_t datalen;

    SHA1_Init(&shactx);
    while(!feof(fp)){
        if(datalen=fread((void *)data,1,CHUNKSIZE,fp)!=0){
            SHA1_Init(&shactx);
            SHA1_Update(&shactx,data,datalen);
            SHA1_Final(digest,&shactx);
            for(int i=0;i<SHA_DIGEST_LENGTH;i++){
                // cout<<digest[i]<<" ";
                sprintf(output+(i*2),"%02x",digest[i]);     //binary to hex 1char=8bits=2 hex chars
            }
            // cout<<endl;
            for(int i=0; i<20; i++){
                // cout<<output[i]<<" ";
                storedHash+=output[i];
            }
            memset(output,0,sizeof(output));
            memset(digest,0,sizeof(digest));
        }
    }
    fclose(fp);
    return storedHash;
}

void *client_peer(void *argv){
    downloadInfo dinfo=*((downloadInfo *)argv);
    string server_ip=dinfo.serverip;
    int server_port=dinfo.serverport;
    cout<<server_ip<<" "<<server_port<<endl;
    int first=dinfo.startpos;
    int last=dinfo.endpos;
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 
    char buffer[1024] = {0}; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return NULL; 
    }

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(server_port);   //PORT is server port

    if(inet_pton(AF_INET, (char *)server_ip.c_str(), &serv_addr.sin_addr)<=0)     //127.0.0.1 is the address of the server 
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return NULL; 
    } 

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return NULL; 
    }


    /* send server the startpos and endpos of data to be sent*/
    string toSend;
    toSend=to_string(first)+" "+to_string(last)+" "+dinfo.fname+" "+dinfo.srcPath+" "+to_string(dinfo.filesize);
    fflush(stdout);
    send(sock, toSend.c_str(), toSend.size(), 0);
    fflush(stdout);
    cout<<"dest: "<<dinfo.dest<<" "<<dinfo.fname<<endl;
    FILE *fp=fopen((dinfo.dest+"/"+dinfo.fname).c_str(),"wb");
    int bytesTorecv=CHUNKSIZE;
    cout<<"first and last: "<<first<<" "<<last<<endl;
    fflush(stdout);
    for(int i=first; i<=last; i++){
        if(i==last && (last+1)*(long)CHUNKSIZE>dinfo.filesize){
            bytesTorecv=dinfo.filesize-(long)last*CHUNKSIZE;
        }
        if(valread = read(sock, (char *)buffer, bytesTorecv)<0){
            perror("read");
        }
        // cout<<buffer<<endl;
        fwrite(buffer,bytesTorecv,1,fp);
        memset(buffer,0,CHUNKSIZE);
    }
    fclose(fp);

    /* test communication */
    // string hello="Hello from peer's client";
    // fflush(stdout);
    // send(sock, hello.c_str(), hello.size(), 0);
    // fflush(stdout);
    // printf("client:Hello message sent\n"); 
    // if(valread = recv(sock, (char *)buffer, 1024, 0)<0){
    //     perror("recv");
    // }
    // printf("%s\n",buffer);

    return NULL;
}

void *server_peer(void * argv){
    struct sockaddr peerServer=*((struct sockaddr *)argv);
    // *peerServer=(struct sockaddr)(*(struct sockaddr *)argv);
    // delete (struct sockaddr *)argc;
    int server_fd,client_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int opt=1;
    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(server_fd, (struct sockaddr *)&peerServer, sizeof(peerServer))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client_addr;
    socklen_t addrlenC=sizeof(client_addr);
    fflush(stdout);
    if((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlenC))<0) {  //the address here should be client address                                                                                //NULL because we dont care who is connecting to us
        perror("accept");
    }

    /*code to send file to peer */
    char buffer[1024] = {0}; 
    int valread;
    if(valread = recv(client_fd , buffer, 1024, 0)<0){
        perror("recv");
        // exit(EXIT_FAILURE);
    }
    else{
        stringstream sbuf(buffer);
        string arr[5];
        string word;
        for(int i=0; i<5; i++){     //start, end, filename, filepath(src), filesize
            sbuf>>word;             // 0       1      2          3            4
            arr[i]=word;
        }
        // string fname=arr[2];
        int start=stoi(arr[0]);
        int end=stoi(arr[1]);
        long fsize=stol(arr[4]);
        /* send required data*/
        memset(buffer,0,1024);
        int bytesToSend=CHUNKSIZE;
        FILE *fp=fopen((arr[3]+"/"+arr[2]).c_str(),"rb");
        string toSend;
        for(int i=start; i<=end; i++){
            if(i==end && (end+1)*(long)CHUNKSIZE>fsize){
                bytesToSend=fsize-(long)end*CHUNKSIZE;
            }
            fseek(fp, CHUNKSIZE*i, 0);
            fread(buffer,bytesToSend,1,fp);
            // cout<<buffer;
            fflush(stdout);
            send(client_fd, buffer, bytesToSend, 0);
            fflush(stdout);
            memset(buffer,0,512);
        }
        fclose(fp);
    }

    /* test communication with peer*/
    // int valread;
    // char buffer[1024] = {0}; 
    // string hello="Hello from server";
    // valread = read(client_fd , buffer, 1024); 
    // printf("%s\n",buffer ); 
    // send(client_fd, hello.c_str(), hello.size(), 0 ); 
    // printf("Hello message sent\n"); 

    return NULL;
}

int main(int argc, char const *argv[]) 
{ 
    if(argc!=5){
        cout<<"The command needs 5 args: ./client <client_add> <client_port> <server_ip> <server_port>";
        exit(0);
    }
    string client_ip=(char *)argv[1];
    int client_port=atoi(argv[2]);
    string server_ip=(char *)argv[3];
    int server_port=atoi(argv[4]);

    int sock = 0, valread; 
    struct sockaddr_in serv_addr, client_addr; 
    string hello = "Hello from client"; 
    char buffer[1024] = {0}; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    }

    /*Bind client to a address and port*/
    client_addr.sin_family=AF_INET;
    // client_addr.sin_addr.s_addr=INADDR_ANY;
    client_addr.sin_port = htons(client_port);
    client_addr.sin_addr.s_addr=inet_addr((char *)client_ip.c_str());
    // if (bind(sock, (struct sockaddr *)&client_addr, sizeof(client_addr))<0)
    // {
    //     perror("bind failed");
    //     exit(0);
    // }

    /* server socket to connect to */
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(server_port);   //PORT is server port
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, (char *)server_ip.c_str(), &serv_addr.sin_addr)<=0)     //127.0.0.1 is the address of the server 
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 


    /*The connect() system call connects the socket referred to by the file descriptor sockfd
     to the address specified by addr.*/  

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 

    pthread_t ptid;
    // int *pclient=new int;
    // *pclient=client_fd;
    if(pthread_create(&ptid,NULL,server_peer,(struct sockaddr *)&client_addr)<0){
        perror("pthread_create()");
    }

    // int idAssigned=0;
    // if(valread = recv(sock, (int *)&idAssigned, sizeof(int), 0)<0){
    //     perror("recv");
    // }

    /* client part of peer */
    // send(sock, hello.c_str(), hello.size(), 0);
    // printf("Hello message sent\n"); 
    // valread = recv(sock, buffer, 1024, 0);
    // printf("%s\n",buffer );
    // sleep(20);

    string command;
    vector<string> parseCmd;
    string word;
    string userLoggedin="";
    bool isLoggedIn=false;
    while(1){
        getline(cin,command);
        istringstream ss(command);
        parseCmd.clear();
        // sockmap.clear();
        while(ss>>word){
            parseCmd.push_back(word);
        }
        // send(sock,command.c_str(),command.size(),0);
        if(parseCmd[0]=="create_user"){
            if(parseCmd.size()!=3){
                cout<<"Syntax: create_user <user_id> <password>"<<endl;
                continue;
            }
            send(sock,command.c_str(),command.size(),0);
            /*receive status of creation*/
            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;
        }
        else if(parseCmd[0]=="login"){
            if(parseCmd.size()!=3){
                cout<<"Syntax: login <user_id> <password>"<<endl;
                continue;
            }
            command=command+" "+client_ip+" "+to_string(client_port);       
            // cout<<command<<endl;
            send(sock,command.c_str(),command.size(),0);
            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;
            if(strcmp(buffer,"Login Successful")==0) {
                userLoggedin=parseCmd[1];
                isLoggedIn=true;
                // cout<<"userLoggedin set"<<endl;
            }

        }
        else if(parseCmd[0]=="create_group"){
            if(parseCmd.size()!=2){
                cout<<"Syntax: create_group <group_id>"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Log in to create group"<<endl;
                continue;
            }
            cout<<command<<endl;
            command=command+" "+userLoggedin;
            send(sock,command.c_str(),command.size(),0);

            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;
            
        }
        else if(parseCmd[0]=="join_group"){
            if(parseCmd.size()!=2){
                cout<<"Syntax: join_group <group_id>"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Log in to join a group"<<endl;
                continue;
            }

            command=command+" "+userLoggedin;
            send(sock,command.c_str(),command.size(),0);

            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;

        }

        else if(parseCmd[0]=="leave_group"){
            if(parseCmd.size()!=2){
                cout<<"Syntax: leave_group <group_id>"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Need to login first"<<endl;
                continue;
            }
            command=command+" "+userLoggedin;
            send(sock,command.c_str(),command.size(),0);

            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;            
        }

        else if(parseCmd[0]=="list_requests"){
            if(parseCmd.size()!=2){
                cout<<"Syntax: list_requests <group_id>"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Need to login first"<<endl;
                continue;
            }
            command=command+" "+userLoggedin;
            send(sock,command.c_str(),command.size(),0);

            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;
        }
        else if(parseCmd[0]=="accept_request"){
            if(parseCmd.size()!=3){
                cout<<"Syntax: accept_request <group_id> <user_id>"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Need to login first"<<endl;
                continue;
            }

            command=command+" "+userLoggedin;
            send(sock,command.c_str(),command.size(),0);
            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;
            
        }
        else if(parseCmd[0]=="list_groups"){
            if(parseCmd.size()!=1){
                cout<<"Syntax: list_groups"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Need to login first"<<endl;
                continue;
            }
            send(sock,command.c_str(),command.size(),0);
            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;
            
        }
        else if(parseCmd[0]=="list_files"){
            if(parseCmd.size()!=2){
                cout<<"Syntax: list_files <group_id>"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Need to login first"<<endl;
                continue;
            }
            send(sock,command.c_str(),command.size(),0);
            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            cout<<buffer<<endl;
            
        }
        else if(parseCmd[0]=="upload_file"){
            if(parseCmd.size()!=3){
                cout<<"Syntax: upload_file <file_path> <group_id>"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Need to login first"<<endl;
                continue;
            }
            FILE* fp;
            if((fp=fopen(parseCmd[1].c_str(),"r"))==NULL){       //file doesn't exist
                perror("fopen");
                continue;
            }
            cout<<"Uploading..."<<endl;
            /* do below if file exists and user is logged in*/
            fseek(fp, 0, SEEK_END);
            long int size=ftell(fp);
            fclose(fp);
            cout<<"Size: "<<size<<endl;

            command=command+" "+userLoggedin+" "+to_string(size);
            fflush(stdout);
            send(sock,command.c_str(),command.size(),0);
            fflush(stdout);

            /* receive status if user belongs to group or not*/
            if(valread = recv(sock, buffer, 1024,0)<0){
                perror("recv");
            }
            string r=buffer;
            if(r=="0") {
                cout<<"Join the group first"<<endl;
                continue;
            }

            /* if user belongs to the group*/
            string hash=computeHash(parseCmd[1]);
            int num=hash.size()/20;
            // cout<<"hashsize: "<<hash.size()<<endl;
            fflush(stdout);
            char toSend[20];
            for(int i=0; i<num; i++){
                strcpy(toSend,(hash.substr(i*20,20)).c_str());
                send(sock,toSend,20,0);
            }
            fflush(stdout);
            cout<<"Done"<<endl;
            // if(valread = recv(sock, buffer, 1024,0)<0){
            //     perror("recv");
            // }
            // cout<<buffer<<endl;
        }
        else if(parseCmd[0]=="download_file"){
            if(parseCmd.size()!=4){
                cout<<"Syntax: download_file <group_id> <file_name> <destination_path>"<<endl;
                continue;
            }
            if(!isLoggedIn){
                cout<<"Need to login first"<<endl;
                continue;
            }
            // FILE* fp;
            // if((fp=fopen(parseCmd[2].c_str(),"r"))==NULL){       //file doesn't exist
            //     perror("fopen");
            //     continue;
            // }
            // fclose(fp);

            /* if file exists - check if user is in the group and group has the file*/
            command=command+" "+userLoggedin;
            fflush(stdout);
            send(sock,command.c_str(),command.size(),0);
            fflush(stdout);

            /*receive status of file and user in group */
            int status;
            if(valread = recv(sock, (int *)&status, sizeof(status),0)<0){
                perror("recv");
            }
            if(status==0) {
                cout<<"Either user is not in the group or file doesn't exist"<<endl;
                continue;
            }

            int arr[3];         //filesize, numOfChunks, numOfSeeders
            if(valread = recv(sock, (int *)arr, sizeof(arr),0)<0){
                perror("recv");
            }
            // for(int i=0; i<3; i++){
            //     cout<<arr[i]<<endl;
            // }
            status=0;
            vector<downloadInfo> a;
            downloadInfo val;
            val.filesize=arr[0];
            // val.numofchnks=arr[1];
            val.fname=parseCmd[2];
            val.dest=parseCmd[3];
            int chnksPerPeer=arr[1]/arr[2];
            int start=0;
            cout<<"No of Seeders: "<<arr[2]<<endl;
            for(int i=0; i<arr[2]; i++){
                memset(buffer,0,1024);
                if(valread = read(sock, buffer, 1024)<0){
                    perror("read");
                }
                string sbuf(buffer);
                cout<<"buffer: "<<buffer<<endl;
                string word[3];
                istringstream ssbuf(sbuf);
                for(int j=0; j<3; j++){     //srcPath, ip, port
                    ssbuf>>word[j];
                }
                val.srcPath=word[0];
                val.serverip=word[1];
                cout<<"word2: "<<word[2]<<endl;
                val.serverport=stoi(word[2]);
                cout<<"From download block: "<<val.srcPath<<" "<<val.serverip<<" "<<val.serverport<<endl;
                val.startpos=start;
                val.endpos=min(start+chnksPerPeer-1,arr[1]-1);    //included
                a.push_back(val);
                start=start+chnksPerPeer;
            }

            /* create threads to receive chunks from each seeder */
            pthread_t ptid[arr[2]];
            for(int i=0; i<arr[2]; i++){
                if(pthread_create(&ptid[i],NULL,client_peer,(void *)&a[i])<0){
                    perror("pthread_create()");
                }
            }
            for(int i=0; i<arr[2]; i++){
                pthread_join(ptid[i],NULL);
            }
            status=1;
            cout<<"Done"<<endl;
            fflush(stdout);
            if(send(sock,(int *)&status,sizeof(int),0)<0){
                perror("send");
                continue;
            }
        }
        else if(parseCmd[0]=="logout"){
            
        }
        else if(parseCmd[0]=="show_downloads"){
            
        }
        else if(parseCmd[0]=="stop_share"){
            
        }
        else{
            cout<<"Oops! Invalid Command!"<<endl;            
        }

        // if(valread = recv(sock, buffer, 1024,0)<0){
        //     perror("recv");
        // }
        // cout<<buffer<<endl;
    }

    return 0; 
} 