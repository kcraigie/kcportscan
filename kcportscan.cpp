
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdarg.h>

typedef unsigned char uint8_t;
typedef unsigned int uint_t;

struct SocketInfo_t {
  int socket;
  uint_t address;
  int port;
  int isValid;
  unsigned long long startTicks;
};

static unsigned long long g_timeout = 3000ULL;
static int g_maxSockets = 1024;

static void printUsage(char * argv0)
{
  printf("Usage: %s -4 [ipv4_address] -t <timeout_ms>\n", argv0);
}

static unsigned long long getticks()
{
  struct timeval tv;
  if(gettimeofday(&tv, NULL)==-1) {
    return 0;
  }
  return (unsigned long long)tv.tv_sec + (unsigned long long)tv.tv_usec / 1000ULL;
}

static int g_needNewline = 0;

// Normal log
static void mylog(const char * fmt, ...)
{
  va_list args;
  if(g_needNewline) {
    printf("\n");
    g_needNewline = 0;
  }
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

// Avoid newlines until next normal log
static void mylogn(const char * fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  fflush(stdout);
  g_needNewline = 1;
}

static void portScanAddress(uint_t address)
{
  SocketInfo_t * sinfos = (SocketInfo_t*)malloc(sizeof(SocketInfo_t) * g_maxSockets);
  pollfd * pollfds = (pollfd*)malloc(sizeof(pollfd) * g_maxSockets);
  int * pollfdsToSocketIndex = (int*)malloc(sizeof(int) * g_maxSockets);

  int numSockets = 0;
  int numpollfds = 0;
  int nextPort = 1;

  mylogn("Portscanning: %d.%d.%d.%d:1",
         address >> 24 & 0xff, address >> 16 & 0xff, address >> 8 & 0xff, address & 0xff);

  // Main loop
  while(1) {
    int j, ret;

    numpollfds = 0;

    // Fill up and compact socket array while refilling pollfd array
    for(j=0;j<g_maxSockets && nextPort<=65535;j++) {
      if(!sinfos[j].isValid) {
        ret = -1;

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if(s!=-1) {

          // Make socket asynchronous
          int flags = fcntl(s, F_GETFL, 0);
          ret = fcntl(s, F_SETFL, flags | O_NONBLOCK);
          if(ret!=-1) {

            // Set up address and port
            sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = htonl(address);
            sa.sin_port = htons(nextPort);

            ret = connect(s, (sockaddr*)&sa, sizeof(sa));
            if(ret==-1) {
              if(errno == EINPROGRESS) {
                ret = 0;
              } else {
                mylog("Unknown error connecting to %d.%d.%d.%d:%d: %d\n",
                      address >> 24 & 0xff, address >> 16 & 0xff, address >> 8 & 0xff, address & 0xff,
                      nextPort, errno);
              }
            } else if(ret==0) {
              mylog("%d.%d.%d.%d:%d accepted IPv4 TCP connection immediately\n",
                    address >> 24 & 0xff, address >> 16 & 0xff, address >> 8 & 0xff, address & 0xff,
                    ntohs(sa.sin_port));
              // Just clean it up like an error
              ret = -1;
            }

          } else {
            mylog("Failed to make socket non-blocking: %d\n", errno);
          }

          // Save socket or clean up
          if(ret!=-1) {
            sinfos[j].socket = s;
            sinfos[j].address = address;
            sinfos[j].port = nextPort;
            sinfos[j].isValid = 1;
            sinfos[j].startTicks = getticks();
            numSockets++;
          } else {
            close(s);
            s = -1;
          }

          nextPort++;
        } else if(errno == EMFILE) {
          // Out of file descriptors
          break;
        } else {
          mylog("Unknown error creating socket: %d\n", errno);
        }
      } // if(!sinfos[j].isValid) {

      if(sinfos[j].isValid) {
        pollfds[numpollfds].fd = sinfos[j].socket;
        pollfds[numpollfds].events = POLLOUT;
        pollfds[numpollfds].revents = 0;
        pollfdsToSocketIndex[numpollfds] = j;
        numpollfds++;
      }

    } // for(;j<g_maxSockets;j++) {

    if(numpollfds==0) {
      break;
    }

    ret = poll(pollfds, numpollfds, g_timeout);

    if(ret == -1) {
      mylog("Unknown error polling %d fds: %d\n", numpollfds, errno);
    } else {
      unsigned long long nowTicks = getticks();
      for(j=0;j<numpollfds;j++) {
        SocketInfo_t * sinfo = &sinfos[pollfdsToSocketIndex[j]];
        int shouldCloseSocket = 0;
        if(pollfds[j].revents & POLLOUT) {
          int err;
          socklen_t errlen = sizeof(err);
          if(getsockopt(sinfo->socket, SOL_SOCKET, SO_ERROR, &err, &errlen)!=-1) {
            if(err==0) {
              mylog("%d.%d.%d.%d:%d accepted IPv4 TCP connection\n",
                    address >> 24 & 0xff, address >> 16 & 0xff, address >> 8 & 0xff, address & 0xff,
                    sinfo->port);
            // } else {
            //   printf("%d.%d.%d.%d:%d rejected IPv4 TCP connection: %d\n",
            //          address >> 24 & 0xff, address >> 16 & 0xff, address >> 8 & 0xff, address & 0xff,
            //          sinfo->port, err);
            }
          } else {
            mylog("Unknown error getting connect status: %d\n", errno);
          }
          shouldCloseSocket = 1;
        } else if(pollfds[j].revents & POLLERR) {
          // mylog("Port %d error on IPv4 TCP connection\n", sinfo->port);
          shouldCloseSocket = 1;
        } else if(pollfds[j].revents) {
          mylog("Unknown poll events received 0x%x\n", pollfds[j].revents);
          shouldCloseSocket = 1;
        } else if(sinfo->startTicks - nowTicks > g_timeout) {
          // mylog("Port %d timed out on IPv4 TCP connection\n", sinfo->port);
          shouldCloseSocket = 1;
        }
        if(shouldCloseSocket) {
          close(sinfo->socket);
          sinfo->isValid = 0;
        }
      }
    }

    mylogn("...%d", nextPort);

  } // while(1) {

  mylog("");

  free(sinfos);
  free(pollfds);
  free(pollfdsToSocketIndex);
}

static uint_t bitmaskifyPrefix(uint_t iPrefix)
{
  uint_t ret = 0;
  for(uint_t i=0;i<iPrefix;i++) {
    ret |= (uint_t)1 << (32U-i-1);
  }
  return ret;
}

int main(int argc, char * argv[])
{
  uint_t address = 0;
  uint_t prefix = 0;

  // Parse command-line arguments
  for(int i=1;i<argc;i++) {
    switch(argv[i][0]) {
    case '-':
      switch(argv[i][1]) {
      case '4':
        {
          if(i+1>argc) {
            printUsage(argv[0]);
            return 1;
          }
          // Parse address block
          int i0, i1, i2, i3, i4 = 32;
          if(sscanf(argv[i+1], "%d.%d.%d.%d/%d", &i0, &i1, &i2, &i3, &i4)==5 ||
             sscanf(argv[i+1], "%d.%d.%d.%d", &i0, &i1, &i2, &i3)==4) {
            address = (uint_t)i0 << 24 & 0xff000000;
            address |= (uint_t)i1 << 16 & 0x00ff0000;
            address |= (uint_t)i2 << 8 & 0x0000ff00;
            address |= (uint_t)i3 << 0 & 0x000000ff;
            prefix = i4;
          }
        }
        break;
      case 't':
        {
          if(i+1>argc) {
            printUsage(argv[0]);
            return 1;
          }
          int i0;
          // Parse user-specified timeout
          if(sscanf(argv[i+1], "%d", &i0)==1) {
            g_timeout = (unsigned long long)i0;
          }
        }
        break;
      }
    }
  }

  // Validate address block
  if(address==0 || (prefix < 1 || prefix > 32)) {
    printUsage(argv[0]);
    return 1;
  }

  // Determine how many sockets we can create
  struct rlimit rlim;
  if(getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
    g_maxSockets = rlim.rlim_cur;
  }
  mylog("Maximum number of file descriptors: %d\n", g_maxSockets);

  // Portscan each address in the block
  uint_t prefixBitmask = bitmaskifyPrefix(prefix);
  mylog("Address block to portscan: %d.%d.%d.%d/%d\n",
        (address & prefixBitmask) >> 24 & 0xff, (address & prefixBitmask) >> 16 & 0xff,
        (address & prefixBitmask) >> 8 & 0xff, (address & prefixBitmask) & 0xff, prefix);
  for(uint_t address2 = address & prefixBitmask;               // Start with first address in range
      (address & prefixBitmask) == (address2 & prefixBitmask); // Continue while address in range
      address2++) {                                            // Increment to next address
    portScanAddress(address2);
  }

  return 0;
}
