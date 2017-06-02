#include<stdio.h>
#include"operation.h"
#include"choose.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
/*#include "creatbtdic.c"
#include "operation.c"
#include "user.c"
#include "soundex.c"*/

#define BACKLOG 2   /* Number of allowed connections */

#define ISUSER 0
#define ISDICT 1
#define SEARCH 1
#define EDIT 2
#define ADD 3
#define DELETE_W 4
#define DELETE_U 5
#define LOGOUT 9
#define FREE_USER 0
#define LOGIN 1
#define REGISTER 2
#define EXIT -1

BTA* btUser;
BTA* btDic;
BTA* btSoundex;
USER* logged;   // users are logging
int nLogged; // number of users are logging

int checkLogged(USER* usr){
  int i;
  int flag = -1;

  if(nLogged == 0)
    return -1;
  for(i=0;i<nLogged;i++){
    if(strcmp(logged[i].userName,usr->userName) == 0)
      if(strcmp(logged[i].passWord,usr->passWord) == 0)
	flag = i;
  }
  if(flag >= 0)return flag;   // if user exsited then return index
  else return -1;          // else return -1
}
void incLogged(USER* usr){
  logged[nLogged++] = *usr;
}
void decLogged(USER* usr){
  int n = checkLogged(usr);
  int i;
  if(n != -1){
    for(i=n;i<nLogged-1;i++){   // Shift
      logged[i] = logged[i+1];
    }
    nLogged --;
  }
}

