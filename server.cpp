// Server side C/C++ program to demonstrate Socket programming 
#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <string.h> 
#include <pthread.h>
#include <bits/stdc++.h> 
// #define PORT 49152  //SERVER PORT
#define CHUNKSIZE 512

using namespace std;

/*-=======================================================================================-*/

/* global variables*/
struct peer_info{
    int sockid;
    string pswd;
    bool isLoggedIn;
};

struct group_info{
    string owner;
    set<string> member;
    set<string> pendingId;
    set<string> files;
};

struct file_info{
    vector<string> hashVal;
    int fileSize;
    int numofChunks;
    map<string,string> seederAndPath;   //key=seederid, value=path
    set<string> leechers;
};

typedef struct peer_info peer_info_s;
typedef struct group_info group_info_s;
typedef struct file_info file_info_s;

map<string,group_info_s> groupInfo; //key=groupid
map<int, pair<string,int>> sockmap; //key=socket fd   //serverIP, serverPort of peer
map<string,peer_info_s> peerInfo;   //key=user id (create_user)
map<string,file_info_s> fileInfo;   //key=file name

/*-=======================================================================================-*/

/*functions*/

void *client_thread(void *argc){    //thread to serve the client(peer)
    // threadid.insert(pthread_self());
    // pthread_detach(pthread_self());
    char buffer[1024] = {0}; 
    int sid=*((int *)argc);
    free(argc);

    // if(send(sid, (int *)&sid, sizeof(int), 0)<0){
    //     perror("send");
    // }

    int valread;
    string word;
    vector<string> parseCmd;
    string sbuf;
    string toPeer;
    while(1){
        parseCmd.clear();
        toPeer="";
        sbuf="";
        memset(buffer, 0, 1024*sizeof(char));
        valread = recv(sid, buffer, 1024, 0);
        sbuf=buffer;
        istringstream ss(sbuf);
        while(ss>>word){
            parseCmd.push_back(word);
        }
        bool ret;
        if(parseCmd[0]=="create_user"){
            /*Check if user exists*/
            bool uexists=false;
            if(peerInfo.find(parseCmd[1])!=peerInfo.end()){
                toPeer="User Exists!";
                uexists=true;
            }

            if(!uexists){
                peerInfo[parseCmd[1]].sockid=sid;
                peerInfo[parseCmd[1]].pswd=parseCmd[2];
                toPeer="User created successfully";
            }

            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }
        }

        else if(parseCmd[0]=="login"){
            cout<<sbuf<<endl;
            bool uexists=false;
            bool pwdmatch=false;
            string uid=parseCmd[1];
            if(peerInfo.find(uid)!=peerInfo.end()){   //if user exists
                if(peerInfo[uid].isLoggedIn){
                    toPeer="Already logged in";
                }
                else{
                    if(peerInfo[uid].pswd==parseCmd[2]){
                        pwdmatch=true;
                        int sockfd=peerInfo[uid].sockid;
                        sockmap[sockfd].first=parseCmd[3];
                        sockmap[sockfd].second=stoi(parseCmd[4]);
                    }
                }
                uexists=true;
            }

            if(!uexists) toPeer="User Doesn't Exist";
            else{
                if(!pwdmatch) toPeer="Incorrect Password: "+parseCmd[2];
                else{
                    peerInfo[uid].isLoggedIn=true;
                    toPeer="Login Successful";
                }
            }

            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }
        }
        else if(parseCmd[0]=="create_group"){
            if(groupInfo.find(parseCmd[1])!=groupInfo.end()){
                toPeer="Group Exists";
            }
            else{
                groupInfo[parseCmd[1]].owner=parseCmd[2];
                toPeer="Group created successfully: "+parseCmd[2];                
            }
            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }
        }
        else if(parseCmd[0]=="join_group"){
            string gid=parseCmd[1];
            bool reqPen=false;
            if(groupInfo.find(gid)!=groupInfo.end()){       //group exists
                if(groupInfo[gid].owner==parseCmd[2]){
                    toPeer="You are already the owner";
                }
                else{
                    if(groupInfo[gid].pendingId.find(parseCmd[2])!=groupInfo[gid].pendingId.end()){
                        toPeer="Request Pending";
                        reqPen=true;
                    }
                    if(!reqPen){
                        groupInfo[gid].pendingId.insert(parseCmd[2]);
                        toPeer="Request Sent";                        
                    }
                }
            }
            else{
                toPeer="Group doesn't exist";
            }

            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }            
        }
        else if(parseCmd[0]=="leave_group"){    //leave_group groupid senderid
            string gid=parseCmd[1];
            bool isMem=false;
            if(groupInfo.find(gid)!=groupInfo.end()){       //group exists
                if(groupInfo[gid].owner==parseCmd[2]) toPeer="You are the owner; cannot exit";
                else{
                    if(groupInfo[gid].member.find(parseCmd[2])!=groupInfo[gid].member.end()){
                        groupInfo[gid].member.erase(parseCmd[2]);
                        toPeer="Exit Successful";
                        isMem=true;
                    }
                }
                if(!isMem){
                    toPeer="You are not a member";
                }
            }
            else{
                toPeer="Group doesn't exist";
            }

            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }

        }
        else if(parseCmd[0]=="list_requests"){
            toPeer="";
            if(groupInfo.find(parseCmd[1])!=groupInfo.end()){   //group exists
                for(auto it:groupInfo[parseCmd[1]].pendingId){
                    toPeer=toPeer+" "+it;
                }
                if(toPeer=="") toPeer="No pending requests";
            }
            else{
                toPeer="Group doesn't exist";
            }

            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }            
        }
        else if(parseCmd[0]=="accept_request"){ //accept_request <gid> <uid> <ownerid>
            bool ret=false;
            string gid=parseCmd[1];
            if(groupInfo.find(gid)!=groupInfo.end()){   //group exists
                if(groupInfo[gid].owner==parseCmd[3]){  //owner sent the req
                    if(groupInfo[gid].pendingId.find(parseCmd[2])!=groupInfo[gid].pendingId.end()){
                        groupInfo[gid].member.insert(parseCmd[2]);
                        groupInfo[gid].pendingId.erase(parseCmd[2]);
                        toPeer="Added to group";
                        ret=true;
                    }
                    if(!ret) toPeer="No pending request from user";
                }
                else{                                   //owner did not send the request
                    toPeer="You need to be the owner";
                }
            }
            else toPeer="Group doesn't exist";
            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }            

        }
        else if(parseCmd[0]=="list_groups"){
            toPeer="";
            for(auto it:groupInfo){
                toPeer=toPeer+" "+it.first;
            }
            if(toPeer=="") toPeer="No groups found";
            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }            
        }
        else if(parseCmd[0]=="list_files"){
            toPeer="";
            string gid=parseCmd[1];
            for(auto it:groupInfo[gid].files){
                toPeer=toPeer+" "+it;
            }
            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
            }            
        }
        else if(parseCmd[0]=="upload_file"){
            string gid=parseCmd[2];                                      //upload_file <file_path> <group_id> userid filesize          
            
            if(groupInfo[gid].member.find(parseCmd[3])==groupInfo[gid].member.end() && groupInfo[gid].owner!=parseCmd[3]){
                toPeer="0";     //Not a member
            }
            else toPeer="1";

            /* send status if user belongs to group or not*/
            fflush(stdout);
            if(send(sid, (char *)toPeer.c_str(), sizeof(toPeer), 0)<0){
                perror("send");
                continue;
            }
            fflush(stdout);

            /* read calculated hash */
            int filsize=stoi(parseCmd[4]);                        
            string filepath=parseCmd[1];
            int i;
            for(i=filepath.size()-1; i>=0; i--){
                if(filepath[i]=='/') break;
            }
            string filename=filepath.substr(i+1,filepath.size()-i-1);
            string path=filepath.substr(0,i);
            cout<<"path: "<<path<<endl;
            groupInfo[gid].files.insert(filename);
            fileInfo[filename].fileSize=filsize;
            int num=(filsize%CHUNKSIZE==0)?filsize/CHUNKSIZE:filsize/CHUNKSIZE+1;
            fileInfo[filename].numofChunks=num;
            fileInfo[filename].seederAndPath[parseCmd[3]]=path;
            char hash[20];
            for(int i=0; i<num; i++){
                if(valread = read(sid, hash, 20)<0){
                    perror("read");
                    break;
                }                
                // printf("okay: %d\n",valread);
                fileInfo[filename].hashVal.push_back(hash);
                memset(hash,0,sizeof(hash));
            }
        }

        else if(parseCmd[0]=="download_file"){      //download_file <group_id> <file_name> <dstpath> uid                        
            string gid=parseCmd[1];
            string fname=parseCmd[2];
            string dstpath=parseCmd[3];
            string uid=parseCmd[4];

            int ret;
            /*check if user belongs to group and file exists or not and send message to peer*/
            if(groupInfo[gid].owner==uid || groupInfo[gid].member.find(uid)!=groupInfo[gid].member.end()){   //user is a member
                ret=1;
            }
            else ret=0;

            if(groupInfo[gid].files.find(fname)!=groupInfo[gid].files.end()){
                ret=ret&1;
            }
            else ret=0;
            fflush(stdout);
            if(send(sid, (int *)&ret, sizeof(ret), 0)<0){
                perror("send");
                continue;
            }
            fflush(stdout);
            if(ret==1){
                // string path=dstpath+"/"+fname;
                int arr[3];      //filesize, numOfChunks, numOfSeeders
                arr[0]=fileInfo[fname].fileSize;
                arr[1]=fileInfo[fname].numofChunks;
                arr[2]=fileInfo[fname].seederAndPath.size();
                // fflush(stdout);
                if(send(sid, (int *)arr, sizeof(arr), 0)<0){
                    perror("send");
                }

                /* send the socket fds of the seeders */
                for(auto it:fileInfo[fname].seederAndPath){    //it=uid
                    int sockd=peerInfo[it.first].sockid;
                    string srcpath=it.second;
                    cout<<"srcPath: "<<srcpath<<endl;
                    toPeer=srcpath+" "+sockmap[sockd].first+" "+to_string(sockmap[sockd].second);
                    // cout<<toPeer;
                    fflush(stdout);
                    if(send(sid, (char *)toPeer.c_str(), 1024, 0)<0){
                        perror("send");
                    }  
                }
                int status=0;
                if(valread = read(sid, (int *)&status, sizeof(int))<0){
                    perror("read");
                    break;
                }
                if(status==1){
                    fileInfo[fname].seederAndPath[uid]=dstpath;
                    cout<<"New Seeder Added"<<endl;
                }
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

        // printf("%s\n",buffer); 
        // sbuf="from server: "+sbuf;
        // if(send(sid, sbuf.c_str(), sbuf.size(), 0)<0){
        //     perror("send");
        // }
        // printf("Message sent\n");         
    }
    close(sid);
    // pthread_exit(0);
    return NULL;
}

