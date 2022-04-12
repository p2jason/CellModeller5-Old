from django.http import HttpResponse
from django.template import Context, Template

from django.apps import apps

def index(request):
	index_data = ""

	with open("static/viewer.html", "r") as index_file:
		index_data = index_file.read()

	sim_uuid = apps.get_app_config("debugservlet").sim_uuid

	context = Context({ "simulation_uuid": sim_uuid, "is_dev_sim": True })
	content = Template(index_data).render(context)

	return HttpResponse(content)

def sim_info(request):
	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	if not request.GET["uuid"] == apps.DUMMY_UUID:
		return HttpResponseBadRequest("Invalid simulation UUID provided")

	sim_uuid = apps.get_app_config("debugservlet").sim_uuid
	index_data = sv_archiver.get_save_archiver().get_sim_index_data(sim_uuid)

	data = {
		"name": index_data["name"],
		"frameCount": index_data["num_frames"],
		"uuid": str(sim_uuid),
		"isOnline": is_simulation_running(sim_uuid)
	}

	response_content = json.dumps(data)
	response = HttpResponse(response_content, content_type="application/json")
	response["Content-Length"] = len(response_content)

	return response

def frame_data(request):
	if not "index" in request.GET:
		return HttpResponseBadRequest("No frame index provided")

	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	if not request.GET["uuid"] == apps.DUMMY_UUID:
		return HttpResponseBadRequest("Invalid simulation UUID provided")

	# Read simulation file
	index = request.GET["index"]

	sim_uuid = apps.get_app_config("debugservlet").sim_uuid
	selected_frame = sv_archiver.get_save_archiver().get_sim_bin_file(sim_uuid, index)

	# Read frame data
	frame_file = open(selected_frame, "rb")
	byte_buffer = io.BytesIO(frame_file.read())

	frame_file.close()

	response_content = byte_buffer.getvalue()	
	response = HttpResponse(response_content, content_type="application/octet-stream")
	response["Content-Length"] = len(response_content)

	return response

def stop_simulation(request):
	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	if not request.GET["uuid"] == apps.DUMMY_UUID:
		return HttpResponseBadRequest("Invalid simulation UUID provided")

	sim_uuid = apps.get_app_config("debugservlet").sim_uuid
	kill_simulation(request.GET["uuid"])

	return HttpResponse()