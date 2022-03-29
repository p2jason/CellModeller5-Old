from django.http import HttpResponse, HttpResponseBadRequest, HttpResponseNotFound
from django.views.generic import TemplateView, RedirectView

import json
import io
import os

from . import archiver as sv_archiver

def get_all_simulations(request):
	archiver = sv_archiver.get_save_archiver()

	all_sims = { "simulations": [] }

	for (uuid, data) in archiver.get_all_sim_data():
		all_sims["simulations"].append({
			"uuid": uuid,
			"name": data["name"],
			"frame_count": data["num_frames"]
		})

	response_content = json.dumps(data)
	response = HttpResponse(response_content, content_type="application/json")
	response["Content-Length"] = len(response_content)

	return response

def sim_info(request):
	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	sim_id = request.GET["uuid"]

	try:
		index_data = sv_archiver.get_save_archiver().get_sim_index_data(sim_id)
	except KeyError:
		return HttpResponseNotFound(f"No simulation with the UUID '{sim_id}' was found")

	data = {
		"name": index_data["name"],
		"frameCount": index_data["num_frames"],
		"uuid": str(sim_id)
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

	# Read simulation file
	sim_id = request.GET["uuid"]
	index = request.GET["index"]

	selected_frame = sv_archiver.get_save_archiver().get_sim_bin_file(sim_id, index)

	# Read frame data
	frame_file = open(selected_frame, "rb")
	byte_buffer = io.BytesIO(frame_file.read())

	frame_file.close()

	response_content = byte_buffer.getvalue()	
	response = HttpResponse(response_content, content_type="application/octet-stream")
	response["Content-Length"] = len(response_content)

	return response