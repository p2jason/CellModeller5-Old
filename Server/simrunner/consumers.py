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

class InitLogsConsumer(WebsocketConsumer):
	def connect(self):
		self.sim_uuid = str(self.scope["url_route"]["kwargs"]["sim_uuid"])

		# We need to accept the web socket before we close it. By doing so, we are opening
		# a connection and then closing it, otherwise, (if we closed the socket without
		# accepting it first) we would be rejecting the connection. When a connection is 
		# rejected, the WebSocket class on the front-end will throw an error.
		self.accept()

		close_parameters = wsgroups.get_close_parameters(f"initlogs/{self.sim_uuid}")
		if not close_parameters is None:
			if not close_parameters[1] is None:
				self.send(text_data=str(close_parameters[1]))
			
			self.close(code=close_parameters[0])
			return

		if not wsgroups.add_websocket_to_group(f"initlogs/{self.sim_uuid}", self):
			self.close(code=4102)

	def disconnect(self, close_code):
		wsgroups.remove_websocket_from_group(f"initlogs/{self.sim_uuid}", self)