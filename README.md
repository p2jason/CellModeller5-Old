# CellModeller5

## Setup

Run the following to clone the repository;

	git clone --recursive https://github.com/p2jason/CellModeller5.git

## Server

To setup the server you need CellModeller4 and Django. Run the following to install the required Django packages:
	
	pip install django channels

To run the server, navigate to the server's root directory (under `Server/`) and run:

	python ./manage.py runserver