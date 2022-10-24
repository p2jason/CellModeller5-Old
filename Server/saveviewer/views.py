from django.http import HttpResponse, FileResponse, HttpResponseBadRequest, HttpResponseNotFound

from . import archiver as sv_archiver

import json, pickle

def frame_data(request):
	if not "index" in request.GET:
		return HttpResponseBadRequest("No frame index provided")

	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	# Read simulation file
	sim_id = request.GET["uuid"]
	index = request.GET["index"]

	selected_frame = sv_archiver.get_save_archiver().get_sim_bin_file(sim_id, index)

	response = FileResponse(open(selected_frame, "rb"))
	response["Content-Encoding"] = "deflate"

	return response

def cell_info_from_index(request):
	if not "cellid" in request.GET:
		return HttpResponseBadRequest("No cell index provided")

	if not "frameindex" in request.GET:
		return HttpResponseBadRequest("No frame index provided")

	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	# Read simulation file
	sim_id = request.GET["uuid"]
	frameindex = request.GET["frameindex"]
	cellid = request.GET["cellid"]

	selected_frame = sv_archiver.get_save_archiver().get_sim_step_file(sim_id, frameindex)

	with open(selected_frame, "rb") as pickle_file:
		frame_pickle = pickle.load(pickle_file)

	cell_data = frame_pickle["cellStates"][int(cellid)]

	data = {
		"Index": cell_data.idx,
		"Radius": float(cell_data.radius),
		"Length": float(cell_data.length),
		"Growth rate": float(cell_data.growthRate),
		"Cell age": int(cell_data.cellAge),
		"Effective growth": float(cell_data.effGrowth),
		"Cell type": int(cell_data.cellType),
		"Cell adhesion": int(cell_data.cellAdh),
		"Target volume": float(cell_data.targetVol),
		"Volume": float(cell_data.volume),
		"Strain rate": float(cell_data.strainRate),
		"Start volume": float(cell_data.startVol),
	}

	response_content = json.dumps(data)
	response = HttpResponse(response_content, content_type="application/json")
	response["Content-Length"] = len(response_content)

	return response