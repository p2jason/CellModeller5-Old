from django.http import HttpResponse, HttpResponseBadRequest, HttpResponseNotFound

import json
import io

from . import archiver as sv_archiver

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