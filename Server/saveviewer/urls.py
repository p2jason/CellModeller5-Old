from django.urls import path

from . import views

urlpatterns = [
    path("framedata", views.frame_data),
    path("simulationinfo", views.sim_info),
    path("allsimulations", views.get_all_simulations),
]