from django.contrib import admin
from django.urls import path, include

from . import views

urlpatterns = [
	path("", views.index),
	path("api/saveviewer/framedata", views.frame_data),
	path("api/saveviewer/simulationinfo", views.sim_info),
	path("api/simrunner/stopsimulation", views.stop_simulation),
]
