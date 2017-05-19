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
#include <sys/uio.h>
#include <fcntl.h>


#include <zlib.h>
#define DIE(x) {perror(x);exit(EXIT_FAILURE);}

#define PORT "80"  // the port users will be connecting to
#define HEADERS "HTTP/1.1 200 k\nContent-Length: %ld\ncontent-encoding: deflate\n\n"
#define FILENAME "index.html"

#define BACKLOG 10     // how many pending connections queue will hold
#define MAX_CONTENT_LENGTH 99999
#define MAX_HEADERS_LENGTH (strlen(HEADERS) + 2)


int 
main(void)
{
  int         sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t   sin_size = sizeof their_addr;
  int         yes=1;
  int         rv;
  char        *buffer;
  long        numbytes;
  long        hdrbytes;
  z_stream    defstream;
  char        *obuf;
  struct stat st;
  int         fd;
  int         pipe1[2];
  int         pipe2[2];
  
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP
  
  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) < 0) 
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
    }
  
  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) 
    {
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
	{
	  perror("socket:");
	  continue;
	}
      
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) 
	DIE("setsockopt reuseaddr:");
#ifdef SO_BUSY_POLL
    if (setsockopt(sockfd, SOL_SOCKET, SO_BUSY_POLL, &yes, sizeof(int)) < 0)
      DIE ("setsockopt busypoll:");
#endif
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) < 0) 
      DIE ("setsockopt tcp nodelay:");
    
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) break;
    close(sockfd);
    }
  
  freeaddrinfo(servinfo);
  
  if (p == NULL) 
    {
      fprintf(stderr, "server: failed to bind\n");
      exit(1);
    }
  
  if (listen(sockfd, BACKLOG) < 0) DIE ("listen:");
  
 
  if ((stat (FILENAME, &st)) < 0) DIE ("stat:");
  if ((fd = open (FILENAME, O_RDONLY)) < 0) DIE ("open:");
  
  numbytes = st.st_size;
  
  // Compress file
  //-----------------------------
  if ((buffer = malloc (numbytes * sizeof (char))) == NULL) return 1;
  if ((obuf = malloc (numbytes * sizeof (char))) == NULL) return 1;
  if ((read (fd, obuf, numbytes)) < 0) DIE("read:");
  close (fd);
  
  defstream.zalloc = Z_NULL; 
  defstream.zfree = Z_NULL;
  
  // Input
  defstream.avail_in = numbytes; 
  defstream.next_in = (Bytef *) obuf; 
  
  // Output
  defstream.avail_out = numbytes;
  defstream.next_out = (Bytef *)buffer; 
  
  // Match default Python script configuration
  deflateInit2 (&defstream, Z_BEST_COMPRESSION, Z_DEFLATED, 
		-15, 8, Z_DEFAULT_STRATEGY);
  deflate (&defstream, Z_FINISH);
  deflateEnd (&defstream);
  
  numbytes = defstream.total_out;
  if(numbytes > MAX_CONTENT_LENGTH) 
    {
      fprintf(stderr, "server: content is longer than maximum size %d\n", 
	      MAX_CONTENT_LENGTH);
    }
  // Reuse obuf for header
  //if ((obuf = realloc (obuf, MAX_HEADERS_LENGTH)) == NULL) DIE("realloc:");
  hdrbytes = sprintf(obuf, HEADERS, numbytes);
  
  if (pipe(pipe1) < 0) DIE ("pipe:");
  if (pipe(pipe2) < 0) DIE ("pipe:");

  // Feed the pipe
  write (pipe1[1], obuf, hdrbytes);
  write (pipe1[1], buffer, numbytes);

  // Buffers are now in the kernel. We do no need the user space
  // buffers anymore
  free (obuf);
  free (buffer);

  // Copy pipe1 content into pipe2 content without consuming
  if (tee (pipe1[0], pipe2[1], numbytes + hdrbytes, 0) < 0) perror ("tee:");
  
  printf ("INFO: TCP payload size: %ld\n", numbytes + hdrbytes);
  printf("server: waiting for connections on port %s...\n", PORT);
  
  while(1) 
    {
      if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) < 0) 
	{
	  perror("accept");
	  continue;
	}

      // Copy pipe2 content into socket consuming
      if (splice (pipe2[0], NULL, new_fd, NULL, numbytes + hdrbytes, 
		  SPLICE_F_MOVE | SPLICE_F_MORE) < 0) perror ("splice:");

      close(new_fd);
      
      // Copy pipe1 content into pipe2 content without consuming
      if (tee (pipe1[0], pipe2[1], numbytes + hdrbytes, 0) < 0) perror ("tee:");
    }
  
  return 0;
}
