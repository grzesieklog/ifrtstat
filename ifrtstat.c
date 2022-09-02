#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

#include <gmp.h>

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/link.h>

#define PROG_NAME "ifrtstat"
#define MAX_IF_LEN 16
#define MAX_GOPT_LEN 20
#define DAY_IN_SEC 60*60*24
#define UINTMAX_MAX_AS_STR sizeof("18446744073709551615")

time_t now;
char date_t[32];
char *interface;

struct rtnl_link *nlink;
struct nl_sock *nlsocket;

mpz_t kB,MB,GB,TB,PB,EB;
mpz_t sa,sb,a,b,aa,bb;
mpz_t maxrx,maxtx;
mpz_t days,timer;
mpz_t min_Bps;
mpz_t r,t,sr,st;
mpz_t rem_r,rem_t,rem_sr,rem_st;
#ifdef OVERFLOW
mpz_t max_u64;
#endif

uint8_t f_int=0,
        f_max=0,
        f_date=0,
        f_timer=0,
        f_greater=0,
        f_Bps=0,
        f_help=0;

void sig_stop(int sig);
void get_time();
void alloc_num();
void free_num();
void print_usage();

int main(int argc, char* argv[]) {
  uint64_t rx_bytes=0,tx_bytes=0;

  int opt;
  const char options[] = "hmdtg:b";
  char *greater;
  struct stat fstat;
  uint8_t flag_sum=0;
  uint8_t int_sum=0;
  // get options
  while ((opt=getopt(argc,argv,options)) != -1) {
    switch (opt) {
      case 'h':
        if (f_help>0){
          printf("Option -h is duplicated.\n");
          exit(7);
        }
        f_help=1;
        flag_sum++;
        break;
      case 'm':
        if (f_max>0){
          printf("Option -m is duplicated.\n");
          exit(7);
        }
        f_max=1;
        flag_sum++;
        break;
      case 'd':
        if (f_date>0){
          printf("Option -d is duplicated.\n");
          exit(7);
        }
        f_date=1;
        flag_sum++;
        break;
      case 't':
        if (f_timer>0){
          printf("Option -t is duplicated.\n");
          exit(7);
        }
        f_timer=1;
        flag_sum++;
        break;
      case 'g':
        if (f_greater>0){
          printf("Option -g is duplicated.\n");
          exit(7);
        }
        if (strlen(optarg)>MAX_GOPT_LEN){
          printf("Value of -g option is too long.\n");
          exit(7);
        }
        uint8_t is_dig=1;
        for (char* s=optarg;*s!=0;s++){
          if (isdigit((unsigned char)*s)==0) is_dig=0;
        }
        if (!is_dig){
          printf("Value of -g option is not digits.\n");
          exit(7);
        }
        // TODO clear '=' from val
        greater = optarg;
        f_greater=1;
        flag_sum++;
        break;
      case 'b':
        if (f_Bps>0){
          printf("Option -b is duplicated.\n");
          exit(7);
        }
        f_Bps=1;
        flag_sum++;
        break;
      case '?':
        printf("Use option -h to print help.\n");
        exit(7);
        break;
    }
  }
  // prepare connection to netlink
  nlsocket = nl_socket_alloc();
  if (!nlsocket)
    exit(30);
  if (nl_connect(nlsocket,NETLINK_ROUTE)!=0){
    nl_socket_free(nlsocket);
    exit(31);
  }
  // search if name
  for (int n=1; n<argc; ++n) {
    if (f_greater)
      if (strcmp(argv[n],greater)==0)
        continue;
    if (strlen(argv[n])>MAX_IF_LEN){
      printf("Too long arg: %s\n",argv[n]);
      exit(10);
    }
    if (argv[n][0]=='-') continue;
    if (rtnl_link_get_kernel(nlsocket,0,argv[n],&nlink)==0){ // if exist?
      rtnl_link_put(nlink);
      interface = argv[n];
      f_int=1;
      int_sum++;
    }
    else{
      printf("unknown arg: %s\n",argv[n]);
      int_sum++;
    }
  }
  // check args count
  if (flag_sum > strlen(options)-1 || // has ':'
      int_sum > 1){
    printf("Too many args...\n");
    nl_socket_free(nlsocket);
    exit(9);
  }
  // check options combination
  if (flag_sum>0){
    if (flag_sum>1 && f_help){
      printf("Option -h cannot be used with other options.\n");
      nl_socket_free(nlsocket);
      exit(8);
    }
  }
  // interface not found
  if (!f_int && !f_help && argc>1){
    printf("Interface not found!\n");
    nl_socket_free(nlsocket);
    exit(12);
  }
  if (f_help || argc==1){
    print_usage();
    nl_socket_free(nlsocket);
    exit(0);
  }
  // Termination Signals
  signal(SIGINT, sig_stop);
  signal(SIGHUP, sig_stop);
  signal(SIGKILL, sig_stop);
  signal(SIGQUIT, sig_stop);
  signal(SIGTERM, sig_stop);
  
  alloc_num();
  mpz_set_str(kB,"1000",10);
  mpz_set_str(MB,"1000000",10);
  mpz_set_str(GB,"1000000000",10);
  mpz_set_str(TB,"1000000000000",10);
  mpz_set_str(PB,"1000000000000000",10);
  mpz_set_str(EB,"1000000000000000000",10);
  mpz_set_ui(timer,0);
  mpz_set_ui(sa,0);
  mpz_set_ui(sb,0);
  mpz_set_ui(a,0);
  mpz_set_ui(b,0);
  mpz_set_ui(aa,0);
  mpz_set_ui(bb,0);
#ifdef OVERFLOW
  mpz_set_str(max_u64,"18446744073709551616",10);
#endif
  if (f_max){
    mpz_set_ui(maxrx,0);
    mpz_set_ui(maxtx,0);
  }
  if (f_greater){
    mpz_set_str(min_Bps,greater,10);
  }
  char rxbuf[UINTMAX_MAX_AS_STR+2];
  char txbuf[UINTMAX_MAX_AS_STR+2];
  uint32_t ui_hours=0,ui_minutes=0,ui_sec=0;
  uint8_t newrxmax=0,newtxmax=0;
  uint8_t bigr=0,bigt=0,bigsr=0,bigst=0;
  char rj[6],tj[6],srj[4],stj[4];
  uint8_t printrt;
  if (!f_date){
    get_time();
    printf("%s %s start at %s\n",PROG_NAME,interface,date_t);
  }
  while(1) {
    printrt=1;
    if(rtnl_link_get_kernel(nlsocket,0,interface,&nlink)==0){
      rx_bytes=rtnl_link_get_stat(nlink,RTNL_LINK_RX_BYTES);
      tx_bytes=rtnl_link_get_stat(nlink,RTNL_LINK_TX_BYTES);
      rtnl_link_put(nlink);
      memset(rxbuf,0,sizeof(rxbuf));
      memset(txbuf,0,sizeof(txbuf));
      sprintf(rxbuf,"%" PRIu64,rx_bytes);
      sprintf(txbuf,"%" PRIu64,tx_bytes);
      mpz_set_str(a,rxbuf,10);
      mpz_set_str(b,txbuf,10);
      // first init
      if (mpz_cmp_ui(timer,0)==0){
        mpz_set(aa,a);
        mpz_set(sa,a);
        mpz_set(bb,b);
        mpz_set(sb,b);
      }
#ifdef OVERFLOW
      else {
        if (mpz_cmp(a,aa)<0) {
          mpz_sub(sa,sa,max_u64);
          mpz_sub(aa,aa,max_u64);
          printf("%s %s INF: RX counter is overflow\n",PROG_NAME,interface);
        }
        if (mpz_cmp(b,bb)<0) {
          mpz_sub(sb,sb,max_u64);
          mpz_sub(bb,bb,max_u64);
          printf("%s %s INF: TX counter is overflow\n",PROG_NAME,interface);
        }
      }
#endif
      // diff of B
      mpz_sub(r,a,aa);
      mpz_sub(t,b,bb);
      mpz_sub(sr,a,sa);
      mpz_sub(st,b,sb);
      // MAX
      if (f_max){
        if (mpz_cmp(r,maxrx)>0){ mpz_set(maxrx,r); newrxmax=1;}
        if (mpz_cmp(t,maxtx)>0){ mpz_set(maxtx,t); newtxmax=1;}
      }
      // over then
      if (f_greater){
        if (mpz_cmp(r,min_Bps)<0 && mpz_cmp(t,min_Bps)<0) printrt=0;
      }
      if (printrt){
        bigr=bigt=bigsr=bigst=0;
        memset(rj,0,sizeof(rj));
        memset(tj,0,sizeof(tj));
        memset(srj,0,sizeof(srj));
        memset(stj,0,sizeof(stj));
      }
      // cal units
      if (printrt && !f_Bps){
        if (mpz_cmp(sr,EB)>0)
          { mpz_fdiv_qr(sr,rem_sr,sr,EB); mpz_fdiv_q(rem_sr,rem_sr,PB); strcpy(srj,"EB"); bigsr=1;}
        else if (mpz_cmp(sr,PB)>0)
          { mpz_fdiv_qr(sr,rem_sr,sr,PB); mpz_fdiv_q(rem_sr,rem_sr,TB); strcpy(srj,"PB"); bigsr=1;}
        else if (mpz_cmp(sr,TB)>0)
          { mpz_fdiv_qr(sr,rem_sr,sr,TB); mpz_fdiv_q(rem_sr,rem_sr,GB); strcpy(srj,"TB"); bigsr=1;}
        else if (mpz_cmp(sr,GB)>0)
          { mpz_fdiv_qr(sr,rem_sr,sr,GB); mpz_fdiv_q(rem_sr,rem_sr,MB); strcpy(srj,"GB"); bigsr=1;}
        else if (mpz_cmp(sr,MB)>0)
          { mpz_fdiv_qr(sr,rem_sr,sr,MB); mpz_fdiv_q(rem_sr,rem_sr,kB); strcpy(srj,"MB"); bigsr=1;}
        else if (mpz_cmp(sr,kB)>0)
          { mpz_fdiv_qr(sr,rem_sr,sr,kB); strcpy(srj,"kB"); bigsr=1;}
        else
          strcpy(srj,"B");
        if (mpz_cmp(st,EB)>0)
          { mpz_fdiv_qr(st,rem_st,st,EB); mpz_fdiv_q(rem_st,rem_st,PB); strcpy(stj,"EB"); bigst=1;}
        else if (mpz_cmp(st,PB)>0)
          { mpz_fdiv_qr(st,rem_st,st,PB); mpz_fdiv_q(rem_st,rem_st,TB); strcpy(stj,"PB"); bigst=1;}
        else if (mpz_cmp(st,TB)>0)
          { mpz_fdiv_qr(st,rem_st,st,TB); mpz_fdiv_q(rem_st,rem_st,GB); strcpy(stj,"TB"); bigst=1;}
        else if (mpz_cmp(st,GB)>0)
          { mpz_fdiv_qr(st,rem_st,st,GB); mpz_fdiv_q(rem_st,rem_st,MB); strcpy(stj,"GB"); bigst=1;}
        else if (mpz_cmp(st,MB)>0)
          { mpz_fdiv_qr(st,rem_st,st,MB); mpz_fdiv_q(rem_st,rem_st,kB); strcpy(stj,"MB"); bigst=1;}
        else if (mpz_cmp(st,kB)>0)
          { mpz_fdiv_qr(st,rem_st,st,kB); strcpy(stj,"kB");bigst=1;}
        else
          strcpy(stj,"B");
        if (mpz_cmp(r,TB)>0)
          { mpz_fdiv_qr(r,rem_r,r,TB); mpz_fdiv_q(rem_r,rem_r,GB); strcpy(rj,"TB/s"); bigr=1;}
        else if (mpz_cmp(r,GB)>0)
          { mpz_fdiv_qr(r,rem_r,r,GB); mpz_fdiv_q(rem_r,rem_r,MB); strcpy(rj,"GB/s"); bigr=1;}
        else if (mpz_cmp(r,MB)>0)
          { mpz_fdiv_qr(r,rem_r,r,MB); mpz_fdiv_q(rem_r,rem_r,kB); strcpy(rj,"MB/s"); bigr=1;}
        else if (mpz_cmp(r,kB)>0)
          { mpz_fdiv_qr(r,rem_r,r,kB); strcpy(rj,"kB/s"); bigr=1;}
        else
          strcpy(rj,"B/s");
        if (mpz_cmp(t,TB)>0)
          { mpz_fdiv_qr(t,rem_t,t,TB); mpz_fdiv_q(rem_t,rem_t,GB); strcpy(tj,"TB/s"); bigt=1;}
        else if (mpz_cmp(t,GB)>0)
          { mpz_fdiv_qr(t,rem_t,t,GB); mpz_fdiv_q(rem_t,rem_t,MB); strcpy(tj,"GB/s"); bigt=1;}
        else if (mpz_cmp(t,MB)>0)
          { mpz_fdiv_qr(t,rem_t,t,MB); mpz_fdiv_q(rem_t,rem_t,kB); strcpy(tj,"MB/s"); bigt=1;}
        else if (mpz_cmp(t,kB)>0)
          { mpz_fdiv_qr(t,rem_t,t,kB); strcpy(tj,"kB/s"); bigt=1;}
        else
          strcpy(tj,"B/s");
      }
      // option -b
      if (printrt && f_Bps){
          strcpy(srj,"B");
          strcpy(stj,"B");
          strcpy(rj,"B/s");
          strcpy(tj,"B/s");
      }
      // print
      if (mpz_cmp_ui(timer,0)>0 && printrt){
        // date
        if (f_date){
          get_time();
          printf("%s ",date_t);
        }
        // interface
        printf("%s",interface);
        // counter to timers
        if (f_timer){
          ui_sec = mpz_fdiv_q_ui(days,timer,DAY_IN_SEC);
          ui_hours = ui_sec/(DAY_IN_SEC/24);
          ui_sec = ui_sec-(ui_hours*(DAY_IN_SEC/24));
          ui_minutes = ui_sec/60;
          ui_sec = ui_sec-(ui_minutes*60);
          printf("[");
          if (mpz_cmp_ui(days,0)>0)
            gmp_printf("%Zd%s%02u%s%02u%s%02u]",days,"d",ui_hours,"h",ui_minutes,"m",ui_sec);
          else if (ui_hours)
            printf("%u%s%02u%s%02u]",ui_hours,"h",ui_minutes,"m",ui_sec);
          else if (ui_minutes)
            printf("%u%s%02u]",ui_minutes,"m",ui_sec);
          else
            printf("%02u]",ui_sec);
        }
        else
          gmp_printf("[%Zd]",timer);
        // sum
        if (bigsr){
          gmp_printf(" Sum rx %Zd.%03Zd %s ",sr,rem_sr,srj);
        }
        else
          gmp_printf(" Sum rx %Zd %s ",sr,srj);
        if (bigst){
          gmp_printf("tx %Zd.%03Zd %s ",st,rem_st,stj);
        }
        else
          gmp_printf("tx %Zd %s ",st,stj);
        // current rate
        if (bigr){
          gmp_printf("Cur rx %Zd.%03Zd %s ",r,rem_r,rj);
        }
        else
          gmp_printf("Cur rx %Zd %s ",r,rj);
        if (bigt){
          gmp_printf("tx %Zd.%03Zd %s",t,rem_t,tj);
        }
        else
          gmp_printf("tx %Zd %s",t,tj);
        // max
        if (f_max){
          if (newrxmax){
            gmp_printf(" Max rx %Zd B/s",maxrx);
            newrxmax=0;
          }
          if (newtxmax){
            gmp_printf(" Max tx %Zd B/s",maxtx);
            newtxmax=0;
          }
        }
        // end
        printf("\n");
        fflush(stdout);
      }
      // cur to prev
      mpz_set(aa,a);
      mpz_set(bb,b);
    }
    else printf("No data...\n");
    sleep(1);
    mpz_add_ui(timer,timer,1);
  }
  nl_socket_free(nlsocket);
  free_num();
  printf("exit\n");
  exit(0);
}

