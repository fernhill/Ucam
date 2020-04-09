/*Version = 1.1.0
Description= Pot Not Hard limit working
Pot Not Soft limit working
*/
net = require('net');
//var sleep = require('sleep');
var server = require("net").createServer();
var io = require("socket.io")(server);
var app = require('http').createServer(handler)
var io = require('socket.io')(app);
var fs = require('fs');
var mp = require('./machine_parser.js');
var loopval,delayval,moveval,tmp,fval;
var rval = 0,g17flag=0,exec_g167=0;
var xdir=1;  //1 is positive value of X. 0 is negative value of X.
var prev_dir = xdir;
var dir_change = xdir;
var tmp=0, posflag=0, alarmnumber=0,cflag1 = 0, cflag2 = 0;
var counter = 0;
var MODE, ECS, P_OBJ, UNDER_EXECUTION, EXEC_LINE, SP_ENABLED;
var ERROR = 0;
var LINEAR = 1, ABS = 0;
var MOV_MODE = LINEAR;
var X_RATIO = 20000;
//var X_OFFSET = 800000;
var X_CUR_POS = 0;
var EXEC_LINE = -1;
var STOP = 0;
var EMER_STOP = 0;
var SINGLE_BLOCK = 0;
var PULSE_RECEIVED = 0;
var UNDER_MOVEMENT = false;
SP_ENABLED = 0;
var PULSE_STATUS = 0;
// Keep track of the chat clients
var clients = [];
var ui_clients = [];
PREVIOUS_COMMAND_EXEC_TS = 0;
var ETHERCAT_STATUS = 0;

var posi;

net.createServer(function (socket) {
  // Identify this client
  // Start a TCP Server
  if(mp.alarm === null ){
  updateClients('ETH_UP', "Ethercat communication up...");
  ETHERCAT_STATUS = 1;
  }
  socket.name = socket.remoteAddress + ":" + socket.remotePort
  // Put this new client in the list
  clients.push(socket);

  console.log(socket.name+" joined the chat..");
  // Handle incoming messages from clients.

  socket.on('data', function (data) {
    broadcast(socket.name + "> " + data, socket);
  });
  // Remove the client from the list when it leaves
  socket.on('end', function () {
    clients.splice(clients.indexOf(socket), 1);
    broadcast(socket.name + " left the chat.\n");
	ETHERCAT_STATUS = 0;
	updateClients('ETH_DOWN', "Ethercat communication down...");
  });
  // Send a message to all clients
  function broadcast(message, sender) {
    clients.forEach(function (client) {
      // Don't want to send it to sender
      if (client === sender) return;
      client.write(message);
    });
    // Log it to the server output too
    process.stdout.write(message)
  }

  //console.log("Initialize settings on the drive");

  while(1){
    if(clients.length > 0){
      //console.log("Will Send Finish Signal now");
      var sock = clients[0];
      sock.write("1 6 0\n");
      //Send all settings to the
      init_drive_settings();


      break;
    }
  }

}).listen(9000);

// pubber
var zmq = require('zmq')
  , sock = zmq.socket('pair');

sock.bindSync('tcp://127.0.0.1:5556');
//console.log('Publisher bound to port 5556');

var writeToEthercatClient = function(message){
	try{
		clients[0].write(message);
	}catch(err){
		console.log(err);
		updateClients('ETH_DOWN', "Ethercat communication down...");
	}

}