main(int args,char* argv[]){
  int listens;
  int connect;
  int fdMax;
  int rcvByte;
  struct hostent *host;
  struct sockaddr_in server; //Structures for handling internet addresses of server
  struct sockaddr_in client; //Structures for handling internet addresses of client
  int PORT; //port connect
  fd_set allSocket; //set of sockets connecting to server
  fd_set readfds; //set of sockets that are available to read data

  USER* usr;
  dict* dic;
  dict* foundWord;
  dict* edittedWord;
  dictsd* hintWord;
  OP* choose;
  RC* rc;
  int k,i;
  int sinSize;
  struct timeval timeout;
  timeout.tv_sec = 1200;
  timeout.tv_usec = 0;

  if(args < 3){
    printf("Error command agrument\n");
    exit(0);
  }
  logged = (USER*)malloc(sizeof(USER)*100);
  nLogged = 0;
  PORT = atoi(argv[2]);
  sinSize = sizeof(struct sockaddr_in);
  printf("Server initializing..........\n");
  if((listens=socket(AF_INET,SOCK_STREAM,0)) == -1){ //create socket
    perror("Socket() error\n");
    exit(-1);
  }
  server.sin_family = AF_INET; //set domain internet ipv4
  server.sin_port = htons(PORT); //set port
  server.sin_addr.s_addr=INADDR_ANY;
  bzero(&(server.sin_zero),8);
  if(bind(listens,(struct sockaddr*)&server,sizeof(struct sockaddr))==-1){
    perror("bind() error\n");
    exit(-1);
  }
  if(listen(listens,BACKLOG) == -1){ //check for maximum connection
    perror("listen() error\n");
    exit(-1);
  }

  btinit();
  btUser = btopn("user.dat",0,0); //mode:update, disallow share access
  btDic = btopn("dict.dat",0,0);
  btSoundex = btopn("dictSoundex.dat",0,0);
  if(btUser == NULL){
    btUser = btcrt("user.dat",0,0);
    creatBtreeUser(btUser); //if dat file doesn't exist then create it
  }

  if(btDic == NULL){
    btDic = btcrt("dict.dat",0,0);
    creat_btree(btDic);
  }

  if(btSoundex == NULL){
    btSoundex = btcrt("dictSoundex.dat",0,0);
    creat_btsoundex(btSoundex);
  }

  dic = (dict*)malloc(sizeof(dict));
  usr= (USER*)malloc(sizeof(USER));
  choose = (OP*)malloc(sizeof(OP));
  rc = (RC*)malloc(sizeof(RC));
  foundWord = (dict*)malloc(sizeof(dict));
  hintWord = (dictsd*)malloc(sizeof(dictsd));

  FD_ZERO(&allSocket);//Initializes the set to the null set.
  FD_ZERO(&readfds);
  FD_SET(listens,&allSocket);//Adds descriptor  to set.
  fdMax = listens; //number of connections
  printf("Ok!!Ready!!\n");
  while(1){
    readfds = allSocket;
    k = select(fdMax+1,&readfds,NULL,NULL,&timeout); //switch blocking-nonblocking status of socket
    if(k == -1){
      perror("select error");
      exit(4);
    }

    else if(k == 0 ){
      perror("Time out");
      break;
    }
    for(i=0;i<fdMax+1;i++){
      if(FD_ISSET(i,&readfds)){ //Nonzero if i is a member of the set readfds. Otherwise, zero.
	if(i == listens){
	  connect = accept(listens,(struct sockaddr *)&client,&sinSize); //Extract first connection request on queue; blocking if queue is empty
	  if(connect == -1){
	    perror("accept error");
	  }
	  else{
	    printf("Have connect from: %d\n",connect);
	    FD_SET(connect,&allSocket);
	    if(fdMax<connect)
	      fdMax = connect;
	  }
	}
	else{
	  recv(i,(char*)choose,sizeof(OP),0); // receive client of choose
	  switch(choose->kind){
	  case ISUSER:
	    switch(choose->opCode){
	    case EXIT:
	      printf("Stop connect at Port:%d\n",i);
	      close(i);
	      FD_CLR(i,&allSocket); //delete connection from the set of connecting socket
	      break;
	    case LOGOUT:
	      recv(i,(char*)usr,sizeof(USER),0);
	      decLogged(usr);
	      break;
	    case LOGIN:
	      recv(i,(char*)usr,sizeof(USER),0);
	      if(checkLogged(usr) == -1){
		rc->value = checkLogin(btUser,usr);
		if(rc->value == 1 || rc->value == 2){
		  incLogged(usr);
		}
	      }// if
	      else{
		rc->value = -3;
	      }
	      send(i,(char*)rc,sizeof(RC),0);
	      break;
	    case REGISTER:
	      recv(i,(char*)usr,sizeof(USER),0);
	      rc->value = newUser(btUser,usr);
	      send(i,(char*)rc,sizeof(RC),0);
	      break;
	    case DELETE_U:
	      recv(i,(char*)usr,sizeof(USER),0);
	      if(checkLogged(usr) == -1){
		rc->value = deleteUser(btUser,usr);
		send(i,(char*)rc,sizeof(RC),0);
	      }
	      else{
		rc->value = -2;
		send(i,(char*)rc,sizeof(RC),0);
	      }
	      break;
	    case FREE_USER:
	      break;
	    default:
	      break;
	    }
	    break;
	  case ISDICT:
	    switch(choose->opCode){
	    case SEARCH:
	      recv(i,(char*)dic,sizeof(dict),0);
	      rc->value = searchWord(btDic,btSoundex,foundWord,hintWord,dic->word);
	      send(i,(char*)rc,sizeof(RC),0);
	      switch(rc->value){
	      case 1:
		send(i,(char*)foundWord,sizeof(dict),0);
		break;
	      case 0:printf("Co hintto:%s\n",hintWord->word);
		send(i,(char*)hintWord,sizeof(dictsd),0);
		break;
	      case -1:
		break;
	      }
	      break;
	    case EDIT:
	      recv(i,(char*)dic,sizeof(dict),0);
	      rc->value = editWord(btDic,btSoundex,edittedWord,dic,1);
	      send(i,(char*)rc,sizeof(RC),0);
	      break;
	    case ADD:
	      recv(i,(char*)dic,sizeof(dict),0);
	      rc->value = addWord(btDic,btSoundex,dic,1);
	      send(i,(char*)rc,sizeof(RC),0);
	      break;
	    case DELETE_W:
	      recv(i,(char*)dic,sizeof(dict),0);
	      rc->value = deleteWord(btDic,btSoundex,dic->word,2);
	      send(i,(char*)rc,sizeof(RC),0);
	      break;
	    }
	    break;
	  }//switch


	}//else
      }//if

    }//for
  }//while
  free(dic);
  free(usr);
  free(rc);
  free(choose);
  btcls(btUser);
  btcls(btDic);
  btcls(btSoundex);
  close(listens);
}

//Compile command
//gcc -o server server.c creatbtdic.c operation.c user.c soundex.c libfdr/libfdr.a bt-3.0.1/lib/libbt.a
//./server 127.0.0.1 5000
