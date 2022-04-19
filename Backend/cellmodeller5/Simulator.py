import pkgutil

import cellmodeller5.native as cmnative

def load_shader(path):
	return pkgutil.get_data(__name__, path).decode("utf-8")