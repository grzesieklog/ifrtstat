# ifrtstat
This is Linux network interface rx/tx status with counter overflow protection. The main purpose creating this tool is server usage. You may redirect it's output to file and see leater how bandwidth use your server all the time.

Output of ifrtstat may contain date (-d, -t) and max data rate (-m). You may also logs only data rate above some value (-g).

Program uses GMP library for big numbers and Netlink Protocol for retrives network device statistic.

Read more at: https://grzesieklog.blogspot.com/2023/07/i-wrote-my-own-network-bandwidth.html
