var net = require('net');

function main()
{
	var count = 1;
	var server = net.createServer();
	console.log(server);
	server.listen(8080);
	server.on('connection', function(socket) {
		console.log(count + ':');
		count++;
		console.log(socket);
		socket.on('data', function(data) {
			console.log('received: ' + data);
			socket.write('HTTP/1.1 200 OK\r\n');
			socket.write('Content-Type: text/plain\r\n');
			socket.write('\r\n');
			socket.write('Hello');
			socket.end();
		});
		socket.on('end', function() {
			console.log('remote closed');
		});
		socket.on('error', function(error) {
			console.log('error: ' + error);
		});
	});
}

main();