var init_drive_settings = function(){
  mp.initParams();
  /*
  1. Set Homing Direction
  2. Set motor Direction
  3. Set Clamp/Declamp Flag
  4. Enable/Disable ECS
  */
  //Homing
  //console.log(mp);
  //console.log("Setting Homing DIR");
  if(mp.home_dir == 1){
    //Positive..
    writeToEthercatClient("1 22 0\n");
  }else {
    //Negative
    writeToEthercatClient("1 23 0\n");
  }
  //console.log("Setting Motor DIR");
  //Direction
  if(mp.motor_dir == 1){
    //Positive..
    writeToEthercatClient("1 24 0\n");
  }else {
    //Negative
    writeToEthercatClient("1 25 0\n");
  }
  //console.log("Setting Clamp Declamp");
  //Clamp Declamp
  if(mp.cl_dl == 1){
    //Positive..
    writeToEthercatClient("1 26 0\n");
  }else {
    //Negative
    writeToEthercatClient("1 27 0\n");
  }
  //console.log("Setting ECS");
  //ECS Setting
  if(mp.ecs == 1){
    //Positive..
    writeToEthercatClient("1 20 0\n");
  }else {
    //Negative
    writeToEthercatClient("1 21 0\n");
  }
  //console.log("Setting Finish Signal");
  if(mp.fin_signal == 1){
    //Positive..
    writeToEthercatClient("1 28 0\n");
  }else {
    //Negative
    writeToEthercatClient("1 29 0\n");
  }
  if(mp.timing > 0){
    //console.log("\n\nSENDING TIMING\n\n");
    var cmd = "1 35 "+mp.timing+"\n";
    //console.log(cmd);
    writeToEthercatClient(cmd);
  }
  if(mp.cldl_timing !== null && mp.cldl_timing !== undefined){
	  writeToEthercatClient("1 39 "+mp.cldl_timing.toString()+"\n");
  }
  //console.log("All Settings Complete");
}
sock.movement = false;
sock.on('message', function(message){
  var m = message.toString();
  if(m == "TARGET"){
  	console.log("DBG:: Target Reached");
	  UNDER_MOVEMENT = false;
  }
            ////console.log("Message : ", m);
  if(m == "REFCOM"){
    //console.log("-------REFERENCE COMPLETED-----");
    updateClients("ref_complete",{'ref':'complete'});
  }
  if(m == "TARGET" && SINGLE_BLOCK == 0 && mp.ecs == 0){
    ////console.log("Reached target. Will execute next line");
    UNDER_MOVEMENT=false;
    execute_line();
  }
  if(m == "TARGET" && mp.ecs == 1){
	  console.log("DBG:: Undermovement is false since ECS is enabled");
	  UNDER_MOVEMENT=false;
  }
  else if (m.indexOf("ALARM") >=0) {
    ////console.log("This is alarm");
	// console.log("--Alarm Status --");
	// console.log(m);
	if(m[9] == 0 && UNDER_MOVEMENT === false){
		PULSE_STATUS = 0;
	}
	//console.log("M[9] --> "+m[9]+", Error -> "+ERROR+", UNDER_MOVEMENT -> "+UNDER_MOVEMENT+", MP.ECS -> "+mp.ecs+", PULSE_STATUS -> "+PULSE_STATUS);
    if(m[9] == 1 && ERROR == 0 && UNDER_MOVEMENT === false && mp.ecs == 1 && PULSE_STATUS == 0){
      console.log("WILL EXECUTE LINE");
	  console.log("DBG:: Setting True, since All conditions met");
	  UNDER_MOVEMENT = true;
	  execute_line();
    }
    updateClients("alarms",m);
  }
  else if (m.indexOf("OUTPUT") >=0) {
    ////console.log("This is OUTPUT");
    ////console.log(m);
    //updateClients("alarms",m);
  }
  /*else if (m.indexOf("ECS") >=0) {
    ////console.log("This is OUTPUT");
    ////console.log(m);
    //updateClients("alarms",m);

    //console.log("\n\n******************************\n");
    //console.log("PULSES RECEIVED. Now do whatever...");
    //console.log("\n\n******************************\n");
    PULSE_RECEIVED = 1;
    if(sock.movement == false){
      execute_line();
    }
  }*/
  else if(m.indexOf("ERROR ")>=0)
  {
	//console.log("DBG:: Setting False due to Error.. Error is ->"+m);
    alarmnumber=parseInt(m.substr(m.indexOf(" "),m.length));
    if(alarmnumber>0)
    alarmnumber=alarmnumber-65280;
    //console.log(alarmnumber);
    var msg = mp.error_codes[alarmnumber];
    if(alarmnumber > 0){
      updateClients("alarm_error", msg);
      UNDER_MOVEMENT = false;
      ERROR = 1;
    }else{
		if(mp.alarm === null ){
      updateClients("alarm_error", "No Alarms");
		ERROR = 0;}
    }
  }
  else if(m.indexOf("FINSIGNAL")>=0)
  {
    //console.log("***************************************FINSIGNAL RECEIVED");
    updateClients("FINSIGNAL", m);
  }
    if(mp.alarm !== null && mp.alarm !== undefined){
      ////console.log(mp.alarm);
      updateClients("alarm_error", mp.alarm);
    }
    if(!isNaN(m)){
      CURRENT_POS = m;
      var p = mp.getCurrentPosition(m);
	  posi = p;
      // console.log("Position --> "+p+", Direction "+mp.direction+", Mid Value "+mp.mid+", Stopped -> "+mp.stopped);
      var mid = 0;
       //console.log("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$" + mp.alarm);
	  if(mp.alarm == "POT Hard Limit Switch Latched"){
		 if(mp.direction == 1  ){
            console.log("%%%%%%%%%%%%%%%%%%%%%");
			writeToEthercatClient("1 16 3");
            //console.log("Pot Stopping Jog");
            mp.stopped = 1;
          }
		   UNDER_MOVEMENT = true;
		  mp.alarm = "POT Hard Limit Switch Latched";
		  //console.log("################################" + mp.alarm);
		  updateClients("alarm_error", "POT Hard Limit Switch Latched");
	  }else if(mp.alarm == "NOT Hard Limit Switch Latched"){
		  if(mp.direction == -1 ){
			  console.log("&&&&&&&&&&&&&&&&&&");
            writeToEthercatClient("1 16 3");
            //console.log("Not Stopping Jog");
            mp.stopped = 1;
          }
		   UNDER_MOVEMENT = true;
		  mp.alarm = "NOT Hard Limit Switch Latched";
		  //console.log("##########&&&&&&&&&&############" + mp.alarm);
		  updateClients("alarm_error", "NOT Hard Limit Switch Latched");
	  }//* else if((mp.potlimit > 0 && p < mp.mid) && mp.direction == 1){
       else if((mp.potlimit > 0 && p < mp.mid) && mp.direction == 1 ){		  
		  //console.log("ppppppppppppppppppppppppppppppp" + p);
	/* 	   if ( p < mp.potlimit)
		  {
			  
			// Jog_feed ///////////////////////////////////////////////////&&&&&&&&&&&&&&&&&&&&&&&

			 var rpm_const = 1333.3334*90;
    var rpm = Math.round(rpm_const*1);
	
    //console.log("RPM Value from JSON is " + mp.feedrate);
	//console.log("Write RPM"+rpm);
	
	//Move in Positive direction
      //var cmd = "1 16 1\n";
      //mp.setDirection(1);
      //console.log("Writing command ==> "+cmd);
      //writeToEthercatClient(cmd);
      
    
      //console.log("Write RPM");
	  mp.stopped = 0;
      var d = "1 17 "+rpm+"\n";
      writeToEthercatClient(d);
      console.log("Jog Pot limit speed variable #1234#");
			  
          }  */
        ////console.log(mp.potlimit);
		// var val = mp.potlimit - mp.threshold;
		// if(mp.program_mode == "JOG"){
		// 	val = (mp.potlimit - mp.threshold);
		// }
		
        if(p >= (mp.potlimit - mp.threshold)){
          if(mp.direction == 1  && mp.stopped != 1){
            writeToEthercatClient("1 16 3");
            console.log("Pot Stopping Jog");
            mp.stopped = 1;
          }
		   UNDER_MOVEMENT = true;
		  //console.log("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB" + mp.alarm);
		  if(mp.alarm == "NOT Limit Exceeded"){
			mp.alarm = mp.alarm + ",POT Limit Exceeded";
			
		  }else if(mp.alarm == null || mp.alarm == "") {			 
				
		    mp.alarm = "POT Limit Exceeded";
		  } 
		   
		  
		  
		  updateClients("alarm_error", mp.alarm);
		  
		  
		  
        }
      }
     // else if (mp.notlimit > 0 && mp.direction == -1){
	 //* else if (mp.notlimit > 0 && mp.direction == -1 && (p<=mp.notlimit && p>=mp.potlimit)){
	 else if (mp.notlimit > 0 && mp.direction == -1 && (p<=mp.notlimit && p>=mp.potlimit)){	 
		  // var val = mp.notlimit
		  // if(mp.program_mode == "JOG"){
  			// val = (mp.potlimit + mp.threshold);
  		  // }
        ////console.log(mp.potlimit);
        if(p <= (360 - mp.potlimit)){
          if(mp.direction == -1 && mp.stopped != 1){
            writeToEthercatClient("1 16 3");
            console.log("Not Stopping Jog");
            mp.stopped = 1;
          }
		   UNDER_MOVEMENT = true;
		   //console.log("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" + mp.alarm);
		  if(mp.alarm == "POT Limit Exceeded"){
			mp.alarm = mp.alarm + ",NOT Limit Exceeded";
			
		  }else if(mp.alarm == null || mp.alarm == ""){			 
				
		    mp.alarm = "NOT Limit Exceeded";
		  }
		  
		  updateClients("alarm_error", mp.alarm);
		 
        }
      } 
      mp.cur_x_pos = p;
      if(posflag == 0){
        //Do whatever
        //console.log("Connection with the drive is successful. Initialising now.");
        mp.initParams();
        init_drive_settings();
        posflag=1;
      }
    }

    if(ui_clients.length > 0){
      if(mp.workoffsetflag==1)
        p=p-mp.workoffset;
      for(var i=0; i<ui_clients.length; i++)
      {
        if(isNaN(p)){
          break;
        }
        // //console.log("------->>"+p+"<<-----------");
        var pe_val = 0;

        try{
          pe_val = mp.getAngleWithPitchError(0,0);
        }catch(e){
          pe_val = 0;
        }

        counter++;
        var final_pos = p+(mp.factor_backlash*mp.drive_backlash)-pe_val;
        final_pos = parseFloat(final_pos).toFixed(3);
        if(counter > 300){
          counter = 0;
        }

		if(mp.workoffsetflag == 1){
			ui_clients[i].emit("destination_position",{"pos":p+(mp.factor_backlash*mp.drive_backlash)-pe_val});
		}else{
			ui_clients[i].emit("destination_position",{"pos":p+(mp.factor_backlash*mp.drive_backlash)-pe_val});
		}
		//console.log(mp.potlimit + "TODO "+p+"#####" + mp.direction);
		
      }
    }
  }
);

