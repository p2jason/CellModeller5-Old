from django.urls import path

from . import apps
from . import consumers

websocket_urlpatterns = [
    path(f"ws/simcomms/{apps.DUMMY_UUID}", consumers.SimCommsConsumer.as_asgi()),
    path(f"ws/initlogs/{apps.DUMMY_UUID}", consumers.InitLogsConsumer.as_asgi()),
]