import threading
import multiprocessing as mp
import traceback
import json
import sys, io, os

import time
import git

from .CellModeller4Backend import CellModeller4Backend
from .DuplexPipeEndpoint import DuplexPipeEndpoint

from simrunner import apps
from simrunner import websocket_groups as wsgroups
from saveviewer import archiver as sv_archiver

class InstanceMessage:
	def __init__(self, action="", data=None):
		self.action = action
		self.data = data

class SimulationProcess:
	def __init__(self, params):
		self.params = params
		self.is_alive = True
		
		# The "spawn" context will start a completely new process of the python
		# interpeter. This is also the only context type that is supported on both
		# Unix and Windows systems.
		ctx = mp.get_context("spawn")

		# We need to create a pipe to communicate with the child process. 'mp.Pipe()' creates
		# two 'Connection' objects. Each of the 'Connection' objects represents one of the two
		# ends of the pipe. One object should be used by the parent, and the other should be 
		# used by the child.
		parent_pipe, child_pipe = mp.Pipe(duplex=True)

		self.pipes = (parent_pipe, child_pipe)

		# Create a new process and start it
		self.process = ctx.Process(target=instance_control_thread, args=(child_pipe,params)) #daemon=True
		self.process.start()

		# We also need to create a thread to communicate with the instance process
		self.endpoint = DuplexPipeEndpoint(parent_pipe, self.on_message_from_instance, self.on_endpoint_closed)
		self.endpoint.start()

		print(f"[INSTANCE CONTROLLER]: Started instance controller")

	def __del__(self):
		self.close()

	def on_message_from_instance(self, message):
		# Another "sanity-check" try-except
		try:
			if not isinstance(message, InstanceMessage):
				return

			# Process the message from the child process and make
			# the message that will be sent to the clients
			message_text = None

			if message.action == "newframe":
				new_step_data = json.loads(message.data["new_data"])
				frame_count = message.data["frame_count"]

				sv_archiver.get_save_archiver().update_step_data(str(self.params.uuid), new_step_data)

				self.endpoint.send_item(InstanceMessage("stepfileadded", None))

				message_text = json.dumps({ "action": "newframe", "data": { "framecount": frame_count } })
			elif message.action == "close":
				self.close()

			if message_text is None:
				return

			# Send the message to all the connected clients
			wsgroups.send_message_to_websocket_group(f"simcomms/{self.params.uuid}", message_text)
		except Exception as e:
			traceback.print_exc()

		return

	def on_endpoint_closed(self):
		kill_simulation(self.params.uuid, True)

		self.pipes[0].close()
		self.pipes[1].close()

		# I don't think joining the child process would be a good idea because it might take a long time
		# for it to actually shutdown (when simulation steps get long)
		# self.process.join()

		print(f"[INSTANCE CONTROLLER]: Closed instance controller")

	def close(self):
		self.is_alive = False
		self.endpoint.send_item(InstanceMessage("close", None))
		#self.endpoint.shutdown()

	def is_closed(self):
		return not self.is_alive

# This is what actually runs the simulation
# !!! It runs in a child process !!!
def instance_control_thread(pipe, params):
	# We don't want the simulation's output to go to the output of the main process, 
	# because that will quickly get very messy. Instead, we can redirect the print
	# streams to a file.
	# Because we are running is a subprocess, changing 'sys.stdout' and 'sys.stderr'
	# will only affect the output streams of this simulation instance.
	out_stream = sys.stdout
	err_stream = sys.stderr

	log_file_path = os.path.join(params.sim_root_dir, "log.txt")

	log_stream = open(log_file_path, "w")
	sys.stdout = log_stream
	sys.stderr = log_stream

	# Create pipe endpoint
	out_stream.write(f"[INSTANCE PROCESS]: Creating instance process\n")

	running = True

	step_files_added = True
	pause_simulation_cond = threading.Condition()

	def endpoint_callback():
		# We don't have any endpoint-related resources to clean up, but there is no point in
		# running the simulation if we have disconnected from the server, so we should stop the
		# simulation.
		# This may be gratuitous since if the simulation process is shut down properly, it would 
		# have already sent a close message, but its better to be safe than sorry
		nonlocal running
		running = False

	def got_user_message(message):
		if not isinstance(message, InstanceMessage):
			return

		if message.action == "close":
			endpoint_callback()

			out_stream.write(f"[INSTANCE PROCESS]: Stopping simulation loop\n")

		return

	endpoint = DuplexPipeEndpoint(pipe, got_user_message, endpoint_callback)
	endpoint.start()

	# This is more of a "sanity try-catch". It is here to make sure that
	# if any exceptions occur, we still properly clean up the simulation instance
	try:
		backend = CellModeller4Backend(params)
		backend.initialize()

		while running:
			# Take another step in the simulation
			backend.step()

			# Write step files
			step_path, viz_bin_path = backend.write_step_files()

			# Its better if we update the index file from the simulation process because, otherwise,
			# some message might get lost when closing the pipe and some step files might not get added
			# to the index file
			index_path = os.path.join(params.sim_root_dir, "index.json")
			sim_data_str, frame_count = sv_archiver.add_entry_to_sim_index(index_path, step_path, viz_bin_path)

			endpoint.send_item(InstanceMessage("newframe", { "frame_count": frame_count, "new_data": sim_data_str }))

			# NOTE(Jason): The stream won't write the results to a file immediately after getting some data.
			# If we close Django from the terminal (with Ctrl+C or Ctrl+Break), then the simulation
			# instance won't be closed properly, and the print output will not be written to the file
			# To avoid this, we'll manually flush the stream after every frame (we might still loose
			# a small amount of print output, but its better than nothing).
			log_stream.flush()

		backend.shutdown()
	except Exception as e:
		exc_message = traceback.format_exc()
		out_stream.write(exc_message)
		print(exc_message)

		endpoint.send_item(InstanceMessage("close", { "abrupt": True }))
		endpoint.shutdown()

	# Clean up instance
	out_stream.write(f"[INSTANCE PROCESS]: Closing instance process\n")

	endpoint.send_item(InstanceMessage("close", { "abrupt": False }))
	endpoint.shutdown()

	log_stream.close()

