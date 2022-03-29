from django.apps import AppConfig

import os

import pickle
import struct
import io
import json

def convert_pickle_to_viz_bin(sim_name, step_name):
	sim_dir = os.path.join("./sim_archive", sim_name)
	cache_dir = os.path.join(sim_dir, "cache")

	byte_buffer = io.BytesIO()

	with open(os.path.join(sim_dir, f"{step_name}.pickle"), "rb") as frame_file:
		stepdata = pickle.load(frame_file)
		cell_states = stepdata["cellStates"]

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

	with open(os.path.join(cache_dir, f"{step_name}.bin"), "wb") as out_file:
		out_file.write(byte_buffer.getbuffer())

def convert_all_sim_files(sim_name):
	sim_dir = os.path.join("./sim_archive", sim_name)

	os.mkdir(os.path.join(sim_dir, "cache"))

	for step_file in os.listdir(sim_dir):
		if not step_file.startswith("step-") or not step_file.endswith(".pickle"):
			continue

		convert_pickle_to_viz_bin(sim_name, step_file[0:-7])

def generate_sim_index_file(sim_name, display_name):
	sim_dir = os.path.join("./sim_archive", sim_name)

	step_index = 0
	sim_info = { "frames": {}, "pickles": {} }

	for step_file in os.listdir(sim_dir):
		if not step_file.startswith("step-") or not step_file.endswith(".pickle"):
			continue

		sim_info["frames"][step_index] = f"./cache/{step_file[0:-7]}.bin"
		sim_info["pickles"][step_index] = f"./cache/{step_file[0:-7]}.pickle"
		step_index += 1

	sim_info["name"] = display_name
	sim_info["num_frames"] = step_index

	with open(os.path.join(sim_dir, "index.json"), "w") as out_file:
		out_file.write(json.dumps(sim_info))

def generate_master_index_file():
	sim_info = {}
	sim_info["saved_simulations"] = {
		"98db8762-a4bc-4c43-b7aa-0691d9e89ec5": "./ex5_colonySector-22-03-11-02-25",
		"c2733b26-b4cb-4678-a6a3-9def5d3003c0": "./ex1a_simpleGrowth2Types-22-03-02-23-43",
		"e545adae-8e2a-41cd-8547-4ef3fda68667": "./ex2_constGene-22-03-11-01-58",
		"071b6909-c305-4409-b125-3c57a2fb4ceb": "./ex2a_dilution-22-03-11-02-19",
		"680a7467-539f-4fd1-8f01-1ab7ae5ebe8e": "./ex1b_simpleGrowthRoundCell-22-03-11-01-55"
	}

	with open("./sim_archive/index.json", "w") as out_file:
		out_file.write(json.dumps(sim_info))

class MainAppConfig(AppConfig):
	name = "VizToolServer"
	verbose_name = "CellModeller Visualization Tool"

	def ready(self):
		#base_path = os.path.join(os.getcwd(), "../CellModeller/data/")
		#archived_sims = [ os.path.join(base_path, d) for d in os.listdir(base_path) ]
		pass
		