#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/inet_diag.h>

int main(int argc, char** argv)
{
  int result = 0;
  
  // Try to create a netlink socket with the parameters blocked by
  // the rootkit and report if the creation was successful
  result = socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG);
  
  if (result == -1)
  {
    printf("detected\n");
    return -1;
  }
  else
  {
    printf("not_detected\n");
    return 0;
  }
  
  return 0;
}