void sig_stop(int sig)
{
  nl_socket_free(nlsocket);
  free_num();
  printf("Exit\n");
  exit(0);
}
void get_time(){
  memset(date_t,0,sizeof(date_t));
  time(&now);
  strcpy(date_t,ctime(&now));
  date_t[strcspn(date_t,"\n")] = 0;
}
void alloc_num(){
  mpz_init(kB);
  mpz_init(MB);
  mpz_init(GB);
  mpz_init(TB);
  mpz_init(PB);
  mpz_init(EB);
  mpz_init(timer);
  mpz_init(days);
  mpz_init(min_Bps);
  mpz_init(sa);  // save first a (tx) B
  mpz_init(sb);  // save first b (tx) B
  mpz_init(a);  // read rx B
  mpz_init(b);  // read tx B
  mpz_init(aa);  // prev rx B
  mpz_init(bb);  // prev tx B
  if (f_max){
    mpz_init(maxrx);
    mpz_init(maxtx);
  }
  mpz_init(r);  // cur rx
  mpz_init(t);  // cur tx
  mpz_init(sr);  // sum rx
  mpz_init(st);  // sum tx
  mpz_init(rem_r);  // remainder from dividing
  mpz_init(rem_t);
  mpz_init(rem_sr);
  mpz_init(rem_st);
#ifdef OVERFLOW
  mpz_init(max_u64);
#endif
}
void free_num(){
  mpz_clear(timer);
  mpz_clear(days);
  mpz_clear(min_Bps);
  mpz_clear(sa);
  mpz_clear(sb);
  mpz_clear(a);
  mpz_clear(b);
  mpz_clear(aa);
  mpz_clear(bb);
  if (f_max){
    mpz_clear(maxrx);
    mpz_clear(maxtx);
  }
  mpz_clear(r);
  mpz_clear(t);
  mpz_clear(sr);
  mpz_clear(st);
  mpz_clear(rem_r);
  mpz_clear(rem_t);
  mpz_clear(rem_sr);
  mpz_clear(rem_st);
  mpz_clear(kB);
  mpz_clear(MB);
  mpz_clear(GB);
  mpz_clear(TB);
  mpz_clear(PB);
  mpz_clear(EB);
#ifdef OVERFLOW
  mpz_clear(max_u64);
#endif
}

void print_usage(){
  printf("Usage: %s [OPTIONS] [INTERFACE]\n",PROG_NAME);
  fputs ("Options:\n\
   -m      print maximum data rate\n\
   -d      print date on each line\n\
   -t      print counter divided into days/hours/minutes\n\
   -g val  print only Bps values greater than val\n\
   -b      print rates as Bps, don't calculate it\n\
   -h      print this message\n\
\n", stdout);
  printf("%s is a Linux network interface rx/tx status.\n",PROG_NAME);
#ifdef OVERFLOW
  printf("Compiled with counter overflow\n");
#endif
}


