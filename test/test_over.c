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

mpz_t rx_sum;
mpz_t rx_start;
mpz_t rx_cur;
mpz_t rx_prev;
mpz_t max_u64;
mpz_t u64;

// sum = start - cur

int main(int argc, char **argv)
{
  mpz_init(rx_sum);
  mpz_init(rx_start);
  mpz_init(rx_cur);
  mpz_init(rx_prev);
  mpz_init(max_u64);
  mpz_init(u64);
  //uint8_t rx=200,rx_prev=0;
  unsigned long long int rx_=18446744073709551500,rx_prev_=0;
  rx_prev_ = rx_;
  mpz_set_str(max_u64,"18446744073709551616",10);
  //mpz_set_str(u64,"18446744073709551300",10);
  mpz_set_ui(rx_cur,rx_);
  mpz_set(rx_prev,rx_cur);
  mpz_set(rx_start,rx_cur);
  mpz_sub(rx_sum,rx_cur,rx_start);
  mpz_sub(u64,rx_cur,rx_prev);
  gmp_printf("counter: %"PRIu64" sum: %Zd start: %Zd cur: %Zd %Zd \n",rx_,rx_sum,rx_start,rx_cur,u64);
  mpz_set_ui(rx_prev,rx_prev_);
  for (int i=0;i<60;i++){
    rx_+=10;
    //mpz_add_ui(rx_cur,rx_cur,10);
    if (i==20) rx_+=18446744073709551300;
    //if (i==20) mpz_add(rx_cur,rx_cur,u64);
    if (rx_<rx_prev_) { 
    //if (mpz_cmp(rx_cur,rx_prev)<0) { 
      printf("over\n");
      mpz_sub(rx_start,rx_start,max_u64);
      mpz_sub(rx_prev,rx_prev,max_u64);
    }
    mpz_set_ui(rx_cur,rx_);
    mpz_sub(rx_sum,rx_cur,rx_start);
    mpz_sub(u64,rx_cur,rx_prev);
    //mpz_sub(rx_cur,rx_cur,rx_prev);
    gmp_printf("counter: %"PRIu64" sum: %Zd start: %Zd cur: %Zd %Zd \n",rx_,rx_sum,rx_start,rx_cur,u64);
    rx_prev_ = rx_;
    mpz_set_ui(rx_prev,rx_prev_);
    //mpz_set(rx_prev,rx_cur);
  }
  mpz_clear(rx_sum); 
  mpz_clear(rx_start); 
  mpz_clear(rx_cur); 
  mpz_clear(rx_prev); 
  mpz_clear(max_u64); 
  mpz_clear(u64); 
  return 0;
}
