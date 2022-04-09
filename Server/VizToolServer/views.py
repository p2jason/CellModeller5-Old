from django.http import HttpResponse
from django.template import Context, Template

from django.views.decorators.csrf import csrf_exempt

@csrf_exempt
def home(request):
	return HttpResponse(open("static/index.html", "rb"))

@csrf_exempt
def viewer(request, sim_uuid):
	index_data = ""

	with open("static/viewer.html", "r") as index_file:
		index_data = index_file.read()

	context = Context({ "simulation_uuid": sim_uuid, "is_dev_sim": False })
	content = Template(index_data).render(context)

	return HttpResponse(content)