int main(int argc, char const *argv[]) 
{ 
    if(argc!=3){
        cout<<"The command needs 3 args: ./server <server_ip> <server_port>";
        exit(0);
    }
    string server_ip=(char *)argv[1];
    int server_port=atoi(argv[2]);

    int server_fd; 
    int client_fd;
    struct sockaddr_in address;     //serveraddress
    int opt = 1; 
    int addrlen = sizeof(address); 
       
    // Creating socket file descriptor 
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    } 
       
    // Forcefully attaching socket to the port 8080 
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                  &opt, sizeof(opt))) 
    { 
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    } 
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = inet_addr((char *)server_ip.c_str());    //it means we are responding to request from any internet address
    address.sin_port = htons(server_port);   //port on which server listens i.e SERVER PORT
       
    // Forcefully attaching socket to the port 8080 

    /*bind() assigns the address specified by addr to the socket referred to by 
    the file descriptor sockfd. */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 

    if (listen(server_fd, 10) < 0) 
    { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    } 

    // pthread_t ptid[30];
    // int i=-1;
    while(1){
        struct sockaddr_in client_addr;
        socklen_t addrlenC=sizeof(client_addr);
        /* return a file descriptor for the accepted socket */
        /*The argument addr is a pointer to a sockaddr structure.  This structure is filled
        in with the address of the peer socket, as known to the communications layer.*/
        fflush(stdout);
        if((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlenC))<0) {  //the address here should be client address
                                                                                //NULL because we dont care who is connecting to us
            perror("accept");
            break; 
        }

        // sockmap[client_fd].first=client_addr.sin_addr.s_addr;
        // sockmap[client_fd].second=client_addr.sin_port;

        pthread_t ptid;
        int *pclient=new int;
        *pclient=client_fd;
        if(pthread_create(&ptid,NULL,client_thread,(void *)pclient)<0){
            perror("pthread_create()");
        }
        // if(getchar()=='q'){
        // }
    }
    // set<int> :: iterator it=threadid.begin();
    // while(it!=threadid.end()){
    //     pthread_kill(*it,0);
    //     threadid.erase(*it);
    //     pthread_exit(NULL);
    // }
} 