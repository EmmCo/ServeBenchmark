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
#include "SerTanaBenchmark.h"
struct list{
    int fd;
    struct list *next;
};


/* values */
volatile int timerexpired=0;
int connections=0;
int failed=0;
long long bytes=0;
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
 {"keepalive",no_argument,&keepalive,1},
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

static void benchcore();
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
    "  -k|--keepalive           keepalive of Http.\n"
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

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return sock;

   // fcntl(sock,F_SETFL,fcntl(sock,F_GETFL,0)|O_NONBLOCK); //do we need NonBlocking ?

    (void)t;
    t.tv_sec  = 5;
    t.tv_usec = 0;
    setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof(t));//set the timeout value of
                                                         //the send operation of the socket
    t.tv_sec  = 8;
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof(t));//set the timeout value of
                                                         //the receive operation of the socket


    int ret = connect(sock, (struct sockaddr *)&ad, sizeof(ad));

    if (ret < 0)
    {
      char *p = strerror(ret);

      if(errno==EINPROGRESS)
      {
          struct tcp_info tcpinfo;
          int tcpinfolen = sizeof(tcpinfo);
          while(0)
          {
            getsockopt(sock,IPPROTO_TCP,TCP_INFO,&tcpinfo,(socklen_t *)&tcpinfolen);
            if(tcpinfo.tcpi_state=TCP_ESTABLISHED)
            {


            }
          }
      }
      else if (errno==EISCONN)//connected
      {
         (void)p;
      }
      else if (errno==ECONNREFUSED)//refused by host
      {
         (void)p;
      }
      else if(errno==ENETUNREACH)//cannot reach to host
      {
         (void)p;
      }
      else if(errno==EHOSTDOWN)//host is down
      {
         (void)p;
      }

      return -1;
    }
    return sock;
}