//console.log("Chat server running at port 5000\n");
function handler (req, res) {
  fs.readFile(__dirname + '/index.html',
  function (err, data) {
    if (err) {
      res.writeHead(500);
      return res.end('Error loading index.html');
    }

    res.writeHead(200);
    res.end(data);
  });
}
app.listen(9090);

io.on("connection", function(socket){
  //console.log("Client Connected");
  ui_clients.push(socket);

  socket.on('set_program_mode', function(message){
    //console.log("Inside Program Mode Setting..");
    if(message.mode == "single"){
      //console.log("\n\n--------Will execute single block---------\n\n\n");
      SINGLE_BLOCK = 1;
    }else{
      //console.log("\n\n\n----------Will execute continuous block-----------\n\n\n");
      SINGLE_BLOCK = 0;
    }
  })

  socket.on("disconnect", function(){
    //console.log("User disconneted");
    io.emit('user disconnected');
    ui_clients.splice(clients.indexOf(socket), 1);
  })
  socket.on("program", function(message){
    //console.log("Someone wants me to execute the program. Should I?");
    //console.log(message);
    socket.broadcast.emit('program',message);
  })
  socket.on("status", function(message) {
    //console.log("status message", message.from);
        socket.client = message.from;
  })
  socket.on("message",function(message){
    //console.log(message)
    socket.broadcast.emit('message', message);
})
  socket.on("stop_execution", function(message){
    //console.log("Stopping execution now...");
    writeToEthercatClient("1 33 0\n");
    EXEC_LINE = mp.parsed_lines.length + 1;
  })

  //Initiliaze all settings on the drive
  socket.on("updateSettings", function(message){
    //console.log("Inside Update Settings");
    init_drive_settings();
    //Set POT & NOT Limits
  })
 
  if(posi >= (mp.potlimit - mp.threshold)){
          if(mp.direction == 1  && mp.stopped != 1){
            writeToEthercatClient("1 16 3");
            console.log("Pot Stopping Jog");
            mp.stopped = 1;
          }
		   UNDER_MOVEMENT = true;
		  mp.alarm = "POT Limit Exceeded";
		  updateClients("alarm_error", "");
        }
  socket.on("reset", function(message){
	  //if( mp.alarm == "POT Limit Exceeded" && posi >= mp.potlimit){
		
		  //updateClients("alarm_error", "POT Limit Exceeded - Adjust to POT limit:" + mp.potlimit);
	  //}else{
		mp.workoffsetflag = 0;
		mp.workoffset = 0;
		UNDER_MOVEMENT = false;
		//Will reset alarms
		if(mp.alarm !=null && mp.direction == 1 && mp.alarm.indexOf("POT Limit Exceeded")>=0 && mp.alarm.indexOf("NOT Limit Exceeded")>=0){
			mp.alarm = "NOT Limit Exceeded";
		}if(mp.alarm !=null && mp.direction == -1 && mp.alarm.indexOf("POT Limit Exceeded")>=0 && mp.alarm.indexOf("NOT Limit Exceeded")>=0){
			mp.alarm = "POT Limit Exceeded";
		}else{
			mp.alarm = null;
		}
		//console.log("Reset command received");
		writeToEthercatClient("1 13 0\n");
		setTimeout(function(){
		  writeToEthercatClient("1 7 0\n");
		  setTimeout(function(){
			writeToEthercatClient("1 8 0\n");
		  },1000);
		},1000);
		  //console.log("Will power on the drive now: \n");
		EXEC_LINE = -1;
	  //}
  })

  socket.on('resetMultiTurn', function(message){
    writeToEthercatClient("1 34 0\n");
  })

  socket.on("emergency", function(message){
    //Will toggle emergency
	UNDER_MOVEMENT = false;
	mp.workoffsetflag = 0;
	mp.workoffset = 0;
    writeToEthercatClient("1 7 0\n");
    EXEC_LINE = mp.parsed_lines.length + 1;
    if(message.status == true){
      //console.log("On Emergency");
      writeToEthercatClient("1 14 1");
    }else{
      //console.log("Off Emergency");
      writeToEthercatClient("1 14 0");
    }

  })

  socket.on("execute", function(message){
    //console.log("Setting Step Mode Disabled");
    //writeToEthercatClient("1 29 0");
    //console.log("Inside Execute Message");
    //console.log(message);
    //socket.broadcast.emit('execute', message);
    mp.resetDirection();
    var line_no = message.lineno;
    execute_file(message, line_no)
  })
  socket.on("line_number", function(message){
    //console.log(message)
    socket.broadcast.emit('line_number', message)
  })

  socket.on("line_complete", function(message){
    socket.emit("line_complete", message);
  })

  socket.on("exec_line", function(message){
    //console.log("Insied EXEC_LINE method");
    //console.log(message);
    socket.emit('exec_line', message);
    socket.broadcast.emit('line_number', message);
    //console.log("Echoing back the message");
  })
  socket.on('enable_step_mode', function(message){
    writeToEthercatClient("1 18 1");
    //console.log("Step Mode Enabled");
  })
  socket.on("step_mode", function(message){
    //writeToEthercatClient("1 18 1");
    //console.log("1 9 "+mp.getPulsesForDeg(message['position'])+" \n");
    writeToEthercatClient("1 9 "+mp.getPulsesForDeg(message['position'])+" \n");
  })

  socket.on("jog_mode", function(message){
    // console.log("Received Jog event");
    //console.log(message);
    var rpm_const = 1333.3334*90;
    var rpm = Math.round(rpm_const*mp.feedrate);
    console.log("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$RPM Value from JSON is " + mp.feedrate);
	console.log("Write RPM"+rpm);
    if(message.action == 1){
      //console.log("Write RPM");
	  mp.stopped = 0;
      
      //console.log("RPM write complete");
		
      //console.log("Will start movement");
      //var pos = mp.drive_x_ratio*360*10;
      //var cmd = "1 12 "+pos+" \n";
      ////console.log(cmd);
      if(message.dir == 1){
		  
        mp.setDirection(1);
       
		//If POTLIMIT set move until potlimit
		if(mp.potlimit>0 ){
		
			//Do nothing when both alaram present
			if(mp.alarm !=null && mp.alarm.indexOf("POT Limit Exceeded")>=0 && mp.alarm.indexOf("NOT Limit Exceeded")>=0){
				updateClients("reset_alert",{"data":""});
				return;
			}
			//writeToEthercatClient("1 16 3\n");
			
			var d = "1 15 "+rpm+"\n";
			writeToEthercatClient(d);			 
			//console.log("My jog Command"+d);
			
			
		
			var CMD = [1,2,mp.potlimit];    
			var dest_angle = mp.getDestinationAngle(CMD);
			
			
			//console.log("Angls with Pitch Error "+parseFloat(mp.getAngleWithPitchError(mp.cur_x_pos,dest_angle)));
			var dest_angle_with_pe = parseFloat(dest_angle)+parseFloat(mp.getAngleWithPitchError(mp.cur_x_pos,dest_angle));
			//dest_angle_with_pe = dest_angle;
			//console.log("Angles with PE Compenstation"+dest_angle_with_pe);
			var pulses = mp.getPulsesRequired(dest_angle_with_pe);

		 
				
			var cmd = "1 2 " + pulses + "\n";
			
			writeToEthercatClient(cmd);
			
			
		}
		else{
			//Set feedrate
			var d = "1 17 "+rpm+"\n";
			writeToEthercatClient(d);
			console.log("+++++++++++++++++++++++++++++++++++"+d);
		
			//Jog in  positive direction
			 
	         var cmd = "1 16 1\n";
	         writeToEthercatClient(cmd);
		}
      }
      else{
        mp.setDirection(-1);
        
		if(mp.notlimit>0 ){
		
			//Do nothing when both alaram present
			if(mp.alarm !=null && mp.alarm.indexOf("POT Limit Exceeded")>=0 && mp.alarm.indexOf("NOT Limit Exceeded")>=0){
				updateClients("reset_alert",{"data":""});
				return;
			}
		
			var d = "1 15 "+rpm+"\n";
			writeToEthercatClient(d);			 
			console.log("My jog Command"+d);
			
			
		
			var CMD = [1,2,(mp.notlimit-360)];    
			var dest_angle = mp.getDestinationAngle(CMD);
			
			
			//console.log("Angls with Pitch Error "+parseFloat(mp.getAngleWithPitchError(mp.cur_x_pos,dest_angle)));
			var dest_angle_with_pe = parseFloat(dest_angle)+parseFloat(mp.getAngleWithPitchError(mp.cur_x_pos,dest_angle));
			//dest_angle_with_pe = dest_angle;
			//console.log("Angles with PE Compenstation"+dest_angle_with_pe);
			var pulses = mp.getPulsesRequired(dest_angle_with_pe);

		 
				
			var cmd = "1 2 " + pulses + "\n";
			
			writeToEthercatClient(cmd);
		
		}else{
			
			//Set feedrate
			var d = "1 17 "+rpm+"\n";
			writeToEthercatClient(d);
			
			var cmd = "1 16 2\n";
			writeToEthercatClient(cmd);
			
		}
		
		
      }
      //console.log("Writing command ==> "+cmd);
      
    }else{
      //Stop movement immediately
      //console.log("Stopping Jog");
      writeToEthercatClient("1 16 3\n");
    }
  })
  socket.on("start_homing", function(data){
    //console.log("Starting Homing");
    EXEC_LINE = -1;
    if(clients.length > 0){
      //console.log("Homing options are :::");
      mp.initParams();
      //console.log("1 4 "+mp.drive_x_offset);
      writeToEthercatClient("1 4 "+mp.drive_x_offset+" \n");
    }
    else {
      //console.log("No connection to the ethercat service");
    }
    //socket.broadcast.emit("start_homing", data);
  })
  socket.on("stop_homing", function(data){
    //console.log("Homing Completed");
    socket.broadcast.emit("stop_homing", data);
  })
  socket.on("homing_complete", function(data){
    //console.log("Homing Offset movement completed");
    socket.broadcast.emit("homing_complete", data);
    mp.dest_x_pos = 0;
    updateClients("pos_data",{"data":mp.dest_x_pos});

  })
  socket.on("pos_data", function(data){
    socket.broadcast.emit("pos_data", data);
  })
  socket.on("destination_position", function(data){
    //console.log("Sending current postion");
    //console.log(data);
    socket.broadcast.emit("destination_position",data);
  })
  socket.on("exec_next_line", function(data){

    if(mp.alarm !== null && mp.alarm !== undefined && UNDER_MOVEMENT == true){
      //console.log("There are some errors. not exexuting");
      return;
    }
	console.log("DBG:: Wants to execute.. You decide.. Current State -> "+UNDER_MOVEMENT);
	if(UNDER_MOVEMENT == true){
		console.log("DBG::Value of under movement --> "+UNDER_MOVEMENT);
		return;
	}else{
		execute_line();
	    socket.broadcast.emit("exec_next_line",data);
	}
    //console.log("Sending Execute Next Line Command");
    //writeToEthercatClient("1 19 1\n");
  })
  socket.on("enable_ecs", function(data){
    //console.log("Sending Execute Next Line Command");
    writeToEthercatClient("1 20 1\n");
    socket.broadcast.emit("Enable ECS",data);
    mp.initParams();
  })
  socket.on("disable_ecs", function(data){
    //console.log("Sending Execute Next Line Command");
    writeToEthercatClient("1 21 1\n");
    socket.broadcast.emit("Disable ECS",data);
    mp.initParams();
  })
  socket.on("goToZero", function(){
    console.log("Move to zero now");
	if(mp.alarm !=null && mp.alarm.indexOf("POT Limit Exceeded")>=0 && mp.alarm.indexOf("NOT Limit Exceeded")>=0){
		//alert("Please Reset to Proceed
		updateClients("reset_alert",{"data":""});
		return;
    }
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
  })

//*********************************************latched code*************************************************

	socket.on("pot_hard_limit", function(data){
		  mp.alarm  = "POT Hard Limit Switch Latched";
		  
		
	})	
	
socket.on("not_hard_limit", function(data){
		  mp.alarm  = "NOT Hard Limit Switch Latched";
		
	})	


//*********************************************code ends here***********************************************
});
function resetCounters(){
  EXEC_LINE = 0;
}

