// gcc ifrtstat.c -o ifrtstat -lgmp
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <gmp.h>


#define PROG_NAME "ifrtstat"
#define IF_LEN 16
#define IF_DIR "/sys/class/net/"
#define IF_PATH_R "/statistics/rx_bytes"
#define IF_PATH_T "/statistics/tx_bytes"
#define DAY_IN_SEC 60*60*24


time_t now;
char date_t[32];
int filedr=0;
int filedt=0;
char *interface;
char testpath[64];
char rxpath[64];
char txpath[64];

mpz_t kB,MB,GB,TB,PB;
mpz_t kBborder,MBborder,GBborder,TBborder,PBborder;
mpz_t sa,sb,a,b,aa,bb;
mpz_t maxrx,maxtx;
mpz_t days,timer;
mpz_t min_Bps;
mpz_t r,t,sr,st;
mpz_t _r,_t,_sr,_st;
mpz_t rem_r,rem_t,rem_sr,rem_st;

unsigned char f_int=0,
              f_max=0,
              f_date=0,
              f_timer=0,
              f_greater=0,
              f_help=0;

void sig_stop(int sig);
void get_time();
void alloc_num();
void free_num();
void print_usage();

int main(int argc, char* argv[]) {
  int opt;
  const char options[] = "hmdtg:";
  char *greater;
  struct stat fstat;
  unsigned char flag_sum=0;
  unsigned char int_sum=0;
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
        // TODO clear '=' from val
        greater = optarg;
        f_greater=1;
        flag_sum++;
        break;
      case '?':
        printf("Use option -h to print help.\n");
        exit(7);
        break;
    }
  }
  // search if name
  for (int n=1; n<argc; ++n) {
    // TODO len of -g val, now is 999..PB
    if (strlen(argv[n])>IF_LEN){
      printf("Too long arg: %s\n",argv[n]);
      exit(10);
    }
    if (f_greater) if (strcmp(argv[n],greater)==0) continue;
    if (argv[n][0]=='-') continue;
    memset(testpath,0,sizeof(testpath));
    strcat(testpath,IF_DIR);
    strcat(testpath,argv[n]);
    if (stat(testpath,&fstat)==0){ // if exist?
      if (S_ISDIR(fstat.st_mode)) {
        interface = argv[n];
        memset(rxpath,0,sizeof(rxpath));
        memset(txpath,0,sizeof(txpath));
        strcat(rxpath,IF_DIR);
        strcat(txpath,IF_DIR);
        strcat(rxpath,interface);
        strcat(txpath,interface);
        strcat(rxpath,IF_PATH_R);
        strcat(txpath,IF_PATH_T);
        if (access(rxpath,R_OK)==0 && access(txpath,R_OK)==0){
          f_int=1;
          int_sum++;
        } else {
          printf("Cannot read:\n%s\n%s\n",rxpath,txpath);
          exit(11);
        }
      }
    }
    else{
      printf("unknown arg: %s\n",argv[n]);
      int_sum++;
    }
  }
  // check args count
  if (flag_sum > strlen(options)+1 || // +1: -g val
      int_sum > 1){
    printf("Too many args...\n");
    exit(9);
  }
  // check options combination
  if (flag_sum>0){
    if (flag_sum>1 && f_help){
      printf("Option -h cannot be used with other options.\n");
      exit(8);
    }
  }
  // interface not found
  if (!f_int && !f_help && argc>1){
    printf("Interface not found!\n");
    exit(12);
  }
  if (f_help || argc==1){
    print_usage();
    exit(0);
  }
  
  signal(SIGINT, sig_stop);
  signal(SIGHUP, sig_stop);
  signal(SIGKILL, sig_stop);
  signal(SIGQUIT, sig_stop);
  signal(SIGTERM, sig_stop);
  //signal(SIGTSTP, sig_stop);
  filedr = open(rxpath, O_RDONLY );
  filedt = open(txpath, O_RDONLY );

  if( !filedr || !filedt ) {
    printf("Open file error\n");
    exit(13);
  }
  alloc_num();
  mpz_init_set_str(kB,"1000",10);
  mpz_init_set_str(MB,"1000000",10);
  mpz_init_set_str(GB,"1000000000",10);
  mpz_init_set_str(TB,"1000000000000",10);
  mpz_init_set_str(PB,"1000000000000000",10);
  mpz_init_set_str(kBborder,"1000",10);
  mpz_init_set_str(MBborder,"1000000",10);
  mpz_init_set_str(GBborder,"1000000000",10);
  mpz_init_set_str(TBborder,"1000000000000",10);
  mpz_init_set_str(PBborder,"1000000000000000",10);
  mpz_set_ui(timer,0);
  mpz_set_ui(sa,0);
  mpz_set_ui(sb,0);
  mpz_set_ui(a,0);
  mpz_set_ui(b,0);
  mpz_set_ui(aa,0);
  mpz_set_ui(bb,0);
  if (f_max){
    mpz_set_ui(maxrx,0);
    mpz_set_ui(maxtx,0);
  }
  if (f_greater){
    mpz_init_set_str(min_Bps,greater,10);
    //gmp_printf("%Zd\n",min_Bps);
  }
  char rxbuf[128];
  char txbuf[128];
  int nbytesr=0, nbytest=0;
  unsigned long int ui_hours=0,ui_minutes=0,ui_sec=0;
  char newrxmax=0,newtxmax=0;
  char bigr=0,bigt=0,bigsr=0,bigst=0;
  char rj[6],tj[6],srj[4],stj[4];
  char printrt;
  if (!f_date){
    get_time();
    printf("%s %s start at %s\n",PROG_NAME,interface,date_t);
  }
  while(1) {
    printrt=1;
    memset(rxbuf,0,sizeof(rxbuf));
    memset(txbuf,0,sizeof(txbuf));
    nbytesr = read(filedr, rxbuf, 128);
    nbytest = read(filedt, txbuf, 128);
    if(nbytesr > 0 && nbytest > 0) {
      mpz_init_set_str(a,rxbuf,10);
      mpz_init_set_str(b,txbuf,10);
      // first init
      if (mpz_cmp_ui(timer,0)==0){
        mpz_init_set(aa,a);
        mpz_init_set(sa,a);
        mpz_init_set(bb,b);
        mpz_init_set(sb,b);
      }
      bigr=bigt=bigsr=bigst=0;
      memset(rj,0,sizeof(rj));
      memset(tj,0,sizeof(tj));
      memset(srj,0,sizeof(srj));
      memset(stj,0,sizeof(stj));
      // diff of B
      mpz_sub(r,a,aa);
      mpz_sub(t,b,bb);
      mpz_sub(sr,a,sa);
      mpz_sub(st,b,sb);
      // save org
      mpz_set(_r,r);
      mpz_set(_t,t);
      mpz_set(_sr,sr);
      mpz_set(_st,st);
      // MAX
      if (f_max){
        if (mpz_cmp(r,maxrx)>0){ mpz_init_set(maxrx,r); newrxmax=1;}
        if (mpz_cmp(t,maxtx)>0){ mpz_init_set(maxtx,t); newtxmax=1;}
      }
      // over then
      if (f_greater){
        if (mpz_cmp(r,min_Bps)<0 && mpz_cmp(t,min_Bps)<0) printrt=0;
      }
      // TODO don't calculate if not printrt
      // cal units
      if (mpz_cmp(sr,PBborder)>0)
        { mpz_fdiv_qr(sr,rem_sr,sr,PB); mpz_fdiv_q(rem_sr,rem_sr,TB); strcpy(srj,"PB"); bigsr=1;}
      else if (mpz_cmp(sr,TBborder)>0)
        { mpz_fdiv_qr(sr,rem_sr,sr,TB); mpz_fdiv_q(rem_sr,rem_sr,GB); strcpy(srj,"TB"); bigsr=1;}
      else if (mpz_cmp(sr,GBborder)>0)
        { mpz_fdiv_qr(sr,rem_sr,sr,GB); mpz_fdiv_q(rem_sr,rem_sr,MB); strcpy(srj,"GB"); bigsr=1;}
      else if (mpz_cmp(sr,MBborder)>0)
        { mpz_fdiv_qr(sr,rem_sr,sr,MB); mpz_fdiv_q(rem_sr,rem_sr,kB); strcpy(srj,"MB"); bigsr=1;}
      else if (mpz_cmp(sr,kBborder)>0)
        { mpz_fdiv_qr(sr,rem_sr,sr,kB); strcpy(srj,"kB"); bigsr=1;}
      else
        strcpy(srj,"B");
      if (mpz_cmp(st,PBborder)>0)
        { mpz_fdiv_qr(st,rem_st,st,PB); mpz_fdiv_q(rem_st,rem_st,TB); strcpy(stj,"PB"); bigst=1;}
      else if (mpz_cmp(st,TBborder)>0)
        { mpz_fdiv_qr(st,rem_st,st,TB); mpz_fdiv_q(rem_st,rem_st,GB); strcpy(stj,"TB"); bigst=1;}
      else if (mpz_cmp(st,GBborder)>0)
        { mpz_fdiv_qr(st,rem_st,st,GB); mpz_fdiv_q(rem_st,rem_st,MB); strcpy(stj,"GB"); bigst=1;}
      else if (mpz_cmp(st,MBborder)>0)
        { mpz_fdiv_qr(st,rem_st,st,MB); mpz_fdiv_q(rem_st,rem_st,kB); strcpy(stj,"MB"); bigst=1;}
      else if (mpz_cmp(st,kBborder)>0)
        { mpz_fdiv_qr(st,rem_st,st,kB); strcpy(stj,"kB");bigst=1;}
      else
        strcpy(stj,"B");
      if (mpz_cmp(r,PBborder)>0)
        { mpz_fdiv_qr(r,rem_r,r,PB); mpz_fdiv_q(rem_r,rem_r,TB); strcpy(rj,"PB/s"); bigr=1;}
      else if (mpz_cmp(r,TBborder)>0)
        { mpz_fdiv_qr(r,rem_r,r,TB); mpz_fdiv_q(rem_r,rem_r,GB); strcpy(rj,"TB/s"); bigr=1;}
      else if (mpz_cmp(r,GBborder)>0)
        { mpz_fdiv_qr(r,rem_r,r,GB); mpz_fdiv_q(rem_r,rem_r,MB); strcpy(rj,"GB/s"); bigr=1;}
      else if (mpz_cmp(r,MBborder)>0)
        { mpz_fdiv_qr(r,rem_r,r,MB); mpz_fdiv_q(rem_r,rem_r,kB); strcpy(rj,"MB/s"); bigr=1;}
      else if (mpz_cmp(r,kBborder)>0)
        { mpz_fdiv_qr(r,rem_r,r,kB); strcpy(rj,"kB/s"); bigr=1;}
      else
        strcpy(rj,"B/s");
      if (mpz_cmp(t,PBborder)>0)
        { mpz_fdiv_qr(t,rem_t,t,PB); mpz_fdiv_q(rem_t,rem_t,TB); strcpy(tj,"PB/s"); bigt=1;}
      else if (mpz_cmp(t,TBborder)>0)
        { mpz_fdiv_qr(t,rem_t,t,TB); mpz_fdiv_q(rem_t,rem_t,GB); strcpy(tj,"TB/s"); bigt=1;}
      else if (mpz_cmp(t,GBborder)>0)
        { mpz_fdiv_qr(t,rem_t,t,GB); mpz_fdiv_q(rem_t,rem_t,MB); strcpy(tj,"GB/s"); bigt=1;}
      else if (mpz_cmp(t,MBborder)>0)
        { mpz_fdiv_qr(t,rem_t,t,MB); mpz_fdiv_q(rem_t,rem_t,kB); strcpy(tj,"MB/s"); bigt=1;}
      else if (mpz_cmp(t,kBborder)>0)
        { mpz_fdiv_qr(t,rem_t,t,kB); strcpy(tj,"kB/s"); bigt=1;}
      else
        strcpy(tj,"B/s");
      // print
      if (mpz_cmp_ui(timer,0)>0 && printrt){
        // date
        if (f_date){
          get_time();
          printf("%s ",date_t);
        }
        // interface
        printf("%s",interface);
        // counter/timers
        if (f_timer){
          ui_sec = mpz_fdiv_q_ui(days,timer,DAY_IN_SEC);
          ui_hours = ui_sec/(DAY_IN_SEC/24);
          ui_sec = ui_sec-(ui_hours*(DAY_IN_SEC/24));
          ui_minutes = ui_sec/60;
          ui_sec = ui_sec-(ui_minutes*60);
          printf("[");
          if (mpz_cmp_ui(days,0)>0)
            //gmp_printf("%Zd%s",days,"d");
            gmp_printf("%Zd%s%02u%s%02u%s%02u]",days,"d",ui_hours,"h",ui_minutes,"m",ui_sec);
          else if (ui_hours)
            //printf("%u%s",ui_hours,"h");
            printf("%u%s%02u%s%02u]",ui_hours,"h",ui_minutes,"m",ui_sec);
          else if (ui_minutes)
            //printf("%u%s",ui_minutes,"m");
            printf("%u%s%02u]",ui_minutes,"m",ui_sec);
          else
            printf("%02u]",ui_sec);
          //gmp_printf("[%Zd%s%u%s%u%s%02u]",days,"d",ui_hours,"h",ui_minutes,"m",ui_sec);
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
      }
      lseek(filedr,(off_t)0,SEEK_SET);
      lseek(filedt,(off_t)0,SEEK_SET);
      // cur to prev
      mpz_init_set(aa,a);
      mpz_init_set(bb,b);
    }
    else printf("No data...\n");
    sleep(1);
    mpz_add_ui(timer,timer,1);
  }
  close(filedr);
  close(filedt);
  free_num();
  printf("exit\n");
  exit(0);
}

