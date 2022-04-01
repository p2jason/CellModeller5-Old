from django.shortcuts import render
from django.http import HttpResponse, HttpResponseBadRequest, HttpResponseNotFound
from django.views.decorators.csrf import csrf_exempt

from . import apps
from .backend.SimulationProcess import spawn_simulation, kill_simulation
from .backend.SimulationBackend import BackendParameters

from saveviewer import archiver as sv_archiver

import re
import json
import uuid
import traceback
import random

import asyncio
import git

class Progress(git.remote.RemoteProgress):
	def update(self, op_code: int, cur_count: float, max_count: float, message=''):
		print(self._cur_line)

@csrf_exempt
def create_new_simulation(request):
	# This needs to be a POST request since this method is not idempotent
	if request.method != "POST":
		return HttpResponseNotAllowed([ "POST" ])

	sim_uuid = uuid.uuid4()

	# Parse the request body
	try:
		creation_parameters = json.loads(request.body)
	except json.JSONDecodeError as e:
		return HttpResponseBadRequest(f"Invalid JSON provided as request body: {str(e)}")

	# Check parameters
	sim_name = creation_parameters.get("name", None)
	sim_source = creation_parameters.get("source", None)
	sim_backend = creation_parameters.get("backend", None)

	if sim_name is None: return HttpResponseBadRequest("Simulation name not provided")
	if sim_source is None: return HttpResponseBadRequest("Simulation source not provided")
	if sim_backend is None: return HttpResponseBadRequest("Simulation backend not specified")

	# Register simulation
	params = BackendParameters()
	params.uuid = sim_uuid
	params.name = sim_name
	params.source = sim_source
	
	use_custom_backend = type(sim_backend) is dict
	id_str = str(sim_uuid)

	try:
		extra_vars = { "backend_version": sim_backend }

		paths = sv_archiver.get_save_archiver().register_simulation(id_str, f"./{id_str}", sim_name, use_custom_backend, extra_init_vars=extra_vars)
	except Exception as e:
		traceback.print_exc()
		return HttpResponseBadRequest(str(e))

	params.sim_root_dir = paths.root_path
	params.cache_dir = paths.cache_path
	params.cache_relative_prefix = paths.relative_cache_path
	params.backend_dir = paths.backend_path
	params.backend_relative_prefix = paths.relative_backend_path

	# Download backend
	params.backend_version = sim_backend

	if use_custom_backend:
		backend_url = sim_backend.get("url", None)
		backend_branch = sim_backend.get("branch", None)
		backend_version = sim_backend.get("version", None)

		if backend_url is None: return HttpResponseBadRequest("Backend URL not provided")
		if backend_branch is None: return HttpResponseBadRequest("Backend branch not provided")
		if backend_version is None: return HttpResponseBadRequest("Backend version not provided")
		
		git.Repo.clone_from(backend_url, params.backend_dir, branch=backend_branch, progress=Progress())
	elif not type(sim_backend) is str:
		return HttpResponseBadRequest(f"Invalid backend data type: {type(sim_backend)}")
	
	print(f"[Simulation Runner]: Creating new simulation '{id_str}': ")

	# Spawn simulation
	spawn_simulation(id_str, params)

	return HttpResponse(id_str)

def stop_simulation(request):
	if not "uuid" in request.GET:
		return HttpResponseBadRequest("No simulation UUID provided")

	kill_simulation(request.GET["uuid"])

	return HttpResponse()