//All file parser commands here
function execute_file(params, line_no){
	UNDER_MOVEMENT = false;
	console.log("My Ref Angle before execution --> "+mp.ref_angle);
	if(mp.ref_angle != 360 &&  mp.ref_angle != 0){
		mp.ref_angle = -1;
	}

  //console.log("Reset all state params first");
  if(clients.length == 0){
    //console.log("No clients connected");
    return;
  }
  if(line_no == -1){
    line_no = 0;
  }
  var client = clients[0];
  resetCounters()
  writeToEthercatClient("1 18 1");
  //console.log("PP mode Activated");
  MODE = params.mode;
  ECS = params.ecs;
  ECS = parseInt(ECS)
  MODE = parseInt(MODE)
  //console.log("----------------------------MODE value is" ,+MODE);
  //console.log("----------------------------ECS value is" ,+ECS);
  //console.log("Sending ECS Command");
  initializeProgram(params.file_name, line_no);
  /*setTimeout(function(){
    client.write("1 5 "+ECS+" \n");
    //console.log("Executing Program --> ",params.file_name);
    cflag1 = 1;
    initializeProgram(params.file_name);
  }, 500);*/

  }

function initializeProgram(file_name, line_no){
	mp.workoffsetflag = 0;
	mp.workoffset = 0;
    if(line_no === undefined || line_no === null || line_no.length == 0){
      line_no = 0;
    }else{
      line_no = parseInt(line_no);
    }
    UNDER_EXECUTION = true;
    mp.compileProgram(file_name);
    mp.initParams(); // Need this initialze default params
	console.log("MP Values are --> ");
	console.log(mp);
    //console.log("File Contents are ::");
    //console.log(mp.parsed_lines);
    //console.log("File Parse Completed");
    xdir = 1;
    //init_drive_settings();
    //console.log("Drive settings initialized");
    //console.log("Power on the drive");
    //Enable Start Program on the Drive. This is to flush the state in drive.
    // if(mp.ecs == 1){
    //   EXEC_LINE = line_no;
    //   return;
    // }

    // if(mp.cl_dl == 1){
    //   //Positive..
    //   writeToEthercatClient("1 38 0\n");
    // }else{
    //   writeToEthercatClient("1 8 0 \n");
    // }
	writeToEthercatClient("1 8 0 \n");
    setTimeout(function(){
      //console.log("Power ON COMMAND SENT....... 1 8 0");
      cflag2 = 1;

      setTimeout(function(){
        EXEC_LINE = line_no;
        //console.log("EXEC LINE VALUE --> "+EXEC_LINE);
        //console.log("EXEC_LINE --> "+mp.parsed_lines[EXEC_LINE]);
        emitAll("exec_line",{'line':EXEC_LINE});
		console.log("I'm inside settimeout.. ECS value is -->"+mp.ecs);
        if(mp.ecs == 0){
          execute_line();
        }
        //console.log("Emit Completed");
      },500);

    },500);
}