# NOTE(Jason): Yes, I know that globals are considered bad practice, but I couldn't find another way to do it.
# This isn't "just some data that you can save in a database", so all solutions that invlove persistent
# storage or caching are out the window. We also cannot use sessions because they are limited to a single
# client connection.
# I'm going to give it a bit of an unorthodox name so that is doesn't get used somewhere else accidentally
global__active_instances = {}
global__instance_lock = threading.Lock()

class CloneProgress(git.remote.RemoteProgress):
	def __init__(self, uuid: str):
		super().__init__()

		self.sim_uuid = uuid

	def update(self, op_code: int, cur_count: float, max_count: float, message=''):
		wsgroups.send_message_to_websocket_group(f"initlogs/{self.sim_uuid}", self._cur_line)

def spawn_simulation(uuid: str, params):
	global global__active_instances
	global global__instance_lock

	wsgroups.create_websocket_group(f"simcomms/{uuid}")

	with global__instance_lock:
		global__active_instances[uuid] = SimulationProcess(params)

def __spawn_deferred(uuid: str, params):
	backend_url = params.backend_version["url"]
	backend_branch = params.backend_version["branch"]

	print(f"[SIMULATION RUNNER]: Cloning repository ({backend_url} @ {backend_branch}) for simulation: {uuid}")
	
	try:
		time.sleep(1.0)
		git.Repo.clone_from(backend_url, params.backend_dir, branch=backend_branch, progress=CloneProgress(params.uuid))
	except Exception as e:
		print(f"[SIMULATION RUNNER]: Failed to close repository for: {uuid}")

		message = "===== Clone Error =====\n" + str(e)
		wsgroups.send_message_to_websocket_group(f"initlogs/{params.uuid}", message)
		wsgroups.close_websocket_group(f"initlogs/{params.uuid}", close_code=4103, close_message=message)
	else:
		print(f"[SIMULATION RUNNER]: Completed cloning for simulation: {uuid}")

		spawn_simulation(uuid, params)
		wsgroups.close_websocket_group(f"initlogs/{params.uuid}")

	return

def spawn_simulation_from_branch(uuid: str, params):
	wsgroups.create_websocket_group(f"initlogs/{params.uuid}")

	threading.Thread(target=__spawn_deferred, args=(uuid, params), daemon=True).start()

def kill_simulation(uuid: str, remove_only=False):
	global global__active_instances
	global global__instance_lock

	wsgroups.close_websocket_group(f"simcomms/{uuid}")

	with global__instance_lock:
		sim_instance = global__active_instances.pop(uuid, None)

		if sim_instance is None:
			return False
		
		if not remove_only:
			print(f"[Simulation Runner]: Stopping simulation '{uuid}'")

			sim_instance.close()

	return True

def is_simulation_running(uuid: str):
	global global__active_instances
	global global__instance_lock

	with global__instance_lock:
		process = global__active_instances.get(uuid, None)
		return not (process is None or process.is_closed())

def send_message_to_simulation(uuid: str, message):
	global global__active_instances
	global global__instance_lock

	with global__instance_lock:
			global__active_instances[uuid].endpoint.send_item(message)
