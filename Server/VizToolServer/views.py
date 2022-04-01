from django.http import HttpResponse

from django.template import Context, Template

def home(request):
	return HttpResponse(open("static/index.html", "rb"))

def viewer(request, sim_uuid):
	index_data = ""

	with open("static/viewer.html", "r") as index_file:
		index_data = index_file.read()

	context = Context({ "simulation_uuid": sim_uuid })
	content = Template(index_data).render(context)

	return HttpResponse(content)
