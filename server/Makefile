# Targets
all: docker docker-run

docker:
	docker build -t server .

docker-run:
	docker run -p 5000:5000 -it server
