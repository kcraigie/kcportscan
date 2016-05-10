#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

typedef unsigned char uint8_t;
typedef unsigned int uint_t;

int main(int argc, char * argv[])
{
  int ss = -1;
  int cs = -1;

  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;

  if(argc!=2) {
    printf("Usage: %s <ad.dr.es.s:port>\n", argv[0]);
    goto bail;
  }
  uint_t octets[5];
  if(sscanf(argv[1], "%u.%u.%u.%u:%u", octets+0, octets+1, octets+2, octets+3, octets+4) != 5) {
    printf("Usage: %s <ad.dr.es.s:port>\n", argv[0]);
    goto bail;
  }

  sa.sin_addr.s_addr = octets[0] << 24 & 0xff000000;
  sa.sin_addr.s_addr |= octets[1] << 16 & 0x00ff0000;
  sa.sin_addr.s_addr |= octets[2] << 8 & 0x0000ff00;
  sa.sin_addr.s_addr |= octets[3] << 0 & 0x000000ff;
  sa.sin_addr.s_addr = htonl(sa.sin_addr.s_addr);
  sa.sin_port = htons(octets[4]);

  ss = socket(AF_INET, SOCK_STREAM, 0);
  if(ss==-1) {
    goto bail;
  }

  {
    int on = 1;
    setsockopt(ss, SOL_SOCKET,  SO_REUSEADDR, (char*)&on, sizeof(on));
  }

  if(bind(ss, (struct sockaddr*)&sa, sizeof(sa))!=0) {
    goto bail;;
  }

  printf("Listening on: %d.%d.%d.%d:%d...\n", octets[0], octets[1], octets[2], octets[3], octets[4]);
  if(listen(ss, 1)!=0) {
    goto bail;
  }

  cs = accept(ss, NULL, NULL);
  if(cs==-1) {
    goto bail;
  }

  printf("Accepted connection!\n");

 bail:
  if(cs!=-1) {
    close(cs);
  }
  if(ss!=-1) {
    close(ss);
  }
  if(cs==-1) {
    return 1;
  }
  return 0;
}