#define atmoic_fetch_add(ptr,value) do{__sync_fetch_and_add((ptr),(value));}while(0)
#define atmoic_fetch_sub(ptr,value) do{__sync_fetch_and_sub((ptr),(value));}while(0)
#define atmoic_read(ptr) __sync_fetch_and_add((ptr),(0))
#define atmoic_add_fetch(ptr,value)  __sync_add_and_fetch((ptr),(value))
#define atmoic_set(ptr,value) __sync_lock_test_and_set((ptr),(value))

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





    printf("-------------------------------\n");
    printf("SerTanaWebBench");
    if(argc==1)
    {
       usage();
       return 2;
    }

    while((opt=getopt_long(argc,argv,"912Vkr:s:t:c:?h",long_options,&options_index))!=EOF )
    {
     switch(opt)
     {
      case  0 : break;
      case 'k': keepalive=1;break;
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

    if(Total_Target==0)  Total_Target = 1 ;
    if(Con_Target==0)    Con_Target   = 1 ;
    if(numthreads==0)    numthreads   = 1 ; //if user has not set the number of threads,there is only 1 thread

    build_request(argv[optind]);
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

    if(numthreads>50)
    {
       printf("Create so many threads is dangerous, so we exit");
       exit(2);
    }
    if(Total_Target<Con_Target)
    {
       printf("Concurrent connections can't be larger than Total connections");
       exit(2);
    }

    printf("%d numthreads are created for connecting %d concurrent connections\n",numthreads,Con_Target);
    printf("%d total connections are trying to connect\n",Total_Target);

     bench();

    perconnum = Con_Target / numthreads;
    perconnumrest = Con_Target % numthreads;


    tid_vec    = (pthread_t*)malloc(sizeof(pthread_t)*numthreads);

    for(i = 0;i < numthreads; i++)
    {
        int ret = 0;
        ret = pthread_create(tid_vec + i, NULL,(void*)benchcore2, NULL);
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
    for(i = 0; i < numthreads; i++)
    {
        int ret = 0;
        ret = pthread_join(*(tid_vec + i), NULL);
        if(0!=ret)
        {
           fprintf(stderr, "thread join error:%s\n",strerror(ret));
           exit(1);
        }
    }
    gettimeofday(&t2,NULL);

    int dts  = t2.tv_sec-t1.tv_sec;

    printf("\nTotal connections are %d, receive %d bytes\nSuccessful connection number rate is %d/sec\nTotal requests: %d susceed, %d failed, and %d sec is used to complete the task \n",
              (int)((total)),
              (int)(bytes),
              (int)(connections/(float)dts),
              connections,
              failed,
              dts);


   if(timerexpired)
   {
       printf("Becafule the test is timed out!\n");
   }
   printf("-------------------------------\n");

   free(tid_vec);

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

     //if(http10>1)


     if(keepalive)
     {
       strcat(request,"Connection: Keep-Alive\r\n");
     }
     else
     {
       strcat(request,"Connection: close\r\n");
     }


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


void benchcore()
{
   while(start==0);

   int *Soqueue=(int *)malloc(sizeof(int)*perconnum);
   struct list* fdlist=(struct list*)malloc(sizeof(struct list));
   //fdlist->fd=-1;
   struct list* head  = fdlist;
   const int port  = proxyport;
   const char *req = request  ;
   int rlen;
   char buf[1500];
   int s,i;


   rlen=strlen(req);

   nexttry:while(1)
   {
      if(timerexpired)
      {
        if(atmoic_read(&failed)>0)
         {
           atmoic_fetch_add(&failed,1);
         }
           break;
      }

      atmoic_set(&total,failed+connections);

      //atmoic_add_fetch(&total,failed+connections)
     if(atmoic_read(&total)< Total_Target)
      {
        if(atmoic_read(&connections)> Con_Target)
        {         
          if(NULL==head)
          {
             continue;
          }
          int ret = close(head->fd);
          if(0!=ret)
          {
             // atmoic_fetch_add(&failed,1);
            //  atmoic_fetch_sub(&connections,1);
          }

          struct list*_th=head;
          head = head->next;
          free(_th);
        }
      }
      else
      {
         break;
         //return ;
      }

     s=Socket(host,port);

     if(s<0)
     {
       atmoic_fetch_add(&failed,1);
       continue;
     }

     if(rlen!=write(s,req,rlen))//it seems not very strict, but rlen is small (65)
     {
       atmoic_fetch_add(&failed,1);
       close(s);
       continue;
     }

     if(http10==0)
     if(shutdown(s,1))
     {
       atmoic_fetch_add(&failed,1);
       close(s);
       continue;
     }

      /* read all available data from socket */
     while(1)
     {
       if(timerexpired)
         break;

        i=read(s,buf,1500);
        /* fprintf(stderr,"%d\n",i); */
        if(i<0)
        {
          atmoic_fetch_add(&failed,1);
           //failed++;
          close(s);
          goto nexttry;
        }
        else
        if(i==0) break;
        else
       atmoic_fetch_add(&bytes,i);
           //  bytes+=i;
     }

/*
    if(close(s))
    {
       //failed++;
       atmoic_fetch_add(&failed,1);
       continue;
    }
*/
 atmoic_fetch_add(&connections,1);

   // connections++;

   struct list* newlist=(struct list*)malloc(sizeof(struct list));
   newlist->fd=s;
   newlist->next=NULL;
   fdlist->next=newlist;
   fdlist=newlist;
  }//nexttry

   while(head)
   {
     struct list*_th=head;
     int ret = close(head->fd);
     if(0!=ret)
     {

     }
     head = head->next;
     free(_th);
   }
 return ;
}

struct epresult
{
    int fd;
    int connection_status;
};

void ClearState(int *queue,int length,int sockfd,unsigned char* bitmap,unsigned char* bitmap2,unsigned char* bitmap3);
int  GetState(int *queue,int length,int sockfd,unsigned char* bitmap) ;
void SetState(int *queue,int length,int sockfd,unsigned char* bitmap);

void benchcore2()
{
    int i    =  0 ;
    int sock = -1 ;
    int ret  ;

    int epollfd  = epoll_create(EPOLL_CLOEXEC);if(epollfd<0)wait_for_debug();
    int *Soqueue = (int *)malloc(sizeof(int)*perconnum);
    unsigned char* conbitmap = (unsigned char *)malloc(sizeof(unsigned char)*perconnum/8+1);
    unsigned char* wribitmap = (unsigned char *)malloc(sizeof(unsigned char)*perconnum/8+1);
    unsigned char* reabitmap = (unsigned char *)malloc(sizeof(unsigned char)*perconnum/8+1);

    int epollret;
    struct epoll_event  event;
    struct epoll_event* eventresult= (struct epoll_event*)malloc(sizeof(struct epoll_event)*perconnum);
    struct epoll_event _tmpevt;
    struct epresult   * eprst;
    char   buf[1500];
    event.events = EPOLLIN | EPOLLET | EPOLLOUT | EPOLLRDHUP;


    for(i = 0; i < perconnum; i++)
    {
       while((sock = socket(AF_INET, SOCK_STREAM, 0))<0);
           *(Soqueue+i) = sock;

       fcntl(sock,F_SETFL,fcntl(sock,F_GETFL,0)|O_NONBLOCK);//NON_BLOCKING sockfd
    }

    QuickSort(Soqueue,i);

    for(i = 0; i < perconnum/8 +1; i++)
    {
       *(conbitmap+i)=*(wribitmap+i)=*(reabitmap+i)=0;
    }

    while(start==0);//wait for starting connect

    for(i = 0; i < perconnum ;i++)
    {
        sock = *(Soqueue+i) ;
        ret  = connect(sock, (struct sockaddr *)&ad, sizeof(ad));

        if(ret<0)
        {
            if(errno==EINPROGRESS)
            {
                struct tcp_info tcpinfo;
                int tcpinfolen = sizeof(tcpinfo);
                while(0)
                {
                  getsockopt(sock,IPPROTO_TCP,TCP_INFO,&tcpinfo,(socklen_t *)&tcpinfolen);
                  if(tcpinfo.tcpi_state=TCP_ESTABLISHED)
                  {


                  }
                }
            }
            else if (errno==EISCONN)//connected
            {
                SetState(Soqueue,perconnum,sock,conbitmap);
            }
            else
            {
                //failed++;
            }
            /*
            else if (error==ECONNREFUSED)//refused by host
            {
            }
            else if(error==ENETUNREACH)//cannot reach to host
            {
            }
            else if(error==EHOSTDOWN)//host is down
            {

            }
            */
        }
        else
        {
           SetState(Soqueue,perconnum,sock,conbitmap);
        }

        event.data.fd = sock;
        epollret   = epoll_ctl(epollfd,EPOLL_CTL_ADD,sock,&event);
        if(epollret<0)
        {
           wait_for_debug();
        }
    }

    while(1)
    {
      epollret = epoll_wait(epollfd, eventresult, perconnum,-1);
      if(epollret < 0)
      {
         wait_for_debug();
      }
      for(i = 0; i < epollret; i++)
      {
         _tmpevt =  eventresult[i];
          eprst = (struct epresult*)(_tmpevt.data.ptr);

         if(_tmpevt.events & (EPOLLERR|EPOLLRDHUP|EPOLLHUP))// socket is closed by host, socket is hup
         {
            //failed++;
            ClearState(Soqueue, perconnum, _tmpevt.data.fd,conbitmap,wribitmap,reabitmap);
            connect(_tmpevt.data.fd, (struct sockaddr *)&ad, sizeof(ad));
         }
         if(_tmpevt.events & EPOLLIN)
         {
            ret = read(_tmpevt.data.fd,buf,1500);
            if(ret==0)
            {

            }
            else if(ret<0)
            {
              //failed++;
              ClearState(Soqueue, perconnum, _tmpevt.data.fd,conbitmap,wribitmap,reabitmap);
              connect(_tmpevt.data.fd, (struct sockaddr *)&ad, sizeof(ad));
            }
            else
            {


            }
            //connections++;
         }
         if(_tmpevt.events& EPOLLOUT)
         {

         }
      }
    }
    close(epollfd);
    free(Soqueue);
    free(conbitmap);
    free(wribitmap);
    free(reabitmap);
}

void _qsort(int *array, int ii, int jj)
{

    int i,j,temp,p;
    if(ii<jj)
    {
        p=i=ii;j=jj;temp=array[i];
        while(i<j)
        {
            while(i<j&&array[p]<=array[j])
                j--;
            temp     = array[p];
            array[p] = array[j];
            array[j] = temp;

            while(i<j&&array[i]<=array[p])
                i++;
            temp     = array[p];
            array[p] = array[i];
            array[i] = temp;
        }
        _qsort(array,ii,i-1);
        _qsort(array,i+1,jj);
    }

}

void QuickSort(int *array,int length)
{
   _qsort(array,0,length-1);
}
int FindIndex(int *array,int length, int sockfd)
{
    int start = 0;
    int end   = length - 1 ;
    int mid   = (start+end)/2;

    while(array[mid]!=sockfd)
    {
        if(array[mid]>sockfd)
            end = mid - 1;
        else
           start = mid + 1;

        if(end<start)
          return -1;

        mid = (start + end )/2;
    }
    return mid;
}

int GetState(int *queue,int length,int sockfd,unsigned char* bitmap)
{
    int index = FindIndex(queue,length,sockfd) ;
    int indexhigh = index/8;
    int indexlow  = index%8;

    return (*(bitmap+indexhigh))&(1<<indexlow);
}

void SetState(int *queue,int length,int sockfd,unsigned char* bitmap)
{
    int index = FindIndex(queue,length,sockfd) ;
    int indexhigh = index/8;
    int indexlow  = index%8;

     *(bitmap+indexhigh)&=~(1<<indexlow);
}
void ClearState(int *queue,int length,int sockfd,unsigned char* bitmap1,unsigned char* bitmap2,unsigned char* bitmap3)
{
    int index = FindIndex(queue,length,sockfd) ;
    int indexhigh = index/8;
    int indexlow  = index%8;

    *(bitmap1+indexhigh)&=~(1<<indexlow);
    *(bitmap2+indexhigh)&=~(1<<indexlow);
    *(bitmap3+indexhigh)&=~(1<<indexlow);
}
