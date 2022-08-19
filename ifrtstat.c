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
char rxpath[100]="/sys/class/net/";
char txpath[100]="/sys/class/net/";

mpz_t kB,MB,GB,TB;
mpz_t kBborder,MBborder,GBborder,TBborder;
mpz_t sa,sb,a,b,aa,bb;
mpz_t maxrx,maxtx;
mpz_t timer;
mpz_t r,t,sr,st;
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
  mpz_init(kBborder);
  mpz_init(MBborder);
  mpz_init(GBborder);
  mpz_init(TBborder);
  mpz_init(timer);
  mpz_init(sa);
  mpz_init(sb);
  mpz_init(a);
  mpz_init(b);
  mpz_init(aa);
  mpz_init(bb);
  mpz_init(maxrx);
  mpz_init(maxtx);
  mpz_init(r);
  mpz_init(t);
  mpz_init(sr);
  mpz_init(st);
  mpz_init(rem_r);
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
  mpz_clear(rem_r);
  mpz_clear(rem_t);
  mpz_clear(rem_sr);
  mpz_clear(rem_st);
  mpz_clear(kB);
  mpz_clear(MB);
  mpz_clear(GB);
  mpz_clear(TB);
  mpz_clear(kBborder);
  mpz_clear(MBborder);
  mpz_clear(GBborder);
  mpz_clear(TBborder);
}
void sig_stop(int sig)
{
  close(filedr);
  close(filedt);
  free_num();
  //printf("SIG \n");
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
  mpz_init_set_str(kBborder,"2000",10);
  mpz_init_set_str(MBborder,"2000000",10);
  mpz_init_set_str(GBborder,"2000000000",10);
  mpz_init_set_str(TBborder,"2000000000000",10);
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
  //unsigned long long int timer=0;
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
      mpz_set_ui(r,0);
      mpz_set_ui(t,0);
      mpz_set_ui(sr,0);
      mpz_set_ui(st,0);
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
      // MAX
      if (mpz_cmp(r,maxrx)>0){ mpz_init_set(maxrx,r); newrxmax=1;}
      if (mpz_cmp(t,maxtx)>0){ mpz_init_set(maxtx,t); newtxmax=1;}
      // cal units
      if (mpz_cmp(r,TBborder)>0)
        { mpz_fdiv_qr(r,rem_r,r,TB); strcpy(rj,"TB/s"); bigr=1;}
      else if (mpz_cmp(r,GBborder)>0)
        { mpz_fdiv_qr(r,rem_r,r,GB); strcpy(rj,"GB/s"); bigr=1;}
      else if (mpz_cmp(r,MBborder)>0)
        { mpz_fdiv_q(r,r,MB); strcpy(rj,"MB/s");}
      else if (mpz_cmp(r,kBborder)>0)
        { mpz_fdiv_q(r,r,kB); strcpy(rj,"kB/s");}
      else
        strcpy(rj,"B/s");
      if (mpz_cmp(t,TBborder)>0)
        { mpz_fdiv_qr(t,rem_t,t,TB); strcpy(tj,"TB/s"); bigt=1;}
      else if (mpz_cmp(t,GBborder)>0)
        { mpz_fdiv_qr(t,rem_t,t,GB); strcpy(tj,"GB/s"); bigt=1;}
      else if (mpz_cmp(t,MBborder)>0)
        { mpz_fdiv_q(t,t,MB); strcpy(tj,"MB/s");}
      else if (mpz_cmp(t,kBborder)>0)
        { mpz_fdiv_q(t,t,kB); strcpy(tj,"kB/s");}
      else
        strcpy(tj,"B/s");
      if (mpz_cmp(sr,TBborder)>0)
        { mpz_fdiv_qr(sr,rem_sr,sr,TB); strcpy(srj,"TB"); bigsr=1;}
      else if (mpz_cmp(sr,GBborder)>0)
        { mpz_fdiv_qr(sr,rem_sr,sr,GB); strcpy(srj,"GB"); bigsr=1;}
      else if (mpz_cmp(sr,MBborder)>0)
        { mpz_fdiv_q(sr,sr,MB); strcpy(srj,"MB");}
      else if (mpz_cmp(sr,kBborder)>0)
        { mpz_fdiv_q(sr,sr,kB); strcpy(srj,"kB");}
      else
        strcpy(srj,"B");
      if (mpz_cmp(st,TBborder)>0)
        { mpz_fdiv_qr(st,rem_st,st,TB); strcpy(stj,"TB"); bigst=1;}
      else if (mpz_cmp(st,GBborder)>0)
        { mpz_fdiv_qr(st,rem_st,st,GB); strcpy(stj,"GB"); bigst=1;}
      else if (mpz_cmp(st,MBborder)>0)
        { mpz_fdiv_q(st,st,MB); strcpy(stj,"MB");}
      else if (mpz_cmp(st,kBborder)>0)
        { mpz_fdiv_q(st,st,kB); strcpy(stj,"kB");}
      else
        strcpy(stj,"B");
      // print
      if (mpz_cmp_ui(timer,0)>0){
        //gmp_printf("%s[%llu]%i/%i Sum rx %Zd %s tx %Zd %s Cur rx %Zd %s tx %Zd %s\n", 
        //      interface,timer,nbytesr,nbytest,sr,srj,st,stj,r,rj,t,tj);
        if (bigsr){
          char tmp[mpz_sizeinbase(rem_sr,10)+2]; 
          mpz_get_str(tmp,10,rem_sr);
          tmp[2]=0;
          gmp_printf("%s[%Zd] Sum rx %Zd.%s %s ",interface,timer,sr,tmp,srj);
        }
        else
          gmp_printf("%s[%Zd] Sum rx %Zd %s ",interface,timer,sr,srj);
        if (bigst){
          char tmp[mpz_sizeinbase(rem_st,10)+2]; 
          mpz_get_str(tmp,10,rem_st);
          tmp[2]=0;
          gmp_printf("tx %Zd.%s %s ",st,tmp,stj);
        }
        else
          gmp_printf("tx %Zd %s ",st,stj);
        if (bigr){
          char tmp[mpz_sizeinbase(rem_r,10)+2]; 
          mpz_get_str(tmp,10,rem_r);
          tmp[2]=0;
          gmp_printf("Cur rx %Zd.%s %s ",r,tmp,rj);
        }
        else
          gmp_printf("Cur rx %Zd %s ",r,rj);
        if (bigt){
          char tmp[mpz_sizeinbase(rem_t,10)+2]; 
          mpz_get_str(tmp,10,rem_t);
          tmp[2]=0;
          gmp_printf("tx %Zd.%s %s",t,tmp,tj);
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


