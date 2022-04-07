import threading
import traceback
import os
import json

from .manager import ISimulationInstance, kill_simulation

from simrunner.backends.cellmodeller4 import CellModeller4Backend
from saveviewer import archiver as sv_archiver

class SimulationThread(ISimulationInstance):
	def __init__(self, params):
		super(SimulationThread, self).__init__(params.uuid)
		self.params = params

		self.thread = threading.Thread(target=instance_control_thread, args=(params, self.send_item_to_clients), daemon=True)
		self.thread.start()

	def send_item_to_instance(self, item):
		pass

	def close(self):
		super().close()

		self.thread.join()

def instance_control_thread(params, send_func):
	running = True

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

			sv_archiver.get_save_archiver().update_step_data(str(params.uuid), json.loads(sim_data_str))

			send_func(json.dumps({ "action": "newframe", "data": { "framecount": frame_count } }))

		backend.shutdown()
	except Exception as e:
		traceback.print_exc()