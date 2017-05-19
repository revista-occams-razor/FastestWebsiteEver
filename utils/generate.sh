#!/bin/sh

SIZE=`wc -c index-deflate.html | awk '{print $1}'`
echo "Size is $SIZE"
cat << EOM > index1.html
HTTP/1.1 200 k
Content-Length: $SIZE
content-encoding: deflate

EOM
cat index-deflate.html >> index1.html
