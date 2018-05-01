#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include "SerTanaBenchmark.h"

/* values */
volatile int timerexpired=0;

int total=0;
/* globals */
int http10=1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
/* Allow: GET, HEAD, OPTIONS, TRACE */
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "0.1"
int method=METHOD_GET;
int clients=1;

int keepalive=0;

int force_reload=0;

int proxyport=80;

char *proxyhost=NULL;

int benchtime=30;
/* internal */

char host[MAXHOSTNAMELEN];

#define REQUEST_SIZE 2048

char request[REQUEST_SIZE];

int start = 0 ;

int numthreads;
int Total_Target;
int Con_Target;
int perconnum;
int perconnumrest;

static const struct option long_options[]=
{
 {"numthreads",required_argument,NULL,'t'},
 {"help",no_argument,NULL,'?'},
 {"http09",no_argument,NULL,'9'},
 {"http10",no_argument,NULL,'1'},
 {"http11",no_argument,NULL,'2'},
 {"get",no_argument,&method,METHOD_GET},
 {"head",no_argument,&method,METHOD_HEAD},
 {"options",no_argument,&method,METHOD_OPTIONS},
 {"trace",no_argument,&method,METHOD_TRACE},
 {"version",no_argument,NULL,'V'},
 {"Con_Target",required_argument,NULL,'c'},
 {"Total_Target",required_argument,NULL,'s'},
 {"Benchtime",required_argument,NULL,'r'},
 {NULL,0,NULL,0}
};


void benchcore2();
static int  bench(void);
static void build_request(const char *url);
static void alarm_handler(int signal)// DNS Query may time out but
                                     // gethostbyname_r() can't do anything
{
   timerexpired=1;
   (void)signal;
}

static void usage(void)
{
   fprintf(stderr,
    "SerTanaBench [option]... URL\n"
    "  -t|--threads             Number of threads. Default one.\n"
    "  -c|--clients <n>         Run <n> HTTP concurrent connections at once. Default one.\n"
    "  -r|--Benchtim <s>        Run <n> HTTP time. Default30s.\n"
    "  -9|--http09              Use HTTP/0.9 style requests.\n"
    "  -1|--http10              Use HTTP/1.0 protocol.\n"
    "  -2|--http11              Use HTTP/1.1 protocol.\n"
    "  --get                    Use GET request method.\n"
    "  --head                   Use HEAD request method.\n"
    "  --options                Use OPTIONS request method.\n"
    "  --trace                  Use TRACE request method.\n"
    "  -?|-h|--help             This information.\n"
    "  -V|--version             Display program version.\n"
    );
};

struct sockaddr_in ad;
static int bulid_socket(const char *host, int clientPort)
{

    unsigned long inaddr;
    struct hostent hp,*hpp=NULL;
    int hperror;
    char buf[1024];

    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;

    inaddr = inet_addr(host);//if host is a ip address
    if (inaddr != INADDR_NONE)
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));
    else
    {
        int ret = gethostbyname_r(host,&hp,buf,1024,&hpp,&hperror);//if host is a address name
        if (0 !=ret )
            return -1;
        memcpy(&ad.sin_addr, hp.h_addr, hp.h_length);
    }
    ad.sin_port = htons(clientPort);

    return 1;
}


static int Socket(const char *host, int clientPort)
{
    int sock;
    struct timeval t;
    (void)host;
    (void)clientPort;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return sock;

    //ret = setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&option,sizeof(option));

    //fcntl(sock,F_SETFL,fcntl(sock,F_GETFL,0)|O_NONBLOCK); //do we need NonBlocking ?

    (void)t;
    t.tv_sec  = 5;
    t.tv_usec = 0;
    setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof(t));//set the timeout value of
                                                         //the send operation of the socket
    t.tv_sec  = 8;
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof(t));//set the timeout value of
                                                         //the receive operation of the socket

    return  connect(sock, (struct sockaddr *)&ad, sizeof(ad));

    //return sock;
}


int count=0;
void countsocket()
{
    while(socket(AF_INET, SOCK_STREAM, 0)>0)
        count++;
}


