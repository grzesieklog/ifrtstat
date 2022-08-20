// gcc ifrtstat.c -o ifrtstat -lgmp
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <gmp.h>




//int hours, minutes, seconds, day, month, year;
time_t now;
char date_t[128];
int filedr=0;
int filedt=0;
char *interface;
char rxpath[128]="/sys/class/net/";
char txpath[128]="/sys/class/net/";

mpz_t kB,MB,GB,TB,PB;
mpz_t kBborder,MBborder,GBborder,TBborder,PBborder;
mpz_t sa,sb,a,b,aa,bb;
mpz_t maxrx,maxtx;
mpz_t timer;
mpz_t r,t,sr,st;
mpz_t _r,_t,_sr,_st;
mpz_t rem_r,rem_t,rem_sr,rem_st;



void gettime(){
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
  mpz_init(sa);  // save first a (tx) B
  mpz_init(sb);  // save first b (tx) B
  mpz_init(a);  // read rx B
  mpz_init(b);  // read tx B
  mpz_init(aa);  // prev rx B
  mpz_init(bb);  // prev tx B
  mpz_init(maxrx);
  mpz_init(maxtx);
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
  mpz_clear(sa);
  mpz_clear(sb);
  mpz_clear(a);
  mpz_clear(b);
  mpz_clear(aa);
  mpz_clear(bb);
  mpz_clear(maxrx);
  mpz_clear(maxtx);
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
void sig_stop(int sig)
{
  close(filedr);
  close(filedt);
  free_num();
  exit(0);
}

int main(int argc, char* argv[]) {
  if (argc == 2) {
    interface = argv[1];
    strcat(rxpath,interface);
    strcat(txpath,interface);
    strcat(rxpath,"/statistics/rx_bytes");
    strcat(txpath,"/statistics/tx_bytes");
    if (access(rxpath,F_OK) != 0)
      return 1;
  }
  else {
    printf("bandstat [interface]\n");
    return 1;
  }
  signal(SIGINT, sig_stop);
  signal(SIGTSTP, sig_stop);
  filedr = open(rxpath, O_RDONLY );
  filedt = open(txpath, O_RDONLY );

  if( !filedr || !filedt ) {
    printf("Open file error\n");
    exit(-1);
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
  mpz_set_ui(maxrx,0);
  mpz_set_ui(maxtx,0);
  char rxbuf[128];
  char txbuf[128];
  int nbytesr=0, nbytest=0;
  char newrxmax=0,newtxmax=0;
  char bigr=0,bigt=0,bigsr=0,bigst=0;
  char rj[6],tj[6],srj[4],stj[4];
  gettime();
  printf("ifrtstat %s start at %s\n",interface,date_t);
  while(1) {
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
      if (mpz_cmp(r,maxrx)>0){ mpz_init_set(maxrx,r); newrxmax=1;}
      if (mpz_cmp(t,maxtx)>0){ mpz_init_set(maxtx,t); newtxmax=1;}
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
      if (mpz_cmp_ui(timer,0)>0){
        if (bigsr){
          gmp_printf("%s[%Zd] Sum rx %Zd.%03Zd %s ",interface,timer,sr,rem_sr,srj);
        }
        else
          gmp_printf("%s[%Zd] Sum rx %Zd %s ",interface,timer,sr,srj);
        if (bigst){
          gmp_printf("tx %Zd.%03Zd %s ",st,rem_st,stj);
        }
        else
          gmp_printf("tx %Zd %s ",st,stj);
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
        if (newrxmax){
          gmp_printf(" Max rx %Zd B/s",maxrx);
          newrxmax=0;
        }
        if (newtxmax){
          gmp_printf(" Max tx %Zd B/s",maxtx);
          newtxmax=0;
        }
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
  return 0;
}


