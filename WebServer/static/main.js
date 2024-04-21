/*
	This file contains the main JavaScript code for the web interface of the robot.
	Much of the basis for this code came from Rui Santos's ESP32 tutorials: https://randomnerdtutorials.com/projects-esp32/
	Modifications made by Collin Schofield, with assistance from Github Copilot.
*/

//=======================================================
// Global variables
//=======================================================

// var botPath = `ws://${window.location.hostname}/ws`;
var botPath = `ws://192.168.137.240:80/ws`;
var websocket;

window.addEventListener('load', onload);// Initialize the websocket when the page is loaded

const outChartEnum = Object.freeze({// Enum for the series's of the output chart
	pOut: 0,
	p2Out: 1,
	dOut: 2,
	iOut: 3,
	out: 4
});


var distValsIN = [];// Array to store the sensor values for the chart
var distValsINWithSpacers = [];// Array to store the sensor values for the chart, with spacers between sensors
// Charts for the sensor values, error values, and output values
var lineDistanceChart;
var distanceChart;


const sensorWidth = 15;// Width of the sensor in degrees
const numSensors = 8;// Number of sensors
const numFrontSensors = 7;// Number of front sensors
const numRearSensors = numSensors - numFrontSensors;// Number of rear sensors
const frontSpace = (180 - (numFrontSensors * sensorWidth))/(numFrontSensors - 1);// Space for the front sensors
const rearSpace = (180 - (numRearSensors * sensorWidth))/(numRearSensors + 1);// Space for the rear sensors



//=======================================================
// WebSocket Functions
//=======================================================

function onload(event) {
	initWebSocket();
	setTimeout(prepCharts, 1000);
}



// Function to initialize the WebSocket connection
function initWebSocket() {
	console.log('Trying to open a WebSocket connection…');
	websocket = new WebSocket(botPath);
	websocket.onopen = onOpen;
	websocket.onclose = onClose;
	websocket.onmessage = onMessage;
}



// Actions to take when the WebSocket connection is opened
function onOpen(event) {
	console.log('Websocket Connection opened');
	document.getElementsByClassName('topnav')[0].style.backgroundColor = 'green';
	// setTimeout(getState, 1000);
}



// Actions to take when the WebSocket connection is closed
function onClose(event) {
	console.log('Websocket Connection closed');
	setTimeout(initWebSocket, 2000);
	document.getElementsByClassName('topnav')[0].style.backgroundColor = 'red';
}



// Function that receives the message from the ESP32 with the readings
function onMessage(event) {
	try {
		tmp = JSON.parse(event.data);
		if(tmp.message == undefined){
			console.log(tmp);// Print the whole JSON of recieved data in the console, if it is not just a message
		}
	}catch(e){// If the JSON parsing fails, print the error and the data in the console
		console.log("Error parsing JSON: " + e);
		console.log(event.data);
	}
	console.log(event.data);
	var receivedObj = JSON.parse(event.data);
	var keys = Object.keys(receivedObj);

	for (var i = 0; i < keys.length; i++){// Loop through all keys in the received object
		var key = keys[i];
		if(key == "message"){// If the key is message, print the message in the console
			console.log(receivedObj[key]);
			continue;
		}else{// Otherwise, update the value of each element in the HTML with the id of the key
			// Check for special keys
			if(key == "distIN"){// If the key is distIN, update the values of the sensor values in the JavaScript array
				distValsIN = receivedObj[key];
				lineDistanceChart.series[0].setData(distValsIN);
				continue;
			}

			try{// Try to update the element with the id of the key
				document.getElementById(key).innerHTML = receivedObj[key];
			}catch(e){
				console.log("Error updating element with id " + key + ": " + e);
			}
		}
	}
}





//=======================================================
// Other Functions
//=======================================================

function prepCharts(){
	// distanceChart = new Highcharts.Chart({
	// 	chart: {
	// 		type: 'variablepie'
	// 	},
	// 	title: {
	// 		text: 'ROund Distance Chart',
	// 		align: 'left'
	// 	},
	// 	// tooltip: {
	// 	// 	headerFormat: '',
	// 	// 	pointFormat: '<span style="color:{point.color}">\u25CF</span> <b> {point.name}</b><br/>' +
	// 	// 		'Area (square km): <b>{point.y}</b><br/>' +
	// 	// 		'Population density (people per square km): <b>{point.z}</b><br/>'
	// 	// },
	// 	plotOptions: {
	// 		variablepie: {
	// 			startAngle: -90,
	// 			dataLabels: {
	// 				enabled: false,
	// 			}
	// 		}
	// 	},
	// 	series: [{
	// 		minPointSize: 1,
	// 		innerSize: '20%',
	// 		zMin: 0,
	// 		name: 'Distance (in) (round graph)',
	// 		borderRadius: 5,
	// 		yAxis:{
	// 			title: {
	// 				text: "Angle (degrees)"
	// 			},
	// 			labels: {
	// 				format: '{value} deg'
	// 			},

				
	// 		},
	// 		series:[{
	// 			name: 'Distance (in)',
	// 			data: distValsINWithSpacers
	// 		}]
	// 	}]
	// });




	lineDistanceChart = new Highcharts.Chart({
		chart: {
			renderTo: 'chart-distance-line',
			type: 'spline',
			animation: false
		},
		title: { text: 'Distance (in)' },
		xAxis:{
			categories: [0,1,2,3,4,5,6,7]
		},
		yAxis: {
			title: {
				text: 'Distance'
			},
			labels: {
				format: '{value} in'
			},
			min: 0,
			max: 200
		},
		plotOptions: {
			spline: {
				marker: {
					radius: 4,
					lineColor: '#666666',
					lineWidth: 1
				}
			},
		},
		series: [{
			name: 'Distance Readings',
			marker: {
				symbol: 'diamond'
			},
			data: distValsIN
		}]
	});


}