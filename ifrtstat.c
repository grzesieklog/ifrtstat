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
#define UINTMAX_AS_STR sizeof("18446744073709551615")

time_t now;
char date_t[32];
char *interface;

struct rtnl_link *nlink;
struct nl_sock *nlsocket;

mpz_t kilo,mega,giga,tera,peta,exa;
mpz_t sa,sb,a,b,aa,bb;
mpz_t maxrx,maxtx,maxrxp,maxtxp;
mpz_t days,timer;
mpz_t pmin;
mpz_t r,t,sr,st;
mpz_t rem_r,rem_t,rem_sr,rem_st;
#ifdef OVERFLOW
mpz_t max_u64;
#endif

uint8_t f_int=0;
uint8_t f_max=0;
uint8_t f_date=0;
uint8_t f_timer=0;
uint8_t f_greater=0;
uint8_t f_Bps=0;
uint8_t f_bps=1;
uint8_t f_conv=0;
uint8_t f_help=0;

void sig_stop(int sig);
void get_time();
void alloc_num();
void free_num();
void print_usage();
void conv(mpz_t* val, mpz_t* rem, uint8_t* big_flag, char* units, uint8_t sum);

int main(int argc, char* argv[]) {
  uint64_t rx_bytes=0,tx_bytes=0;

  int opt;
  const char options[] = "hmdtg:bBc";
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
      case 'B':
        if (f_Bps>0){
          printf("Option -B is duplicated.\n");
          exit(7);
        }
        if (f_bps>1){
          printf("Option -b and -B can't use simultaneously.\n");
          exit(7);
        }
        f_Bps=1;
        f_bps--;
        flag_sum++;
        break;
      case 'b':
        if (f_bps>1){
          printf("Option -b is duplicated.\n");
          exit(7);
        } 
        if (f_Bps){
          printf("Option -b and -B can't use simultaneously.\n");
          exit(7);
        }
        f_bps++;
        flag_sum++;
        break;
      case 'c':
        if (f_conv>0){
          printf("Option -c is duplicated.\n");
          exit(7);
        }
        f_conv=1;
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
  mpz_set_str(kilo,"1000",10);
  mpz_set_str(mega,"1000000",10);
  mpz_set_str(giga,"1000000000",10);
  mpz_set_str(tera,"1000000000000",10);
  mpz_set_str(peta,"1000000000000000",10);
  mpz_set_str(exa,"1000000000000000000",10);
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
    mpz_set_ui(maxrxp,0);
    mpz_set_ui(maxtxp,0);
  }
  if (f_greater){
    mpz_set_str(pmin,greater,10);
  }
  char rxbuf[UINTMAX_AS_STR+2];
  char txbuf[UINTMAX_AS_STR+2];
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
        if (mpz_cmp(r,pmin)<0 && mpz_cmp(t,pmin)<0) printrt=0;
      }
      if (printrt){
        bigr=bigt=bigsr=bigst=0;
        memset(rj,0,sizeof(rj));
        memset(tj,0,sizeof(tj));
        memset(srj,0,sizeof(srj));
        memset(stj,0,sizeof(stj));
      }
      // cal units
      if (printrt && f_conv){
        // bit or Bytes
        if (f_bps && !f_Bps)
        {
          mpz_mul_ui(r,r,8);
          mpz_mul_ui(t,t,8);
          mpz_mul_ui(sr,sr,8);
          mpz_mul_ui(st,st,8);
        }
        // sum
        conv(&sr, &rem_sr, &bigsr, srj, 1);
        conv(&st, &rem_st, &bigst, stj, 1);
        // rate
        conv(&r, &rem_r, &bigr, rj, 0);
        conv(&t, &rem_t, &bigt, tj, 0);
      }
      // option -B
      if (printrt && f_Bps && !f_conv){
          strcpy(srj,"B");
          strcpy(stj,"B");
          strcpy(rj,"B/s");
          strcpy(tj,"B/s");
      }
      // option -b (default f_bps)
      if (printrt && f_bps && !f_conv){
          strcpy(srj,"bit");
          strcpy(stj,"bit");
          strcpy(rj,"bit/s");
          strcpy(tj,"bit/s");
          mpz_mul_ui(r,r,8);
          mpz_mul_ui(t,t,8);
          mpz_mul_ui(sr,sr,8);
          mpz_mul_ui(st,st,8);
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
          if (f_bps && !f_Bps)
          {
            mpz_mul_ui(maxrxp,maxrx,8);
            mpz_mul_ui(maxtxp,maxtx,8);
          }
          else
          {
            mpz_set(maxrxp,maxrx);
            mpz_set(maxtxp,maxtx);
          }
          if (newrxmax){
            gmp_printf(" Max rx %Zd",maxrxp);
            newrxmax=0;
            if (f_bps && !f_Bps)
              printf(" bit/s");
            else
              printf(" B/s");
            }
          if (newtxmax){
            gmp_printf(" Max tx %Zd",maxtxp);
            newtxmax=0;
            if (f_bps && !f_Bps)
              printf(" bit/s");
            else
              printf(" B/s");
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
  mpz_init(kilo);
  mpz_init(mega);
  mpz_init(giga);
  mpz_init(tera);
  mpz_init(peta);
  mpz_init(exa);
  mpz_init(timer);
  mpz_init(days);
  mpz_init(pmin);
  mpz_init(sa);  // save first a (tx) B
  mpz_init(sb);  // save first b (tx) B
  mpz_init(a);  // read rx B
  mpz_init(b);  // read tx B
  mpz_init(aa);  // prev rx B
  mpz_init(bb);  // prev tx B
  if (f_max){
    mpz_init(maxrx);
    mpz_init(maxtx);
    mpz_init(maxrxp);
    mpz_init(maxtxp);
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
  mpz_clear(pmin);
  mpz_clear(sa);
  mpz_clear(sb);
  mpz_clear(a);
  mpz_clear(b);
  mpz_clear(aa);
  mpz_clear(bb);
  if (f_max){
    mpz_clear(maxrx);
    mpz_clear(maxtx);
    mpz_clear(maxrxp);
    mpz_clear(maxtxp);
  }
  mpz_clear(r);
  mpz_clear(t);
  mpz_clear(sr);
  mpz_clear(st);
  mpz_clear(rem_r);
  mpz_clear(rem_t);
  mpz_clear(rem_sr);
  mpz_clear(rem_st);
  mpz_clear(kilo);
  mpz_clear(mega);
  mpz_clear(giga);
  mpz_clear(tera);
  mpz_clear(peta);
  mpz_clear(exa);
#ifdef OVERFLOW
  mpz_clear(max_u64);
#endif
}

void conv(mpz_t* val, mpz_t* rem, uint8_t* big_flag, char* units, uint8_t sum){
  if (mpz_cmp(*val,exa)>0)
  {
    mpz_fdiv_qr(*val,*rem,*val,exa);
    mpz_fdiv_q(*rem,*rem,peta);
    if (f_bps)
      if (sum)
        strcpy(units,"Eb");
      else
        strcpy(units,"Eb/s");
    else
      if (sum)
        strcpy(units,"EB");
      else
        strcpy(units,"EB/s");
    *big_flag=1;
  }
  else if (mpz_cmp(*val,peta)>0)
  {
    mpz_fdiv_qr(*val,*rem,*val,peta);
    mpz_fdiv_q(*rem,*rem,tera);
    if (f_bps)
      if (sum)
        strcpy(units,"Pb");
      else
        strcpy(units,"Pb/s");
    else
      if (sum)
        strcpy(units,"PB");
      else
        strcpy(units,"PB/s");
    *big_flag=1;
  }
  else if (mpz_cmp(*val,tera)>0)
  {
    mpz_fdiv_qr(*val,*rem,*val,tera);
    mpz_fdiv_q(*rem,*rem,giga);
    if (f_bps)
      if (sum)
        strcpy(units,"Tb");
      else
        strcpy(units,"Tb/s");
    else
      if (sum)
        strcpy(units,"TB");
      else
        strcpy(units,"TB/s");
    *big_flag=1;
  }
  else if (mpz_cmp(*val,giga)>0)
  {
    mpz_fdiv_qr(*val,*rem,*val,giga);
    mpz_fdiv_q(*rem,*rem,mega);
    if (f_bps)
      if (sum)
        strcpy(units,"Gb");
      else
        strcpy(units,"Gb/s");
    else
      if (sum)
        strcpy(units,"GB");
      else
        strcpy(units,"GB/s");
    *big_flag=1;
  }
  else if (mpz_cmp(*val,mega)>0)
  {
    mpz_fdiv_qr(*val,*rem,*val,mega);
    mpz_fdiv_q(*rem,*rem,kilo);
    if (f_bps)
      if (sum)
        strcpy(units,"Mb");
      else
        strcpy(units,"Mb/s");
    else
      if (sum)
        strcpy(units,"MB");
      else
        strcpy(units,"MB/s");
    *big_flag=1;
  }
  else if (mpz_cmp(*val,kilo)>0)
  {
    mpz_fdiv_qr(*val,*rem,*val,kilo);
    if (f_bps)
      if (sum)
        strcpy(units,"kb");
      else
        strcpy(units,"kb/s");
    else
      if (sum)
        strcpy(units,"kB");
      else
        strcpy(units,"kB/s");
    *big_flag=1;
  }
  else
    if (f_bps)
      if (sum)
        strcpy(units,"bit");
      else
        strcpy(units,"bit/s");
    else
      if (sum)
        strcpy(units,"B");
      else
        strcpy(units,"B/s");
}

void print_usage(){
  printf("Usage: %s [OPTIONS] [INTERFACE]\n",PROG_NAME);
  fputs ("Options:\n\
   -m      print maximum data rate\n\
   -d      print date on each line\n\
   -t      print counter divided into days/hours/minutes\n\
   -g val  print only values greater than val\n\
   -B      print rates as Bps\n\
   -b      print rates as bps (default)\n\
   -c      converting to larger units (k,M,G,T,P,E)\n\
   -h      print this message\n\
\n", stdout);
  printf("%s is a Linux network interface rx/tx status.\n",PROG_NAME);
#ifdef OVERFLOW
  printf("Compiled with counter overflow\n");
#endif
}


