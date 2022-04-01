from channels.generic.websocket import WebsocketConsumer
import json

from . import websocket_groups as wsgroups
from .backend.SimulationProcess import is_simulation_running, send_message_to_simulation

class SimCommsConsumer(WebsocketConsumer):
	def connect(self):
		self.sim_uuid = str(self.scope["url_route"]["kwargs"]["sim_uuid"])

		if not is_simulation_running(self.sim_uuid):
			self.close(code=4101)
			return

		wsgroups.add_websocket_to_group(f"simcomms/{self.sim_uuid}", self)

		self.accept()

	def disconnect(self, close_code):
		wsgroups.remove_websocket_from_group(f"simcomms/{self.sim_uuid}", self)

	def receive(self, text_data):
		send_message_to_simulation(self.sim_uuid, json.loads(text_data))