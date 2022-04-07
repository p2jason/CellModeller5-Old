from django.contrib import admin
from django.urls import path, include

from . import views

urlpatterns = [
	path("", views.index),
	path("api/saveviewer/", include("saveviewer.urls")),
	path("api/simrunner/", include("simrunner.urls")),
]
