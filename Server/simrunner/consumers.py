from channels.generic.websocket import WebsocketConsumer
import json

from . import apps

class SimCommsConsumer(WebsocketConsumer):
	def connect(self):
		self.sim_uuid = str(self.scope["url_route"]["kwargs"]["sim_uuid"])

		instances, sim_lock = apps.get_active_simulations()

		with sim_lock:
			process = instances.get(self.sim_uuid, None)

			if process is None or process.is_closed():
				self.close(code=4101)
				return

			with process.clients_lock:
				process.clients.append(self)

		self.accept()

	def disconnect(self, close_code):
		instances, sim_lock = apps.get_active_simulations()
		with sim_lock:
			process = instances.get(self.sim_uuid, None)

			if process is None:
				return

			with process.clients_lock:
				if self in process.clients:
					process.clients.remove(self)
		
		return

	def receive(self, text_data):
		instances, sim_lock = apps.get_active_simulations()

		json_data = json.loads(text_data)

		with sim_lock:
			instances[str(self.sim_uuid)].endpoint.send_item(json_data)