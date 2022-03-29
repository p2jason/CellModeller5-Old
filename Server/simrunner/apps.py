from django.apps import AppConfig

import threading
import traceback

# Yes, I know that globals are considered bad practice, but I couldn't find another way to do it.
# This isn't "just some data that you can save in a database", so all solutions that invlove persistent
# storage or caching are out the window. We also cannot use sessions because they are limited to a single
# client connection.
# I'm going to give it a bit of an unorthodox name so that is doesn't get used somewhere else accidentally
global__active_instances = {}
global__instance_lock = threading.Lock()

def get_active_simulations():
	global global__active_instances
	global global__instance_lock

	return (global__active_instances, global__instance_lock)

def kill_simulation(uuid, remove_only=False):
	global global__active_instances
	global global__instance_lock

	with global__instance_lock:
		sim_instance = global__active_instances.pop(str(uuid), None)

		if sim_instance is None:
			return False
		
		if not remove_only:
			print(f"[Simulation Runner]: Stopping simulation '{str(uuid)}'")

			sim_instance.close()

	return True

class SimRunnerConfig(AppConfig):
	default_auto_field = 'django.db.models.BigAutoField'
	name = 'simrunner'