int main(int argc, char *argv[])
{
    struct timeval t1,t2;
    struct sigaction sa;
    int i = 0;
    pthread_t *tid_vec;
    int opt=0;
    int options_index=0;
    int allcon=0,allfailed=0;
    long long allbytes=0;

    struct rlimit slimit;
    struct rlimit limit;

    printf("-------------------------------\n");
    printf("SerTanaWebBench");
    if(argc==1)
    {
       usage();
       return 2;
    }

    while((opt=getopt_long(argc,argv,"912Vr:s:t:c:?h",long_options,&options_index))!=EOF )
    {
     switch(opt)
     {
      case  0 : break;
      case 's': Total_Target=atoi(optarg);break;
      case '9': http10=0;break;
      case '1': http10=1;break;
      case '2': http10=2;break;
      case 'V': printf(PROGRAM_VERSION"\n");exit(0);
      case 't': numthreads=atoi(optarg);break;
      case 'r': benchtime=atoi(optarg); break;

      case ':':
      case 'h':
      case '?': usage();return 2;break;
      case 'c': Con_Target=atoi(optarg);break;
     }
    }

    if(optind==argc)
    {
       fprintf(stderr,"SerTanabench: Missing URL!\n");
       usage();
       return 2;
    }

    if(Total_Target<=0)  Total_Target = 1 ;
    if(Con_Target<=0)    Con_Target   = 1 ;
    if(numthreads<=0)    numthreads   = 1 ; //if user has not set the number of threads,there is only 1 thread

    /**/
    char *_temp=(char *) malloc(strlen(argv[optind])+2);
    _temp=strcpy(_temp,argv[optind]);
    *(_temp+strlen(argv[optind]))='/';
    *(_temp+strlen(argv[optind])+1)=0;

    build_request(_temp);
    free(_temp);

   // build_request(argv[optind]);

    /* print bench info */
    printf("\nTrying to connect: ");

    switch(method)
    {
        case METHOD_GET:
        default:
            printf("GET");break;
        case METHOD_OPTIONS:
            printf("OPTIONS");break;
        case METHOD_HEAD:
            printf("HEAD");break;
        case METHOD_TRACE:
            printf("TRACE");break;
    }
    printf(" %s",argv[optind]);
    switch(http10)
     {
       case 0: printf(" (using HTTP/0.9)");break;
       case 2: printf(" (using HTTP/1.1)");break;
     }
    printf("\n");

    if(numthreads>100)
    {
       printf("Create so many threads is dangerous, so we exit");
       exit(2);
    }
    if(Total_Target < Con_Target)
    {
       printf("Concurrent connections can't be larger than Total connections");
       exit(2);
    }

    if(getrlimit(RLIMIT_NOFILE,&limit)<0)
    {
      printf("\n---------WARNING ! ! !---------\n");
      printf("Can't read the number of fd of the process\n");
      exit(2);
    }
    if(limit.rlim_cur < Con_Target+3)
    {
      printf("\n---------WARNING ! ! !---------\n");
      printf("The number of fd of the process is %d, less than concurrent connections %d\n",(unsigned int)limit.rlim_cur,Con_Target);
      slimit.rlim_cur = Con_Target+3;
      if(setrlimit(RLIMIT_NOFILE,&slimit)<0)
      {
         int erro=errno;
         if(erro==EPERM)
         printf("Setfdlimit failed casued by operation not permitted\n");
      }
      else
      {
         printf("Setfdlimit sucessful and the number of fd of the process is set to %d\n",(unsigned int)slimit.rlim_cur);
         goto going;
      }

      printf("Please makesure to let the number of fd is 3 more than concurrent connections\n");
      exit(2);
    }
 going:
    printf("%d numthreads are created for connecting %d concurrent connections\n",numthreads,Con_Target);
    printf("%d total connections are trying to connect\n",Total_Target);

    bench();

    perconnum = Con_Target / numthreads;
    perconnumrest = Con_Target % numthreads;

    tid_vec    = (pthread_t*)malloc(sizeof(pthread_t)*numthreads);

    thread      = (int*)malloc(sizeof(int)*numthreads);
    connections = (int*)malloc(sizeof(int)*numthreads);
    failed      = (int*)malloc(sizeof(int)*numthreads);
    bytes       = (long long*)malloc(sizeof(long long)*numthreads);

    for(i = 0;i < numthreads; i++)
    {
        *(connections+i)=*(bytes+i)=*(failed+i)=0;
        int ret = 0;
        *(thread+i)= i;
        ret = pthread_create(tid_vec + i, NULL,(void*)benchcore2, thread+i);
        if(0!=ret)
        {
           fprintf(stderr, "thread create error:%s\n",strerror(ret));
           exit(1);
        }
    }
    sa.sa_handler=alarm_handler;
    sa.sa_flags=0;
    if(sigaction(SIGALRM,&sa,NULL))
         exit(3);
    alarm(benchtime);

    start = 1;

    gettimeofday(&t1,NULL);

    while(timerexpired==0)
    {
        for(i = 0; i < numthreads; i++)
        {
            allcon    += *(connections+i);
            allfailed += *(failed+i);
        }
        if(allcon+allfailed >= Total_Target)
               timerexpired = 1;

        allcon=allfailed=0;
    }

    for(i = 0; i < numthreads; i++)
    {
        int ret = 0;
        ret = pthread_join(*(tid_vec + i), NULL);
        if(0!=ret)
        {
           fprintf(stderr, "thread join error:%s\n",strerror(ret));
           exit(1);
        }
        allcon    += *(connections+i);
        allfailed += *(failed+i);
        allbytes  += *(bytes+i);
    }
    gettimeofday(&t2,NULL);

    int dts  = t2.tv_sec-t1.tv_sec;

    printf("\nTotal connections are %d, receive %d bytes\nSuccessful connection number rate is %d/sec\nTotal requests: %d susceed, %d failed, and %d sec is used to complete the task \n",
              (int)((allcon+allfailed)),
              (int)(allbytes),
              (int)(allcon/(float)dts),
              allcon,
              allfailed,
              dts);

   printf("-------------------------------\n");

  // free(tid_vec);already freed by pthread_join.
   free(connections);free(failed);free(bytes);free(thread);
   return 0;
}
void build_request(const char *url)
{
  char tmp[10];
  int i;

  bzero(host,MAXHOSTNAMELEN);
  bzero(request,REQUEST_SIZE);
  if(force_reload && proxyhost!=NULL && http10<1) http10=1;
  if(method==METHOD_HEAD && http10<1) http10=1;
  if(method==METHOD_OPTIONS && http10<2) http10=2;
  if(method==METHOD_TRACE && http10<2) http10=2;
  switch(method)
  {
      default:
      case METHOD_GET: strcpy(request,"GET");break;
      case METHOD_HEAD: strcpy(request,"HEAD");break;
      case METHOD_OPTIONS: strcpy(request,"OPT/*IONS");break;
      case METHOD_TRACE: strcpy(request,"TRACE");break;
  }
  strcat(request," ");

  if(NULL==strstr(url,"://"))//we may let url just like "http://127.0.0.1"
  {
    fprintf(stderr, "\n%s: is not a valid URL.\n",url);
    exit(2);
  }

  if(strlen(url)>1500) //we need smaller url
  {
    fprintf(stderr,"URL is too long.\n");
    exit(2);
  }


  if(0!=strncasecmp("http://",url,7))
  {
    fprintf(stderr,"\nOnly HTTP protocol is directly supported.\n");
    exit(2);
  }

  /* protocol/host delimiter */
   i=strstr(url,"://")-url+3;
  /* printf("%d\n",i); */

  /* if(strchr(url+i,'/')==NULL)
   {
     fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
     exit(2);
   }
*/

      /* get port from hostname */
    if(index(url+i,':')!=NULL &&
        index(url+i,':')<index(url+i,'/'))
      {
          strncpy(host,url+i,strchr(url+i,':')-url-i);
          bzero(tmp,10);
          strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
          /* printf("tmp=%s\n",tmp); */
          proxyport=atoi(tmp);
          if(proxyport==0) proxyport=80;
      } else
      {
        strncpy(host,url+i,strcspn(url+i,"/"));
      }
      // printf("Host=%s\n",host);
      strcat(request+strlen(request),url+i+strcspn(url+i,"/"));


     if(http10==1)
         strcat(request," HTTP/1.0");
     else if (http10==2)
         strcat(request," HTTP/1.1");
     strcat(request,"\r\n");
     if(http10>0)
         strcat(request,"User-Agent: SerTanaBench "PROGRAM_VERSION"\r\n");
     if(http10>0)
     {
         strcat(request,"Host: ");
         strcat(request,host);
         strcat(request,"\r\n");
     }

     strcat(request,"Connection: close\r\n");
     /* add empty line at end */
     if(http10>0) strcat(request,"\r\n");
     // printf("Req=%s\n",request);
}

