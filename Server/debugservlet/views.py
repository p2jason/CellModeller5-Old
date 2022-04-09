from django.http import HttpResponse
from django.template import Context, Template

from django.apps import apps

from debugservlet.apps import is_dev_simulation

def index(request):
	index_data = ""

	with open("static/viewer.html", "r") as index_file:
		index_data = index_file.read()

	sim_uuid = apps.get_app_config("debugservlet").sim_uuid

	context = Context({ "simulation_uuid": sim_uuid, "is_dev_sim": True })
	content = Template(index_data).render(context)

	return HttpResponse(content)
