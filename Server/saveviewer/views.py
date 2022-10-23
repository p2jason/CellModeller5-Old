from django.http import FileResponse, HttpResponseBadRequest, HttpResponseNotFound

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

	response = FileResponse(open(selected_frame, "rb"))
	response["Content-Encoding"] = "deflate"

	return response