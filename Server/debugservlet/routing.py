from django.urls import path

from . import apps
from . import consumers

from . import settings

websocket_urlpatterns = [
    path(f"ws/simcomms/{settings.DUMMY_UUID}", consumers.SimCommsConsumer.as_asgi()),
    path(f"ws/initlogs/{settings.DUMMY_UUID}", consumers.InitLogsConsumer.as_asgi()),
]