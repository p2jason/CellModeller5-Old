from django.shortcuts import render
from django.http import HttpResponse, HttpResponseBadRequest, HttpResponseNotFound
from django.views.decorators.csrf import csrf_exempt

from . import apps
from .backend.SimulationProcess import SimulationProcess
from .backend.SimulationBackend import BackendParameters

from saveviewer import archiver as sv_archiver

import json
import uuid
import traceback

@csrf_exempt
def create_new_simulation(request):
	# This needs to be a POST request since this method is not idempotent
	if request.method != "POST":
		return HttpResponseNotAllowed([ "POST" ])

	sim_uuid = uuid.uuid4()

	try:
		creation_parameters = json.loads(request.body)

		sim_name = creation_parameters["name"]
		sim_source = creation_parameters["source"]

		paths = sv_archiver.get_save_archiver().register_simulation(str(sim_uuid), f"./{str(sim_uuid)}", sim_name)
	except Exception as e:
		traceback.print_exc()
		return HttpResponseBadRequest()

	print(f"[Simulation Runner]: Creating new simulation '{str(sim_uuid)}': ")

	(root_path, cache_path, relative_cache_path) = paths

	params = BackendParameters()
	params.uuid = sim_uuid
	params.name = sim_name
	params.source = sim_source
	params.sim_root_dir = root_path
	params.cache_dir = cache_path
	params.cache_relative_prefix = relative_cache_path

	instances, sim_lock = apps.get_active_simulations()

	with sim_lock:
		instances[str(sim_uuid)] = SimulationProcess(params)

	return HttpResponse(str(sim_uuid))

def stop_simulation(request):
	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	sim_id = request.GET["uuid"]
	apps.kill_simulation(sim_id)

	return HttpResponse()