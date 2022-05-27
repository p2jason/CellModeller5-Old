import threading
import git
import json

from saveviewer import archiver as sv_archiver
from simrunner import websocket_groups as wsgroups

from enum import Enum

class InstanceAction(Enum):
	NEW_FRAME = 1
	STEP_FILE_ADDED = 2
	ERROR_MESSAGE = 3
	MESSAGE_TO_CLIENTS = 4
	CLOSE = 5

class InstanceMessage:
	def __init__(self, action: InstanceAction, data=None):
		self.action = action
		self.data = data

class ISimulationInstance:
	def __init__(self, uuid):
		self.is_alive = True
		self.uuid = uuid
	
	def send_item_to_instance(self, item):
		pass

	def send_item_to_clients(self, item):
		wsgroups.send_message_to_websocket_group(f"simcomms/{self.uuid}", json.dumps(item))

	def process_message_from_instance(self, message):
		if not isinstance(message, InstanceMessage):
			return

		# Process the message from the child process and make
		# the message that will be sent to the clients
		if message.action == InstanceAction.NEW_FRAME:
			new_step_data = json.loads(message.data["new_data"])
			frame_count = message.data["frame_count"]

			sv_archiver.get_save_archiver().update_step_data(str(self.params.uuid), new_step_data)

			self.send_item_to_instance(InstanceMessage(InstanceAction.STEP_FILE_ADDED, None))

			self.send_item_to_clients({ "action": "newframe", "data": { "frameCount": frame_count } })
		elif message.action == InstanceAction.ERROR_MESSAGE:
			self.send_item_to_clients({ "action": "error_message", "data": str(message.data) }) 
		elif message.action == InstanceAction.CLOSE:
			self.close()

	def close(self):
		self.is_alive = False

	def is_closed(self):
		return not self.is_alive

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
		wsgroups.send_message_to_websocket_group(f"simcomms/{self.sim_uuid}", "{\"action\":\"infolog\",\"data\":\"" + self._cur_line + "\"}")

def spawn_simulation(uuid: str, proc_class: type, proc_args: tuple=None, should_create_ws_group: bool=True):
	global global__active_instances
	global global__instance_lock

	if should_create_ws_group:
		wsgroups.create_websocket_group(f"simcomms/{uuid}")

	with global__instance_lock:
		if proc_args is None:
			global__active_instances[uuid] = proc_class()
		else:
			global__active_instances[uuid] = proc_class(*proc_args)

	return

def __spawn_deferred(uuid: str, backend_args, proc_class: type, proc_args: tuple=None):
	backend_url, backend_branch, backend_dir = backend_args

	print(f"[SIMULATION RUNNER]: Cloning repository ({backend_url} @ {backend_branch}) for simulation: {uuid}")
	
	try:
		git.Repo.clone_from(backend_url, backend_dir, branch=backend_branch, progress=CloneProgress(uuid))
	except Exception as e:
		print(f"[SIMULATION RUNNER]: Failed to close repository for: {uuid}")

		message = "===== Clone Error =====\n" + str(e)
		wsgroups.send_message_to_websocket_group(f"simcomms/{uuid}", "{\"action\":\"infolog\",\"data\":\"" + message + "\"}")
		wsgroups.send_message_to_websocket_group(f"simcomms/{uuid}", "{\"action\":\"closeinfolog\",\"data\":\"\"}")
	else:
		print(f"[SIMULATION RUNNER]: Completed cloning for simulation: {uuid}")

		spawn_simulation(uuid, proc_class, proc_args, False)
		wsgroups.send_message_to_websocket_group(f"simcomms/{uuid}", "{\"action\":\"closeinfolog\",\"data\":\"\"}")

	return

def spawn_simulation_from_branch(uuid: str, backend_url, backend_branch, backend_dir, proc_class: type, proc_args: tuple=None):
	backend_args = (backend_url, backend_branch, backend_dir)

	# There needs to be an entry in global__active_instances so that the manager can
	# detect that the simulation is active, even if the entry is None
	with global__instance_lock:
		global__active_instances[uuid] = None

	wsgroups.create_websocket_group(f"simcomms/{uuid}")

	threading.Thread(target=__spawn_deferred, args=(uuid, backend_args, proc_class, proc_args), daemon=True).start()

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
		if not uuid in global__active_instances:
			return False

		print("[0]", uuid)
		print("[1]", global__active_instances)

		process = global__active_instances[uuid]
		return not process.is_closed() if not process is None else True

def send_message_to_simulation(uuid: str, message):
	global global__active_instances
	global global__instance_lock

	# NOTE(Jason): Originally, the message was sent to the simulation while the lock
	# was still acquired. I changed it because it caused some problems with 'simthread'.
	# There is a slight chance that this could cause an issue (e.g. if someone closes but before
	# the simulation instance after the instance is retreived from 'global__active_instances',
	# 'send_item_to_instance' is invoked), but I think its highly unlikely that it will happen.
	with global__instance_lock:
		sim_instance = global__active_instances[uuid]
	
	sim_instance.send_item_to_instance(message)