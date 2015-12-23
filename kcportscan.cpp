
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

typedef unsigned char uint8_t;

struct SocketInfo_t {
  int socket;
  // int address;
  int port;
  int isValid;
  unsigned long long startTicks;
};

static unsigned long long g_timeout = 3000ULL;

static int g_maxSockets = 1024;
static int g_numSockets = 0;
static SocketInfo_t * g_sinfos = NULL;

static pollfd * g_pollfds = NULL;
static int * g_pollfdsToSocketIndex = NULL;
static int g_numpollfds = 0;

static int g_nextPort = 1;
static unsigned int g_address = 0;

static void printUsage(char * argv0)
{
  fprintf(stderr, "Usage: %s -4 [ipv4_address] -d <timeout>\n", argv0);
}

static unsigned long long getticks()
{
  struct timeval tv;
  if(gettimeofday(&tv, NULL)==-1) {
    return 0;
  }
  return (unsigned long long)tv.tv_sec + (unsigned long long)tv.tv_usec / 1000ULL;
}

int main(int argc, char * argv[])
{
  int needNewline = 0;

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
          int i0, i1, i2, i3, iMask;
          if(sscanf(argv[i+1], "%d.%d.%d.%d/%d", &i0, &i1, &i2, &i3, &iMask)==5) {
            // User specified address including mask
            g_address = (unsigned int)i0 << 24 & 0xff000000;
            g_address |= (unsigned int)i1 << 16 & 0x00ff0000;
            g_address |= (unsigned int)i2 << 8 & 0x0000ff00;
            g_address |= (unsigned int)i3 << 0 & 0x000000ff;
            // mask = iMask;
          } else if(sscanf(argv[i+1], "%d.%d.%d.%d", &i0, &i1, &i2, &i3)==4) {
            // User specified address without mask
            g_address = (unsigned int)i0 << 24 & 0xff000000;
            g_address |= (unsigned int)i1 << 16 & 0x00ff0000;
            g_address |= (unsigned int)i2 << 8 & 0x0000ff00;
            g_address |= (unsigned int)i3 << 0 & 0x000000ff;
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
          if(sscanf(argv[i+1], "%d", &i0)==1) {
            // User specified timeout
            g_timeout = (unsigned long long)i0 * 1000ULL;
          }
        }
        break;
      }
    }
  }

  if(g_address==0) {
    printUsage(argv[0]);
    return 1;
  }

  fprintf(stderr, "Address block to portscan: %d.%d.%d.%d/%d\n",
          g_address >> 24 & 0xff, g_address >> 16 & 0xff, g_address >> 8 & 0xff, g_address & 0xff, 32);

  struct rlimit rlim;
  if(getrlimit(RLIMIT_NOFILE, &rlim) == 0) {
    g_maxSockets = rlim.rlim_cur;
  }
  fprintf(stderr, "Maximum number of file descriptors: %d\n", g_maxSockets);
  g_sinfos = (SocketInfo_t*)malloc(sizeof(SocketInfo_t) * g_maxSockets);
  g_pollfds = (pollfd*)malloc(sizeof(pollfd) * g_maxSockets);
  g_pollfdsToSocketIndex = (int*)malloc(sizeof(int) * g_maxSockets);

  // Main loop
  while(1) {
    int j, ret;

    g_numpollfds = 0;

    // Fill up and compact socket array while refilling pollfd array
    for(j=0;j<g_maxSockets && g_nextPort<=65535;j++) {
      if(!g_sinfos[j].isValid) {
        ret = -1;

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if(s!=-1) {

          // Make socket asynchronous
          ret = fcntl(s, F_SETFL, O_NONBLOCK);
          if(ret!=-1) {

            // Set up address and port
            sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = htonl(g_address);
            sa.sin_port = htons(g_nextPort);

            ret = connect(s, (sockaddr*)&sa, sizeof(sa));
            if(ret==-1) {
              if(errno == EINPROGRESS) {
                ret = 0;
              } else {
                fprintf(stderr, "Unknown error connecting to port %d: %d\n", g_nextPort, errno);
              }
            } else if(ret==0) {
              printf("%sPort %d accepted IPv4 TCP connection immediately\n", (needNewline?"\n":""), ntohs(sa.sin_port));
              needNewline = 0;
              // Just clean it up like an error
              ret = -1;
            }

          } else {
            fprintf(stderr, "Failed to make socket non-blocking: %d\n", errno);
          }

          // Save socket or clean up
          if(ret!=-1) {
            g_sinfos[j].socket = s;
            // g_sinfos[j].address = g_address;
            g_sinfos[j].port = g_nextPort;
            g_sinfos[j].isValid = 1;
            g_sinfos[j].startTicks = getticks();
            g_numSockets++;
          } else {
            close(s);
            s = -1;
          }

          g_nextPort++;
        } else if(errno == EMFILE) {
          // Out of file descriptors
          break;
        } else {
          fprintf(stderr, "Unknown error creating socket: %d\n", errno);
        }
      } // if(!g_sinfos[j].isValid) {

      if(g_sinfos[j].isValid) {
        g_pollfds[g_numpollfds].fd = g_sinfos[j].socket;
        g_pollfds[g_numpollfds].events = POLLOUT;
        g_pollfds[g_numpollfds].revents = 0;
        g_pollfdsToSocketIndex[g_numpollfds] = j;
        g_numpollfds++;
      }

    } // for(;j<g_maxSockets;j++) {

    if(g_numpollfds==0) {
      break;
    }

    ret = poll(g_pollfds, g_numpollfds, 1000);

    if(ret == -1) {
      fprintf(stderr, "Unknown error in poll: %d\n", errno);
    } else {
      unsigned long long nowTicks = getticks();
      for(j=0;j<g_numpollfds;j++) {
        SocketInfo_t * sinfo = &g_sinfos[g_pollfdsToSocketIndex[j]];
        if(g_pollfds[j].revents & POLLOUT) {
          int err;
          socklen_t errlen = sizeof(err);
          if(getsockopt(sinfo->socket, SOL_SOCKET, SO_ERROR, &err, &errlen)==0) {
            if(err==0) {
              printf("%sPort %d accepted IPv4 TCP connection (indexes: %d and %d)\n", (needNewline?"\n":""), sinfo->port, j, g_pollfdsToSocketIndex[j]);
              needNewline = 0;
              if(sinfo->port>10000) {
                int k = 0;
              }
            } else {
              // printf("\rPort %d rejected IPv4 TCP connection: %d\n", sinfo->port, err);
            }
          }
          close(sinfo->socket);
          sinfo->isValid = 0;
        } else if(sinfo->startTicks - nowTicks > 1000ULL) {
          // printf("Port %d timed out on IPv4 TCP connection\n", sinfo->port);
          close(sinfo->socket);
          sinfo->isValid = 0;
        }
      }
    }

    printf("...%d", g_nextPort);
    fflush(stdout);
    needNewline = 1;

  } // while(1) {

  if(needNewline) {
    printf("\n");
  }
}
