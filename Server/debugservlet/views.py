from django.http import HttpResponse, HttpResponseBadRequest
from django.template import Context, Template

from saveviewer import archiver as sv_archiver
from simrunner.instances.manager import is_simulation_running, kill_simulation

from .apps import get_dbgservlet_instance, launch_default_simulation
from . import settings

import io
import json

def index(request):
	index_data = ""

	with open("static/viewer.html", "r") as index_file:
		index_data = index_file.read()

	context = Context({ "simulation_uuid": settings.DUMMY_UUID, "is_dev_sim": True })
	content = Template(index_data).render(context)

	return HttpResponse(content)

def sim_info(request):
	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	if not request.GET["uuid"] == settings.DUMMY_UUID:
		return HttpResponseBadRequest("Invalid simulation UUID provided")

	sim_uuid = get_dbgservlet_instance().sim_uuid
	id_str = str(sim_uuid)

	index_data = sv_archiver.get_save_archiver().get_sim_index_data(id_str)

	data = {
		"name": index_data["name"],
		"frameCount": index_data["num_frames"],
		"uuid": id_str,
		"isOnline": is_simulation_running(id_str)
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

	if not request.GET["uuid"] == settings.DUMMY_UUID:
		return HttpResponseBadRequest("Invalid simulation UUID provided")

	# Read simulation file
	index = request.GET["index"]

	sim_uuid = get_dbgservlet_instance().sim_uuid
	selected_frame = sv_archiver.get_save_archiver().get_sim_bin_file(str(sim_uuid), index)

	# Read frame data
	frame_file = open(selected_frame, "rb")
	byte_buffer = io.BytesIO(frame_file.read())

	frame_file.close()

	response_content = byte_buffer.getvalue()	
	response = HttpResponse(response_content, content_type="application/octet-stream")
	response["Content-Length"] = len(response_content)

	return response

# api/dbgservlet/reload
def reload_simulation(request):
	sim_uuid = get_dbgservlet_instance().sim_uuid
	kill_simulation(str(sim_uuid))

	launch_default_simulation()

	return HttpResponse()

# api/dbgservlet/recompile
def recompile_simulation(request):
	return HttpResponse()

def stop_simulation(request):
	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	if not request.GET["uuid"] == settings.DUMMY_UUID:
		return HttpResponseBadRequest("Invalid simulation UUID provided")

	sim_uuid = get_dbgservlet_instance().sim_uuid
	kill_simulation(str(sim_uuid))

	return HttpResponse()