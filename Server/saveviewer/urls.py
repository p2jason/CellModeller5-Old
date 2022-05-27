from django.urls import path

from . import views

urlpatterns = [
    path("framedata", views.frame_data),
    path("allsimulations", views.get_all_simulations),
]