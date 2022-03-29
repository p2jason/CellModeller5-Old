import * as render from './render.js'

function setSimFrame(index, frameCount) {
	document.getElementById("sim-frame").innerHTML = `Frame: ${index} / ${frameCount}`;
}

function requestSimulationInfo(context, uuid) {
	context["simInfo"] = {};
	context["simInfo"].frameIndex = 0;

	setSimFrame(0, 0);

	return fetch(`/api/saveviewer/simulationinfo?uuid=${uuid}`)
		.then(response => {
			if (!response.ok) throw new Error(`Request failed with status ${response.status}`);
			return response.json();
		})
		.then(info => {
			context["simInfo"].name = info.name;
			context["simInfo"].frameCount = info.frameCount;

			document.getElementById("sim-name").innerHTML = `Name: ${info.name}`;

			var timeline = context["timelineSlider"];
			timeline.min = 1;
			timeline.max = context["simInfo"].frameCount;
			timeline.step = 1;
			timeline.value = 0;
		})
		.catch(error => console.log("Error when reuesting simulation info:", error));
}

function requestFrame(context, uuid, index) {
	return fetch(`/api/saveviewer/framedata?index=${index}&uuid=${uuid}`)
		.then(response => {
			if (!response.ok) throw new Error(`Request failed with status ${response.status}`);
			return response.arrayBuffer();
		})
		.then(buffer => {
			context["simInfo"].frameIndex = index;

			setSimFrame(index + 1, context["simInfo"].frameCount);

			render.pushFrameData(context["gl"], context, buffer)
		})
		.catch(error => console.log("Error when reuesting simulation info:", error));
}

function openSimComms(context, uuid) {
	var chatSocket = new WebSocket(`ws://${window.location.host}/ws/simcomms/${uuid}`);
			
	chatSocket.onopen = function(e) {
		//chatSocket.send(JSON.stringify({ }));
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
		//TODO: fetch(`http://localhost:8000/api/simrunner/stopsimulation?uuid=${simUUID}`);
		//console.error("Chat socket closed unexpectedly");
	};
}

async function createNewSimulation(context) {
	const testName = "This is the name";
	const testProgram = "import random\nfrom CellModeller.Regulation.ModuleRegulator import ModuleRegulator\nfrom CellModeller.Biophysics.BacterialModels.CLBacterium import CLBacterium\nfrom CellModeller.GUI import Renderers\nimport numpy\nimport math\n\nN0 = 10\n\ndef setup(sim):\n    # Set biophysics, signalling, and regulation models\n    biophys = CLBacterium(sim, jitter_z=False, gamma = 100, max_cells=100000, max_planes=1)\n\n    regul = ModuleRegulator(sim, sim.moduleName)	# use this file for reg too\n    # Only biophys and regulation\n    sim.init(biophys, regul, None, None)\n\n    #biophys.addPlane((0,0,0),(0,0,1),1.0) #Base plane\n    #biophys.addPlane((10,0,0),(-1,0,0),1.0)\n    #biophys.addPlane((-10,0,0),(1,0,0),1.0)\n    #biophys.addPlane((0,10,0),(0,-1,0),1.0)\n    #biophys.addPlane((0,-10,0),(0,1,0),1.0)\n\n    sim.addCell(cellType=0, pos=(0,0,0))\n\n    # Add some objects to draw the models\n    therenderer = Renderers.GLBacteriumRenderer(sim)\n    sim.addRenderer(therenderer)\n    sim.pickleSteps = 1\n\ndef init(cell):\n    cell.targetVol = 3.5 + random.uniform(0.0,0.5)\n    cell.growthRate = 1.0\n    cell.n_a = N0//2\n    cell.n_b = N0 - cell.n_a\n\ndef update(cells):\n    for (id, cell) in cells.items():\n        cell.color = [0.1, cell.n_a/3.0, cell.n_b/3.0]\n        if cell.volume > cell.targetVol:\n            cell.divideFlag = True\n\ndef divide(parent, d1, d2):\n    d1.targetVol = 3.5 + random.uniform(0.0,0.5)\n    d2.targetVol = 3.5 + random.uniform(0.0,0.5)\n    plasmids = [0]*parent.n_a*2 + [1]*parent.n_b*2\n    random.shuffle(plasmids)\n    d1.n_a = 0\n    d1.n_b = 0\n    d2.n_a = 0\n    d2.n_b = 0\n    for p in plasmids[:N0]:\n        if p == 0: d1.n_a +=1\n        else: d1.n_b +=1\n    for p in plasmids[N0:2*N0]:\n        if p == 0: d2.n_a +=1\n        else: d2.n_b +=1\n    assert parent.n_a + parent.n_b == N0\n    assert d1.n_a + d1.n_b == N0\n    assert d2.n_a + d2.n_b == N0\n    assert parent.n_a*2 == d1.n_a+d2.n_a\n    assert parent.n_b*2 == d1.n_b+d2.n_b\n    assert parent.n_a > 0 or (d1.n_a == 0 and d2.n_a == 0)\n    assert parent.n_b > 0 or (d1.n_b == 0 and d2.n_b == 0)\n";

	return fetch("/api/simrunner/createnewsimulation", {
			method: "POST",
			headers: {
				"Accept": "text/plain",
				"Content-Type": "text/plain",
			},
			body: JSON.stringify({
				"name": testName,
				"source": testProgram,
			})
		})
		.then(response => {
			if (!response.ok) throw new Error(`Request failed with status ${response.status}`);
			return response.text();
		})
		.then(uuid => {
			context["simUUID"] = uuid;
			return requestSimulationInfo(context, context["simUUID"]);
		})
		.then(() => openSimComms(context, context["simUUID"]))
		.catch(error => console.log("Error when creating new simulation:", error));
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

	context["simUUID"] = "98db8762-a4bc-4c43-b7aa-0691d9e89ec5";

	//Initialize timeline slider 
	context["timelineSlider"] = document.getElementById("frameTimeline");
	context["timelineSlider"].oninput = function() { processTimelineChange(this.value, context); };
	context["timelineSlider"].min = 1;
	context["timelineSlider"].max = 1;

	var snapToLastCheckbox = document.getElementById("snap-to-last");
	snapToLastCheckbox.onchange = function(event) { context["alwaysUseLatestStep"] = this.checked; };

	context["alwaysUseLatestStep"] = snapToLastCheckbox.checked;

	//
	render.init(gl, context)
		//.then(() => { createNewSimulation(context); });
		.then(() => requestSimulationInfo(context, context["simUUID"]))
		.then(() => {
			const frameCount = context["simInfo"].frameCount;

			if (frameCount > 0) {
				return requestFrame(context, context["simUUID"], 0);
			}
		});
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