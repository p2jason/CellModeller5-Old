from django.http import HttpResponse

def home(request):
    return HttpResponse(open("static/index.html", "rb"))
