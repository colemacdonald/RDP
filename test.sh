#sws test shell script

echo "Sending 'GET / HTTP/1.0\\r\\n\\r\\n'"
echo -e -n "GET / HTTP/1.0\r\n\r\n" |  nc -u -q 1 -s 192.168.1.100 10.10.1.100 3080 &&
echo "Sending 'GET /nofile HTTP/1.0\\r\\n\\r\\n'"
echo -e -n "GET /nofile HTTP/1.0\r\n\r\n" |  nc -u -q 1 -s 192.168.1.100 10.10.1.100 3080 &&
echo "Sending 'GET / HTTP/1.1\\r\\n\\r\\n'"
echo -e -n "GET / HTTP/1.1\r\n\r\n" |  nc -u -q 1 -s 192.168.1.100 10.10.1.100 3080 &&
echo "Sending 'GET /../sws_server.c HTTP1.0\\r\\n\\r\\n'"
echo -e -n "GET /../sws_server.c HTTP/1.0\r\n\r\n" |  nc -u -q 1 -s 192.168.1.100 10.10.1.100 3080 &&
echo "Sending 'GET /AUTHORS.txt HTTP/1.0\\r\\n\\r\\n"
echo -e -n "GET /AUTHORS.txt HTTP/1.0\r\n\r\n" |  nc -u -q 1 -s 192.168.1.100 10.10.1.100 3080