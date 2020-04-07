var fs = require('fs');
var home_path = "/home/pi/v3";
var path = home_path+"/machine_code";
var settings_path = home_path+"/settings";
var pitchmove;

//Settings and constants mapping..
var obj = {
  file_contents : [],
  parsed_lines : [],
  drive_x_offset : 0,
  feedrate : 0,
  dir_change:0,
  drive_x_ratio : 20000,
  mode : 'ABS',
  shortest_path : false,
  workoffsetflag:0,
  potflag :0,
  notflag : 0,
  workoffset: 0,
  drive_backlash : 0,
  ref_angle: -1,
  cur_x_pos : undefined,
  cur_y_pos : undefined,
  dest_x_pos : undefined,
  dest_y_pos : undefined,
  pitch_error : [],
  mid : 0,
  stopped : 1,
  timing : 500,
  factor_backlash : 0,
  error_codes:{
    0 : "No Alarms",
    11: "Inaccurate ESM demand error protection",
    12: "ESM undefined request error protection",
    13: "Bootstrap requests error protection",
    14: "Over-current protection",
    15: "Over-heat protection",
    16: "Over-load protection",
    18: "Over re-generation load protection",
    21: "Encode communication error",
    23: "Encode communication data protection error",
    24: "Position deviation excess protection",
    25: "Hybrid deviation excess protection",
    26: "Over-Speed Protection",
    27: "Absolute clear protection",
    28: "Limit of pulse replay error protection",
    29: "Deviation counter overflow protection 2",
    30: "Safety detection",
    33: "Overlaps allocation error 1 protection",
    34: "Software limit protection",
    36: "EEPROM parameter error protection",
    37: "EEPROM check code error protection",
    38: "Over-travel inhibit input protection 1",
    40: "Absolute counter over error protection",
    41: "Safety detection",
    42: "Absolute over-speed error protection",
    43: "Incremental encoder initialization error protection",
    44: "Absolute single turn counter error protection or Incremental signal turn counter error protection",
    45: "Absolute multi-turn counter error protection or Incremental multi-turn counter error protection",
    47: "Absolute status error protection",
    48: "Incremental encoder Z-phase error protection",
    49: "Incremental encoder CS signal error protection",
    45: "Incomplete PLL error protection",
    27: "PDO watchdog error protection",
    50: "PLL error protection",
    44: "Synchronization signal error protection",
    53: "Synchronization cycle error protection",
    22: "Mailbox error protection",
    31: "PDO watchdog error protection",
    48: "DC error protection",
    40: "SM event mode error protection",
    29: "SyncManager2/3 error protection",
    36: "TxPDO assignment error protection",
    37: "RxPDO assignment error protection",
    80: "ESM unauthorized request error protection",
    81: "Synchronization cycle error protection",
    85: "TxPDO assignment error protection",
    87: "Hardware Emergency pressed",
    88: "ESM requirements during operation error protection",
    98: "Clamp failed",
    99: "Declamp failed",
    96: "POT Limit Exceeded",
    97: "NOT Limit Exceeded",
  },
  //This is to reset the direction. This is needed to ignore backlash values.
  resetDirection : function(){
    fs = require('fs');
    try{
        var contents = fs.readFileSync('./direction_cache', 'utf-8');
    }catch(e){
        fs.writeFileSync('./direction_cache', 0);
        var contents = fs.readFileSync('./direction_cache', 'utf-8');
    }
    console.log(contents);
  },

  //Method to determine if the movement needs backlash..
  isBacklashRequired : function(dest_angle){
    //dir -> +1 / -1. +1 for +ve direction, -1 for -ve direction
    console.log("\n\n((((((((MODE --> ))))))))", this.mode);
    var dir = (dest_angle >= 0) ? 1 : -1;
    var contents = 1;
    try{
        contents = fs.readFileSync('./direction_cache', 'utf-8');
    }catch(e){
      fs.writeFileSync('./direction_cache', contents);
      return 0;
    }

    if(this.mode == "ABS"){
      if(dir == -1){
        this.factor_backlash = 1;
      }else{
        this.factor_backlash = 0;
      }
      if (contents == dir) {
        console.log("File Contents are --> "+contents);
        console.log("Direction is --> "+dir);
        console.log("$$$$$$$$$Direction is same or in +ve. Not doing anything$$$$$$$$\n\n\n");
        this.dir_change = 0;
      }
      else{
        console.log("File Contents are --> "+contents);
        console.log("Direction is --> "+dir);
        console.log("$$$$$$$$$$Change in direction detected.$$$$$$$$$$$$$\n\n\n\n");
        this.dir_change = 1;
        fs.writeFileSync('./direction_cache', dir);
      }
      return (dir == -1) ? dir : 0;
    }else{
      console.log("Inside linear condition");
      console.log("Contents --> "+contents+", Direction --> "+dir);
      if(dir == -1){
        this.factor_backlash = 1;
      }else{
        this.factor_backlash = 0;
      }
	  if (contents == dir) {
        console.log("File Contents are --> "+contents);
        console.log("Direction is --> "+dir);
        console.log("$$$$$$$$$Direction is same or in +ve. Not doing anything$$$$$$$$\n\n\n");
        this.dir_change = 0;
      }
      else{
        console.log("File Contents are --> "+contents);
        console.log("Direction is --> "+dir);
        console.log("$$$$$$$$$$Change in direction detected.$$$$$$$$$$$$$\n\n\n\n");
        this.dir_change = 1;
        fs.writeFileSync('./direction_cache', dir);
      }
      return (dir == -1) ? dir : 0;
	  // if(dir == -1){
		//   fs.writeFileSync('./direction_cache', dir);
		//   this.dir_change = 1;
		//   return -1;
	  // }else{
		//   fs.writeFileSync('./direction_cache', 0);
		//   this.dir_change = 0;
		//   return 0;
	  // }
	  // return -1;
      // if (contents == dir) {
      //   console.log("$$$$$$$$$Direction is same or in +ve. Not doing anything$$$$$$$$\n\n\n");
      //   return 0;
      // }else{
      //   console.log("There is a change in direction");
      //   fs.writeFileSync('./direction_cache', dir);
      //   return dir;
      // }

    }
  },
  //Store direction
  setDirection : function(dir){
    //This is required for -ve direction in Jog mode..
    if(dir == 1){
      this.factor_backlash = 0;
      this.direction = 1;
    }else{
      this.factor_backlash = 1;
      this.direction = -1;
    }
    console.log("Direction is --> "+this.direction);
  },

  //Basic syntax checker for G & M Codes..
  compileProgram : function(file_name){
    var tmp_file_contents = fs.readFileSync(path+"/"+file_name).toString().split("\n");
    var mode2= "ABS";
    var g16mode= false;
    var parsed_lines2=[];
    var pcommand,pval,command2,xdeg,c,d,movecount,i,j
    this.file_contents = [];
    for(var i=0; i<tmp_file_contents.length;i++){
      if(tmp_file_contents[i].length > 0){
        this.file_contents.push(tmp_file_contents[i]);
      }
    }
    this.parsed_lines = [];
    for(var i=0; i<this.file_contents.length; i++){
      var line = this.file_contents[i]
      line = this.parseLine(line)
      this.parsed_lines.push(line)
      commands = line.split(" ")
      console.log(commands)
      command = commands[0]
      console.log(command);
      value = undefined;
      if(commands.length > 1){
        value = commands[1]
      }

      if( !command.startsWith("G") &&
          !command.startsWith("M") &&
          !command.startsWith("R") &&
          !command.startsWith("F") &&
          !command.startsWith("A") &&
          !command.startsWith("Y") &&
          !command.startsWith("P") &&
          !command.startsWith("D")){
            console.log("Invalid Command '%s', line number '%d'" % (line, i+1));
            return;
          }
          if(command.startsWith("G91"))
          {
            mode2= "INC";                 //G91(incrermental mode) is activated.
          }
          if(command.startsWith("G90")){
            mode2= "ABS";                 //G90(Absolute mode) is activated.
          }
          if (command.startsWith("G16") || command.startsWith("G17")){
            if(mode2!="INC"){
              console.log("G16 and G17 macro can be used only for Incremental mode.");
              return;
            }
          }
      }
      console.log("Validation complete. Now entering the Editing phase\n");
      console.log(this.parsed_lines);
      var n = this.parsed_lines.length;
      for(var i=0; i<n; i++){
            parsed_lines2=[];
            command2=this.parsed_lines[i];
            if (command2.startsWith("G16")){
              pcommand=this.parsed_lines[i+1];
              console.log("P Command Received is "+ pcommand);
              pval=parseInt(pcommand.substr(1,pcommand.length));
              xdeg=360/pval;
              pcommand= "A"+xdeg;
              console.log("Will update the Commands by adding "+Math.abs(pval) +"values of " +pcommand);
              for(var j=0;j <Math.abs(pval);j++){
                parsed_lines2[j]=pcommand;}
                console.log("Array to be added is "+parsed_lines2);
              this.parsed_lines.splice.apply(this.parsed_lines, [i,2].concat(parsed_lines2));
              n=this.parsed_lines.length;
              }
              if (command2.startsWith("G17")){
                pcommand=this.parsed_lines[i+2];
                pval=parseInt(pcommand.substr(1,pcommand.length));
              	xcommand=this.parsed_lines[i+1];
                if(!(pcommand.startsWith("P") && xcommand.startsWith("A")))
                  {
                    console.log("Invalid order of writing commands.");
                    return;
                  }
                console.log("P Command Received is "+ pcommand);
                console.log("A Command Received is "+ xcommand);
                xval=parseFloat(xcommand.substr(1,xcommand.length));
                xdeg=xval/pval;
                xcommand= "A"+xdeg;
                console.log("Will update the Commands by adding "+Math.abs(pval) +"values of " +xcommand);
              for(var j=0;j <Math.abs(pval);j++){
                parsed_lines2[j]=xcommand;}
                this.parsed_lines.splice.apply(this.parsed_lines, [i,3].concat(parsed_lines2));
                n=this.parsed_lines.length;
                }
            }
          console.log("Updated Array is "+ this.parsed_lines);
          console.log("Number of lines in the New updated array is "+n);
        },
    parseLine : function(line){
    line = line.trim();
    line = line.toUpperCase();
    line = line.replace("\n","")
    line = line.replace(";","")
    return line
  },

  //Get the Drive ID based on axis label..
  getAxisIndex : function(axis){
    if (axis in ["y","Y","b","B"]){
      return 2;
    }
    else{
      return 1;
    }
  },

  //Method to determine Rapid Feed..
  isRapidFeedCommand : function(cmd){
    if (cmd in ["G0","G0X","G0Y"]){
      return true;
    }
    return false;
  },

  parse_command : function(cmd){
    cmd = cmd.replace("A","");
    count = parseInt(cmd);
    var CMD = [1,2,count];
    /*if (count < 0){
      CMD[1] = 3;
      CMD[2] = count * -1;
    }*/
    return CMD;
  },
  //NOT Used..
  getCommandPulses : function(){
    return 500000;
  },

  //Get Display position from pulses received.
  getCurrentPosition : function(pos){
    var p = pos-this.drive_x_offset;
    p = p/20000;
    p = p%360;
    if(p<0)
    p=p+360;
    p=parseFloat(p.toFixed(3));
	// var p1 = Math.round(p);
	// if((p1-p) < 0.003){
	// 	p = p1;
	// }
    return p;
  },

  //Convert Position to Pulses..
  getPulsesRequired : function(pos){
    console.log("Inside getPulses required for angle --> "+pos);
    var val = Math.round(pos*20000);
    console.log("\n\n\nVALUE BEFORE BACKLASH\n------------------");
    console.log(val);
    val = val + this.drive_backlash*(this.isBacklashRequired(val)*this.drive_x_ratio);
    console.log("\n\n\nVALUE AFTER BACKLASH\n------------------");
    console.log(val);
    console.log("Pulses required are", val);
    return val;
  },

  //Get final angle with pitche error compensation..
  getAngleWithPitchError:function(cur, dest){
    var angle = null;
    if(isNaN(this.dest_x_pos)){
      return 0;
    }
    if(dest < 0){
      //Rotating in reverse direction
      console.log("Rotating in reverse direction");
      angle = this.getPEComponstationAt(this.dest_x_pos, 0);
    }else{
      //Rotating in positive direction
      angle = this.getPEComponstationAt(this.dest_x_pos, 1);
    }
    //console.log("Angle after PE Compensation -->"+angle);
    return angle;
  },

  //Determine the pitch error compensation for a destination angle..
  getPEComponstationAt : function(angle, dir){
    try{
      var index = Math.abs(parseInt((this.dest_x_pos)/10));
      index = index-1;
      if(index<0){
        return 0;
      }
      return this.pitch_errors[index];
    }catch(e){
      console.log(e);
      return 0;
    }
  },

  //Get Display value in UI ignoring the pitch error compensation..
  getPEComponstationAtUI : function(cur_angle){
    try{
      if(isNaN(cur_angle)){
        return 0;
      }
      cur_angle = Number(Math.round(cur_angle+'e2')+'e-2'); // 1.01
      //console.log("MP Rounded Cur angle --> "+cur_angle);
      var index = Math.abs(parseInt((cur_angle)/10));
      //console.log("MP Curr Angle --> "+cur_angle);
      //console.log("MP Index -->"+index);
      if(index<0){
        return 0;
      }
      index = index-1;
      if(index < 0){
        return 0;
      }
      // console.log("\n\nINDEX is --> "+index+"\n\n");
      // console.log(this.pitch_errors[index]);
      return this.pitch_errors[index]*-1;

    }catch(e){
      console.log(e);
      return 0;
    }
  },

  //Calculate destination angle based on the command.
  getDestinationAngle : function(cmd){
    var my_pos = parseFloat((this.cur_x_pos).toFixed(3));
	my_pos = my_pos % 360;
    this.potflag = 0;
    this.notflag = 0;
    console.log("Mode --> "+this.mode);
    console.log("Command -->");
    console.log(cmd);
    console.log("Current Position is  -->"+my_pos);
    if(this.mode == "ABS"){
      console.log("2- Absolute mode");
      if(this.shortest_path == false){
		  var dest = cmd[2];
		  console.log("2-Shortest path is false");
		  if(this.workoffsetflag ==1){
				  dest = (dest+this.workoffset)%360;
				  console.log("Work ofset is ",this.workoffset);
				  console.log("My_pos is now ", my_pos);
				  console.log("Destination is now ", dest);
		  }
		  my_pos = my_pos- cmd[2] ;
		  my_pos=360-my_pos;
		  if(Math.abs(my_pos) > 360){
			my_pos = my_pos % 360;
		  }
		  if(cmd[2]<0){
			  my_pos=(my_pos-360);
			  if(my_pos<=-360)
			  my_pos=my_pos+360;
		  }
		  console.log("Destination Postion --> "+my_pos);
		  this.dest_x_pos = dest;
      }
      else{
        console.log("2-Shortest path is true.");
		console.log("-------------Shortest Path Log------------");
		var dest = cmd[2];
		if(this.workoffsetflag ==1)
        {
		  dest = (dest+this.workoffset)%360;
          console.log("Work ofset is ",this.workoffset);
          console.log("My_pos is now ", my_pos);
		  console.log("Destination is now ", dest);
        }

		console.log("MyPosition -> "+my_pos);
		console.log("CMD[2] -> "+dest);
        my_pos = dest - my_pos;
        if(my_pos > 0){
          if(my_pos > 180){
            my_pos = (360 - my_pos) * -1;
          }
        }else {
          if(Math.abs(my_pos) > 180){
            my_pos = 360 - Math.abs(my_pos);
          }
        }
		console.log("Calculated MyPOS -> "+my_pos);
      }
      console.log("Destination Postion --> "+my_pos);
	  this.dest_x_pos = dest;
	  this.ref_angle = dest;
    }
    else{
      console.log("2- Relative mode");
      my_pos = cmd[2];
	  //Correct cumulative error. Last position was known in ref_angle. Check if cur_x_pos = ref_angle.
	  // If not add delta to my_pos..
	  console.log("---------------------------------");
	  console.log(this.ref_angle);
	  if(this.ref_angle !== undefined && this.ref_angle !== null && this.ref_angle != -1){
		  console.log("I've previos details.. Correcting..");
		  console.log("Current Position");
		  console.log(this.cur_x_pos);
		  console.log("MyPos before correction --> ");
		  console.log(my_pos);
		  console.log("Reference position");
		  console.log(this.ref_angle);
		  this.ref_angle = this.ref_angle % 360;
		  this.cur_x_pos = this.cur_x_pos % 360;
		  console.log(this.ref_angle);

		  if(this.cur_x_pos != this.ref_angle){
              console.log("Ref Angle -> "+this.ref_angle+", Cur Pos -> "+this.cur_x_pos);
			  // var val = this.getDistanceDifference(this.ref_angle, this.cur_x_pos);
              // console.log("After::Ref Angle -> "+this.ref_angle+", Cur Pos -> "+this.cur_x_pos);

              var diff1 = 360 - this.cur_x_pos;
              var diff2 = Math.abs(this.ref_angle - this.cur_x_pos);

              if(diff1 > diff2){
                my_pos = this.ref_angle + my_pos - (this.cur_x_pos);
              }else{
                my_pos = (this.ref_angle+360) + my_pos - (this.cur_x_pos);
              }
              console.log(diff1);
              console.log(diff2);
			  console.log("My POS at the final stage..");
			  var my_pos_before = my_pos;
			  console.log(my_pos);
              my_pos = my_pos % 360;
			  console.log(my_pos);
			  //There was an issue with +360 and -360. 0.1 is the threshold error
			  //value due to powere off and on. Ideally it's around 0.003 now.
			  //Need to tweak this value if required.
			  if(my_pos < (my_pos_before-0.1)){
				my_pos = my_pos_before
			  }

              // my_pos = (my_pos - val) %360;

		  }
		  my_pos = Math.round(my_pos * 1000) / 1000;
		  console.log("MyPos after correction --> ");
		  console.log(my_pos);
	  }
	  var dest = this.cur_x_pos + my_pos;
	  if(this.ref_angle != -1){
		  dest = this.ref_angle + cmd[2];
	  }

      console.log("My DEST -> "+dest);
      console.log("dest is "+dest);
	  if(dest < 0){
	  	this.ref_angle = 360 + dest;
	  }else{
	  	this.ref_angle = dest;
		// this.ref_angle = dest%360;
	  }

	  this.dest_x_pos = dest;
	  console.log(this.dest_x_pos);
	  if(dest > 0){ //Some threshold in case of backlash and other error..
	  	this.dest_x_pos = dest % 360;
	  }else{
	    this.dest_x_pos = (dest+360) % 360;
	  }
    }

	my_pos= my_pos.toFixed(3);
    my_pos=parseFloat(my_pos);

    console.log("my_pos value is "+my_pos);

    // var dest = parseFloat(this.cur_x_pos) + my_pos;
    // console.log("Dest before parse: "+dest);
    // dest = parseFloat(parseFloat(dest).toFixed(3));

    this.dest_x_pos = this.dest_x_pos.toFixed(3);
    console.log("1-----------Dest pos is ",this.dest_x_pos);

    console.log(" Mode is "+this.mode+" Delta Angle "+my_pos+" Destination Position "+this.dest_x_pos+" Referece angle -> "+this.ref_angle);
	console.log("---------------------------------");
    return my_pos;
  },

  getDistanceDifference : function(ref, current){
      var phi = Math.abs(ref - current) % 360;       // This is either the distance or 360 - distance
      var distance = phi > 180 ? 360 - phi : phi;
      distance=parseFloat(distance.toFixed(3));
      console.log(distance);
      return distance;
  },

  //Initilize settings by reading the settings file.
  initParams  : function(){
    var tmp_file_contents = fs.readFileSync(settings_path+"/settings.json").toString();
    console.log("Parsed Contents are::");
    console.log(tmp_file_contents);
    var json_data = JSON.parse(tmp_file_contents);
    var keys = ["homing_offset", "back_lash","work_offset"];
    try{
      for(var i=0; i<keys.length; i++){
        json_data.X[keys[i]] = parseFloat(json_data.X[keys[i]]);
      }
    }catch(err){
      console.log("error while parsing");
    }

    //console.log("Ekey is "+ekey);
    this.drive_x_offset = parseFloat(json_data.X.homing_offset) * this.drive_x_ratio;
    this.drive_backlash = json_data.X.back_lash;
    this.pitch_errors = json_data.X.pitch_error;
    this.potlimit = parseFloat(json_data.X.pot);
    this.notlimit = parseFloat(json_data.X.not);
    this.feedrate = parseFloat(json_data.X.jog_feed);
    this.threshold = parseInt(this.feedrate*1.7);
    this.workoffset = 0;
	this.fin_signal = parseFloat(json_data.X.fin_signal);
	this.g54 = parseFloat(json_data.X.g54);
	this.g55 = parseFloat(json_data.X.g55);
	this.g56 = parseFloat(json_data.X.g56);
	this.g57 = parseFloat(json_data.X.g57);
	this.g58 = parseFloat(json_data.X.g58);
    this.cldl_timing = parseInt(json_data.X.cldl_timing);
	this.IS_OFFSET_ENABLED = 0;
	this.ENABLED_OFFSET = 0;
    this.mid = 0;
    if(this.potlimit !=0 || this.notlimit !=0){
      this.mid = (360-this.notlimit+this.potlimit)/2;
    }
    this.mode = "ABS";
    //Setting all boolean status in the app
    try{
      this.ecs = json_data.X.ecs;
    }catch(e){
      console.log("ECS is not available");
      this.ecs = 0;
    }

    try{
      this.timing = parseInt(json_data.X.timing);
    }catch(e){
      console.log("ECS is not available");
      this.timing = 5000;
    }

    try{
      this.motor_dir = json_data.X.motor_dir;
    }catch(e){
      console.log("Motor direction is not available");
      this.motor_dir = 0;
    }
    try{
      this.cl_dl = json_data.X.cl_dl;
    }catch(e){
      console.log("Clamp/Declamp is not available");
      this.cl_dl = 0;
    }
    try{
      this.home_dir = json_data.X.home_dir;
    }catch(e){
      console.log("Home Direction is not available");
      this.home_dir = 1;
    }
    try{
      this.fin_signal = json_data.X.fin_signal;
    }catch(e){
      console.log("Home Direction is not available");
      this.fin_signal = 0;
    }
	console.log("Finish Signal --> "+this.fin_signal);
  },
  getPulsesForDeg : function(val){
    return val * this.drive_x_ratio;
  }
}
//1.Check if file exists and backlash needs to be factored..
try{
  var contents = fs.readFileSync('./direction_cache', 'utf-8');
  contents = parseInt(contents);
  if(contents < 1){
    obj.factor_backlash = 1;
  }
  else{
    obj.factor_backlash = 0;
  }
}catch(e){
  console.log(e);
  obj.factor_backlash = 0;
}

obj.fb = obj.factor_backlash;

module.exports = obj;

