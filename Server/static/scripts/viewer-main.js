import * as render from './viewer-render.js'

function setSimName(name) {
	document.getElementById("sim-name").innerHTML = `Name: ${name}`;
}

function setSimFrame(index, frameCount) {
	document.getElementById("sim-frame").innerHTML = `Frame: ${index} / ${frameCount}`;
}

function setStatusMessage(message) {
	document.getElementById("status-label").innerHTML = message;
}

function requestFrame(context, uuid, index) {
	context["current_index"] = index

	return fetch(`/api/saveviewer/framedata?index=${index}&uuid=${uuid}`)
		.then(response => {
			if (!response.ok) throw new Error(`Request failed with status ${response.status}`);
			return response.arrayBuffer();
		})
		.then(buffer => {
			if (context["current_index"] != index) {
				return;
			}

			context["simInfo"].frameIndex = index;

			setSimFrame(index + 1, context["simInfo"].frameCount);

			render.pushFrameData(context["gl"], context, buffer)
		})
		.catch(error => console.log("Error when reuesting simulation info:", error));
}

async function waitForInitialization(context, uuid) {
	
}

async function beginSimulation(context, uuid) {
	context["simUUID"] = uuid;

	context["simInfo"] = {};
	context["simInfo"].frameIndex = 0;

	setSimFrame(0, 0);
	setStatusMessage("Offline");

	//Request simulation info
	const simulationData = await fetch(`/api/saveviewer/simulationinfo?uuid=${uuid}`);
	const simulationInfo = await simulationData.json();

	context["simInfo"].name = simulationInfo.name;
	context["simInfo"].frameCount = simulationInfo.frameCount;
	context["simInfo"].isOnline = simulationInfo.isOnline;

	setSimName(simulationInfo.name);

	//Update timeline 
	context["timelineSlider"].max = simulationInfo.frameCount;
	
	//Request the first frame of the simulation
	if (simulationInfo.frameCount > 0) {
		await requestFrame(context, context["simUUID"], 0);
	}

	//Open a web socket if the simulation is live
	if (simulationInfo.isOnline) {
		setStatusMessage("Conneceting");

		var chatSocket = new WebSocket(`ws://${window.location.host}/ws/simcomms/${uuid}`);
			
		chatSocket.onopen = function(e) {
			setStatusMessage("Running");
		};
		
		chatSocket.onmessage = function(e) {
			const message = JSON.parse(e.data);

			if (message["action"] == "newframe") {
				const frameCount = message["data"]["framecount"];

				context["simInfo"].frameCount = frameCount;
				context["timelineSlider"].max = frameCount;

				setSimFrame(context["simInfo"].frameIndex, frameCount);

				if (context["alwaysUseLatestStep"] && frameCount > 0) {
					requestFrame(context, context["simUUID"], frameCount - 1);

					context["timelineSlider"].value = frameCount;
				}
			}
		};
		
		chatSocket.onclose = function(e) {
			setStatusMessage("Terminated");
		};
	}
}

function processTimelineChange(value, context) {
	requestFrame(context, context["simUUID"], value - 1);
}

function initFrame(gl, context) {
	//Initialize camera details
	context["camera"] = {
		"orbitCenter": vec3.fromValues(0, 0.0, 0.0),
		"orbitRadius": 50.0,
		"orbitMinRadius": 2.0,

		"orbitLookSensitivity": 0.4,
		"orbitRadiusSensitivity": 0.02,

		"fovAngle": 70.0,
		"nearZ": 0.01,
		"farZ": 1000.0,
		"position": vec3.fromValues(0, 3.0, 10.0),
		"rotation": quat.create(),
		"pitch": -90.0,
		"yaw": 0.0,

		"projectionMatrix": mat4.create(),
		"viewMatrix": mat4.create(),
	};

	//Initialize input details
	context["input"] = {
		"leftButtonPressed": false,
		"middleButtonPressed": false,
		"shiftPressed": false,

		"lastMouseX": 0,
		"lastMouseY": 0
	};

	//Initialize simulation info
	context["simInfo"] = {
		"name": "",
		"frameIndex": 0,
		"frameCount": 0
	};

	//Initialize timeline slider 
	var timelineSlider = document.getElementById("frame-timeline");
	timelineSlider.oninput = function() { processTimelineChange(this.value, context); };
	timelineSlider.min = 1;
	timelineSlider.max = 1;
	timelineSlider.step = 1;
	timelineSlider.value = 0;

	context["timelineSlider"] = timelineSlider;

	var snapToLastCheckbox = document.getElementById("snap-to-last");
	snapToLastCheckbox.onchange = function(event) { context["alwaysUseLatestStep"] = this.checked; };

	context["alwaysUseLatestStep"] = snapToLastCheckbox.checked;

	//Initialize the renderer
	setStatusMessage("Initializing");

	const uuid = document.getElementById("uuid-field").value;

	render.init(gl, context)
		.then(() => beginSimulation(context, uuid))
		.catch((error) => { console.log(`Error: ${error}`); });
}

