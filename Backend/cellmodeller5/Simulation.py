import pkgutil

import cellmodeller5.native as cmnative

def load_shader(path):
	return pkgutil.get_data(__name__, path).decode("utf-8")

class Simulator:
	def __init__(self):
		self.native = cmnative.NativeSimulator(load_shader)

		self.step_index = 0
		self.is_running = True

	def step(self):
		self.step_index += 1
		self.native.step()

		if self.step_index >= 100:
			self.is_running = False

	def get_step_time(self):
		return self.native.get_last_step_time()

	def dump_to_step_file(self, path):
		self.native.dump_to_step_file(path)
	
	def dump_to_viz_file(self, path):
		self.native.dump_to_viz_file(path)

	def get_step_index(self):
		return self.step_index