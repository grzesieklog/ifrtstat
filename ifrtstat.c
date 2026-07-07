#include "ifrtstat.h"


void print_usage(){
  printf("Usage: %s [OPTIONS] [INTERFACE]\n",PROG_NAME);
  fputs ("Options:\n\
   -m      print maximum data rate\n\
   -d      print date on each line\n\
   -t      print counter divided into days/hours/minutes\n\
   -g val  print only values greater than val\n\
   -B      print rates as Bps\n\
           (default print as bps)\n\
   -c      converting to larger units (k,M,G,T,P,E)\n\
   -h      print this message\n\
\n", stdout);
  printf("%s is a Linux network interface rx/tx status.\n",PROG_NAME);
}

volatile sig_atomic_t running = 1;
void sig_stop(int sig)
{
  running = 0;
}

static inline void set_flag(uint64_t *opts, uint64_t flag)
{
  *opts |= flag;
}

static inline bool is_flag_set(uint64_t opts, uint64_t flag)
{
  return (opts & flag) != 0;
}

static inline int get_count_flags(uint64_t opts)
{
  return __builtin_popcountll(opts);
}

uint64_t parse_options(int argc, char *argv[], const char *options, uint64_t *opts, char *greater)
{
  int opt;
  optind = 1;
  while ((opt=getopt(argc,argv,options)) != -1)
  {
    switch (opt) {
      case 'h':
        if (is_flag_set(*opts,FLAG_HELP)){
          printf("Option -h is duplicated.\n");
          exit(7);
        }
        set_flag(opts,FLAG_HELP);
        break;
      case 'm':
        if (is_flag_set(*opts,FLAG_MAX)){
          printf("Option -m is duplicated.\n");
          exit(7);
        }
        set_flag(opts,FLAG_MAX);
        break;
      case 'd':
        if (is_flag_set(*opts,FLAG_DATE)){
          printf("Option -d is duplicated.\n");
          exit(7);
        }
        set_flag(opts,FLAG_DATE);
        break;
      case 't':
        if (is_flag_set(*opts,FLAG_TIMER)){
          printf("Option -t is duplicated.\n");
          exit(7);
        }
        set_flag(opts,FLAG_TIMER);
        break;
      case 'g':
        if (is_flag_set(*opts,FLAG_GREATER)){
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
        strncpy(greater, optarg, MAX_GOPT_LEN);
        greater[MAX_GOPT_LEN] = '\0';
        set_flag(opts,FLAG_GREATER);
        break;
      case 'B':
        if (is_flag_set(*opts,FLAG_BYTES_PS)){
          printf("Option -B is duplicated.\n");
          exit(7);
        }
        set_flag(opts,FLAG_BYTES_PS);
        break;
      case 'c':
        if (is_flag_set(*opts,FLAG_CONV)){
          printf("Option -c is duplicated.\n");
          exit(7);
        }
        set_flag(opts,FLAG_CONV);
        break;
      case '?':
        printf("Use option -h to print help.\n");
        exit(7);
        break;
    }
  }
}

void get_time(char *print_date){
  time_t now = 0;
  time(&now);
  strcpy(print_date,ctime(&now));
  print_date[strcspn(print_date,"\n")] = 0;
}

void conv(uint64_t* val, uint64_t* rem, uint8_t* big_flag, char* units, uint8_t sum, uint64_t opts)
{
  char prefix = '\0';
  if (*val > KILO) {
    *big_flag=1;
    if (*val > MEGA) {
      if (*val > GIGA) {
        if (*val > TERA) {
          if (*val > PETA) {
            if (*val > EKSA) {
              *rem = (*val % EKSA) / PETA;
              *val = *val / EKSA;
              prefix = EKSA_PREFIX[0];
            }
            else {
              *rem = (*val % PETA) / TERA;
              *val = *val / PETA;
              prefix = PETA_PREFIX[0];
            }
          }
          else {
            *rem = (*val % TERA) / GIGA;
            *val = *val / TERA;
            prefix = TERA_PREFIX[0];
          }
        }
        else {
          *rem = (*val % GIGA) / MEGA;
          *val = *val / GIGA;
          prefix = GIGA_PREFIX[0];
        }
      }
      else {
        *rem = (*val % MEGA) / KILO;
        *val = *val / MEGA;
        prefix = MEGA_PREFIX[0];
      }
    }
    else {
      *rem = *val % KILO;
      *val = *val / KILO;
      prefix = KILO_PREFIX[0];
    }
  }
  
  const char* suffix;
  if (!is_flag_set(opts, FLAG_BYTES_PS)) {
    suffix = sum ? BIT_SUM_SUFFIX : BIT_RATE_SUFFIX;
  } else {
    suffix = sum ? BYTE_SUM_SUFFIX : BYTE_RATE_SUFFIX;
  }
  
  if (prefix != '\0') {
    if (sum)
      snprintf(units, MAXLEN_SUM_UNIT, "%c%s", prefix, suffix);
    else
      snprintf(units, MAXLEN_RATE_UNIT, "%c%s", prefix, suffix);
  } else {
    if (sum)
      snprintf(units, MAXLEN_SUM_UNIT, "%s", suffix);
    else
      snprintf(units, MAXLEN_RATE_UNIT, "%s", suffix);
  }
}

void print_data(uint64_t program_options,
                char *interface, 
                char *date_t, 
                uint64_t run_timer, 
                uint8_t bigr,
                uint8_t bigt,
                uint8_t bigsr,
                uint8_t bigst,
                uint8_t *newrxmax,
                uint8_t *newtxmax,
                uint64_t rx_start_sum,
                uint64_t tx_start_sum,
                uint64_t rx_current_rate,
                uint64_t tx_current_rate,
                uint64_t rx_start_sum_remainder,
                uint64_t tx_start_sum_remainder,
                uint64_t rx_current_rate_remainder,
                uint64_t tx_current_rate_remainder,
                uint64_t rx_current_rate_max,
                uint64_t tx_current_rate_max,
                char *rt_rate_unit,
                char *rt_sum_unit)
{
  char buffer[256];
  int offset = 0;
  int max_size = sizeof(buffer);
  // date
  if (is_flag_set(program_options, FLAG_DATE)){
    get_time(date_t);
    offset += snprintf(buffer + offset, max_size - offset,"%s ",date_t);
  }
  // interface
  offset += snprintf(buffer + offset, max_size - offset,"%s",interface);
  // counter to timers
  if (is_flag_set(program_options,FLAG_TIMER)){
    uint64_t days_count = 0;// days
    uint32_t ui_hours=0,ui_minutes=0,ui_sec=0;
    days_count = run_timer / DAY_IN_SEC;
    ui_sec = run_timer % DAY_IN_SEC;
    ui_hours = ui_sec/(DAY_IN_SEC/24);
    ui_sec = ui_sec-(ui_hours*(DAY_IN_SEC/24));
    ui_minutes = ui_sec/60;
    ui_sec = ui_sec-(ui_minutes*60);
    offset += snprintf(buffer + offset, max_size - offset,"[");
    if (days_count > 0)
      offset += snprintf(buffer + offset, max_size - offset,
                "%"PRIu64"%s%02u%s%02u%s%02u]",days_count,"d",ui_hours,"h",ui_minutes,"m",ui_sec);
    else if (ui_hours)
      offset += snprintf(buffer + offset, max_size - offset,
                "%u%s%02u%s%02u]",ui_hours,"h",ui_minutes,"m",ui_sec);
    else if (ui_minutes)
      offset += snprintf(buffer + offset, max_size - offset,
                "%u%s%02u]",ui_minutes,"m",ui_sec);
    else
      offset += snprintf(buffer + offset, max_size - offset,
                "%02u]",ui_sec);
  }
  else
    offset += snprintf(buffer + offset, max_size - offset,"[%"PRIu64"]",run_timer);
  // sum
  if (bigsr){
    offset += snprintf(buffer + offset, max_size - offset,
              " Sum rx %"PRIu64".%03"PRIu64" %s ",rx_start_sum,rx_start_sum_remainder,rt_sum_unit);
  }
  else
    offset += snprintf(buffer + offset, max_size - offset,
              " Sum rx %"PRIu64" %s ",rx_start_sum,rt_sum_unit);
  if (bigst){
    offset += snprintf(buffer + offset, max_size - offset,
              "tx %"PRIu64".%03"PRIu64" %s ",tx_start_sum,tx_start_sum_remainder,rt_sum_unit);
  }
  else
    offset += snprintf(buffer + offset, max_size - offset,
              "tx %"PRIu64" %s ",tx_start_sum,rt_sum_unit);
  // current rate
  if (bigr){
    offset += snprintf(buffer + offset, max_size - offset,
              "Cur rx %"PRIu64".%03"PRIu64" %s ",rx_current_rate,rx_current_rate_remainder,rt_rate_unit);
  }
  else
    offset += snprintf(buffer + offset, max_size - offset,
              "Cur rx %"PRIu64" %s ",rx_current_rate,rt_rate_unit);
  if (bigt){
    offset += snprintf(buffer + offset, max_size - offset,
              "tx %"PRIu64".%03"PRIu64" %s",tx_current_rate,tx_current_rate_remainder,rt_rate_unit);
  }
  else
    offset += snprintf(buffer + offset, max_size - offset,
              "tx %"PRIu64" %s",tx_current_rate,rt_rate_unit);
  // max
  if (is_flag_set(program_options,FLAG_MAX)){
    if (!is_flag_set(program_options,FLAG_BYTES_PS))
    {
      rx_current_rate_max = rx_current_rate_max * BITS;
      tx_current_rate_max = tx_current_rate_max * BITS;
    }
    if (*newrxmax){
      offset += snprintf(buffer + offset, max_size - offset,
                " Max rx %"PRIu64,rx_current_rate_max);
      *newrxmax=0;
      if (!is_flag_set(program_options,FLAG_BYTES_PS))
        offset += snprintf(buffer + offset, max_size - offset," "BIT_RATE_SUFFIX);
      else
        offset += snprintf(buffer + offset, max_size - offset," "BYTE_RATE_SUFFIX);
      }
    if (*newtxmax){
      offset += snprintf(buffer + offset, max_size - offset,
                " Max tx %"PRIu64,tx_current_rate_max);
      *newtxmax=0;
      if (!is_flag_set(program_options,FLAG_BYTES_PS))
        offset += snprintf(buffer + offset, max_size - offset," "BIT_RATE_SUFFIX);
      else
        offset += snprintf(buffer + offset, max_size - offset," "BYTE_RATE_SUFFIX);
    }
  }
  // end
  offset += snprintf(buffer + offset, max_size - offset,"\n");
  fputs(buffer, stdout);
}

