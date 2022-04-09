from django.apps import AppConfig, apps

from simrunner.instances.manager import spawn_simulation, kill_simulation
from simrunner.instances.simthread import SimulationThread
from simrunner.backends.backend import BackendParameters

from saveviewer import archiver as sv_archiver

import uuid

class DebugServletAppConfig(AppConfig):
	name = "debugservlet"
	verbose_name = "Debug Servlet for CellModeller5"

	def ready(self):
		# Register simulation
		self.sim_uuid = uuid.uuid4()
		sim_name = "CellModeller5 Development Simulation"
		sim_backend = "CellModeller5 Development Build"
		sim_source = """
import random
from CellModeller.Regulation.ModuleRegulator import ModuleRegulator
from CellModeller.Biophysics.BacterialModels.CLBacterium import CLBacterium
import numpy
import math

#Import Euler integrator for solving ODE system of chemical species inside the cells
from CellModeller.Integration.CLEulerIntegrator import CLEulerIntegrator

max_cells = 2**15


def setup(sim):
	# Set biophysics, signalling, and regulation models
	biophys = CLBacterium(sim, jitter_z=False)
 
	
	integ = CLEulerIntegrator(sim, 1, max_cells)

	regul = ModuleRegulator(sim, sim.moduleName)	
	# Only biophys and regulation
	sim.init(biophys, regul, None, integ)

	# Specify the initial cell and its location in the simulation
	sim.addCell(cellType=0, pos=(0,0,0)) 

	if sim.is_gui:
		# Add some objects to draw the models
		from CellModeller.GUI import Renderers
		therenderer = Renderers.GLBacteriumRenderer(sim)
		sim.addRenderer(therenderer)

	sim.pickleSteps = 10



def init(cell):
	# Specify mean and distribution of initial cell size
	cell.targetVol = 2.5 + random.uniform(0.0,0.5)
	# Specify growth rate of cells
	cell.growthRate = 2.0
	# Specify initial concentration of chemical species
	cell.species[:] = [0]

def specRateCL(): # Add
	return '''
	const float k1 = 1.f;
	float x0 = species[0];
	rates[0] = k1;
	'''
	# k1 = production rate of x0


def update(cells):
	#Iterate through each cell and flag cells that reach target size for division
	for (id, cell) in cells.items():
		cell.color = [0.1+cell.species[0]/20.0, 0.1, 0.1] # Add/change
		if cell.volume > cell.targetVol:
			cell.divideFlag = True

def divide(parent, d1, d2):
	# Specify target cell size that triggers cell division
	d1.targetVol = 2.5 + random.uniform(0.0,0.5)
	d2.targetVol = 2.5 + random.uniform(0.0,0.5)
		"""

		id_str = str(self.sim_uuid)

		params = BackendParameters()
		params.uuid = self.sim_uuid
		params.name = sim_name
		params.source = sim_source
		
		extra_vars = { "backend_version": sim_backend }
		paths = sv_archiver.get_save_archiver().register_simulation(id_str, f"./{id_str}", sim_name, False, extra_init_vars=extra_vars)

		params.sim_root_dir = paths.root_path
		params.cache_dir = paths.cache_path
		params.cache_relative_prefix = paths.relative_cache_path
		params.backend_dir = paths.backend_path
		params.backend_relative_prefix = paths.relative_backend_path

		params.backend_version = sim_backend

		# Spawn simulation
		spawn_simulation(id_str, proc_class=SimulationThread, proc_args=(params,))

def is_dev_simulation(uuid: str):
	return uuid == apps.get_app_config("debugservlet").sim_uuid