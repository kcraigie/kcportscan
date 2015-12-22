
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/resource.h>

typedef unsigned char uint8_t;

static int g_maxSockets = 1024;
static int * g_sockets = NULL;
static pollfd * g_pollfds = NULL;
static struct MyAddress_t {
  uint8_t octets[4];
  uint8_t mask;
} g_address = { {0,0,0,0}, 32 };

static void printUsage(char * argv0)
{
  fprintf(stderr, "Usage: %s -a [address]\n", argv0);
}

int main(int argc, char * argv[])
{
  int addressSpecified = 0;

  for(int i=1;i<argc;i++) {
    switch(argv[i][0]) {
    case '-':
      switch(argv[i][1]) {
      case 'a':
        {
          if(i+1>argc) {
            printUsage(argv[0]);
            return 1;
          }
          int i0, i1, i2, i3, iMask;
          if(sscanf(argv[i+1], "%d.%d.%d.%d/%d", &i0, &i1, &i2, &i3, &iMask)==5) {
            // User specified address including mask
            addressSpecified = 1;
            g_address.mask = iMask;
          } else if(sscanf(argv[i+1], "%d.%d.%d.%d", &i0, &i1, &i2, &i3)==4) {
            // User specified address without mask
            addressSpecified = 1;
          }
          if(addressSpecified) {
            g_address.octets[0] = i0;
            g_address.octets[1] = i1;
            g_address.octets[2] = i2;
            g_address.octets[3] = i3;
          }
        }
        break;
      }
    }
  }
  if(!addressSpecified) {
    printUsage(argv[0]);
    return 1;
  }
  fprintf(stderr, "Address block to portscan: %d.%d.%d.%d/%d\n",
          g_address.octets[0], g_address.octets[1], g_address.octets[2], g_address.octets[3], g_address.mask);

  struct rlimit rlim;
  if(getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
    g_maxSockets = rlim.rlim_cur;
  }
  fprintf(stderr, "Maximum number of sockets is: %d\n", g_maxSockets);
  g_sockets = (int*)malloc(sizeof(int) * g_maxSockets);
  g_pollfds = (pollfd*)malloc(sizeof(pollfd) * g_maxSockets);
}
