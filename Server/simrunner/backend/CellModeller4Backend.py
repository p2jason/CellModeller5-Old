import os

from .SimulationBackend import SimulationBackend

from CellModeller.Simulator import Simulator

import pickle
import struct
import io

import time

class CellModeller4Backend(SimulationBackend):
	def __init__(self, params):
		super().__init__(params)

		self.params = params
		self.simulation = None
	
	def initialize(self):
		self.simulation = Simulator(self.params.name, self.params.delta_time, moduleStr=self.params.source, clPlatformNum=0, clDeviceNum=0, is_gui=False, saveOutput=False)
		self.simulation.outputDirPath = self.params.sim_root_dir

		if self.simulation.moduleStr:
			self.simulation.moduleOutput = self.simulation.moduleStr
		else:
			self.simulation.moduleOutput = inspect.getsource(self.simulation.module)
	
	def step(self):
		self.simulation.step()

	def write_step_files(self):
		base_file_name = "step-%05i" % self.simulation.stepNum

		pickle_path = os.path.join(self.simulation.outputDirPath, f"{base_file_name}.pickle")
		viz_bin_path = os.path.join(self.params.cache_dir, f"{base_file_name}.bin")

		pickle_file_relative = os.path.join(".", f"{base_file_name}.pickle")
		cached_file_relative = os.path.join(self.params.cache_relative_prefix, f"{base_file_name}.bin")

		# Write pickle
		self.simulation.writePickle()

		# Write binary finle
		cell_states = self.simulation.cellStates

		byte_buffer = io.BytesIO()
		byte_buffer.write(struct.pack("i", len(cell_states)))

		for it in cell_states:
			state = cell_states[it]

			color_r = int(255.0 * min(state.color[0], 1.0))
			color_g = int(255.0 * min(state.color[1], 1.0))
			color_b = int(255.0 * min(state.color[2], 1.0))
			packed_color = 0xFF000000 | (color_b << 16) | (color_g << 8) | color_r

			byte_buffer.write(struct.pack("fff", state.pos[0], state.pos[2], state.pos[1]))
			byte_buffer.write(struct.pack("fff", state.dir[0], state.dir[2], state.dir[1]))
			byte_buffer.write(struct.pack("ffI", state.length, state.radius, packed_color))

		with open(viz_bin_path, "wb") as out_file:
			out_file.write(byte_buffer.getbuffer())

		return pickle_path, cached_file_relative

	def shutdown(self):
		del self.simulation
		self.simulation = None