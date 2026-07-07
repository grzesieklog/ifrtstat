#include "ifrtstat.c"

int main(int argc, char* argv[])
{
  char greater[MAX_GOPT_LEN + 1] = {0};
  const char options[] = "hmdtg:Bc";
  uint8_t int_sum=0;
  // get options
  uint64_t program_options = 0;
  parse_options(argc,argv,options,&program_options,greater);
  // prepare connection to netlink
  struct rtnl_link *nlink = 0;
  struct nl_sock *nlsocket = 0;
  nlsocket = nl_socket_alloc();
  if (!nlsocket)
    exit(30);
  if (nl_connect(nlsocket,NETLINK_ROUTE)!=0){
    nl_socket_free(nlsocket);
    exit(31);
  }
  // search if name
  char *interface = 0;
  for (int n=1; n<argc; ++n) {
    if (is_flag_set(program_options,FLAG_GREATER))
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
      set_flag(&program_options,FLAG_INTERFACE);
      int_sum++;
    }
    else{
      printf("unknown arg: %s\n",argv[n]);
      int_sum++;
    }
  }
  // check args count
  if (get_count_flags(program_options) > strlen(options)-1 || // has ':'
      int_sum > 1){
    printf("Too many args...\n");
    nl_socket_free(nlsocket);
    exit(9);
  }
  // check options combination
  if (get_count_flags(program_options) > 1 && is_flag_set(program_options,FLAG_HELP)){
    printf("Option -h cannot be used with other options.\n");
    nl_socket_free(nlsocket);
    exit(8);
  }
  // interface not found
  if (!is_flag_set(program_options,FLAG_INTERFACE) && !is_flag_set(program_options,FLAG_HELP) && argc>1){
    printf("Interface not found!\n");
    nl_socket_free(nlsocket);
    exit(12);
  }
  if (is_flag_set(program_options,FLAG_HELP) || argc==1){
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
  
  uint64_t greater_val = 0;
  if (is_flag_set(program_options,FLAG_GREATER)){
    greater_val = (uint64_t)strtoull(greater, NULL, 10);
  }
  uint8_t newrxmax=0,newtxmax=0;
  uint8_t bigr=0,bigt=0,bigsr=0,bigst=0;
  char rt_rate_unit[MAXLEN_RATE_UNIT];
  char rt_sum_unit[MAXLEN_SUM_UNIT];
  memset(rt_rate_unit,0,sizeof(rt_rate_unit));
  memset(rt_sum_unit,0,sizeof(rt_sum_unit));
  // option -B
  if (is_flag_set(program_options,FLAG_BYTES_PS) && !is_flag_set(program_options,FLAG_CONV)){
    strcpy(rt_sum_unit,BYTE_SUM_SUFFIX);
    strcpy(rt_rate_unit,BYTE_RATE_SUFFIX);
  }
  // default print in bps
  if (!is_flag_set(program_options,FLAG_BYTES_PS) && !is_flag_set(program_options,FLAG_CONV))
  {
    strcpy(rt_sum_unit,BIT_SUM_SUFFIX);
    strcpy(rt_rate_unit,BIT_RATE_SUFFIX);
  }
  uint8_t printrt = 1;
  
  uint64_t rx_start = 0;
  uint64_t tx_start = 0;
  uint64_t rx_prev = 0;
  uint64_t tx_prev = 0;
  uint64_t run_timer = 0;
  uint64_t rx_current_rate_max = 0;
  uint64_t tx_current_rate_max = 0;
  char date_t[32]; // need define MAX!
  while(running) {
    nlink = 0;
    printrt = 1;
    if(rtnl_link_get_kernel(nlsocket,0,interface,&nlink)==0){
      uint64_t rx_current = 0;
      uint64_t tx_current = 0;
      rx_current = rtnl_link_get_stat(nlink,RTNL_LINK_RX_BYTES);
      tx_current = rtnl_link_get_stat(nlink,RTNL_LINK_TX_BYTES);
      rtnl_link_put(nlink);
      // first init
      if (run_timer == 0){
        rx_start = rx_current;
        tx_start = tx_current;
        rx_prev = rx_current;
        tx_prev = tx_current;
        if (!is_flag_set(program_options,FLAG_DATE)){
          get_time(date_t);
          printf("%s start at %s on %s (rx %"PRIu64" %s,tx %"PRIu64" %s)\n",
                 PROG_NAME,
                 date_t,
                 interface,
                 rx_start,BYTE_SUM_SUFFIX,
                 tx_start,BYTE_SUM_SUFFIX);
        }
      }
      // diff of B
      uint64_t rx_current_rate = 0;
      uint64_t tx_current_rate = 0;
      uint64_t rx_start_sum = 0;
      uint64_t tx_start_sum = 0;
      rx_current_rate = rx_current - rx_prev;
      tx_current_rate = tx_current - tx_prev;
      rx_start_sum = rx_current - rx_start;
      tx_start_sum = tx_current - tx_start;
      // MAX
      if (is_flag_set(program_options,FLAG_MAX)){
        if (rx_current_rate > rx_current_rate_max)
        {
          rx_current_rate_max = rx_current_rate;
          newrxmax=1;
        }
        if (tx_current_rate > tx_current_rate_max)
        {
          tx_current_rate_max = tx_current_rate;
          newtxmax=1;
        }
      }
      // over then
      if (is_flag_set(program_options,FLAG_GREATER)){
        if (rx_current_rate <= greater_val && tx_current_rate <= greater_val)
          printrt=0;
      }
      if (printrt){
        bigr=bigt=bigsr=bigst=0;
      }
      // cal units
      uint64_t rx_start_sum_remainder = 0;
      uint64_t tx_start_sum_remainder = 0;
      uint64_t rx_current_rate_remainder = 0;
      uint64_t tx_current_rate_remainder = 0;
      if (printrt && is_flag_set(program_options,FLAG_CONV)){
        // bit or Bytes
        if (!is_flag_set(program_options,FLAG_BYTES_PS))
        {
          rx_current_rate = rx_current_rate * BITS;
          tx_current_rate = tx_current_rate * BITS;
          rx_start_sum = rx_start_sum * BITS;
          tx_start_sum = tx_start_sum * BITS;
        }
        // sum
        conv(&rx_start_sum, &rx_start_sum_remainder, &bigsr, rt_sum_unit, 1, program_options);
        conv(&tx_start_sum, &tx_start_sum_remainder, &bigst, rt_sum_unit, 1, program_options);
        // rate
        conv(&rx_current_rate, &rx_current_rate_remainder, &bigr, rt_rate_unit, 0, program_options);
        conv(&tx_current_rate, &tx_current_rate_remainder, &bigt, rt_rate_unit, 0, program_options);
      }
      // default print in bps
      if (printrt && !is_flag_set(program_options,FLAG_BYTES_PS) && !is_flag_set(program_options,FLAG_CONV))
      {
          rx_current_rate = rx_current_rate * BITS;
          tx_current_rate = tx_current_rate * BITS;
          rx_start_sum = rx_start_sum * BITS;
          tx_start_sum = tx_start_sum * BITS;
      }
      // print
      if (run_timer > 0 && printrt)
      {
        print_data(program_options,
                   interface,
                   date_t,
                   run_timer,
                   bigr,
                   bigt,
                   bigsr,
                   bigst,
                   &newrxmax,
                   &newtxmax,
                   rx_start_sum,
                   tx_start_sum,
                   rx_current_rate,
                   tx_current_rate,
                   rx_start_sum_remainder,
                   tx_start_sum_remainder,
                   rx_current_rate_remainder,
                   tx_current_rate_remainder,
                   rx_current_rate_max,
                   tx_current_rate_max,
                   rt_rate_unit,
                   rt_sum_unit);
      }
      // cur to prev
      rx_prev = rx_current;
      tx_prev = tx_current;
    }
    else printf("No data...\n");
    sleep(1);
    run_timer = run_timer + 1;
  }
  nl_socket_free(nlsocket);
  printf("Exit\n");
  exit(0);
}