void sig_stop(int sig)
{
  close(filedr);
  close(filedt);
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
  mpz_init(kBborder);
  mpz_init(MBborder);
  mpz_init(GBborder);
  mpz_init(TBborder);
  mpz_init(PBborder);
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
  mpz_init(_r);  // cur rx B
  mpz_init(_t);  // cur tx B
  mpz_init(_sr);  // sum rx B
  mpz_init(_st);  // sum tx B
  mpz_init(rem_r);  // remainder from dividing
  mpz_init(rem_t);
  mpz_init(rem_sr);
  mpz_init(rem_st);
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
  mpz_clear(_r);
  mpz_clear(_t);
  mpz_clear(_sr);
  mpz_clear(_st);
  mpz_clear(rem_r);
  mpz_clear(rem_t);
  mpz_clear(rem_sr);
  mpz_clear(rem_st);
  mpz_clear(kB);
  mpz_clear(MB);
  mpz_clear(GB);
  mpz_clear(TB);
  mpz_clear(PB);
  mpz_clear(kBborder);
  mpz_clear(MBborder);
  mpz_clear(GBborder);
  mpz_clear(TBborder);
  mpz_clear(PBborder);
}

void print_usage(){
  printf("Usage: %s [OPTIONS] [INTERFACE]\n",PROG_NAME);
  fputs ("Options:\n\
   -m      print maximum data rate\n\
   -d      print date on each line\n\
   -t      print counter divided into days/hours/minutes\n\
   -g val  print only Bps values greater than val\n\
   -h      print this message\n\
\n", stdout);
  printf("%s is a Linux network interface rx/tx status.\n",PROG_NAME);
}

