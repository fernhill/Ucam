var http = require('http');
var fs = require('fs');
var url = require('url');

// Loading the file index.html displayed to the client
var server = http.createServer(function(req, res) {
    fs.readFile('./index.html', 'utf-8', function(error, content) {
        res.writeHead(200, {"Content-Type": "text/html"});
        res.end(content);
    });
	

// Loading socket.io
var io = require('socket.io').listen(server);

io.sockets.on('connection', function (socket, username) {

    
	
	socket.on('save_program', function (message1) {
		//todo: write file code
		var jsobj = JSON.parse(message1);
	
		   
		var fn = "prms1/" + jsobj.filename;
		fs.writeFile(fn, jsobj.prgm, (err) => { 
			  
			// In case of a error throw err. 
			if (err) throw err; 
		});

		socket.emit('save_progra_reply', 'Program saved successfully');
	 
     });
	 
	 
	 socket.on('getAllFiles', function (message3) {
		 
        
		//todo readfile
		
		var path = "prms1";
		 var filelist="";
		fs.readdir(path, function(err, items) {
			
			
			for (var i=0; i<items.length; i++) {
				filelist = filelist + '<div style="height:40px; font-size: 40px;" class="fa fa-code" ></div><font  size="6">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; <a href="editprogramv1.html?fn='+items[i]+'">'+ items[i] + '</a></font><br/>';
			}

			socket.emit('allFiles', filelist);
		});
		
		
		
    });

	 socket.on('readFile',function(message4) {
	
	          fs.readFile('prms1/'+message4,"utf8", function(error, content) {
					
					socket.emit('Filecontent',content);
				});
		
    }); 
	
	
		
		socket.on('dlt_program', function(message6){
			fs.unlink('prms1/'+message6, function (err) {
				if (err) throw err;
				
			});
		socket.emit('dltpgrm','Program file deleted successfully');
		});
		

        socket.on('getparams',function(message4) {
	
	          fs.readFile('Output1.txt',"utf8", function(error, content) {
					
					
					socket.emit('params',content);
				});
		
    }); 	
		
		
		socket.on('getpitch',function(message4) {
	
	          fs.readFile('Output.txt',"utf8", function(error, content) {
					
					socket.emit('pitch',content);
				});
		
    }); 	
	
	
		socket.on('getoffset',function(message4) {
	
	          fs.readFile('Output2.txt',"utf8", function(error, content) {
					
					
					socket.emit('offset',content);
				});
		
    }); 
		
		socket.on('getuserpw',function(message4) {
	        
			 fs.readFile('userpassword.txt',"utf8", function(error, content) {
					
					
					socket.emit('upw',content);
				});
	       
			
    });

        socket.on('getadminpw',function(message5) {
			

	        
			 fs.readFile('adminpassword.txt',"utf8", function(error, content) {
					socket.emit('apw',content);
				});
    }); 
		
		socket.on('changepass',function(message4){
			fs.writeFile('userpassword.txt',message4, (err) => { 
			  
					// In case of a error throw err. 
					if (err) throw err; 
				});
		    socket.emit('pwsuccess','Password changed successfully');
		});
		
		
		socket.on('putparams',function(message1){
		   fs.writeFile('Output1.txt',message1, (err) => { 
			  
					// In case of a error throw err. 
					if (err) throw err; 
					socket.emit('paramssaved','saved successfully');
				});
		});
		
		
		socket.on('putpitch',function(message1){
		   fs.writeFile('Output.txt',message1, (err) => { 
			  
					// In case of a error throw err. 
					if (err) throw err; 
					socket.emit('pitchsaved','saved successfully');
				});
		
			
		});
		
		
		socket.on('putoffset',function(message1){
		   fs.writeFile('Output2.txt',message1, (err) => { 
			  
					// In case of a error throw err. 
					if (err) throw err; 
					socket.emit('offsetsaved','saved successfully');
				});
		
			
		});
		
		socket.on('deactivatedate',function(message1){
		   fs.writeFile('activation_date.txt',message1, (err) => { 
			  
					// In case of a error throw err. 
					if (err) throw err; 
					socket.emit('active_date','saved successfully');
				});
		
			
		});
		socket.on('getdeactivatedate',function(message1){
		  fs.readFile('activation_date.txt',"utf8", function(error, content) {
					
					
					socket.emit('put_date',content);
				});
		
			
		});
		
		socket.on("destination_position", function(data){
		// console.log(data.pos);
		var pos = data.pos;
		if(pos === null || pos === undefined){
		  return;
		}
		if(Object.keys(data).length == 0){
		  return;
		}
		pos = Math.round(pos*10000)/10000;
		var rnd = Math.round(pos);
		if(Math.abs(rnd-pos) <= 0.001){
		   pos = rnd;
		}
		pos = pos % 360;
		//console.log(Math.round(a));
		// if(pos<0){
		// 	pos = 360+pos;
		// }
		pos = parseFloat(pos).toFixed(3);
		////console.log(pos);
		//code_content
		//$(".code_content").scrollTop()
		////console.log(pos);
		var t_obj = document.getElementById('x_cur');
		var t1_obj = document.getElementById('x1_cur');
		////console.log(t_obj);
		t_obj.innerHTML = pos.toString();
		if(t1_obj){
		  t1_obj.innerHTML = pos.toString();
		}
		//t_obj.textContent = " "+pos+" ";
		/*if(obj){
		  obj.innerHTML = pos;
		}
		$scope.x_cur = pos;*/
	  });
	  
	  
	  
	  
		socket.on('pos_data', function(data){
		console.log("Position Data",data);
		var d = data.data.split(".");
		if(d.length > 2){
		  data.data = d[0]+"."+d[1];
		}
		var pos = data.data;
		pos = Math.round(pos*10000)/10000;
		pos = parseFloat(pos).toFixed(3);
		console.log(pos);
		var obj = document.getElementById('x_dest');
		var obj1 = document.getElementById('x1_dest');
		if(obj){
		  obj.innerHTML = pos;
		}
		if(obj1){
		  obj1.innerHTML = pos;
		}
	  });
	  
	  
	  
	    socket.on('goToZero',function(message){
			console.log("Move to zero now");
			//1. Disable backlash now.
			fs.writeFileSync('./direction_cache', 1);
			xdir = 1;
			mp.initParams();
			console.log("Params initialized");
			MOVE_TYPE = ABS;
			mp.mode = "ABS";
			//console.log("*******************MOvetype mode is ",MOVE_TYPE);
			//console.log("*******************MOde is "+mp.mode);
			writeToEthercatClient("1 18 1\n");
			MOVE_TYPE = ABS;
			mp.mode = "ABS";
			mp.shortest_path = false;
			//If POT or Not is set then shortest_path should be enabled.
			if(mp.potlimit > 0 || mp.notlimit > 0){
				mp.shortest_path = true;
			}
			var cmd=[1, 31, 0];
			tmp=mp.getDestinationAngle(cmd);
			console.log("TMP value is" +tmp);
			writeToEthercatClient("1 8 0\n");
			dir_change = 0;
			fs.writeFileSync('./direction_cache', 1);
			if(mp.home_dir == 0){
			  console.log("Homing direction is negative.. So reverse");
			  tmp = (360-tmp)*-1;
			  fs.writeFileSync('./direction_cache', -1);
			}
			tmp =mp.getPulsesRequired(tmp);
			cmd[2] =tmp;
			cmd=cmd.join(" ");
			cmd=cmd+ "\n";
			console.log("1--------------Will execute command "+cmd);
			//fs.writeFileSync('./direction_cache', 0);
			setTimeout(function(){
			  console.log("2------------Will execute command "+cmd);
			  writeToEthercatClient(cmd);
			}, 1000);
			mp.ref_angle = 0;
			
			socket.emit('finish','finished');
		  });
		
		
		
    }); 
	
});


server.listen(8080);