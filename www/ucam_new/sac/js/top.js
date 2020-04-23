var sdate = new Date().getTime();
var host="192.168.202.1";

$(document).ready(function(){
$("#header").load("header.html?date="+sdate);
});
$(document).ready(function(){
$("#positionA").load("actualpositionA.html");
});
$(document).ready(function(){
$("#positionB").load("actualpositionB.html");
});
$(document).ready(function(){
$("#fault").load("fault.html");

var socket = io.connect('http://'+host+':9090');



socket.on("ETH_DOWN", function(data){
  
     // console.log("Ethercat Down");
		 document.getElementById("img6").src ="images/on.png";
		
		 var ob2=document.getElementById("myBtn");
         ob2.style.backgroundColor= "#FF0000";
		 var ob3=document.getElementById("ethercatbtn");
         ob3.style.backgroundColor= "#FF0000";
		 document.getElementById("messagedisplay").innerHTML = "Communication down. Please check connection and restart";
    
  });

  socket.on("ETH_UP", function(data){
     // console.log("Ethercat Up");
	
		 document.getElementById("img6").src ="images/off.png";
		  
         var ob2=document.getElementById("myBtn");
         ob2.style.backgroundColor= "#35d12f";
		var ob3=document.getElementById("ethercatbtn");
         ob3.style.backgroundColor= "#35d12f";
	     document.getElementById("messagedisplay").innerHTML = "Communication link is up..";
     // alert("Ethercat Up");
  });






});
$(document).ready(function(){
$("#menu").load("menu.html");
});



	
						