static int bench(void)
{
  int i ;

  i=bulid_socket(host,proxyport);

  if(i<0)
  {
    fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
    exit(1);
  }

  //test();
  /* check avaibility of target server */
  i=Socket(host,proxyport);

  if(i<0)
  {
    fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
    exit(1);
  }
  close(i);

  return i;
}

struct epresult
{
    int fd;
    int connection_status;
};

void DelandCre(struct Socknode *socknode,struct rb_root *socktree)
{
   int sock,option=1,oldsock=-1;

   if(socknode)
   {
     oldsock=socknode->sockfd;
   }
   else
   {
     wait_for_debug();
   }
next:
   while((sock = socket(AF_INET, SOCK_STREAM, 0))<0);
   fcntl(sock,F_SETFL,fcntl(sock,F_GETFL,0)|O_NONBLOCK);//NON_BLOCKING sockfd
   setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&option,sizeof(option));
   if(connect(sock, (struct sockaddr *)&ad, sizeof(ad))<0)
   {
      if(errno!=EINPROGRESS&&errno!=EISCONN)
       goto next;
   }

   socknode->sockfd = sock;
   socknode->bitmap = 0;
   if(sock!=oldsock)
   {
     rb_erase(&socknode->node,socktree);
     my_insert(socktree,socknode);//insert into rbtree;
   }
}

