# Playing with small webservers

Based on the awesome code from 

https://github.com/diracdeltas/FastestWebsiteEver

We built a few experiments:

* server.c : This is the original server
* server-deflate.c : This is the original server using write but compressing the file instead of reading an already compressed file from disk
* server-writev.c : Same than server-deflate.c but using writev so the HTTP header and the compressed HTML page can be hold in different buffers
* server-splice.c : Idem but using tee and splice. The page to be served is feed in a pipe (kernel buffer) and then it is sent using splice without buffer transfers between user and kernel space
* server-sendfile.c: This uses sendfile to feed directly the file into the socket. Now the file read from the disk is compressed as in the original program but the file also includes the header so the code is more compact. Check the utils folder for a small tool to generate the file required by this example. First you have to generate the compressed index.html with the corresponding tool under utils

