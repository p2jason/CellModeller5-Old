import threading

global__ws_groups = {}
global__ws_group_lock = threading.Lock()

def create_websocket_group(group_name: str):
	global global__ws_groups
	global global__ws_group_lock

	with global__ws_group_lock:
		if group_name in global__ws_groups:
			raise KeyError(f"WebSocket group '{group_name}' already exists")

		global__ws_groups[group_name] = []

def close_websocket_group(group_name: str):
	global global__ws_groups
	global global__ws_group_lock

	with global__ws_group_lock:
		# When we close the connection object, the communication thread will remove the 
		# client from the clients list. This is going to cause a problem, since we are
		# also iterating over the list of clients. To solve this, we can just create a
		# copy (shallow copy) of the clients list and iterate over that
		clients = global__ws_groups.pop(group_name, None)

	if clients is None:
		return
		
	for client in clients:
		client.close()

def add_websocket_to_group(group_name: str, consumer):
	global global__ws_groups
	global global__ws_group_lock

	with global__ws_group_lock:
		global__ws_groups[group_name].append(consumer)

def remove_websocket_from_group(group_name: str, consumer):
	global global__ws_groups
	global global__ws_group_lock

	with global__ws_group_lock:
		if not group_name in global__ws_groups:
			return

		group = global__ws_groups.get[group_name]

		if consumer in group:
			group.remove(consumer)

def send_message_to_websocket_group(group_name: str, message_text: str):
	global global__ws_groups
	global global__ws_group_lock

	with global__ws_group_lock:
		for client in global__ws_groups[group_name]:
			client.send(text_data=message_text)