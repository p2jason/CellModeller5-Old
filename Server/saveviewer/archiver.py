import os
import json
import pathlib


class ArchivePaths:
	def __init__(self):
		self.root_path = None
		self.cache_path = None
		self.relative_cache_path = None
		self.backend_path = None
		self.relative_backend_path = None

class SaveArchiver:
	def __init__(self):
		self.archive_root = "./save-archive/"
		self.master_path = os.path.join(self.archive_root, "index.json")
		self.sim_data = {}
		self.online_sims = set()

		pathlib.Path(self.archive_root).mkdir(parents=False, exist_ok=True)

		# We may want to move the index file to a database
		if not os.path.exists(self.master_path):
			with open(self.master_path, "w") as master_file:
				master_file.write("{ \"saved_simulations\": {} }")

		with open(self.master_path, "r") as master_file:
			self.master_data = json.loads(master_file.read())

		for uuid, relative_path in self.master_data["saved_simulations"].items():
			index_path = os.path.join(self.archive_root, relative_path, "index.json")

			with open(index_path, "r") as index_file:
				self.sim_data[str(uuid)] = json.loads(index_file.read())

		return

	def update_master_file(self):
		with open(self.master_path, "w") as master_file:
			master_file.seek(0)
			master_file.write(json.dumps(self.master_data))
			master_file.truncate()

	def register_simulation(self, uuid: str, path: str, name: str, create_backend_dir: bool, extra_init_vars: object=None):
		self.master_data["saved_simulations"][uuid] = path
		self.update_master_file()

		root_path = os.path.join(self.archive_root, path)

		relative_cache_path = "./cache"
		cache_path = os.path.join(root_path, relative_cache_path)

		relative_backend_path = "./backend"
		backend_path = os.path.join(root_path, relative_backend_path)

		os.mkdir(root_path)
		os.mkdir(cache_path)

		if create_backend_dir:
			os.mkdir(backend_path)

		self.sim_data[uuid] = { "frames": {}, "name": name, "num_frames": 0 }
		self.online_sims.add(uuid)

		if not extra_init_vars is None:
			self.sim_data[uuid].update(extra_init_vars)
		
		with open(os.path.join(root_path, "index.json"), "w+") as index_file:
			index_file.seek(0)
			index_file.write(json.dumps(self.sim_data[uuid]))
			index_file.truncate()

		paths = ArchivePaths()
		paths.root_path = root_path
		paths.cache_path = cache_path
		paths.relative_cache_path = relative_cache_path

		if create_backend_dir:
			paths.backend_path = backend_path
			paths.relative_backend_path = relative_backend_path
		else:
			paths.backend_path = None
			paths.relative_backend_path = None

		return paths

	def update_step_data(self, uuid: str, step_data: object):
		self.sim_data[uuid] = step_data

	def get_all_sim_data(self):
		return self.sim_data

	def get_sim_index_data(self, uuid: str):
		return self.sim_data[uuid]

	def is_sim_online(self, uuid: str):
		return uuid in self.online_sims

	def get_sim_bin_file(self, uuid: str, index: int):
		simulation_root = self.master_data["saved_simulations"][uuid]
		frame_relative_path = self.sim_data[uuid]["frames"][index]

		return os.path.join(self.archive_root, simulation_root, frame_relative_path)

def add_entry_to_sim_index(index_path, step_file: str, viz_bin_file: str):
	with open(index_path, "r+") as index_file:
		sim_data = json.loads(index_file.read())

		frame_count = len(sim_data["frames"])
		sim_data["frames"][frame_count] = viz_bin_file
		sim_data["num_frames"] = frame_count + 1

		sim_data_str = json.dumps(sim_data)

		index_file.seek(0)
		index_file.write(sim_data_str)
		index_file.truncate()

	return (sim_data_str, frame_count)

global__save_archiver = SaveArchiver()

def get_save_archiver():
	global global__save_archiver
	return global__save_archiver