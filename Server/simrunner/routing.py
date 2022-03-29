from django.urls import re_path, path

from . import consumers

websocket_urlpatterns = [
    re_path(r'ws/simcomms/(?P<sim_uuid>[0-9a-fA-F]{8}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{4}\-[0-9a-fA-F]{12})$', consumers.SimCommsConsumer.as_asgi()),
]