/** @file main.c */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>

#define DIE(x) {perror(x);exit(EXIT_FAILURE);}
#define PORT "80"             // the port users will be connecting to
#define FILENAME "index1.hml" // XXX: Include HTTP Header
#define BACKLOG 10            // how many pending connections queue will hold

int 
main(void)
{
  int                     s, s1;  // listen on sock_fd, new connection on new_fd
  struct addrinfo hints,  *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t               sin_size = sizeof (their_addr);
  int                     fd, rv, yes = 1;
  long                    numbytes;
  struct stat             st;
 
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP
  
  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) 
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) 
    {
      if ((s = socket (p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
	continue;
      
      if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) 
	DIE("setsockopt reuseaddr:");
#ifdef SO_BUSY_POLL
      if (setsockopt (s, SOL_SOCKET, SO_BUSY_POLL, &yes, sizeof(int)) < 0) 
	DIE ("setsockopt busypoll:");
#endif     
      if (setsockopt (s, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) < 0)
	DIE ("setsockopt tcp nodelay:");
      
      if (bind (s, p->ai_addr, p->ai_addrlen) == 0) break;

      close (s);
    }
  
  freeaddrinfo(servinfo);
  
  if (!p) 
    {
      fprintf(stderr, "server: failed to bind\n");
      exit(1);
    }
  
  if (listen (s, BACKLOG) == -1) DIE ("listen:");
  
  // Header also included in the file to load...
  if ((stat ("index1.html", &st)) < 0) DIE ("stat:");
  if ((fd = open ("index1.html", O_RDONLY)) < 0) DIE ("open:");

  numbytes = st.st_size;
  printf("server: waiting for connections on port %s...\n", PORT);
  
  while(1) 
    {
      if ((s1 = accept (s, (struct sockaddr *)&their_addr, &sin_size)) < 0)
        {
	  perror("accept");
	  continue;
        }
      if (sendfile (s1, fd, 0, numbytes) == -1)	perror("sendfile:");
      if (lseek (fd,0,SEEK_SET) < 0) perror ("lseek:");

      close (s1);
    }
  
  return 0;
}