function drawScene(gl, context, delta) {
	var camera = context["camera"];
	var cameraPosition = camera["position"];
	var cameraRotation = camera["rotation"];

	quat.fromEuler(cameraRotation, camera["pitch"], camera["yaw"], 0);

	var forward = vec3.fromValues(0, 0, 1);
	vec3.transformQuat(forward, forward, cameraRotation);
	vec3.scaleAndAdd(cameraPosition, camera["orbitCenter"], forward, camera["orbitRadius"]);

	const aspectRatio = gl.canvas.width / gl.canvas.height;
	const projectionMatrix = mat4.perspective(mat4.create(), glMatrix.toRadian(camera["fovAngle"]), aspectRatio, camera["nearZ"], camera["farZ"]);

	const viewMatrix = mat4.create();
	mat4.transpose(viewMatrix, mat4.fromQuat(mat4.create(), cameraRotation));
	mat4.translate(viewMatrix, viewMatrix, vec3.negate(vec3.create(), cameraPosition));

	camera["position"] = cameraPosition;
	camera["rotation"] = cameraRotation;
	camera["projectionMatrix"] = projectionMatrix;
	camera["viewMatrix"] = viewMatrix;

	render.drawFrame(gl, context, delta);
}

function resizeCanvas(gl, context, canvas) {
	canvas.width = canvas.clientWidth;
	canvas.height = canvas.clientHeight;

	render.resize(gl, context, canvas);
}

function processKeyButon(event, context, isdown) {
	var input = context["input"];

	switch (event.code) {
	case "ShiftLeft":
	case "ShiftRight":
		input["shiftPressed"] = isdown;
		break;
	}
}

function processMouseMove(event, context) {
	var input = context["input"];

	const deltaX = event.offsetX - input["lastMouseX"];
	const deltaY = event.offsetY - input["lastMouseY"];

	input["lastMouseX"] = event.offsetX;
	input["lastMouseY"] = event.offsetY;

	//Move orbit
	if (input["middleButtonPressed"]) {
		var camera = context["camera"];

		if (input["shiftPressed"]) {
			var cameraRotation = camera["rotation"];

			const sensitivity = 0.08;
			
			//Update up direction
			const sensY = sensitivity * deltaY;

			var up = vec3.fromValues(0, 1, 0);
			vec3.transformQuat(up, up, cameraRotation);
			vec3.multiply(up, up, vec3.fromValues(sensY, sensY, sensY));
			
			vec3.add(camera["orbitCenter"], up, camera["orbitCenter"]);

			//Update right direction
			const sensX = -sensitivity * deltaX;

			var right = vec3.fromValues(1, 0, 0);
			vec3.transformQuat(right, right, cameraRotation);
			vec3.multiply(right, right, vec3.fromValues(sensX, sensX, sensX));

			vec3.add(camera["orbitCenter"], right, camera["orbitCenter"]);
		} else {
			const sensitivity = camera["orbitLookSensitivity"];
	
			camera["yaw"] = (camera["yaw"] - sensitivity * deltaX) % 360.0;
			camera["pitch"] = (camera["pitch"] - sensitivity * deltaY) % 360.0;
		}
	}
}

function processMouseButton(event, context, isdown) {
	event.stopPropagation();
	event.preventDefault();

	switch (event.button) {
	case 0:
		context["input"]["leftButtonPressed"] = isdown;
		break;
	case 1:
		context["input"]["middleButtonPressed"] = isdown;
		break;
	}

	if (isdown) {
		context["canvasElement"].focus();
	}
}

function processMouseWheel(event, context) {
	event.stopPropagation();
	event.preventDefault();

	var camera = context["camera"];

	var radius = camera["orbitRadius"];
	radius += camera["orbitRadiusSensitivity"] * event.deltaY;
	radius = Math.max(radius, camera["orbitMinRadius"]);

	camera["orbitRadius"] = radius;
}

function main() {
	//Create canvas
	var canvas = document.getElementById("renderTargetCanvas");
	const gl = canvas.getContext("webgl2", {antialias: false});
	
	if (gl === null) {
		alert("Unable to initialize WebGL");
		return;
	}

	canvas.focus();
	
	var context = { "canvasElement": canvas, "gl": gl };
	initFrame(gl, context);
	resizeCanvas(gl, context, canvas);

	canvas.addEventListener("mousemove", e => processMouseMove(e, context));
	canvas.addEventListener("mousedown", e => processMouseButton(e, context, true));
	canvas.addEventListener("mouseup", e => processMouseButton(e, context, false));
	canvas.addEventListener("keydown", e => processKeyButon(e, context, true));
	canvas.addEventListener("keyup", e => processKeyButon(e, context, false));
	canvas.addEventListener("wheel", e => processMouseWheel(e, context));
	canvas.addEventListener("contextmenu", e => { e.preventDefault() });

	//Initialize render loop
	var lastTime = 0;

	function render(now) {
		//Check if a resize is needed
		if (canvas.width  !== canvas.clientWidth || canvas.height !== canvas.clientHeight) {
			resizeCanvas(gl, context, canvas);
		}

		const delta = (now - lastTime) * 0.001;
		lastTime = now;

		drawScene(gl, context, delta);

		window.requestAnimationFrame(render);
	}
	
	window.requestAnimationFrame(render);
}

window.onload = main;