function emitAll(evt, m){
  for(var i=0; i< ui_clients.length; i++){
    ui_clients[i].emit(evt, m);
  }
}



/*G & M Codes command parser. Convert G & M Code to CIA 402 compliant command.
This method determines the pulses and drive id required.
After computation, the message is sent to the Ethercat server*/
function execute_line(){
  if(EXEC_LINE >= mp.parsed_lines.length || EXEC_LINE < 0){
    //console.log("Program End reached. Terminate now");
	g17flag = 0;
    UNDER_EXECUTION = false;
	emitAll("program_complete",{'status':'end'});
    return;
  }
//   if(EXEC_LINE == 0){
//       while((cflag1 == 0) &&(cflag2 == 0)){
//         ;
//       }
// }
  PULSE_STATUS = 1;
  console.log("DBG:: Setting True, since exec_line");
  UNDER_MOVEMENT = true;
  var line = mp.parsed_lines[EXEC_LINE];
  var lettr= line.substr(0,1);
  var len= line.length;
  console.log("Executing Line "+line);
  updateLineNumberInUI();
  EXEC_LINE++;
  console.log("Entered Execution stage");
  if(line == "G91"){
    //console.log("Linear mode. (Relative mode) activated.");
    MOVE_TYPE = LINEAR;
    mp.mode = "LINEAR";
	// mp.ref_angle = mp.cur_x_pos;
    execute_line();
    return;
  }
  else if (line.startsWith("G01"))
  {
    //console.log("Linear Interpolation Activated");
    var p=line.indexOf("F");
    fval=line.substr(p+1,line.length);
    //console.log("RPM received is "+fval);
    rpm = Math.round(1333.33*90*fval);
    var CMD= [1, 15, rpm];
    var c = CMD.join(" ");
    c = c+" \n";
    //console.log("Will execute command "+c);
    writeToEthercatClient(c);
    updateClients("pos_data",{"data":mp.dest_x_pos});
    setTimeout(function(){
      execute_line();
	  // UNDER_MOVEMENT = false;
    },500);
    //upload reg 6081 with the value of rpm.
    //console.log("Update RPM Complete");
    return;
  }
  else if (line=="G0")
  {
    //console.log("Rapid feed rate activated.");
    var CMD= [1, 15, 1200000];
    var c = CMD.join(" ");
    c = c+" \n";
    //console.log("Will execute command "+c);
    writeToEthercatClient(c);
    updateClients("pos_data",{"data":mp.dest_x_pos});
	console.log("DBG:: Setting false, since G0");
	UNDER_MOVEMENT = false;
    execute_line();
        // Upload the value of default rpm value in the 6081 register. (10,00,000)
    return;
  }
  else if (line == "G90") {
    //console.log("Absolute mode activated.");
    MOVE_TYPE = ABS;
    mp.mode = "ABS";
	console.log("DBG:: Setting false, since G90");
	UNDER_MOVEMENT = false;
    execute_line();
    return;
  }
  else if (line== "G68")
  {
    MOVE_TYPE= ABS;
    mp.shortest_path=true;
    MOVE_TYPE= ABS;
	console.log("DBG:: Setting false, since G68");
	UNDER_MOVEMENT = false;
    //console.log("shortest_path is "+ mp.shortest_path);
    //console.log("Shortest path activated.");
    execute_line();
    return;
  }
  else if (line== "G69")
  {
    mp.shortest_path=false;
    MOVE_TYPE= ABS;
	console.log("DBG:: Setting false, since G69");
	UNDER_MOVEMENT = false;
    //console.log("shortest_path is " + mp.shortest_path);
    //console.log("Shortest path deactivated.");
    execute_line();
    return;
  }
  else if(line == "G10")
  {
    loopval=loopval-1;
	console.log("DBG:: Setting false, since G10");
	UNDER_MOVEMENT = false;
    //console.log("Command G10 Received");
    if(loopval>0)
    {
      EXEC_LINE=curpos;
      
    }
	execute_line();
    return;
  }
  else if(line == "G53")
  {
    //console.log("Work offset is deactivated");
    mp.workoffsetflag=0;
	mp.workoffset = 0
	console.log("DBG:: Setting false, since G53");
	UNDER_MOVEMENT = false;
    execute_line();
  }
  else if (["G54", "G55", "G56", "G57", "G58"].indexOf(line) >= 0 ) {
	console.log("Found Work Offset.. "+line);
	mp.workoffsetflag = 1;
	mp.workoffset = mp[line.toLowerCase()];
	console.log("Work Offset enabled now.. Values is "+mp.workoffset.toString());
	console.log("DBG:: Setting false, since G54-58");
	UNDER_MOVEMENT = false;
	execute_line();
  }
  else if(lettr == "R")
  {
    tmp=parseInt(line.substr(1,len));
    loopval=tmp;
    //console.log("Looping started");
    curpos=EXEC_LINE;
	console.log("DBG:: Setting false, since R");
	UNDER_MOVEMENT = false;
    execute_line();
    return;
  }
  else if (lettr == "D")
  {
    delayval=parseFloat(line.substr(1,len));
	console.log("DBG:: Setting false, since D");
    setTimeout(function(){ UNDER_MOVEMENT = false; execute_line() }, delayval*1000);
    return;
  }
  else if(lettr == "F")
  {
    fval=parseInt(line.substr(1,len));
    //console.log("RPM received is "+fval);
    rpm=Math.round(1333.33*90*fval);
    if(rpm<=0){
      //console.log("Invalid Entry. Exiting");
      return;
    }
    var CMD= [1, 15, rpm];
    var c = CMD.join(" ");
    c = c+" \n";
    //console.log("Will execute command "+c);
    writeToEthercatClient(c);
    updateClients("pos_data",{"data":mp.dest_x_pos});
    setTimeout(function(){
	  console.log("DBG:: Setting false, since F");
	  UNDER_MOVEMENT = false;
      execute_line();
    },100);
    //upload reg 6081 with the value of rpm.
    //console.log("Update RPM Complete");
    return;
  }
  else if(line == "M30"){
    EXEC_LINE = mp.parsed_lines.length + 1;
	console.log("DBG:: Setting false, since M30");
	UNDER_MOVEMENT = false;
	for(var y=0;y<mp.parsed_lines.length;y++){
console.log("*****************" + mp.parsed_lines[y]);
	
		
	 }
	g17flag = 0;
	
    return;
  }
  else if(line == "M99"){
    console.log("M99 Mode activated");
    EXEC_LINE = 0;
	console.log("DBG:: Setting false, since M99");
	
	UNDER_MOVEMENT = false;
    execute_line();
    return;
  }else if(line == "G167S"){
    console.log("G17 Mode activated");
    g17flag = 1;
	exec_g167 = EXEC_LINE;
	console.log("DBG:: Setting false, since G17");
	UNDER_MOVEMENT = false;
    execute_line();
    return;
  }else if(line == "G167"){
    console.log("G17 End activated");
    g17flag = 2;
	exec_g167 = exec_g167 + 2;
	console.log("DBG:: Setting false, since G17");
	
	UNDER_MOVEMENT = false;
    execute_line();
    return;
  }
  else if(lettr == "A"){
    sock.movement = true;
	console.log("DBG:: Setting True, since A");
	UNDER_MOVEMENT = true;
    moveval=parseFloat(line.substr(1,len));
	
	/*if(g17flag != 0){
		rval = moveval;
		execute_line();
		return;
	}*/
if(moveval == mp.ref_angle && MOVE_TYPE == "ABS"){
console.log("\n\n----IN ABS Not moving since it's already in expected position..---\n\n");
execute_line();
return;
}

    console.log("Command received A"+moveval);
    if(moveval == 0){
      fs.writeFileSync('./direction_cache', 1);
    }
    //console.log("Xdir value is "+xdir);

    mp.dest_x_pos = moveval;
    var CMD = mp.parse_command(line);
    CMD[2] = moveval;
    var dest_angle = mp.getDestinationAngle(CMD);
    console.log("Destination Angle --> "+dest_angle);
    /*if(mp.potflag)
    updateClients("POTLIMIT",{"data":"POT LIMIT EXCEEDED. CANNOT MOVE"});
    if(mp.notflag)
    updateClients("NOTLIMIT",{"data":"NOT LIMIT EXCEEDED. CANNOT MOVE"});*/

    var d1 = fs.readFileSync('./direction_cache', 'utf-8');
    //console.log("Angls with Pitch Error "+parseFloat(mp.getAngleWithPitchError(mp.cur_x_pos,dest_angle)));
    var dest_angle_with_pe = parseFloat(dest_angle)+parseFloat(mp.getAngleWithPitchError(mp.cur_x_pos,dest_angle));
    //dest_angle_with_pe = dest_angle;
    //console.log("Angles with PE Compenstation"+dest_angle_with_pe);
    var pulses = mp.getPulsesRequired(dest_angle_with_pe);

    //Check for pot/not limit
    //console.log("-------------------");
    //console.log(mp.potlimit);
    //console.log(pulses);
    //console.log(dest_angle);
    //console.log("-------------------");
    if(mp.potlimit > 0 && pulses > 0){
      var balance = mp.potlimit - mp.cur_x_pos;
      if(balance < 0){
        balance = (360+mp.potlimit) - mp.cur_x_pos;
      }
      var traverse_angle = parseFloat(pulses/20000);
      if(traverse_angle > balance){
        mp.alarm = "Destination breaches POT Limit";
        return;
      }
    }else if (mp.notlimit > 0 && pulses < 0) {
      xdir = 0;
      var balance = mp.cur_x_pos - (360-mp.notlimit);

      if(balance < 0){
        balance = mp.cur_x_pos + mp.notlimit;
      }
      var traverse_angle = parseFloat((pulses*-1)/20000);
      if(traverse_angle > balance){
        mp.alarm = "Destination breaches NOT Limit";
        return;
      }
    }else {
      mp.alarm = null;
    }

    prev_dir = xdir;
    console.log("Pulses to move --> "+pulses);
    CMD[2] = pulses;
    if(pulses < 0){
      CMD[1] = 3
      CMD[2] = pulses * -1;
      PULSE_RECEIVED = 0; //REset the backlash dependency on ECS
      xdir = 0;
    }else{
      PULSE_RECEIVED = 0; //REset the backlash dependency on ECS
      xdir = 1;
    }
    //If direction_cache == 1 && direction == 1, then factor is required
    //console.log("***********");
    //console.log(d1);
    //console.log("***********");
    if(d1 <= 0 && xdir == 1){
      //Change fron neg to positive
      dir_change = 1;
      //fs.writeFileSync('./direction_cache', 0);
    }else if(xdir != prev_dir){
      dir_change = 1;
      //fs.writeFileSync('./direction_cache', 1);
    }else {
      /*if(xdir == 1){
        //If negative continue the same backlash value. If positive, then remove the backlash value
        fs.writeFileSync('./direction_cache', 0);
      }*/
      dir_change = 0;
      //fs.writeFileSync('./direction_cache', 0);
    }


    var c = CMD.join(" ");
    c = c+"\n";
    console.log("wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwWill execute command "+c);
    writeToEthercatClient(c);
    console.log("*****************");
    console.log(mp.dest_x_pos);
    console.log("*****************");
    updateClients("pos_data",{"data":mp.dest_x_pos});
	
	return;
  }
  
  else if(lettr == "P"){
console.log("*************888888888888888888888888888888888888****");
	g17flag = 2;
	exec_g167 = exec_g167 + 2;
	console.log("DBG:: Setting false, since G17");
	UNDER_MOVEMENT = false;
    execute_line();
	
    return;
  }
}
function updateLineNumberInUI(){
  for(var i=0; i<ui_clients.length; i++){
    //console.log("Pushing Line number "+EXEC_LINE+" to --> "+i);
    if(g17flag==2){
		exec_g167++;
	}
	if(g17flag != 0){
		ui_clients[i].emit("line_number", {line: exec_g167});
	}else{
		ui_clients[i].emit("line_number", {line: EXEC_LINE});
	}
	
  }
}
//getPEComponstationAt
function updateClients(evt, message){
 
  for(var i=0; i< ui_clients.length; i++){
    if(evt == "pos_data" && xdir == 0){
      message.data = mp.dest_x_pos+mp.drive_backlash;
      ui_clients[i].emit(evt, message);
	  
		
    }else {
      ui_clients[i].emit(evt, message);
    }
	
	

  }
}

setTimeout(function(){
	if(clients.length == 0){
		console.log("No Connection after 10 mins. Hence resetting it..");
		updateClients('ETH_DOWN', "Ethercat communication down...");
	}
}, 10000)