void benchcore2(void *arg)
{
    int i    =  0 ,wrfin = 0 ,option=1,threadid = *(int *)arg,_cnt=0;
    int sock =-1;
    int ret ,rlen=strlen(request);

    *(connections + threadid)=0;
    *(failed + threadid)=0;
    *(bytes + threadid)=0;

    struct rb_root   socktree = RB_ROOT;
    struct rb_node  *node;
    struct Socknode *socknode;
    struct Socknode *socknodelist = (struct Socknode *)malloc(sizeof(struct Socknode)*perconnum);

    int epollfd  = epoll_create(EPOLL_CLOEXEC);if(epollfd<0)wait_for_debug();
    int epollret,ctlreturn;
    struct epoll_event  event;
    struct epoll_event* eventresult= (struct epoll_event*)malloc(sizeof(struct epoll_event)*perconnum);
    struct epoll_event _tmpevt;
    struct epresult   * eprst;
    (void)eprst;
    char   buf[1500];
    event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP| EPOLLERR;

    for(i = 0; i < perconnum; i++)
    {     
       while((sock = socket(AF_INET, SOCK_STREAM, 0))<0);

       socknode =  (socknodelist+i);
       socknode->sockfd = sock;
       socknode->bitmap = 0;
       fcntl(sock,F_SETFL,fcntl(sock,F_GETFL,0)|O_NONBLOCK);//NON_BLOCKING sockfd
       setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&option,sizeof(option));
       my_insert(&socktree,socknode);//insert into rbtree;
    }

    while(start==0);//wait for starting connect

    for (node = rb_first(&socktree); node; node = rb_next(node))
    {
       socknode = rb_entry(node, struct Socknode, node);
       sock     = socknode->sockfd;
       ret  = connect(sock, (struct sockaddr *)&ad, sizeof(ad));
       if(ret<0)
       {
           if(errno==EINPROGRESS||errno==EISCONN)
           {
              /*struct tcp_info tcpinfo; int tcpinfolen = sizeof(tcpinfo);
              while(0)
              {
                getsockopt(sock,IPPROTO_TCP,TCP_INFO,&tcpinfo,(socklen_t *)&tcpinfolen);
                if(tcpinfo.tcpi_state==TCP_ESTABLISHED);
              }
              */
           }
           else
           {
             *(failed+threadid)=*(failed+threadid)+1;
              DelandCre(socknode,&socktree);
              sock = socknode->sockfd;
           }
       }
       event.data.fd = sock;
       epollret   = epoll_ctl(epollfd,EPOLL_CTL_ADD,sock,&event);
       if(epollret<0)
       {
          wait_for_debug();
       }
       _cnt++;
    }

    while(1)
    {
      epollret = epoll_wait(epollfd, eventresult, perconnum, benchtime);
      if(epollret < 0)
      {
         int e=errno;
         (void)e;
         //wait_for_debug();
         continue;
      }
      for(i = 0; i < epollret; i++)
      {                 
          _tmpevt  =  eventresult[i];
          sock     =  _tmpevt.data.fd;
          eprst    =  (struct epresult*)(_tmpevt.data.ptr);
          socknode =  my_search(&socktree,sock);

         if((_tmpevt.events & (EPOLLERR|EPOLLHUP))&&!(_tmpevt.events&EPOLLIN))
             // socket is closed by host, socket is hup
         {
              *(failed+threadid)=*(failed+threadid)+1;
              close(sock);
             goto nexttry;
         }
         if(_tmpevt.events& EPOLLOUT)
         {
            wrfin = socknode->bitmap;
            ret   = write(sock,request + wrfin,rlen-wrfin);
            if(ret < 0) //error
            {
               *(failed+threadid)=*(failed+threadid)+1;
               close(sock);
               goto nexttry;
            }
            else if (rlen==ret+wrfin)// write finish
            {
               if(timerexpired)
                    break;
               socknode->bitmap = rlen;
               event.data.fd = sock;
               event.events  = EPOLLRDHUP| EPOLLERR| EPOLLIN;
               epoll_ctl(epollfd,EPOLL_CTL_MOD,sock,&event);
               if(ctlreturn<0)
               {
                   int e = errno;
                   (void)e;
                  // wait_for_debug();
               }
               shutdown(sock,1);
               continue;
            }
            else
            {
               socknode->bitmap = ret+wrfin;
               socknode->bitmap = rlen;
               /*
               event.data.fd = sock;
               event.events  = EPOLLIN | EPOLLOUT|EPOLLRDHUP| EPOLLERR;
               epoll_ctl(epollfd,EPOLL_CTL_MOD,sock,&event);
               */
               if(timerexpired)
                   break;
               continue;
            }
         }
         if(_tmpevt.events & EPOLLIN)
         {
            if(socknode->bitmap != rlen)
            {
              int e = errno;
              (void)e;
              //wait_for_debug();
              *(failed+threadid)=*(failed+threadid)+1;
              close(sock);
              goto nexttry;
            }
            ret = read(sock,buf,1500);
            if(ret==0)
            {
              if(close(sock)!=0)
              {
                *(failed+threadid)=*(failed+threadid)+1;
              }
              else
              {
                *(connections+threadid)=*(connections+threadid)+1;
              }
            }
            else if(ret<0)
            {        
               *(failed+threadid)=*(failed+threadid)+1;
               close(sock);
            }
            else if(ret>=1492)
            {
               *(bytes+threadid)=*(bytes+threadid)+ret;

                if(timerexpired)
                    break;
               continue;
            }
            else
            {

               *(bytes+threadid)=*(bytes+threadid)+ret;
              // *(connections+threadid)=*(connections+threadid)+1;
                if(timerexpired)
                    break;

              // event.data.fd = sock;
             //  event.events = EPOLLIN | EPOLLRDHUP| EPOLLERR|EPOLLOUT;
              // epoll_ctl(epollfd,EPOLL_CTL_MOD,sock,&event);
              // socknode->bitmap=0;
               continue;
            }
         }
         else
         {
            *(failed+threadid)=*(failed+threadid)+1;
            close(sock);
         }
nexttry:
        // event.data.fd = sock;
        // close(sock);
        // ctlreturn =  epoll_ctl(epollfd,EPOLL_CTL_DEL,sock,&event);
        // close(fd) causes fd removed from all epoll sets automatically
         if(timerexpired)
             break;
         DelandCre(socknode,&socktree);
         event.data.fd=socknode->sockfd;
         event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP| EPOLLERR;
         ctlreturn = epoll_ctl(epollfd,EPOLL_CTL_ADD,socknode->sockfd,&event);
         if(ctlreturn<0)
         {
             int e = errno;
             (void)e;
            // wait_for_debug();
         }
      }//for(i = 0; i < epollret; i++)

      if(0
       // ||atmoic_add_fetch(&total,*(failed +threadid)+*(connections+threadid))>Total_Target
          ||timerexpired)
          break;

    }//while(1);

    for (node = rb_first(&socktree); node; node = rb_next(node))
    {
       socknode = rb_entry(node, struct Socknode, node);
       sock     = socknode->sockfd;
       close(sock);
    }
    free(socknodelist);
    free(eventresult);
    close(epollfd);
    return;
}
