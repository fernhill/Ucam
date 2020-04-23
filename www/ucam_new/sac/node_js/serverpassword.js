
var http = require('http');
var url = require('url');

http.createServer(function (req, res) {
	if(req.url.indexOf("data=")>0){
		  res.writeHead(200, {'Content-Type': 'text/html'});
		  var q = url.parse(req.url, true).query;
		  var data = q.data;
		  const fs = require('fs') 
  
		// Write data in 'Output.txt' . 
		fs.writeFile('password.txt', data, (err) => { 
			  
			// In case of a error throw err. 
			if (err) throw err; 
		});

		res.end(data);
	} 
}).listen(8050);
